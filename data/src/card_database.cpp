#include "edopro_next/data/card_database.h"

#include <sqlite3.h>

#include <memory>
#include <utility>

namespace edopro_next::data {

namespace {

// The Link type bit, verified against ocgcore's own public API
// (ocgcore/ocgapi_constants.h:58, `#define TYPE_LINK 0x4000000`), which is
// also what gframe/ocgapi_constants.h mirrors and what
// DataManager::ParseDB tests to redirect `datas.def` into a link-marker
// mask instead of a defense value. This is a card-schema constant, not a
// legacy-client one - it is defined by the rules engine's own public
// header - so a single cited, internal constant here is the clean
// authoritative representation the alternative (copying gframe headers, or
// scattering the literal 0x4000000 with no citation) would not be.
constexpr std::uint32_t kTypeLinkBit = 0x4000000;

constexpr auto kSelectStmt =
	"SELECT datas.id,datas.ot,datas.alias,datas.setcode,datas.type,datas.atk,datas.def,"
	"datas.level,datas.race,datas.attribute,datas.category,"
	"texts.name,texts.desc,"
	"texts.str1,texts.str2,texts.str3,texts.str4,texts.str5,texts.str6,texts.str7,texts.str8,"
	"texts.str9,texts.str10,texts.str11,texts.str12,texts.str13,texts.str14,texts.str15,texts.str16 "
	"FROM datas,texts WHERE texts.id = datas.id ORDER BY texts.id;";

constexpr auto kSelectStmtLocale =
	"SELECT id,name,desc,"
	"str1,str2,str3,str4,str5,str6,str7,str8,str9,str10,str11,str12,str13,str14,str15,str16 "
	"FROM texts ORDER BY texts.id;";

struct SqliteCloser {
	void operator()(sqlite3* db) const noexcept {
		if(db)
			sqlite3_close(db);
	}
};
using UniqueDb = std::unique_ptr<sqlite3, SqliteCloser>;

struct StmtFinalizer {
	void operator()(sqlite3_stmt* stmt) const noexcept {
		if(stmt)
			sqlite3_finalize(stmt);
	}
};
using UniqueStmt = std::unique_ptr<sqlite3_stmt, StmtFinalizer>;

// SQLite's own contract for sqlite3_open()/sqlite3_open_v2() is that the
// filename is UTF-8, on every platform - it does its own UTF-8 -> UTF-16
// conversion internally on Windows. path::string() would instead give the
// platform's native narrow encoding (the active code page on Windows),
// which is not the same thing and would mis-open a path with non-ASCII
// characters there. u8string() is the portable way to get what SQLite
// actually wants.
std::string utf8_path(const std::filesystem::path& path) {
	const auto u8 = path.u8string();
	return std::string(u8.begin(), u8.end());
}

// Opens `path` read-only. Never creates, never writes - a missing file or a
// permission problem is reported as a load failure, not silently turned into
// a new empty database, which is what SQLITE_OPEN_READONLY without
// SQLITE_OPEN_CREATE guarantees.
UniqueDb open_readonly(const std::filesystem::path& path, std::string& error) {
	sqlite3* raw = nullptr;
	const auto rc = sqlite3_open_v2(utf8_path(path).c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
	UniqueDb db(raw);
	if(rc != SQLITE_OK) {
		error = db ? sqlite3_errmsg(db.get()) : sqlite3_errstr(rc);
		return nullptr;
	}
	return db;
}

UniqueStmt prepare(sqlite3* db, const char* sql, std::string& error) {
	sqlite3_stmt* raw = nullptr;
	if(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db);
		return nullptr;
	}
	return UniqueStmt(raw);
}

// Every text column this module reads is read as UTF-8 directly.
// DataManager has to branch on WCHAR_MAX because it stores std::wstring; this
// module stores std::string, so that detour does not exist here at all - this
// is the same sqlite3_column_text() accessor upstream itself uses on the
// platforms where wchar_t is UTF-32 (gframe/data_manager.cpp's #else
// branch), not a new decoding path.
std::string column_utf8(sqlite3_stmt* stmt, int column) {
	const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
	if(text == nullptr)
		return {};
	const auto length = static_cast<std::size_t>(sqlite3_column_bytes(stmt, column));
	return std::string(text, length);
}

// Decodes one row of kSelectStmt (columns 0-28) into a CardRecord, applying
// every transformation DataManager::ParseDB applies: the setcode unpack, the
// Link defense/link-marker swap, and the signed level/pendulum-scale unpack.
// See docs/architecture/card-database.md for the source citation of each.
CardRecord decode_row(sqlite3_stmt* stmt) {
	CardRecord record;
	record.code = static_cast<CardCode>(static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 0)));
	record.scope = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 1));
	record.alias = static_cast<CardCode>(static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 2)));

	const auto packed_setcode = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
	for(int slot = 0; slot < 4; ++slot) {
		const auto setcode = static_cast<std::uint16_t>((packed_setcode >> (slot * 16)) & 0xffffu);
		if(setcode != 0)
			record.setcodes.push_back(setcode);
	}

	record.type = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 4));

	const auto attack = sqlite3_column_int(stmt, 5);
	const auto raw_defense = sqlite3_column_int(stmt, 6);
	record.attack = attack;
	if((record.type & kTypeLinkBit) != 0) {
		record.link_marker = static_cast<std::uint32_t>(raw_defense);
		record.defense = 0;
	} else {
		record.link_marker = 0;
		record.defense = raw_defense;
	}

	// Bit-identical to DataManager::ParseDB: the sign comes from the whole
	// packed value, the magnitude from its low byte, and the scales from the
	// next two bytes up - all via signed arithmetic shift on the raw int, not
	// on a reinterpreted unsigned value, which would change the result for a
	// negative `level`.
	const std::int32_t packed_level = sqlite3_column_int(stmt, 7);
	record.level = packed_level < 0 ? -(packed_level & 0xff) : (packed_level & 0xff);
	record.left_scale = static_cast<std::uint32_t>((packed_level >> 24) & 0xff);
	record.right_scale = static_cast<std::uint32_t>((packed_level >> 16) & 0xff);

	record.race = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 8));
	record.attribute = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 9));
	record.category = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 10));

	record.name = column_utf8(stmt, 11);
	record.text = column_utf8(stmt, 12);
	for(int i = 0; i < 16; ++i)
		record.strings[static_cast<std::size_t>(i)] = column_utf8(stmt, 13 + i);

	return record;
}

struct LocaleRow {
	std::string name;
	std::string text;
	std::array<std::string, 16> strings{};
};

// Overlays a non-empty locale field onto `base`, leaving an empty locale
// field untouched. Deliberately per-field, not the whole-CardString swap
// DataManager::GetStrings()/GetDesc() perform - see
// docs/architecture/card-database.md#locale-overlay for why.
void apply_locale(CardRecord& base, const LocaleRow& locale) {
	if(!locale.name.empty())
		base.name = locale.name;
	if(!locale.text.empty())
		base.text = locale.text;
	for(std::size_t i = 0; i < base.strings.size(); ++i) {
		if(!locale.strings[i].empty())
			base.strings[i] = locale.strings[i];
	}
}

} // namespace

LoadResult CardDatabase::load_database(const std::filesystem::path& path) {
	std::string error;
	auto db = open_readonly(path, error);
	if(!db)
		return {false, "failed to open '" + path.string() + "': " + error, 0};

	auto stmt = prepare(db.get(), kSelectStmt, error);
	if(!stmt)
		return {false, "failed to prepare card query on '" + path.string() + "': " + error, 0};

	// Parsed into a private staging map first. Nothing is merged into
	// `cards_` until the whole file has been read without error, so a file
	// that fails partway through leaves the catalogue exactly as it was
	// before this call - a stronger guarantee than DataManager::ParseDB
	// itself provides (docs/architecture/card-database.md#load-atomicity).
	std::map<CardCode, CardRecord> staged;
	for(;;) {
		const auto step = sqlite3_step(stmt.get());
		if(step == SQLITE_DONE)
			break;
		if(step != SQLITE_ROW)
			return {false, "failed to read '" + path.string() + "': " + sqlite3_errmsg(db.get()), 0};
		auto record = decode_row(stmt.get());
		const auto code = record.code;
		staged.insert_or_assign(code, std::move(record));
	}

	const auto rows_loaded = staged.size();
	for(auto& [code, record] : staged)
		cards_.insert_or_assign(code, std::move(record));
	return {true, "", rows_loaded};
}

LoadResult CardDatabase::load_locale(const std::filesystem::path& path) {
	std::string error;
	auto db = open_readonly(path, error);
	if(!db)
		return {false, "failed to open '" + path.string() + "': " + error, 0};

	auto stmt = prepare(db.get(), kSelectStmtLocale, error);
	if(!stmt)
		return {false, "failed to prepare locale query on '" + path.string() + "': " + error, 0};

	std::map<CardCode, LocaleRow> staged;
	for(;;) {
		const auto step = sqlite3_step(stmt.get());
		if(step == SQLITE_DONE)
			break;
		if(step != SQLITE_ROW)
			return {false, "failed to read '" + path.string() + "': " + sqlite3_errmsg(db.get()), 0};
		const auto code =
			static_cast<CardCode>(static_cast<std::uint32_t>(sqlite3_column_int64(stmt.get(), 0)));
		LocaleRow row;
		row.name = column_utf8(stmt.get(), 1);
		row.text = column_utf8(stmt.get(), 2);
		for(int i = 0; i < 16; ++i)
			row.strings[static_cast<std::size_t>(i)] = column_utf8(stmt.get(), 3 + i);
		staged.insert_or_assign(code, std::move(row));
	}

	// Applied only to cards already present. A locale row for a code this
	// catalogue has not loaded via load_database() is ignored - see
	// docs/architecture/card-database.md#locale-overlay.
	std::size_t rows_applied = 0;
	for(auto& [code, row] : staged) {
		auto it = cards_.find(code);
		if(it == cards_.end())
			continue;
		apply_locale(it->second, row);
		++rows_applied;
	}
	return {true, "", rows_applied};
}

const CardRecord* CardDatabase::find(CardCode code) const noexcept {
	auto it = cards_.find(code);
	return it == cards_.end() ? nullptr : &it->second;
}

bool CardDatabase::contains(CardCode code) const noexcept {
	return cards_.find(code) != cards_.end();
}

} // namespace edopro_next::data

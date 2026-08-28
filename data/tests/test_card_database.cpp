// Exercises edopro_next::data::CardDatabase against tiny, synthetic SQLite
// databases built at runtime with the real `datas`/`texts` table layout
// documented in docs/architecture/card-database.md - never a committed
// Project Ignis `.cdb` file (CLAUDE.md forbids committing those) and never
// real card text.
#include "edopro_next/data/card_database.h"

#include <sqlite3.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::CardDatabase;
using edopro_next::data::CardRecord;

namespace {

// ---------------------------------------------------------------------
// Synthetic .cdb construction. Every schema fact here (table/column names,
// join shape) is verified against gframe/data_manager.cpp's SELECT
// statements - see docs/architecture/card-database.md.
// ---------------------------------------------------------------------

std::filesystem::path unique_temp_path(const char* label) {
	static std::atomic<int> counter{0};
	const auto n = counter.fetch_add(1);
	return std::filesystem::temp_directory_path() /
		("edopro_next_data_test_" + std::string(label) + "_" + std::to_string(n) + ".sqlite3");
}

class TempFile {
public:
	explicit TempFile(const char* label) : path_(unique_temp_path(label)) {
		std::filesystem::remove(path_);
	}
	~TempFile() { std::filesystem::remove(path_); }
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;

	const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};

// Test setup only: aborts the process if the synthetic schema itself cannot
// be created, since a broken fixture makes every check below it meaningless.
void run(sqlite3* db, const std::string& sql) {
	char* err = nullptr;
	if(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
		std::fprintf(stderr, "test fixture setup failed: %s\n  sql: %s\n",
					 err ? err : "unknown sqlite error", sql.c_str());
		sqlite3_free(err);
		std::abort();
	}
}

sqlite3* open_writable(const std::filesystem::path& path) {
	sqlite3* db = nullptr;
	const auto rc = sqlite3_open_v2(path.string().c_str(), &db,
									 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	EDOPRO_DATA_CHECK_EQ(rc, SQLITE_OK);
	return db;
}

void create_datas_texts_schema(sqlite3* db) {
	run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
			"alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
			"atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
			"race INTEGER NOT NULL, attribute INTEGER NOT NULL, category INTEGER NOT NULL);");
	run(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
			"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
			"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
			"str14 TEXT, str15 TEXT, str16 TEXT);");
}

void create_texts_only_schema(sqlite3* db) {
	run(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
			"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
			"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
			"str14 TEXT, str15 TEXT, str16 TEXT);");
}

struct DataRow {
	std::uint32_t id;
	std::uint32_t ot = 0;
	std::uint32_t alias = 0;
	std::uint64_t setcode = 0;
	std::uint32_t type = 0;
	std::int32_t atk = 0;
	std::int32_t def = 0;
	std::int32_t level = 0;
	std::uint64_t race = 0;
	std::uint32_t attribute = 0;
	std::uint32_t category = 0;
};

struct TextRow {
	std::uint32_t id;
	std::string name;
	std::string desc;
	std::array<std::string, 16> str{};
};

void bind_int64(sqlite3_stmt* stmt, int index, std::uint64_t value) {
	sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value));
}

void insert_data_row(sqlite3* db, const DataRow& row) {
	static constexpr auto sql =
		"INSERT INTO datas (id,ot,alias,setcode,type,atk,def,level,race,attribute,category) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?);";
	sqlite3_stmt* stmt = nullptr;
	EDOPRO_DATA_CHECK_EQ(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr), SQLITE_OK);
	bind_int64(stmt, 1, row.id);
	bind_int64(stmt, 2, row.ot);
	bind_int64(stmt, 3, row.alias);
	bind_int64(stmt, 4, row.setcode);
	bind_int64(stmt, 5, row.type);
	sqlite3_bind_int(stmt, 6, row.atk);
	sqlite3_bind_int(stmt, 7, row.def);
	sqlite3_bind_int(stmt, 8, row.level);
	bind_int64(stmt, 9, row.race);
	bind_int64(stmt, 10, row.attribute);
	bind_int64(stmt, 11, row.category);
	EDOPRO_DATA_CHECK_EQ(sqlite3_step(stmt), SQLITE_DONE);
	sqlite3_finalize(stmt);
}

void insert_text_row(sqlite3* db, const TextRow& row) {
	static constexpr auto sql =
		"INSERT INTO texts (id,name,desc,str1,str2,str3,str4,str5,str6,str7,str8,str9,str10,"
		"str11,str12,str13,str14,str15,str16) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
	sqlite3_stmt* stmt = nullptr;
	EDOPRO_DATA_CHECK_EQ(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr), SQLITE_OK);
	sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(row.id));
	sqlite3_bind_text(stmt, 2, row.name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, row.desc.c_str(), -1, SQLITE_TRANSIENT);
	for(int i = 0; i < 16; ++i)
		sqlite3_bind_text(stmt, 4 + i, row.str[static_cast<std::size_t>(i)].c_str(), -1,
						   SQLITE_TRANSIENT);
	EDOPRO_DATA_CHECK_EQ(sqlite3_step(stmt), SQLITE_DONE);
	sqlite3_finalize(stmt);
}

// A minimal, plausible card: present in both tables, no special fields set.
void insert_card(sqlite3* db, std::uint32_t id, const std::string& name) {
	insert_data_row(db, DataRow{id});
	TextRow text;
	text.id = id;
	text.name = name;
	insert_text_row(db, text);
}

constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeLink = 0x4000000;

} // namespace

EDOPRO_DATA_TEST(ordinary_monster_row_decodes_every_field) {
	TempFile file("ordinary");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	DataRow data{};
	data.id = 10000000;
	data.ot = 0x1 | 0x2; // SCOPE_OCG | SCOPE_TCG
	data.alias = 0;
	data.setcode = 0;
	data.type = kTypeMonster;
	data.atk = 2500;
	data.def = 2000;
	data.level = 7;
	data.race = 0x1; // one race bit
	data.attribute = 0x10; // one attribute bit
	data.category = 0;
	insert_data_row(db, data);

	TextRow text{};
	text.id = data.id;
	text.name = "Synthetic Warrior";
	text.desc = "A card that exists only for this test.";
	insert_text_row(db, text);
	sqlite3_close(db);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(result.ok);
	EDOPRO_DATA_CHECK_EQ(result.rows_loaded, 1u);

	const auto* record = catalogue.find(static_cast<CardCode>(data.id));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->scope, data.ot);
	EDOPRO_DATA_CHECK_EQ(record->type, data.type);
	EDOPRO_DATA_CHECK_EQ(record->attack, data.atk);
	EDOPRO_DATA_CHECK_EQ(record->defense, data.def);
	EDOPRO_DATA_CHECK_EQ(record->level, 7u);
	EDOPRO_DATA_CHECK_EQ(record->race, data.race);
	EDOPRO_DATA_CHECK_EQ(record->attribute, data.attribute);
	EDOPRO_DATA_CHECK_EQ(record->link_marker, 0u);
	EDOPRO_DATA_CHECK_EQ(record->name, text.name);
	EDOPRO_DATA_CHECK_EQ(record->text, text.desc);
}

EDOPRO_DATA_TEST(unicode_names_and_text_round_trip_as_utf8) {
	TempFile file("unicode");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	// A synthetic name, not real card text: Latin, CJK and an emoji, so the
	// UTF-8 path is exercised beyond ASCII without copying any actual
	// Project Ignis text.
	const std::string name = "Test Card é法兵 \U0001F409";
	const std::string desc = "テキスト description with an em dash — here.";

	insert_data_row(db, DataRow{424242});
	TextRow text{};
	text.id = 424242;
	text.name = name;
	text.desc = desc;
	text.str[0] = "ßéñ";
	insert_text_row(db, text);
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(424242));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, name);
	EDOPRO_DATA_CHECK_EQ(record->text, desc);
	EDOPRO_DATA_CHECK_EQ(record->strings[0], text.str[0]);
}

EDOPRO_DATA_TEST(negative_attack_and_defense_are_preserved) {
	TempFile file("negative_stats");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	// "?" ATK/DEF cards use negative values on the wire; they are not a
	// sentinel this module strips.
	DataRow data{};
	data.id = 55;
	data.type = kTypeMonster;
	data.atk = -1;
	data.def = -2;
	insert_data_row(db, data);
	insert_text_row(db, TextRow{55, "Question Mark Stats", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(55));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->attack, -1);
	EDOPRO_DATA_CHECK_EQ(record->defense, -2);
}

EDOPRO_DATA_TEST(positive_level_with_pendulum_scale_unpacks) {
	TempFile file("level_positive");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	// lscale=3, rscale=5, level=4: packed = (3<<24)|(5<<16)|4.
	DataRow data{};
	data.id = 77;
	data.type = kTypeMonster;
	data.level = (3 << 24) | (5 << 16) | 4;
	insert_data_row(db, data);
	insert_text_row(db, TextRow{77, "Pendulum Test", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(77));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->level, 4u);
	EDOPRO_DATA_CHECK_EQ(record->left_scale, 3u);
	EDOPRO_DATA_CHECK_EQ(record->right_scale, 5u);
}

EDOPRO_DATA_TEST(negative_level_wraps_to_the_same_uint32_upstream_stores) {
	TempFile file("level_negative");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	// raw = -249 (0xFFFFFF07). DataManager::ParseDB's formula is
	// `level = raw < 0 ? -(raw & 0xff) : (raw & 0xff)`, and `raw & 0xff` here
	// is 0x07 = 7, so the SIGNED INTERMEDIATE result is -7 - not -249, and
	// not naively "-(raw)". But CardData::level/CardDataC::level (and the
	// OCG_CardData::level the engine actually receives, via CardReader's
	// memcpy) are uint32_t, not int32_t - upstream's own assignment of that
	// signed -7 into an unsigned field wraps it, so the value actually
	// stored - and what this facade must expose to match upstream's own
	// deck-search level filter (gframe/deck_con.cpp, which compares this
	// field as uint32_t) - is static_cast<uint32_t>(-7), not -7 itself.
	// lscale/rscale are unaffected: (raw>>24)&0xff and (raw>>16)&0xff are
	// already small non-negative ints once the &0xff mask is applied, so
	// converting them to uint32_t changes nothing; both come out as 0xff =
	// 255 here, a mechanical consequence of arithmetic-shifting a negative
	// packed value, not a second encoding.
	DataRow data{};
	data.id = 88;
	data.type = kTypeMonster;
	data.level = -249;
	insert_data_row(db, data);
	insert_text_row(db, TextRow{88, "Negative Level Test", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(88));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->level, static_cast<std::uint32_t>(-7));
	EDOPRO_DATA_CHECK_EQ(record->level, 4294967289u);
	EDOPRO_DATA_CHECK_EQ(record->left_scale, 255u);
	EDOPRO_DATA_CHECK_EQ(record->right_scale, 255u);
}

EDOPRO_DATA_TEST(link_monster_defense_column_becomes_link_marker) {
	TempFile file("link_monster");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	DataRow data{};
	data.id = 99;
	data.type = kTypeMonster | kTypeLink;
	data.atk = 2000;
	data.def = 0x45; // link-marker bitmask, not a defense value
	insert_data_row(db, data);
	insert_text_row(db, TextRow{99, "Link Test", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(99));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->attack, 2000);
	EDOPRO_DATA_CHECK_EQ(record->defense, 0);
	EDOPRO_DATA_CHECK_EQ(record->link_marker, 0x45u);
}

EDOPRO_DATA_TEST(race_is_carried_as_a_full_64_bit_value) {
	TempFile file("race64");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	const std::uint64_t race = (std::uint64_t{1} << 40) | 0x8;
	DataRow data{};
	data.id = 111;
	data.type = kTypeMonster;
	data.race = race;
	insert_data_row(db, data);
	insert_text_row(db, TextRow{111, "High Bit Race", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(111));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->race, race);
}

EDOPRO_DATA_TEST(setcode_unpacks_only_nonzero_slots_in_order) {
	TempFile file("setcode");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);

	// Slot 1 (bits 16-31) left zero on purpose: the unpacked list must skip
	// it, not emit a placeholder zero in the middle.
	const std::uint64_t packed =
		std::uint64_t{0x0101} | (std::uint64_t{0} << 16) | (std::uint64_t{0x0205} << 32);
	DataRow data{};
	data.id = 222;
	data.setcode = packed;
	insert_data_row(db, data);
	insert_text_row(db, TextRow{222, "Setcode Test", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(222));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->setcodes.size(), 2u);
	EDOPRO_DATA_CHECK_EQ(record->setcodes[0], static_cast<std::uint16_t>(0x0101));
	EDOPRO_DATA_CHECK_EQ(record->setcodes[1], static_cast<std::uint16_t>(0x0205));
}

EDOPRO_DATA_TEST(all_zero_setcode_is_an_empty_list) {
	TempFile file("setcode_zero");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 223, "No Set");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(223));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK(record->setcodes.empty());
}

EDOPRO_DATA_TEST(alias_field_is_carried_verbatim) {
	TempFile file("alias");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 300, "Original Printing");
	DataRow aliased{};
	aliased.id = 301;
	aliased.alias = 300;
	insert_data_row(db, aliased);
	insert_text_row(db, TextRow{301, "Alternate Artwork", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	EDOPRO_DATA_CHECK_EQ(catalogue.find(static_cast<CardCode>(300))->alias,
						 edopro_next::data::CardCode::None);
	EDOPRO_DATA_CHECK_EQ(catalogue.find(static_cast<CardCode>(301))->alias,
						 static_cast<CardCode>(300));
}

EDOPRO_DATA_TEST(all_sixteen_auxiliary_strings_are_read) {
	TempFile file("aux_strings");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_data_row(db, DataRow{400});
	TextRow text{};
	text.id = 400;
	for(int i = 0; i < 16; ++i)
		text.str[static_cast<std::size_t>(i)] = "aux" + std::to_string(i + 1);
	insert_text_row(db, text);
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(static_cast<CardCode>(400));
	EDOPRO_DATA_CHECK(record != nullptr);
	for(int i = 0; i < 16; ++i)
		EDOPRO_DATA_CHECK_EQ(record->strings[static_cast<std::size_t>(i)],
							 "aux" + std::to_string(i + 1));
}

EDOPRO_DATA_TEST(card_present_in_only_one_table_is_not_loaded) {
	TempFile file("inner_join");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	// datas row with no matching texts row.
	insert_data_row(db, DataRow{500});
	// texts row with no matching datas row.
	insert_text_row(db, TextRow{501, "Orphan Text", "", {}});
	sqlite3_close(db);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(result.ok);
	EDOPRO_DATA_CHECK_EQ(result.rows_loaded, 0u);
	EDOPRO_DATA_CHECK(catalogue.find(static_cast<CardCode>(500)) == nullptr);
	EDOPRO_DATA_CHECK(catalogue.find(static_cast<CardCode>(501)) == nullptr);
}

EDOPRO_DATA_TEST(missing_card_lookup_returns_null) {
	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.find(static_cast<CardCode>(999999)) == nullptr);
	EDOPRO_DATA_CHECK(!catalogue.contains(static_cast<CardCode>(999999)));
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
	EDOPRO_DATA_CHECK(catalogue.empty());
}

EDOPRO_DATA_TEST(enumeration_is_deterministic_and_ascending) {
	TempFile file("enumeration");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	// Inserted out of order on purpose.
	insert_card(db, 300, "Third");
	insert_card(db, 100, "First");
	insert_card(db, 200, "Second");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);

	std::vector<std::uint32_t> codes;
	for(const auto& [code, record] : catalogue)
		codes.push_back(to_number(code));
	EDOPRO_DATA_CHECK_EQ(codes.size(), 3u);
	EDOPRO_DATA_CHECK_EQ(codes[0], 100u);
	EDOPRO_DATA_CHECK_EQ(codes[1], 200u);
	EDOPRO_DATA_CHECK_EQ(codes[2], 300u);

	// A second pass gives the same order - not a one-time artifact of
	// insertion, but a property of the underlying ordered container.
	std::vector<std::uint32_t> codes_again;
	for(const auto& [code, record] : catalogue)
		codes_again.push_back(to_number(code));
	EDOPRO_DATA_CHECK(codes == codes_again);
}

EDOPRO_DATA_TEST(later_database_completely_overwrites_a_duplicate_code) {
	TempFile first("dup_first");
	{
		sqlite3* db = open_writable(first.path());
		create_datas_texts_schema(db);
		DataRow data{};
		data.id = 700;
		data.atk = 100;
		insert_data_row(db, data);
		insert_text_row(db, TextRow{700, "Original Name", "Original text", {}});
		insert_card(db, 701, "Untouched By Second File");
		sqlite3_close(db);
	}
	TempFile second("dup_second");
	{
		sqlite3* db = open_writable(second.path());
		create_datas_texts_schema(db);
		DataRow data{};
		data.id = 700;
		data.atk = 9999;
		insert_data_row(db, data);
		insert_text_row(db, TextRow{700, "Replaced Name", "Replaced text", {}});
		sqlite3_close(db);
	}

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(first.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_database(second.path()).ok);

	// The card present in both files reflects only the second file - a full
	// overwrite, not a per-field merge of the two.
	const auto* record = catalogue.find(static_cast<CardCode>(700));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->attack, 9999);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Replaced Name"));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Replaced text"));

	// The card unique to the first file survives the second load.
	EDOPRO_DATA_CHECK(catalogue.find(static_cast<CardCode>(701)) != nullptr);
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 2u);
}

namespace {

// A base card with distinguishable name/text/all-16-strings, for the locale
// lifecycle tests below.
void insert_full_card(sqlite3* db, std::uint32_t id, const std::string& tag) {
	insert_data_row(db, DataRow{id});
	TextRow text{};
	text.id = id;
	text.name = tag + " Name";
	text.desc = tag + " Text";
	for(int i = 0; i < 16; ++i)
		text.str[static_cast<std::size_t>(i)] = tag + " Str" + std::to_string(i + 1);
	insert_text_row(db, text);
}

// A locale-only (texts-table-only) file with one row. Fields left as "" are
// genuinely empty in the row, not omitted. Ownership of the temp file is
// returned to the caller, who must keep it alive (in a local variable) for
// as long as any load_locale() call still needs to read it.
std::unique_ptr<TempFile> make_locale_file(const char* label, std::uint32_t id,
											const std::string& name, const std::string& desc,
											const std::string& str1 = "",
											const std::string& str2 = "") {
	auto file = std::make_unique<TempFile>(label);
	sqlite3* db = open_writable(file->path());
	create_texts_only_schema(db);
	TextRow text{};
	text.id = id;
	text.name = name;
	text.desc = desc;
	text.str[0] = str1;
	text.str[1] = str2;
	insert_text_row(db, text);
	sqlite3_close(db);
	return file;
}

// Same, but with name/text and all sixteen strings set from `tag`, mirroring
// insert_full_card() - for tests that need every field distinguishable.
std::unique_ptr<TempFile> make_full_locale_file(const char* label, std::uint32_t id,
												 const std::string& tag) {
	auto file = std::make_unique<TempFile>(label);
	sqlite3* db = open_writable(file->path());
	create_texts_only_schema(db);
	TextRow text{};
	text.id = id;
	text.name = tag + " Name";
	text.desc = tag + " Text";
	for(int i = 0; i < 16; ++i)
		text.str[static_cast<std::size_t>(i)] = tag + " Str" + std::to_string(i + 1);
	insert_text_row(db, text);
	sqlite3_close(db);
	return file;
}

} // namespace

// D. name/text fallback semantics: once a code is linked to the active
// locale at all, name and text are taken from the locale layer as a pair -
// even when one of them is empty - and do NOT fall back to the base value
// field by field. This pins CardDataM::GetStrings() returning the whole
// linked CardString or the whole base one, never a mix
// (gframe/data_manager.h:111-115).
EDOPRO_DATA_TEST(locale_name_and_text_are_linked_as_a_pair_not_per_field_fallback) {
	TempFile base("locale_base_nametext");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 800, "Base");
	sqlite3_close(db);

	const auto locale_file = make_locale_file("locale_nametext", 800, "Locale Name", "",
											   "Locale Str1", "");

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	const auto locale_result = catalogue.load_locale(locale_file->path());
	EDOPRO_DATA_CHECK(locale_result.ok);
	EDOPRO_DATA_CHECK_EQ(locale_result.rows_loaded, 1u);

	const auto* record = catalogue.find(static_cast<CardCode>(800));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Locale Name"));
	// The locale row's text is empty, and the code IS linked, so text stays
	// empty - it does not fall back to "Base Text".
	EDOPRO_DATA_CHECK_EQ(record->text, std::string(""));
}

// E. auxiliary-string fallback semantics: each of the sixteen strings falls
// back to its own base value independently whenever the locale layer's same
// slot is empty - pins CardDataM::GetDesc() (gframe/data_manager.h:116-124),
// which is deliberately NOT the same rule as D above.
EDOPRO_DATA_TEST(auxiliary_strings_fall_back_per_slot_when_locale_slot_is_empty) {
	TempFile base("locale_base_aux");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 801, "Base");
	sqlite3_close(db);

	const auto locale_file =
		make_locale_file("locale_aux", 801, "Locale Name", "Locale Text", "Locale Str1", "");

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_file->path()).ok);

	const auto* record = catalogue.find(static_cast<CardCode>(801));
	EDOPRO_DATA_CHECK(record != nullptr);
	// str1 non-empty in the locale row: overrides.
	EDOPRO_DATA_CHECK_EQ(record->strings[0], std::string("Locale Str1"));
	// str2 empty in the locale row: falls back to this one slot's base value.
	EDOPRO_DATA_CHECK_EQ(record->strings[1], std::string("Base Str2"));
	// Every other slot was never mentioned by the locale row either -
	// same per-slot fallback.
	for(int i = 2; i < 16; ++i)
		EDOPRO_DATA_CHECK_EQ(record->strings[static_cast<std::size_t>(i)],
							 "Base Str" + std::to_string(i + 1));
}

// A. base -> locale A -> clear -> base: every localized field returns to the
// original base value, matching the data-layer half of
// Game::ApplyLocale()'s unconditional ClearLocaleTexts() call
// (gframe/game.cpp:3994-3995).
EDOPRO_DATA_TEST(clearing_the_active_locale_restores_base_strings) {
	TempFile base("locale_base_clear");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 810, "Base");
	sqlite3_close(db);

	const auto locale_a_file = make_full_locale_file("locale_a_clear", 810, "Locale A");

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_a_file->path()).ok);

	// Sanity: locale A is visible before clearing.
	{
		const auto* record = catalogue.find(static_cast<CardCode>(810));
		EDOPRO_DATA_CHECK(record != nullptr);
		EDOPRO_DATA_CHECK_EQ(record->name, std::string("Locale A Name"));
	}

	catalogue.clear_locale();

	const auto* record = catalogue.find(static_cast<CardCode>(810));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Base Name"));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Base Text"));
	for(int i = 0; i < 16; ++i)
		EDOPRO_DATA_CHECK_EQ(record->strings[static_cast<std::size_t>(i)],
							 "Base Str" + std::to_string(i + 1));
}

// B. base -> locale A -> clear -> locale B, where B leaves some fields
// empty: nothing from A survives. An empty field in B falls back to base
// (per-slot, for the auxiliary strings) or resolves to empty (for
// name/text, per test D above) - never to A's value.
EDOPRO_DATA_TEST(switching_locale_after_clear_does_not_leak_the_previous_locale) {
	TempFile base("locale_base_switch");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 820, "Base");
	sqlite3_close(db);

	const auto locale_a_file = make_locale_file("locale_a_switch", 820, "Locale A Name",
												 "Locale A Text", "Locale A Str1", "Locale A Str2");
	// Locale B leaves name and str1 empty on purpose.
	const auto locale_b_file =
		make_locale_file("locale_b_switch", 820, "", "Locale B Text", "", "Locale B Str2");

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_a_file->path()).ok);
	catalogue.clear_locale();
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_b_file->path()).ok);

	const auto* record = catalogue.find(static_cast<CardCode>(820));
	EDOPRO_DATA_CHECK(record != nullptr);
	// Linked to B, whose name is empty: resolves to empty, not to A's name
	// and not to base's name.
	EDOPRO_DATA_CHECK_EQ(record->name, std::string(""));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Locale B Text"));
	// str1 empty in B: falls back to BASE, not to A's "Locale A Str1".
	EDOPRO_DATA_CHECK_EQ(record->strings[0], std::string("Base Str1"));
	EDOPRO_DATA_CHECK_EQ(record->strings[1], std::string("Locale B Str2"));
}

// C. Multiple locale files loaded into ONE active locale (no clear_locale()
// between them - contrast with test B above, which does clear between
// locales). A later file's row for a code an earlier file already touched
// replaces it completely, field by field, even with an empty value -
// matching DataManager::ParseLocaleDB reusing `locales[code]` across files
// and GetWstring unconditionally clearing an empty column
// (gframe/data_manager.cpp:195-223, :77-96). A card only the first file
// mentions is unaffected by the second file.
EDOPRO_DATA_TEST(later_locale_file_in_the_same_active_locale_overwrites_the_earlier_one) {
	TempFile base("locale_base_multi");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 830, "Base");
	insert_full_card(db, 831, "Base");
	sqlite3_close(db);

	// File 1 sets both cards.
	TempFile locale1("locale_multi_1");
	{
		sqlite3* locale_db = open_writable(locale1.path());
		create_texts_only_schema(locale_db);
		TextRow row830{};
		row830.id = 830;
		row830.name = "File1 Name";
		row830.str[0] = "File1 Str1";
		insert_text_row(locale_db, row830);
		TextRow row831{};
		row831.id = 831;
		row831.name = "File1 Only Name";
		insert_text_row(locale_db, row831);
		sqlite3_close(locale_db);
	}
	// File 2, same active locale, only re-mentions card 830 - and clears its
	// name to empty while giving str1 a new value.
	TempFile locale2("locale_multi_2");
	{
		sqlite3* locale_db = open_writable(locale2.path());
		create_texts_only_schema(locale_db);
		TextRow row830{};
		row830.id = 830;
		row830.name = ""; // explicitly empty - must overwrite File 1's "File1 Name"
		row830.str[0] = "File2 Str1";
		insert_text_row(locale_db, row830);
		sqlite3_close(locale_db);
	}

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale1.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale2.path()).ok);

	const auto* card830 = catalogue.find(static_cast<CardCode>(830));
	EDOPRO_DATA_CHECK(card830 != nullptr);
	// File 2 completely replaced File 1's row for 830: name is empty (not
	// "File1 Name"), str1 is File 2's value (not "File1 Str1").
	EDOPRO_DATA_CHECK_EQ(card830->name, std::string(""));
	EDOPRO_DATA_CHECK_EQ(card830->strings[0], std::string("File2 Str1"));

	// Card 831 was never mentioned by File 2, so File 1's row for it stands.
	const auto* card831 = catalogue.find(static_cast<CardCode>(831));
	EDOPRO_DATA_CHECK(card831 != nullptr);
	EDOPRO_DATA_CHECK_EQ(card831->name, std::string("File1 Only Name"));
}

// A base reload while a locale is active must not drop the locale overlay -
// matching DataManager::ParseDB's own re-link via `indexes` when a base row
// is parsed while a locale is active (see load_database()'s doc comment) -
// and the per-slot auxiliary fallback must resolve against the NEW base
// value, not a stale copy of the old one, proving base_text_ (not records_
// itself) is what resolve_text() actually reads.
EDOPRO_DATA_TEST(base_reload_keeps_the_active_locale_overlay_and_uses_the_new_base_fallback) {
	TempFile base_a("locale_base_reload_a");
	{
		sqlite3* db = open_writable(base_a.path());
		create_datas_texts_schema(db);
		insert_data_row(db, DataRow{860});
		TextRow text{};
		text.id = 860;
		text.name = "Base A Name";
		text.desc = "Base A Text";
		text.str[0] = "Base A Str1";
		insert_text_row(db, text);
		sqlite3_close(db);
	}
	// Locale: name overrides (linked as a pair), str1 left empty so it must
	// fall back to whatever the CURRENT base value is.
	const auto locale_file =
		make_locale_file("locale_base_reload", 860, "Locale Name", "Locale Text", "");

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base_a.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_file->path()).ok);

	// Sanity before the reload.
	{
		const auto* record = catalogue.find(static_cast<CardCode>(860));
		EDOPRO_DATA_CHECK(record != nullptr);
		EDOPRO_DATA_CHECK_EQ(record->name, std::string("Locale Name"));
		EDOPRO_DATA_CHECK_EQ(record->strings[0], std::string("Base A Str1"));
	}

	// A later base load overwrites card 860's base row entirely (also
	// changing a non-text field, to confirm that side of the reload too).
	TempFile base_b("locale_base_reload_b");
	{
		sqlite3* db = open_writable(base_b.path());
		create_datas_texts_schema(db);
		DataRow data{};
		data.id = 860;
		data.atk = 3000;
		insert_data_row(db, data);
		TextRow text{};
		text.id = 860;
		text.name = "Base B Name";
		text.desc = "Base B Text";
		text.str[0] = "Base B Str1";
		insert_text_row(db, text);
		sqlite3_close(db);
	}
	EDOPRO_DATA_CHECK(catalogue.load_database(base_b.path()).ok);

	const auto* record = catalogue.find(static_cast<CardCode>(860));
	EDOPRO_DATA_CHECK(record != nullptr);
	// The new base field is visible...
	EDOPRO_DATA_CHECK_EQ(record->attack, 3000);
	// ...the locale overlay is still active for name/text, untouched by the
	// base reload...
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Locale Name"));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Locale Text"));
	// ...and the empty locale str1 slot now falls back to base FILE B's
	// value, not a stale copy of base file A's.
	EDOPRO_DATA_CHECK_EQ(record->strings[0], std::string("Base B Str1"));
}

// F. A failed load_locale() call must leave the currently active locale
// exactly as it was, the same load-atomicity guarantee load_database()
// already gives the base layer.
EDOPRO_DATA_TEST(failed_locale_load_leaves_the_active_locale_unchanged) {
	TempFile base("locale_base_atomic");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 840, "Base");
	sqlite3_close(db);

	const auto locale_a_file =
		make_locale_file("locale_a_atomic", 840, "Locale A Name", "Locale A Text", "Locale A Str1");

	TempFile bad_locale("locale_bad_atomic");
	{
		// Present but wrong shape: no `texts` table at all.
		sqlite3* bad_db = open_writable(bad_locale.path());
		run(bad_db, "CREATE TABLE not_texts (id INTEGER PRIMARY KEY NOT NULL);");
		sqlite3_close(bad_db);
	}

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);
	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_a_file->path()).ok);

	const auto failed = catalogue.load_locale(bad_locale.path());
	EDOPRO_DATA_CHECK(!failed.ok);
	EDOPRO_DATA_CHECK(!failed.error.empty());

	const auto* record = catalogue.find(static_cast<CardCode>(840));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Locale A Name"));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Locale A Text"));
	EDOPRO_DATA_CHECK_EQ(record->strings[0], std::string("Locale A Str1"));
}

// G. clear_locale() with no active locale is a harmless no-op; base data
// remains visible, and clearing twice is equally harmless.
EDOPRO_DATA_TEST(clear_locale_without_an_active_locale_is_harmless) {
	TempFile base("locale_base_noop");
	sqlite3* db = open_writable(base.path());
	create_datas_texts_schema(db);
	insert_full_card(db, 850, "Base");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base.path()).ok);

	catalogue.clear_locale();
	catalogue.clear_locale();

	const auto* record = catalogue.find(static_cast<CardCode>(850));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Base Name"));
	EDOPRO_DATA_CHECK_EQ(record->text, std::string("Base Text"));
}

EDOPRO_DATA_TEST(locale_row_for_unknown_code_is_ignored) {
	const auto locale_file = make_locale_file("locale_unknown", 900123,
											   "Nobody Loaded This Card", "");

	CardDatabase catalogue;
	const auto result = catalogue.load_locale(locale_file->path());
	EDOPRO_DATA_CHECK(result.ok);
	EDOPRO_DATA_CHECK_EQ(result.rows_loaded, 0u);
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
}

EDOPRO_DATA_TEST(card_code_zero_is_rejected_as_a_load_failure) {
	TempFile file("code_zero");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	// 0 is reserved as CardCode::None ("no card") throughout this module and
	// upstream's own code (getRealCode(), DeckManager's dummy entries) -
	// never a real .cdb row. A valid row alongside it must not survive
	// either: the whole file is rejected atomically.
	insert_card(db, 0, "Should Never Load");
	insert_card(db, 950, "Valid Sibling Row");
	sqlite3_close(db);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
	EDOPRO_DATA_CHECK(catalogue.find(static_cast<CardCode>(950)) == nullptr);
}

EDOPRO_DATA_TEST(loading_a_missing_file_reports_failure) {
	const auto path =
		std::filesystem::temp_directory_path() / "edopro_next_data_test_does_not_exist.cdb";
	std::filesystem::remove(path);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(path);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
}

EDOPRO_DATA_TEST(loading_a_non_sqlite_file_reports_failure) {
	TempFile file("not_sqlite");
	{
		std::ofstream out(file.path(), std::ios::binary);
		out << "this is not a sqlite database, just some bytes\n";
	}

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
}

EDOPRO_DATA_TEST(loading_a_database_with_incompatible_schema_reports_failure) {
	TempFile file("bad_schema");
	sqlite3* db = open_writable(file.path());
	// A `datas` table missing every column this module requires, and no
	// `texts` table at all.
	run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, name TEXT);");
	run(db, "INSERT INTO datas (id, name) VALUES (1, 'wrong shape');");
	sqlite3_close(db);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
}

EDOPRO_DATA_TEST(loading_a_database_missing_one_required_column_reports_failure) {
	TempFile file("near_miss_schema");
	sqlite3* db = open_writable(file.path());
	// Both tables present, every column right except `datas` is missing
	// `category` - the kind of drift a hand-edited or partially-migrated
	// database could plausibly have, as opposed to a wholly different shape.
	run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
			"alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
			"atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
			"race INTEGER NOT NULL, attribute INTEGER NOT NULL);");
	run(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
			"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
			"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
			"str14 TEXT, str15 TEXT, str16 TEXT);");
	sqlite3_close(db);

	CardDatabase catalogue;
	const auto result = catalogue.load_database(file.path());
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 0u);
}

EDOPRO_DATA_TEST(failed_load_does_not_corrupt_previously_loaded_catalogue) {
	TempFile good("atomicity_good");
	{
		sqlite3* db = open_writable(good.path());
		create_datas_texts_schema(db);
		insert_card(db, 1000, "Survivor");
		sqlite3_close(db);
	}
	TempFile bad("atomicity_bad");
	{
		sqlite3* db = open_writable(bad.path());
		run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL);");
		sqlite3_close(db);
	}

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(good.path()).ok);
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 1u);

	const auto failed = catalogue.load_database(bad.path());
	EDOPRO_DATA_CHECK(!failed.ok);

	// The catalogue is exactly as it was before the failed call.
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 1u);
	const auto* record = catalogue.find(static_cast<CardCode>(1000));
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->name, std::string("Survivor"));

	// And a subsequent good load still works normally afterward.
	TempFile more("atomicity_more");
	{
		sqlite3* db = open_writable(more.path());
		create_datas_texts_schema(db);
		insert_card(db, 1001, "Loaded After Failure");
		sqlite3_close(db);
	}
	EDOPRO_DATA_CHECK(catalogue.load_database(more.path()).ok);
	EDOPRO_DATA_CHECK_EQ(catalogue.size(), 2u);
}

// Shared, test-only helpers for building tiny synthetic SQLite databases
// with the real `datas`/`texts` table layout (docs/architecture/
// card-database.md), used by both test_card_search.cpp and
// bench_card_search.cpp. Header-only (every function `inline`) so neither
// executable needs an extra library target just for this.
//
// Never a committed Project Ignis `.cdb` file (CLAUDE.md forbids
// committing those) and never real card text - every name/desc string
// used with this header is synthetic, generated at runtime.
#ifndef EDOPRO_NEXT_DATA_TESTS_SYNTHETIC_CDB_H
#define EDOPRO_NEXT_DATA_TESTS_SYNTHETIC_CDB_H

#include <sqlite3.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace edopro_next::data::testing {

inline std::filesystem::path unique_temp_path(const char* label) {
	static std::atomic<int> counter{0};
	const auto n = counter.fetch_add(1);
	return std::filesystem::temp_directory_path() /
		("edopro_next_search_synthetic_" + std::string(label) + "_" + std::to_string(n) + ".sqlite3");
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

// Test/benchmark setup only: aborts if the synthetic schema itself cannot
// be built, since a broken fixture makes everything built on it
// meaningless.
inline void run(sqlite3* db, const std::string& sql) {
	char* err = nullptr;
	if(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
		std::fprintf(stderr, "synthetic_cdb setup failed: %s\n  sql: %s\n",
					 err ? err : "unknown sqlite error", sql.c_str());
		sqlite3_free(err);
		std::abort();
	}
}

inline sqlite3* open_writable(const std::filesystem::path& path) {
	sqlite3* db = nullptr;
	const auto rc = sqlite3_open_v2(path.string().c_str(), &db,
									 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if(rc != SQLITE_OK) {
		std::fprintf(stderr, "synthetic_cdb: failed to open %s: %s\n", path.string().c_str(),
					 sqlite3_errstr(rc));
		std::abort();
	}
	return db;
}

inline void create_datas_texts_schema(sqlite3* db) {
	run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
			"alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
			"atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
			"race INTEGER NOT NULL, attribute INTEGER NOT NULL, category INTEGER NOT NULL);");
	run(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
			"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
			"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
			"str14 TEXT, str15 TEXT, str16 TEXT);");
}

// Locale files carry only `texts`, matching DataManager::ParseLocaleDB's
// own query (docs/architecture/card-database.md#1.2) - insert_text_row
// works unchanged against a database built with this schema instead of
// create_datas_texts_schema's.
inline void create_texts_only_schema(sqlite3* db) {
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

inline void bind_int64(sqlite3_stmt* stmt, int index, std::uint64_t value) {
	sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value));
}

inline void insert_data_row(sqlite3* db, const DataRow& row) {
	static constexpr auto sql =
		"INSERT INTO datas (id,ot,alias,setcode,type,atk,def,level,race,attribute,category) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?);";
	sqlite3_stmt* stmt = nullptr;
	if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		std::abort();
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
	if(sqlite3_step(stmt) != SQLITE_DONE)
		std::abort();
	sqlite3_finalize(stmt);
}

inline void insert_text_row(sqlite3* db, const TextRow& row) {
	static constexpr auto sql =
		"INSERT INTO texts (id,name,desc,str1,str2,str3,str4,str5,str6,str7,str8,str9,str10,"
		"str11,str12,str13,str14,str15,str16) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
	sqlite3_stmt* stmt = nullptr;
	if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		std::abort();
	sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(row.id));
	sqlite3_bind_text(stmt, 2, row.name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, row.desc.c_str(), -1, SQLITE_TRANSIENT);
	for(int i = 0; i < 16; ++i)
		sqlite3_bind_text(stmt, 4 + i, row.str[static_cast<std::size_t>(i)].c_str(), -1,
						   SQLITE_TRANSIENT);
	if(sqlite3_step(stmt) != SQLITE_DONE)
		std::abort();
	sqlite3_finalize(stmt);
}

inline void insert_card(sqlite3* db, std::uint32_t id, const std::string& name,
						 const std::string& desc = "") {
	insert_data_row(db, DataRow{id});
	TextRow text;
	text.id = id;
	text.name = name;
	text.desc = desc;
	insert_text_row(db, text);
}

} // namespace edopro_next::data::testing

#endif // EDOPRO_NEXT_DATA_TESTS_SYNTHETIC_CDB_H

#include "ydk_interop.h"

#include "data_manager.h"
#include "deck_manager.h"
#include "utils.h"

#include "edopro_next/data/deck.h"
#include "edopro_next/data/ydk.h"

#include <sqlite3.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edopro_next::legacy_observer {
namespace {

namespace next_data = edopro_next::data;

// ---------------------------------------------------------------------
// Fixture card codes and the fixed Deck built from them (§6 of the task
// brief this harness implements). Each code's role is load-bearing and
// documented in docs/architecture/ydk-interoperability.md#fixture:
//   A (kCardMain)    - an ordinary Main-deck card.
//   B (kCardFusion)  - a real Extra-deck-typed card, written by OUR
//                      serializer under "#main" - proves LoadDeck
//                      reclassifies it regardless of our file section.
//   C (kCardExtra)   - an ordinary, non-Extra-typed card, written
//                      explicitly under "#extra" - contrasts separated=false
//                      (where "#extra" is inert, so this line is read back
//                      as a plain Main-section line) against separated=true
//                      (where it is captured by the real extralist and kept
//                      in Extra unconditionally, with no type check at all).
//   D (kCardSide)    - an ordinary Side-deck card.
//   E (kCardUnknown) - a non-zero code never inserted into the synthetic
//                      database - the unknown-card asymmetry (§8 of
//                      docs/architecture/deck-model.md, reproduced here
//                      against the real loader instead of read from source).
constexpr std::uint32_t kCardMain = 10000001;
constexpr std::uint32_t kCardFusion = 10000002;
constexpr std::uint32_t kCardExtra = 10000003;
constexpr std::uint32_t kCardSide = 10000004;
constexpr std::uint32_t kCardUnknown = 10000099;

// ---------------------------------------------------------------------
// A minimal synthetic `datas`/`texts` SQLite fixture, built directly
// against the SQLite C API.
//
// This is a deliberate, independent, minimal implementation - NOT a reuse
// of data/tests/synthetic_cdb.h, which builds the same shaped schema for
// data/'s own CMake test suite (test_card_search.cpp, bench_card_search.cpp).
// Two reasons, recorded here rather than only in the doc, since this is the
// code the decision governs:
//  1. data/tests/ is scoped to data/'s own CMake test targets.
//     integration/legacy/ is a Premake-built, opt-in production harness
//     leg (the observer/interop build), not a data/ test - reaching into a
//     sibling module's tests/ directory from here would blur exactly the
//     ownership boundary CLAUDE.md's "Where code belongs" table draws.
//  2. This harness only ever reads the columns DeckManager::LoadDeck
//     actually consults for the load path (id, alias, type); every other
//     column (ot, setcode, atk, def, level, race, attribute, category) is
//     irrelevant here and left at a fixed 0 - a smaller, purpose-built
//     writer is clearer than a larger shared one used only partially.
// The schema itself mirrors DataManager::ParseDB's own query exactly
// (gframe/data_manager.cpp's SELECT_STMT: `datas`/`texts` joined on `id`).
constexpr auto kCreateSchemaSql =
	"CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
	"alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
	"atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
	"race INTEGER NOT NULL, attribute INTEGER NOT NULL, category INTEGER NOT NULL);"
	"CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
	"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
	"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
	"str14 TEXT, str15 TEXT, str16 TEXT);";

struct SyntheticCard {
	std::uint32_t code;
	std::uint32_t type;
};

// Aborts on failure: a broken fixture makes every comparison downstream
// meaningless, matching data/tests/synthetic_cdb.h's own stance on this -
// this is setup for a proof, not something to degrade gracefully from.
void sqlite_check(int rc, sqlite3* db, const char* what) {
	if(rc != SQLITE_OK && rc != SQLITE_DONE) {
		std::fprintf(stderr, "ydk interop: synthetic database setup failed (%s): %s\n", what,
					 db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
		std::abort();
	}
}

void insert_card(sqlite3* db, const SyntheticCard& card) {
	// Every name is a synthetic, generated-at-runtime placeholder, never
	// real card text - CLAUDE.md forbids committing or fabricating real
	// BabelCDB content, and DataManager::ParseDB only needs a non-null
	// string here, never a specific one.
	const std::string name = "synthetic card " + std::to_string(card.code);

	sqlite3_stmt* stmt = nullptr;
	sqlite_check(sqlite3_prepare_v2(db,
										"INSERT INTO datas "
										"(id,ot,alias,setcode,type,atk,def,level,race,attribute,category) "
										"VALUES (?,0,0,0,?,0,0,0,0,0,0);",
										-1, &stmt, nullptr),
				 db, "prepare datas insert");
	sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(card.code));
	sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(card.type));
	sqlite_check(sqlite3_step(stmt), db, "insert datas row");
	sqlite3_finalize(stmt);

	sqlite_check(sqlite3_prepare_v2(db,
										"INSERT INTO texts "
										"(id,name,desc,str1,str2,str3,str4,str5,str6,str7,str8,str9,str10,"
										"str11,str12,str13,str14,str15,str16) "
										"VALUES (?,?,'','','','','','','','','','','','','','','','','');",
										-1, &stmt, nullptr),
				 db, "prepare texts insert");
	sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(card.code));
	sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite_check(sqlite3_step(stmt), db, "insert texts row");
	sqlite3_finalize(stmt);
}

void build_synthetic_database(const std::filesystem::path& path, const std::vector<SyntheticCard>& cards) {
	sqlite3* db = nullptr;
	// Sequenced as its own statement, deliberately: db is an out-parameter
	// of this call, and sibling function-call arguments are only
	// indeterminately sequenced relative to each other in C++ - folding the
	// open call directly into sqlite_check(sqlite3_open_v2(...), db, ...)
	// would let the compiler legally read `db` (the second argument) before
	// the open call (the first argument) has written it.
	const int open_rc =
		sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	sqlite_check(open_rc, db, "open");
	sqlite_check(sqlite3_exec(db, kCreateSchemaSql, nullptr, nullptr, nullptr), db, "create schema");
	for(const auto& card : cards)
		insert_card(db, card);
	// Closing (rather than relying on process exit) forces the final flush
	// and releases the file lock before DataManager::OpenDb() opens the
	// same path read-only for the real load below.
	sqlite_check(sqlite3_close(db), nullptr, "close");
}

// A small RAII temp-file helper, local to this harness for the same reason
// build_synthetic_database() is: not a reuse of data/tests/synthetic_cdb.h's
// TempFile, kept minimal and self-contained.
class TempPath {
public:
	explicit TempPath(std::string_view suffix) {
		static std::atomic<int> counter{0};
		path_ = std::filesystem::temp_directory_path() /
			("edopro_next_ydk_interop_" + std::to_string(counter.fetch_add(1)) + std::string(suffix));
	}
	~TempPath() {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}
	TempPath(const TempPath&) = delete;
	TempPath& operator=(const TempPath&) = delete;

	const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};

// Installs the two legacy globals DeckManager::LoadDeckFromFile()/LoadDeck()
// actually touch (audited against gframe/deck_manager.cpp and
// gframe/data_manager.cpp - see docs/architecture/ydk-interoperability.md
// #minimal-legacy-state) and restores them to null on every exit path,
// including an exception, mirroring replay_verifier.cpp's own
// ActiveVerification RAII pattern. Deliberately does NOT construct
// ygo::GameConfig, ygo::SoundManager, ygo::Game, or any Irrlicht device -
// the load direction this harness exercises never touches any of them.
class ScopedLegacyState {
public:
	ScopedLegacyState() {
		ygo::gDataManager = &data_manager_;
		ygo::gdeckManager = &deck_manager_;
	}
	~ScopedLegacyState() {
		ygo::gdeckManager = nullptr;
		ygo::gDataManager = nullptr;
	}
	ScopedLegacyState(const ScopedLegacyState&) = delete;
	ScopedLegacyState& operator=(const ScopedLegacyState&) = delete;

	ygo::DataManager& data_manager() { return data_manager_; }

private:
	ygo::DataManager data_manager_;
	ygo::DeckManager deck_manager_;
};

next_data::Deck build_fixture_deck() {
	using next_data::CardCode;
	next_data::Deck deck;
	deck.main = { CardCode{kCardMain}, CardCode{kCardFusion}, CardCode{kCardUnknown} };
	deck.extra = { CardCode{kCardExtra} };
	deck.side = { CardCode{kCardSide} };
	return deck;
}

// A deterministic, per-card textual rendering of one loaded section:
// "<code>[,<code>...]", each entry the card's getRealCode() (upstream's own
// dummy-aware accessor - gframe/data_manager.h: `code ? code : alias`),
// with "[dummy]" appended whenever code == 0 - upstream's own in-memory
// marker for a placeholder entry GetDummyOrMappedCardData() synthesized
// rather than a real database row (gframe/deck_manager.cpp). Printing
// getRealCode() alone would silently hide exactly the distinction this
// harness's unknown-card case exists to surface.
std::string describe_section(const ygo::Deck::Vector& cards) {
	std::string out;
	for(const auto* card : cards) {
		if(!out.empty())
			out += ',';
		out += std::to_string(card->getRealCode());
		if(card->code == 0)
			out += "[dummy]";
	}
	return out;
}

struct SectionResult {
	std::string main;
	std::string extra;
	std::string side;
};

SectionResult describe(const ygo::Deck& deck) {
	return { describe_section(deck.main), describe_section(deck.extra), describe_section(deck.side) };
}

// Both expected results below are transcribed by hand from reading
// gframe/deck_manager.cpp's LoadCardList/LoadDeck source directly (see
// docs/architecture/ydk-interoperability.md#separated-modes for the
// line-by-line derivation) - never produced by running this harness, this
// project's own parser, or upstream itself and capturing the output. Only
// the already-fixed kCard* constants are reused to spell them out as text.
std::string join(std::uint32_t code) {
	return std::to_string(code);
}
std::string join(std::uint32_t first, std::uint32_t second) {
	return std::to_string(first) + "," + std::to_string(second);
}

const SectionResult kExpectedNonSeparated{
	join(kCardMain, kCardExtra), // main: A, then C (C falls through as a
								  // plain Main-section line, since "#extra"
								  // is inert with no extralist supplied)
	join(kCardFusion),           // extra: B, reclassified by card type
	join(kCardSide),             // side: D
};

const SectionResult kExpectedSeparated{
	std::to_string(kCardMain) + "," + std::to_string(kCardUnknown) + "[dummy]",
	// main: A, then E as a kept dummy (code==0, alias==E) - loadalways is
	// true when separated=true, so LoadDeck's drop-on-unknown branch does
	// not apply, and it fails the extra-deck reclassification's own
	// `cd->code != 0` gate, landing in the else branch: deck.main.
	join(kCardFusion, kCardExtra), // extra: B (reclassified from #main, as
									// above) then C (captured by the real
									// extralist and kept in Extra
									// unconditionally, with no type check)
	join(kCardSide),                // side: D
};

} // namespace

YdkInteropStats verify_ydk_interop(bool inject_fault) {
	YdkInteropStats stats;

	TempPath cdb_path(".cdb");
	build_synthetic_database(cdb_path.path(), {
		{kCardMain, TYPE_MONSTER},
		{kCardFusion, TYPE_MONSTER | TYPE_FUSION},
		{kCardExtra, TYPE_SPELL},
		{kCardSide, TYPE_TRAP},
		// kCardUnknown is deliberately never inserted.
	});

	TempPath ydk_path(".ydk");
	const auto fixture_deck = build_fixture_deck();
	const auto save_result = next_data::save_ydk(ydk_path.path(), fixture_deck);
	if(!save_result) {
		std::fprintf(stderr, "ydk interop: failed to write fixture .ydk: %s\n",
					 save_result.error.c_str());
		return stats;
	}

	ScopedLegacyState legacy;
	if(!legacy.data_manager().LoadDB(ygo::Utils::ToPathString(cdb_path.path().string()))) {
		std::fprintf(stderr, "ydk interop: failed to load synthetic database\n");
		return stats;
	}

	struct Mode {
		bool separated;
		const char* label;
		const SectionResult* expected;
	};
	const Mode modes[] = {
		{false, "separated=false", &kExpectedNonSeparated},
		{true, "separated=true", &kExpectedSeparated},
	};

	std::cout << "ydk interop: fixed synthetic fixture\n";
	bool all_matched = true;
	for(const auto& mode : modes) {
		ygo::Deck out;
		const bool loaded = ygo::DeckManager::LoadDeckFromFile(
			ygo::Utils::ToPathString(ydk_path.path().string()), out, mode.separated,
			ygo::RITUAL_LOCATION::DEFAULT);
		std::cout << mode.label << "\n";
		if(!loaded) {
			std::fprintf(stderr, "ydk interop: LoadDeckFromFile failed for %s\n", mode.label);
			all_matched = false;
			++stats.modes_compared;
			continue;
		}

		auto observed = describe(out);
		if(inject_fault && !mode.separated) {
			// Deliberately corrupts one already-observed comparison value,
			// AFTER the real loader has run, never the loading path itself
			// - the same principle as M2's fault injection
			// (integration/legacy/replay_verifier.cpp: `dInfo.lp[0] += 500`
			// after the real handler runs). Fixed and deterministic, so two
			// runs of --verify-ydk-interop-fault produce byte-identical
			// output.
			observed.main += ",424242";
		}

		++stats.modes_compared;
		const bool main_ok = observed.main == mode.expected->main;
		const bool extra_ok = observed.extra == mode.expected->extra;
		const bool side_ok = observed.side == mode.expected->side;
		const bool mode_ok = main_ok && extra_ok && side_ok;
		if(mode_ok)
			++stats.modes_matched;
		else
			all_matched = false;

		std::cout << "  main:  " << observed.main << "\n";
		std::cout << "  extra: " << observed.extra << "\n";
		std::cout << "  side:  " << observed.side << "\n";
		std::cout << "  expected main:  " << mode.expected->main << "\n";
		std::cout << "  expected extra: " << mode.expected->extra << "\n";
		std::cout << "  expected side:  " << mode.expected->side << "\n";
		std::cout << "  result: " << (mode_ok ? "match" : "MISMATCH") << "\n";
		if(!main_ok)
			std::cerr << "mismatch: " << mode.label << " main: got '" << observed.main
					   << "' expected '" << mode.expected->main << "'\n";
		if(!extra_ok)
			std::cerr << "mismatch: " << mode.label << " extra: got '" << observed.extra
					   << "' expected '" << mode.expected->extra << "'\n";
		if(!side_ok)
			std::cerr << "mismatch: " << mode.label << " side: got '" << observed.side
					   << "' expected '" << mode.expected->side << "'\n";
	}

	stats.completed = true;
	std::cout << "overall: " << (all_matched ? "match" : "mismatch") << "\n";
	return stats;
}

int verify_ydk_interop_cli(bool inject_fault) {
	const auto stats = verify_ydk_interop(inject_fault);
	return stats.equivalent() ? 0 : 1;
}

} // namespace edopro_next::legacy_observer

extern "C" int edopro_next_verify_ydk_interop_cli() noexcept {
	try {
		return edopro_next::legacy_observer::verify_ydk_interop_cli(false);
	} catch(const std::exception& e) {
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	} catch(...) {
		std::fprintf(stderr, "error: unknown exception\n");
		return 1;
	}
}

extern "C" int edopro_next_verify_ydk_interop_fault_cli() noexcept {
	try {
		return edopro_next::legacy_observer::verify_ydk_interop_cli(true);
	} catch(const std::exception& e) {
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	} catch(...) {
		std::fprintf(stderr, "error: unknown exception\n");
		return 1;
	}
}

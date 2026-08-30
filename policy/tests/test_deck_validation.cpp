// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins validate_deck() against the exact source-verified ordering and
// per-check semantics documented in docs/architecture/deck-legality.md
// (gframe/deck_manager.cpp's CheckDeckSize/CheckDeckContent/CheckCards,
// gframe/generic_duel.cpp's PlayerReady). Every test builds an explicit
// ValidationPolicy, per this module's own "no implicit defaults" rule
// (validation_policy.h) - permissive_policy() below is a *test-only*
// convenience for the fields a given test does not care about, not a
// production default.
//
// Synthetic CardDatabase fixtures only - never a committed Project Ignis
// `.cdb` (CLAUDE.md forbids that), matching the exact precedent already
// established in data/tests/synthetic_cdb.h and ui/tests. A fresh, small
// copy here rather than including that header directly, for the same
// reason data/tests/test_support.h is not shared with client/: a
// test-only include across a module boundary is exactly the kind of
// accidental coupling these boundaries exist to avoid.

#include "edopro_next/policy/deck_validation.h"

#include <sqlite3.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "edopro_next/data/card_database.h"
#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::CardDatabase;
using edopro_next::data::Deck;
using edopro_next::policy::AllowedCardPool;
using edopro_next::policy::DeckErrorType;
using edopro_next::policy::DeckSizePolicy;
using edopro_next::policy::LfList;
using edopro_next::policy::SectionSizeRange;
using edopro_next::policy::ValidationPolicy;
using edopro_next::policy::validate_deck;

namespace {

// TYPE_*/SCOPE_* bit values - see policy/src/deck_validation.cpp's own
// citation of the same constants against ocgcore/ocgapi_constants.h and
// gframe/data_manager.h. Duplicated here rather than exposed from the
// library, matching how these bits are opaque, caller-supplied data
// everywhere else in this project (CardEntry, CardSearchIndex).
constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeSpell = 0x2;
constexpr std::uint32_t kTypeTrap = 0x4;
constexpr std::uint32_t kTypeFusion = 0x40;
constexpr std::uint32_t kTypeRitual = 0x80;
constexpr std::uint32_t kTypeSynchro = 0x2000;
constexpr std::uint32_t kTypeXyz = 0x800000;
constexpr std::uint32_t kTypeLink = 0x4000000;
constexpr std::uint32_t kTypeSkill = 0x8000000;

constexpr std::uint32_t kScopeOcg = 0x1;
constexpr std::uint32_t kScopeTcg = 0x2;
constexpr std::uint32_t kScopeOcgTcg = kScopeOcg | kScopeTcg;
constexpr std::uint32_t kScopeLegend = 0x400;

struct SyntheticCard {
	std::uint32_t code;
	std::uint32_t alias = 0;
	std::uint32_t scope = kScopeOcgTcg;
	std::uint32_t type = kTypeMonster;
	std::int32_t attack = 0;
	std::int32_t defense = 0;
	std::int32_t level = 0;
};

void run_sql(sqlite3* db, const std::string& sql) {
	char* err = nullptr;
	if(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
		std::fprintf(stderr, "synthetic .cdb setup failed: %s\n", err ? err : "unknown error");
		sqlite3_free(err);
		std::abort();
	}
}

std::filesystem::path write_synthetic_database(const std::string& label,
												 const std::vector<SyntheticCard>& cards) {
	static std::atomic<int> counter{0};
	const auto path = std::filesystem::temp_directory_path() /
					   ("edopro_next_policy_test_" + label + "_" + std::to_string(counter++) + ".cdb");
	std::filesystem::remove(path);

	sqlite3* db = nullptr;
	if(sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
		std::abort();
	run_sql(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
				"alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
				"atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
				"race INTEGER NOT NULL, attribute INTEGER NOT NULL, category INTEGER NOT NULL);");
	run_sql(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
				"str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
				"str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
				"str14 TEXT, str15 TEXT, str16 TEXT);");
	for(const auto& card : cards) {
		std::ostringstream sql;
		sql << "INSERT INTO datas (id,ot,alias,setcode,type,atk,def,level,race,attribute,category) "
			<< "VALUES (" << card.code << "," << card.scope << "," << card.alias << ",0," << card.type
			<< "," << card.attack << "," << card.defense << "," << card.level << ",0,0,0);";
		run_sql(db, sql.str());
		std::ostringstream text_sql;
		text_sql << "INSERT INTO texts (id,name,desc) VALUES (" << card.code << ",'card" << card.code
				 << "','synthetic');";
		run_sql(db, text_sql.str());
	}
	sqlite3_close(db);
	return path;
}

CardDatabase load_database(const std::vector<SyntheticCard>& cards, const std::string& label) {
	const auto path = write_synthetic_database(label, cards);
	CardDatabase database;
	const auto result = database.load_database(path);
	if(!result.ok)
		std::abort();
	std::filesystem::remove(path);
	return database;
}

// A test-only convenience for the ValidationPolicy fields a given test does
// not care about - NOT a production default (validate_deck() itself has no
// such thing, and ValidationPolicy has no default constructor that would
// silently pick this). Every test still sets every field it actually cares
// about explicitly.
ValidationPolicy permissive_policy() {
	return ValidationPolicy{
		DeckSizePolicy{SectionSizeRange{0, 999}, SectionSizeRange{0, 999}, SectionSizeRange{0, 999}},
		AllowedCardPool::Any,
		0,
		false,
		true,
		std::nullopt,
	};
}

} // namespace

EDOPRO_POLICY_TEST(passingDeckReturnsNoError) {
	auto database = load_database({SyntheticCard{1}, SyntheticCard{2}, SyntheticCard{3}},
								   "passing");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}, CardCode{3}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::None);
}

EDOPRO_POLICY_TEST(mainCountOutOfRangeFails) {
	auto database = load_database({SyntheticCard{1}}, "main_count");
	Deck deck;
	deck.main = {CardCode{1}};
	auto policy = permissive_policy();
	policy.deck_sizes.main = SectionSizeRange{40, 60};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::MainCount);
	EDOPRO_POLICY_CHECK_EQ(error.count.current, 1u);
	EDOPRO_POLICY_CHECK_EQ(error.count.minimum, 40u);
	EDOPRO_POLICY_CHECK_EQ(error.count.maximum, 60u);
}

EDOPRO_POLICY_TEST(skillCardsExcludedFromMainCount) {
	// 1 skill + 1 ordinary card = counted Main size of 1, not 2. Exactly one
	// Skill card, not more - TooManySkills (gframe/deck_manager.cpp:214-215)
	// fires on more than one and would otherwise mask what this test means
	// to isolate.
	auto database = load_database(
		{SyntheticCard{1}, SyntheticCard{2, 0, kScopeOcgTcg, kTypeSkill}}, "skill_count");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}};
	auto policy = permissive_policy();
	policy.deck_sizes.main = SectionSizeRange{1, 1};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
}

EDOPRO_POLICY_TEST(extraAndSideCountOutOfRangeFail) {
	auto database =
		load_database({SyntheticCard{1, 0, kScopeOcgTcg, kTypeMonster | kTypeFusion},
						SyntheticCard{2}},
					  "extra_side_count");
	{
		Deck deck;
		deck.extra = {CardCode{1}};
		auto policy = permissive_policy();
		policy.deck_sizes.extra = SectionSizeRange{2, 15};
		auto error = validate_deck(deck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
	{
		Deck deck;
		deck.side = {CardCode{2}};
		auto policy = permissive_policy();
		policy.deck_sizes.side = SectionSizeRange{2, 15};
		auto error = validate_deck(deck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::SideCount);
	}
}

EDOPRO_POLICY_TEST(unknownCardDetectedWhenContentCheckingEnabled) {
	auto database = load_database({SyntheticCard{1}}, "unknown_card");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{999}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::UnknownCard);
	EDOPRO_POLICY_CHECK_EQ(error.card, CardCode{999});
}

EDOPRO_POLICY_TEST(unknownCardScanOrderIsMainThenExtraThenSide) {
	auto database = load_database({}, "unknown_order");
	Deck deck;
	deck.main = {CardCode{100}};
	deck.extra = {CardCode{200}};
	deck.side = {CardCode{300}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::UnknownCard);
	EDOPRO_POLICY_CHECK_EQ(error.card, CardCode{100});
}

EDOPRO_POLICY_TEST(contentCheckingDisabledSkipsUnknownCardAndContent) {
	auto database = load_database({}, "content_disabled");
	Deck deck;
	deck.main = {CardCode{999}};
	auto policy = permissive_policy();
	policy.deck_sizes.main = SectionSizeRange{0, 999};
	policy.content_checking_enabled = false;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
}

EDOPRO_POLICY_TEST(forbiddenTypeFailsBeforeLegendAndSkillChecks) {
	// TWO Legend monsters (a real TooManyLegends violation on its own) AND
	// a forbidden Fusion type present - forbidden type must win, since
	// CheckDeckContent tests it first (gframe/deck_manager.cpp:206-215).
	auto database = load_database(
		{SyntheticCard{1, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster},
		 SyntheticCard{2, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster},
		 SyntheticCard{3, 0, kScopeOcgTcg, kTypeMonster | kTypeFusion}},
		"forbidden_type");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}, CardCode{3}};
	auto policy = permissive_policy();
	policy.forbidden_types = kTypeFusion;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ForbiddenType);
}

EDOPRO_POLICY_TEST(legendMonsterCheckPrecedesLegendSpellCheck) {
	auto database = load_database(
		{SyntheticCard{1, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster},
		 SyntheticCard{2, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster},
		 SyntheticCard{3, 0, kScopeOcgTcg | kScopeLegend, kTypeSpell},
		 SyntheticCard{4, 0, kScopeOcgTcg | kScopeLegend, kTypeSpell}},
		"legend_order");
	Deck deck;
	// Two Legend monsters AND two Legend spells - both violate on their
	// own; upstream checks monster-legends first.
	deck.main = {CardCode{1}, CardCode{2}, CardCode{3}, CardCode{4}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::TooManyLegends);
}

EDOPRO_POLICY_TEST(legendMonsterCountedAcrossMainAndExtra) {
	auto database = load_database(
		{SyntheticCard{1, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster},
		 SyntheticCard{2, 0, kScopeOcgTcg | kScopeLegend, kTypeMonster | kTypeFusion}},
		"legend_main_extra");
	Deck deck;
	deck.main = {CardCode{1}};
	deck.extra = {CardCode{2}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::TooManyLegends);
}

EDOPRO_POLICY_TEST(tooManySkillsFails) {
	auto database = load_database({SyntheticCard{1, 0, kScopeOcgTcg, kTypeSkill},
									SyntheticCard{2, 0, kScopeOcgTcg, kTypeSkill}},
								   "too_many_skills");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}};
	auto policy = permissive_policy();

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::TooManySkills);
}

EDOPRO_POLICY_TEST(nullLflistSkipsScopeZoneCopyCountAndBanlist) {
	// A Fusion card sitting in Main (a real zone violation), an OCG-only
	// card under a TcgOnly policy (a real scope violation), and no LFList
	// at all (nullopt) - none of it is ever checked, matching
	// gframe/deck_manager.cpp:217-218's `if(!lflist) return ret;` exactly.
	auto database =
		load_database({SyntheticCard{1, 0, kScopeOcg, kTypeMonster | kTypeFusion}}, "null_lflist");
	Deck deck;
	deck.main = {CardCode{1}};
	auto policy = permissive_policy();
	policy.allowed_cards = AllowedCardPool::TcgOnly;
	policy.lflist = std::nullopt;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
}

EDOPRO_POLICY_TEST(concreteEmptyLflistStillEnforcesScopeZoneAndCopyCount) {
	// The SAME setup as nullLflistSkips... above, but with a concrete,
	// empty, non-whitelist LfList (upstream's synthetic "N/A" list) - this
	// is a DIFFERENT state from nullopt, and must still run every check.
	auto zoneDb =
		load_database({SyntheticCard{1, 0, kScopeOcg, kTypeMonster | kTypeFusion}}, "na_zone");
	{
		Deck deck;
		deck.main = {CardCode{1}};
		auto policy = permissive_policy();
		policy.lflist = LfList{}; // concrete, empty, non-whitelist
		auto error = validate_deck(deck, zoneDb, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
	auto scopeDb = load_database({SyntheticCard{2, 0, kScopeOcg, kTypeMonster}}, "na_scope");
	{
		Deck deck;
		deck.main = {CardCode{2}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::TcgOnly;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, scopeDb, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::OcgOnly);
	}
	auto copyDb = load_database({SyntheticCard{3}}, "na_copycount");
	{
		Deck deck;
		deck.main = {CardCode{3}, CardCode{3}, CardCode{3}, CardCode{3}};
		auto policy = permissive_policy();
		policy.lflist = LfList{};
		auto error = validate_deck(deck, copyDb, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::CardCount);
	}
}

EDOPRO_POLICY_TEST(scopeChecksPerAllowedCardPool) {
	auto ocgCard = load_database({SyntheticCard{1, 0, kScopeOcg, kTypeMonster}}, "scope_ocg");
	auto tcgCard = load_database({SyntheticCard{2, 0, kScopeTcg, kTypeMonster}}, "scope_tcg");
	auto unofficial =
		load_database({SyntheticCard{3, 0, kScopeLegend, kTypeMonster}}, "scope_unofficial");

	{
		Deck deck;
		deck.main = {CardCode{2}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::OcgOnly;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, tcgCard, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::TcgOnly);
	}
	{
		Deck deck;
		deck.main = {CardCode{1}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::TcgOnly;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, ocgCard, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::OcgOnly);
	}
	{
		// scope=SCOPE_LEGEND alone (0x400) is > 0x3, so CHECK_UNOFFICIAL
		// fires in OcgOnly/TcgOnly/OcgAndTcg regardless of OCG/TCG bits -
		// the exact magnitude-test quirk this module preserves.
		Deck deck;
		deck.main = {CardCode{3}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::OcgAndTcg;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, unofficial, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::UnofficialCard);
	}
	{
		// WithPrerelease uses a real bitwise test, not the magnitude quirk -
		// a Legend-only card (no OCG/TCG/prerelease bit) is still rejected,
		// but for a different, bit-based reason.
		Deck deck;
		deck.main = {CardCode{3}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::WithPrerelease;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, unofficial, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::UnofficialCard);
	}
	{
		// AllowedCardPool::Any performs no scope check at all.
		Deck deck;
		deck.main = {CardCode{3}};
		auto policy = permissive_policy();
		policy.allowed_cards = AllowedCardPool::Any;
		policy.lflist = LfList{};
		auto error = validate_deck(deck, unofficial, policy);
		EDOPRO_POLICY_CHECK(!error);
	}
}

EDOPRO_POLICY_TEST(mainScopeErrorPrecedesLaterCopyCountError) {
	// Card 1 is scope-invalid AND appears 4 times later in the deck (would
	// also fail CardCount on its own) - the scope error on its first
	// occurrence must win, since CheckCards evaluates scope before
	// copy-count for every card, in order.
	auto database = load_database({SyntheticCard{1, 0, kScopeTcg, kTypeMonster}}, "scope_before_copy");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{1}, CardCode{1}, CardCode{1}};
	auto policy = permissive_policy();
	policy.allowed_cards = AllowedCardPool::OcgOnly;
	policy.lflist = LfList{};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::TcgOnly);
}

EDOPRO_POLICY_TEST(extraSectionErrorPrecedesSideSectionError) {
	// An Extra-zone violation (ordinary card sitting in Extra) AND a
	// Side-section scope violation both present - Extra is checked first.
	auto database = load_database(
		{SyntheticCard{1, 0, kScopeOcgTcg, kTypeMonster}, SyntheticCard{2, 0, kScopeTcg, kTypeMonster}},
		"extra_before_side");
	Deck deck;
	deck.extra = {CardCode{1}}; // ordinary monster in Extra - zone violation
	deck.side = {CardCode{2}}; // TCG-only card under an OcgOnly policy
	auto policy = permissive_policy();
	policy.allowed_cards = AllowedCardPool::OcgOnly;
	policy.lflist = LfList{};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	EDOPRO_POLICY_CHECK_EQ(error.card, CardCode{1});
}

EDOPRO_POLICY_TEST(fourthCopySpreadAcrossAllThreeSectionsIsCaught) {
	auto database = load_database({SyntheticCard{1}}, "spread_copies");
	Deck deck;
	deck.main = {CardCode{1}, CardCode{1}};
	deck.side = {CardCode{1}, CardCode{1}};
	auto policy = permissive_policy();
	policy.lflist = LfList{};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::CardCount);
	EDOPRO_POLICY_CHECK_EQ(error.card, CardCode{1});
}

EDOPRO_POLICY_TEST(aliasesShareTheCopyCounter) {
	// Code 100 and its alias-target 50 share one copy-count slot: two of
	// each is four total against the shared cap of 3.
	auto database =
		load_database({SyntheticCard{50}, SyntheticCard{100, 50}}, "alias_shared_counter");
	Deck deck;
	deck.main = {CardCode{50}, CardCode{50}, CardCode{100}, CardCode{100}};
	auto policy = permissive_policy();
	policy.lflist = LfList{};

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::CardCount);
}

EDOPRO_POLICY_TEST(ordinaryLflistAliasFallbackAppliesTheOriginalsLimit) {
	// Card 200 has alias 100; the LFList only lists code 100 as limited to
	// 1. An ordinary (non-whitelist) list falls back to the alias
	// regardless of artwork-offset distance.
	auto database = load_database({SyntheticCard{100}, SyntheticCard{200, 100}},
								   "ordinary_alias_fallback");
	Deck deck;
	deck.main = {CardCode{200}, CardCode{200}};
	auto policy = permissive_policy();
	LfList list;
	list.whitelist = false;
	list.content[CardCode{100}] = 1;
	policy.lflist = list;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::Lflist);
}

EDOPRO_POLICY_TEST(whitelistAliasFallbackOutsideArtworkOffsetRangeIsNotApplied) {
	// Alias delta is 20 (>= 10) - a true errata/different-card alias, not
	// an artwork-variant reprint. A whitelist must NOT fall back to the
	// original's entry for this pair - the card is absent from the list,
	// which for a whitelist means illegal.
	auto database = load_database({SyntheticCard{100}, SyntheticCard{120, 100}},
								   "whitelist_outside_offset");
	Deck deck;
	deck.main = {CardCode{120}};
	auto policy = permissive_policy();
	LfList list;
	list.whitelist = true;
	list.content[CardCode{100}] = 3;
	policy.lflist = list;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::Lflist);
}

EDOPRO_POLICY_TEST(whitelistAliasFallbackInsideArtworkOffsetRangeIsApplied) {
	// Alias delta is 5 (< 10) - within the artwork-variant window, so a
	// whitelist DOES fall back to the original's entry for this pair.
	auto database = load_database({SyntheticCard{100}, SyntheticCard{105, 100}},
								   "whitelist_inside_offset");
	Deck deck;
	deck.main = {CardCode{105}};
	auto policy = permissive_policy();
	LfList list;
	list.whitelist = true;
	list.content[CardCode{100}] = 3;
	policy.lflist = list;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
}

EDOPRO_POLICY_TEST(whitelistMissingCardIsIllegal) {
	auto database = load_database({SyntheticCard{1}}, "whitelist_missing");
	Deck deck;
	deck.main = {CardCode{1}};
	auto policy = permissive_policy();
	LfList list;
	list.whitelist = true; // empty content - nothing is whitelisted
	policy.lflist = list;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::Lflist);
}

EDOPRO_POLICY_TEST(blacklistAbsentCardIsUnrestricted) {
	auto database = load_database({SyntheticCard{1}}, "blacklist_absent");
	Deck deck;
	deck.main = {CardCode{1}};
	auto policy = permissive_policy();
	LfList list;
	list.whitelist = false; // absence from an ordinary list means unrestricted
	policy.lflist = list;

	auto error = validate_deck(deck, database, policy);
	EDOPRO_POLICY_CHECK(!error);
}

EDOPRO_POLICY_TEST(ordinaryLevelMonsterAllowedInMainNotExtra) {
	auto database = load_database({SyntheticCard{1, 0, kScopeOcgTcg, kTypeMonster}}, "level_zone");
	auto policy = permissive_policy();
	policy.lflist = LfList{};

	{
		Deck deck;
		deck.main = {CardCode{1}};
		EDOPRO_POLICY_CHECK(!validate_deck(deck, database, policy));
	}
	{
		Deck deck;
		deck.extra = {CardCode{1}};
		auto error = validate_deck(deck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
}

EDOPRO_POLICY_TEST(fusionSynchroXyzLinkAllowedInExtraNotMain) {
	auto database = load_database(
		{SyntheticCard{1, 0, kScopeOcgTcg, kTypeMonster | kTypeFusion},
		 SyntheticCard{2, 0, kScopeOcgTcg, kTypeMonster | kTypeSynchro},
		 SyntheticCard{3, 0, kScopeOcgTcg, kTypeMonster | kTypeXyz},
		 SyntheticCard{4, 0, kScopeOcgTcg, kTypeMonster | kTypeLink}},
		"extra_types");
	auto policy = permissive_policy();
	policy.lflist = LfList{};

	for(auto code : {CardCode{1}, CardCode{2}, CardCode{3}, CardCode{4}}) {
		Deck extraDeck;
		extraDeck.extra = {code};
		EDOPRO_POLICY_CHECK(!validate_deck(extraDeck, database, policy));

		Deck mainDeck;
		mainDeck.main = {code};
		auto error = validate_deck(mainDeck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
}

EDOPRO_POLICY_TEST(ritualPlacementFollowsPolicy) {
	auto database =
		load_database({SyntheticCard{1, 0, kScopeOcgTcg, kTypeMonster | kTypeRitual}}, "ritual");

	{
		// rituals_belong_in_extra = false: Ritual is a Main card.
		auto policy = permissive_policy();
		policy.lflist = LfList{};
		policy.rituals_belong_in_extra = false;

		Deck mainDeck;
		mainDeck.main = {CardCode{1}};
		EDOPRO_POLICY_CHECK(!validate_deck(mainDeck, database, policy));

		Deck extraDeck;
		extraDeck.extra = {CardCode{1}};
		auto error = validate_deck(extraDeck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
	{
		// rituals_belong_in_extra = true: Ritual is an Extra card.
		auto policy = permissive_policy();
		policy.lflist = LfList{};
		policy.rituals_belong_in_extra = true;

		Deck extraDeck;
		extraDeck.extra = {CardCode{1}};
		EDOPRO_POLICY_CHECK(!validate_deck(extraDeck, database, policy));

		Deck mainDeck;
		mainDeck.main = {CardCode{1}};
		auto error = validate_deck(mainDeck, database, policy);
		EDOPRO_POLICY_CHECK_EQ(error.type, DeckErrorType::ExtraCount);
	}
}

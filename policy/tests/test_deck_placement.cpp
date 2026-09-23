// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins the one Extra Deck rule (edopro_next/policy/deck_placement.h) against
// upstream's own source, case by case, and pins that validate_deck()'s zone
// checks cannot drift away from it. Every expected value below is written out
// by hand from the upstream lines quoted in docs/architecture/
// deck-placement.md - never computed by calling the function under test - so
// a wrong rule fails here rather than agreeing with itself.
//
// Synthetic CardRecord values and synthetic .cdb fixtures only; no Project
// Ignis data is read or committed (AGENTS.md).

#include "edopro_next/policy/deck_placement.h"

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
#include "edopro_next/policy/deck_validation.h"
#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::CardDatabase;
using edopro_next::data::CardRecord;
using edopro_next::data::Deck;
using edopro_next::policy::AllowedCardPool;
using edopro_next::policy::belongs_in_extra_deck;
using edopro_next::policy::CardPlacement;
using edopro_next::policy::classify_card;
using edopro_next::policy::DeckErrorType;
using edopro_next::policy::DeckSizePolicy;
using edopro_next::policy::is_extra_deck_type;
using edopro_next::policy::is_ritual_monster;
using edopro_next::policy::is_rush;
using edopro_next::policy::is_token;
using edopro_next::policy::LfList;
using edopro_next::policy::ritual_placement_for;
using edopro_next::policy::RitualPlacement;
using edopro_next::policy::SectionSizeRange;
using edopro_next::policy::validate_deck;
using edopro_next::policy::ValidationPolicy;

namespace {

// ocgcore/ocgapi_constants.h:33-58 and gframe/data_manager.h:20-29.
constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeSpell = 0x2;
constexpr std::uint32_t kTypeTrap = 0x4;
constexpr std::uint32_t kTypeNormal = 0x10;
constexpr std::uint32_t kTypeEffect = 0x20;
constexpr std::uint32_t kTypeFusion = 0x40;
constexpr std::uint32_t kTypeRitual = 0x80;
constexpr std::uint32_t kTypeSynchro = 0x2000;
constexpr std::uint32_t kTypeToken = 0x4000;
constexpr std::uint32_t kTypeXyz = 0x800000;
constexpr std::uint32_t kTypePendulum = 0x1000000;
constexpr std::uint32_t kTypeLink = 0x4000000;

constexpr std::uint32_t kScopeOcgTcg = 0x3;
constexpr std::uint32_t kScopeRush = 0x200;

constexpr RitualPlacement kAllPlacements[] = {RitualPlacement::RushInExtra, RitualPlacement::Main,
											  RitualPlacement::Extra};

CardRecord card(std::uint32_t type, std::uint32_t scope = kScopeOcgTcg) {
	CardRecord record;
	record.code = CardCode{1};
	record.type = type;
	record.scope = scope;
	return record;
}

// ---- synthetic .cdb, same shape as test_deck_validation.cpp's local copy ----

struct SyntheticCard {
	std::uint32_t code;
	std::uint32_t type;
	std::uint32_t scope = kScopeOcgTcg;
};

void run_sql(sqlite3* db, const std::string& sql) {
	char* err = nullptr;
	if(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
		std::fprintf(stderr, "synthetic .cdb setup failed: %s\n", err ? err : "unknown error");
		sqlite3_free(err);
		std::abort();
	}
}

CardDatabase load_database(const std::vector<SyntheticCard>& cards, const std::string& label) {
	static std::atomic<int> counter{0};
	const auto path = std::filesystem::temp_directory_path() /
					   ("edopro_next_placement_test_" + label + "_" + std::to_string(counter++) + ".cdb");
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
	run_sql(db, "BEGIN;");
	for(const auto& c : cards) {
		std::ostringstream sql;
		sql << "INSERT INTO datas (id,ot,alias,setcode,type,atk,def,level,race,attribute,category) "
			<< "VALUES (" << c.code << "," << c.scope << ",0,0," << c.type << ",0,0,0,0,0,0);";
		run_sql(db, sql.str());
		std::ostringstream text_sql;
		text_sql << "INSERT INTO texts (id,name,desc) VALUES (" << c.code << ",'card" << c.code
				 << "','synthetic');";
		run_sql(db, text_sql.str());
	}
	run_sql(db, "COMMIT;");
	sqlite3_close(db);
	CardDatabase database;
	if(!database.load_database(path).ok)
		std::abort();
	std::filesystem::remove(path);
	return database;
}

} // namespace

// ---- Part 1: every classification case, against hand-written expectations ----

// deck_manager.cpp:336-337: Fusion/Synchro/Xyz go to the Extra Deck under
// every RITUAL_LOCATION, before the Ritual question is asked.
EDOPRO_POLICY_TEST(fusionSynchroXyzBelongInExtraUnderEveryRitualPlacement) {
	for(auto rituals : kAllPlacements) {
		EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeFusion), rituals));
		EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeSynchro), rituals));
		EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeXyz), rituals));
		EDOPRO_POLICY_CHECK(
			belongs_in_extra_deck(card(kTypeMonster | kTypeXyz | kTypePendulum), rituals));
	}
}

// deck_manager.cpp:338-339: a Link card goes to the Extra Deck only when it is
// also a Monster (deck-placement.md §2.1 for the literal expression).
EDOPRO_POLICY_TEST(linkMonsterBelongsInExtra) {
	for(auto rituals : kAllPlacements)
		EDOPRO_POLICY_CHECK(
			belongs_in_extra_deck(card(kTypeMonster | kTypeEffect | kTypeLink), rituals));
}

EDOPRO_POLICY_TEST(linkWithoutMonsterBelongsInMain) {
	for(auto rituals : kAllPlacements) {
		// A Link Spell - the one real shape (deck-placement.md §6).
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeSpell | kTypeLink), rituals));
		// The bare Link bit, and Link Trap: not Monsters either.
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeLink), rituals));
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeTrap | kTypeLink), rituals));
	}
}

// deck_manager.cpp:340-346 with rituals_in_extra == DEFAULT: a Ritual Monster
// goes where isRush() says.
EDOPRO_POLICY_TEST(ritualMonsterUnderRushInExtraFollowsRushScope) {
	const auto ritual = kTypeMonster | kTypeRitual;
	EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(ritual), RitualPlacement::RushInExtra));
	EDOPRO_POLICY_CHECK(
		belongs_in_extra_deck(card(ritual, kScopeRush), RitualPlacement::RushInExtra));
	EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(ritual | kTypeEffect, kScopeOcgTcg | kScopeRush),
											  RitualPlacement::RushInExtra));
}

// deck_manager.cpp:343-344: MAIN/EXTRA decide every Ritual Monster, Rush or
// not.
EDOPRO_POLICY_TEST(ritualMonsterUnderMainOrExtraIgnoresRushScope) {
	const auto ritual = kTypeMonster | kTypeRitual;
	for(auto scope : {kScopeOcgTcg, kScopeRush}) {
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(ritual, scope), RitualPlacement::Main));
		EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(ritual, scope), RitualPlacement::Extra));
	}
}

// data_manager.h:92-94: a Ritual Spell is not a Ritual Monster, so no
// RITUAL_LOCATION moves it.
EDOPRO_POLICY_TEST(ritualSpellIsNeverMovedByRitualPlacement) {
	for(auto rituals : kAllPlacements) {
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeSpell | kTypeRitual), rituals));
		EDOPRO_POLICY_CHECK(
			!belongs_in_extra_deck(card(kTypeSpell | kTypeRitual, kScopeRush), rituals));
	}
	EDOPRO_POLICY_CHECK(!is_ritual_monster(card(kTypeSpell | kTypeRitual)));
	EDOPRO_POLICY_CHECK(is_ritual_monster(card(kTypeMonster | kTypeRitual)));
}

// Rush scope alone moves nothing: isRush() is only consulted for a Ritual
// Monster (deck_manager.cpp:340-342).
EDOPRO_POLICY_TEST(rushScopeAloneDoesNotMoveANonRitualCard) {
	for(auto rituals : kAllPlacements) {
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeMonster | kTypeNormal, kScopeRush), rituals));
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeSpell, kScopeRush), rituals));
	}
	EDOPRO_POLICY_CHECK(is_rush(card(kTypeMonster, kScopeRush)));
	EDOPRO_POLICY_CHECK(!is_rush(card(kTypeMonster)));
}

// The lambda asks Fusion/Synchro/Xyz/Link first, so a Ritual Monster that is
// also one of those goes to the Extra Deck even under RITUAL_LOCATION::MAIN.
EDOPRO_POLICY_TEST(ritualHybridIsExtraBecauseTheTypeQuestionComesFirst) {
	EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeRitual | kTypeFusion),
											  RitualPlacement::Main));
	EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeRitual | kTypeLink),
											  RitualPlacement::Main));
	EDOPRO_POLICY_CHECK(belongs_in_extra_deck(card(kTypeMonster | kTypeRitual | kTypeSynchro),
											  RitualPlacement::RushInExtra));
}

EDOPRO_POLICY_TEST(ordinaryMonsterSpellAndTrapBelongInMain) {
	for(auto rituals : kAllPlacements) {
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeMonster | kTypeNormal), rituals));
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeMonster | kTypeEffect), rituals));
		EDOPRO_POLICY_CHECK(
			!belongs_in_extra_deck(card(kTypeMonster | kTypeEffect | kTypePendulum), rituals));
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeSpell), rituals));
		EDOPRO_POLICY_CHECK(!belongs_in_extra_deck(card(kTypeTrap), rituals));
	}
}

// generic_duel.cpp:423, deck_manager.cpp:407, menu_handler.cpp:315/:812:
// `rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN`.
EDOPRO_POLICY_TEST(duelFlagMapsToMainOrExtraNeverRushInExtra) {
	EDOPRO_POLICY_CHECK(ritual_placement_for(true) == RitualPlacement::Extra);
	EDOPRO_POLICY_CHECK(ritual_placement_for(false) == RitualPlacement::Main);
}

// deck_manager.cpp:357-358: a token is skipped before the lambda is asked -
// even a token carrying an Extra Deck type bit. An unknown code has nothing
// to classify.
EDOPRO_POLICY_TEST(tokensAndUnknownCodesAreNeverPlaced) {
	auto database = load_database(
		{
			SyntheticCard{10, kTypeMonster | kTypeNormal},
			SyntheticCard{20, kTypeMonster | kTypeFusion},
			SyntheticCard{30, kTypeMonster | kTypeToken},
			SyntheticCard{31, kTypeMonster | kTypeToken | kTypeFusion},
			SyntheticCard{40, kTypeMonster | kTypeRitual, kScopeRush},
		},
		"classify");
	for(auto rituals : kAllPlacements) {
		EDOPRO_POLICY_CHECK(classify_card(database, CardCode{10}, rituals) == CardPlacement::Main);
		EDOPRO_POLICY_CHECK(classify_card(database, CardCode{20}, rituals) == CardPlacement::Extra);
		EDOPRO_POLICY_CHECK(classify_card(database, CardCode{30}, rituals) == CardPlacement::Token);
		EDOPRO_POLICY_CHECK(classify_card(database, CardCode{31}, rituals) == CardPlacement::Token);
		EDOPRO_POLICY_CHECK(classify_card(database, CardCode{999}, rituals) ==
							CardPlacement::Unknown);
	}
	EDOPRO_POLICY_CHECK(classify_card(database, CardCode{40}, RitualPlacement::RushInExtra) ==
						CardPlacement::Extra);
	EDOPRO_POLICY_CHECK(classify_card(database, CardCode{40}, RitualPlacement::Main) ==
						CardPlacement::Main);
	EDOPRO_POLICY_CHECK(is_token(card(kTypeMonster | kTypeToken)));
	EDOPRO_POLICY_CHECK(!is_token(card(kTypeMonster)));
	CardDatabase empty;
	EDOPRO_POLICY_CHECK(classify_card(empty, CardCode{10}, RitualPlacement::RushInExtra) ==
						CardPlacement::Unknown);
}

// ---- Part 2: validate_deck() cannot drift from the rule ----
//
// Every combination of the eight type bits either rule reads, in both Rush
// and non-Rush scope, under both values of rituals_belong_in_extra. For each
// card: validate a one-card deck with it in Main, then with it in Extra.
//
// The expected results are transcribed independently from
// gframe/deck_manager.cpp:220-223 (Main callback) and :228-232 (Extra
// callback). The Main transcription is also required to equal
// belongs_in_extra_deck() for every card, which is what makes the lambda
// (:335-348) and the Main callback provably the same rule; the Extra
// transcription is required to equal "belongs in Extra" for every card except
// the one shape where upstream's two orders disagree (a Ritual Monster that is
// also Fusion/Synchro/Xyz/Link Monster), which is counted and checked too.
EDOPRO_POLICY_TEST(validationZoneChecksAgreeWithTheRuleForEveryTypeCombination) {
	constexpr std::uint32_t kBits[] = {kTypeMonster, kTypeSpell,   kTypeTrap, kTypeFusion,
									   kTypeRitual,  kTypeSynchro, kTypeXyz,  kTypeLink};
	constexpr std::uint32_t kCombinations = 1u << 8;

	std::vector<SyntheticCard> cards;
	for(std::uint32_t scope_index = 0; scope_index < 2; ++scope_index) {
		for(std::uint32_t mask = 0; mask < kCombinations; ++mask) {
			std::uint32_t type = 0;
			for(std::uint32_t bit = 0; bit < 8; ++bit)
				if(mask & (1u << bit))
					type |= kBits[bit];
			const std::uint32_t code = 1 + scope_index * kCombinations + mask;
			cards.push_back(SyntheticCard{code, type, scope_index ? kScopeOcgTcg | kScopeRush : kScopeOcgTcg});
		}
	}
	const auto database = load_database(cards, "equivalence");

	int checked = 0;
	int upstream_order_disagreements = 0;
	for(bool rituals_in_extra : {false, true}) {
		const ValidationPolicy policy{
			DeckSizePolicy{SectionSizeRange{0, 999}, SectionSizeRange{0, 999}, SectionSizeRange{0, 999}},
			AllowedCardPool::Any,
			0,
			rituals_in_extra,
			true,
			LfList{},
		};
		for(const auto& c : cards) {
			const std::uint32_t t = c.type;
			const bool fsx = (t & (kTypeFusion | kTypeSynchro | kTypeXyz)) != 0;
			const bool link_monster = (t & kTypeLink) && (t & kTypeMonster);
			const bool ritual_monster = (t & (kTypeMonster | kTypeRitual)) == (kTypeMonster | kTypeRitual);

			// deck_manager.cpp:220-223.
			const bool upstream_main_rejects = fsx || link_monster || (ritual_monster && rituals_in_extra);
			// deck_manager.cpp:228-232.
			const bool upstream_extra_rejects =
				ritual_monster ? !rituals_in_extra : (!fsx && !link_monster);

			const auto* record = database.find(CardCode{c.code});
			EDOPRO_POLICY_CHECK(record != nullptr);
			if(!record)
				continue;
			const bool rule = belongs_in_extra_deck(*record, ritual_placement_for(rituals_in_extra));
			EDOPRO_POLICY_CHECK_EQ(rule, upstream_main_rejects);
			if(ritual_monster && (fsx || link_monster)) {
				// The hybrid: the lambda says Extra, the Extra callback says
				// "only if rituals_in_extra". Upstream's own disagreement.
				EDOPRO_POLICY_CHECK(rule);
				EDOPRO_POLICY_CHECK_EQ(upstream_extra_rejects, !rituals_in_extra);
				if(rule && upstream_extra_rejects)
					++upstream_order_disagreements;
			} else {
				EDOPRO_POLICY_CHECK_EQ(upstream_extra_rejects, !rule);
			}

			Deck in_main;
			in_main.main = {CardCode{c.code}};
			const auto main_error = validate_deck(in_main, database, policy);
			EDOPRO_POLICY_CHECK_EQ(main_error.type == DeckErrorType::ExtraCount, upstream_main_rejects);
			EDOPRO_POLICY_CHECK_EQ(main_error.type == DeckErrorType::None, !upstream_main_rejects);

			Deck in_extra;
			in_extra.extra = {CardCode{c.code}};
			const auto extra_error = validate_deck(in_extra, database, policy);
			EDOPRO_POLICY_CHECK_EQ(extra_error.type == DeckErrorType::ExtraCount, upstream_extra_rejects);
			EDOPRO_POLICY_CHECK_EQ(extra_error.type == DeckErrorType::None, !upstream_extra_rejects);
			++checked;
		}
	}
	// 2 scopes x 256 type combinations x 2 values of rituals_in_extra.
	EDOPRO_POLICY_CHECK_EQ(checked, 1024);
	// A hybrid has Ritual and Monster set and at least one of Fusion,
	// Synchro, Xyz, Link (2^4 - 1 = 15 ways), with Spell and Trap free
	// (x 4) = 60 type combinations per scope, x 2 scopes = 120 cards. They
	// disagree only when rituals_in_extra is false (the Extra callback
	// rejects them while the lambda says Extra), so exactly 120.
	EDOPRO_POLICY_CHECK_EQ(upstream_order_disagreements, 120);
}

// The unconditional half on its own - the piece validate_deck()'s Extra
// callback reuses.
EDOPRO_POLICY_TEST(extraDeckTypeIsTheUnconditionalHalfOnly) {
	EDOPRO_POLICY_CHECK(is_extra_deck_type(card(kTypeMonster | kTypeFusion)));
	EDOPRO_POLICY_CHECK(is_extra_deck_type(card(kTypeMonster | kTypeLink)));
	EDOPRO_POLICY_CHECK(!is_extra_deck_type(card(kTypeSpell | kTypeLink)));
	EDOPRO_POLICY_CHECK(!is_extra_deck_type(card(kTypeMonster | kTypeRitual, kScopeRush)));
}

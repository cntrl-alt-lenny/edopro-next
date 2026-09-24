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
#include <iterator>
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
		// Link and Spell bits, no Monster bit (deck-placement.md §2.1). This is a
		// bit combination the rule must handle, not a claim that any card has it.
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

// ---- Part 3: upstream's deck builder against this project's rule ----
//
// deck-placement.md §3.3 records every card shape for which upstream's deck
// builder (DeckBuilder::push_main / push_extra, gframe/deck_con.cpp:1577-1636)
// puts a card somewhere other than where this project's rule (the
// is_extra_deck_card lambda under RITUAL_LOCATION::DEFAULT,
// gframe/deck_manager.cpp:335-348) puts it. This test is what keeps that
// record exact: it transcribes the two push functions, runs every
// combination of the type bits either rule reads in both scopes, and requires
// the set of cards the two disagree on to be exactly the union of the
// recorded families - none missing, none extra, each with the outcome the
// record states and the card count it states.
//
// Changing the transcription, the recorded families or belongs_in_extra_deck()
// without the other two makes this fail.
namespace {

enum class Lands { Main, Extra, Side };

constexpr const char* name_of(Lands where) {
	switch(where) {
	case Lands::Main:
		return "Main";
	case Lands::Extra:
		return "Extra";
	case Lands::Side:
		return "Side";
	}
	return "?";
}

bool is_ritual_monster_bits(std::uint32_t type) {
	return (type & (kTypeMonster | kTypeRitual)) == (kTypeMonster | kTypeRitual);
}

bool has_fusion_synchro_xyz(std::uint32_t type) {
	return (type & (kTypeFusion | kTypeSynchro | kTypeXyz)) != 0;
}

// gframe/deck_con.cpp:1577-1588, DeckBuilder::push_main, the type gates only:
// forced is false, mainGame->is_siding is false, and the Legend / Skill / 60
// card refusals at :1590-1601 do not fire (the section is not full).
//
//   if(pointer->isRitualMonster()) {
//   	if(mainGame->is_siding) { ... }
//   	else if(pointer->isRush() && !forced)
//   		return false;
//   }
//   if(pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
//   	return false;
//   if((pointer->type & (TYPE_LINK | TYPE_SPELL)) == TYPE_LINK)
//   	return false;
bool builder_push_main_accepts(std::uint32_t type, bool rush) {
	if(is_ritual_monster_bits(type)) {
		if(rush)
			return false;
	}
	if(type & (kTypeFusion | kTypeSynchro | kTypeXyz))
		return false;
	if((type & (kTypeLink | kTypeSpell)) == kTypeLink)
		return false;
	return true;
}

// gframe/deck_con.cpp:1610-1621, DeckBuilder::push_extra, the type gates only,
// under the same conditions (the 15 card and Legend refusals at :1623-1628 do
// not fire).
//
//   if(pointer->isRitualMonster()) {
//   	if(mainGame->is_siding) { ... }
//   	else if(!pointer->isRush() && !forced)
//   		return false;
//   } else if(pointer->type & TYPE_LINK) {
//   	if(pointer->type & TYPE_SPELL)
//   		return false;
//   } else if((pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) == 0)
//   	return false;
bool builder_push_extra_accepts(std::uint32_t type, bool rush) {
	if(is_ritual_monster_bits(type)) {
		if(!rush)
			return false;
	} else if(type & kTypeLink) {
		if(type & kTypeSpell)
			return false;
	} else if((type & (kTypeFusion | kTypeSynchro | kTypeXyz)) == 0)
		return false;
	return true;
}

// The call sites that add a card unforced outside side-decking try the two
// pushes in one of two orders and fall back to Side (deck-placement.md §3.2):
// right-click, deck_con.cpp:725-726, `!push_main(...) && !push_extra(...)`
// then push_side; middle-click, :767-769, `!push_extra(...) && !push_main(...)`
// then push_side.
Lands builder_lands(std::uint32_t type, bool rush, bool main_first) {
	const bool main = builder_push_main_accepts(type, rush);
	const bool extra = builder_push_extra_accepts(type, rush);
	if(main_first) {
		if(main)
			return Lands::Main;
		if(extra)
			return Lands::Extra;
	} else {
		if(extra)
			return Lands::Extra;
		if(main)
			return Lands::Main;
	}
	return Lands::Side;
}

// One recorded family: the cards (type bits + scope) for which the deck
// builder and this project disagree, with both outcomes and how many
// (type, scope) combinations of the eight bits it holds. The predicates are
// written from the family's description in deck-placement.md §3.3, not from
// the transcription above, so a slip in one is not repeated in the other.
struct RecordedFamily {
	const char* id;
	bool (*matches)(std::uint32_t type, bool rush);
	Lands upstream_builder;
	Lands this_project;
	int cards;
};

const RecordedFamily kRecordedFamilies[] = {
	// A: a Link card that is neither a Monster nor a Spell (and not
	// Fusion/Synchro/Xyz). Builder: push_main refuses it (:1587), push_extra
	// takes its Link branch (:1617-1619) -> Extra. Project: Link needs the
	// Monster bit -> Main. {Trap, Ritual} free x 2 scopes.
	{"A link-not-monster-not-spell",
	 [](std::uint32_t t, bool) {
		 return (t & kTypeLink) && !(t & kTypeSpell) && !(t & kTypeMonster) &&
				!has_fusion_synchro_xyz(t);
	 },
	 Lands::Extra, Lands::Main, 8},
	// B1: Link + Spell + Monster, not Ritual, no Fusion/Synchro/Xyz.
	// Builder: push_main lets it through (:1587 needs Link without Spell),
	// push_extra refuses it (:1618) -> Main. Project: Link and Monster ->
	// Extra. {Trap} free x 2 scopes.
	{"B1 link-spell-monster",
	 [](std::uint32_t t, bool) {
		 return (t & kTypeLink) && (t & kTypeSpell) && (t & kTypeMonster) && !(t & kTypeRitual) &&
				!has_fusion_synchro_xyz(t);
	 },
	 Lands::Main, Lands::Extra, 4},
	// B2: the same with the Ritual bit, so a Ritual Monster, outside Rush
	// scope. Builder: push_main lets it through (a non-Rush Ritual Monster
	// passes :1582, and :1585/:1587 do not fire), push_extra refuses a
	// non-Rush Ritual Monster (:1615) -> Main. Project: Link and Monster ->
	// Extra. In Rush scope both agree on Extra (push_main refuses at :1582).
	// {Trap} free.
	{"B2 ritual-link-spell-monster-not-rush",
	 [](std::uint32_t t, bool rush) {
		 return !rush && is_ritual_monster_bits(t) && (t & kTypeLink) && (t & kTypeSpell) &&
				!has_fusion_synchro_xyz(t);
	 },
	 Lands::Main, Lands::Extra, 2},
	// C1: a non-Rush Ritual Monster that is also Link, without Spell, no
	// Fusion/Synchro/Xyz. Builder: push_main refuses (:1587), push_extra
	// refuses a non-Rush Ritual Monster (:1615) -> Side. Project: Link and
	// Monster -> Extra. {Trap} free.
	{"C1 ritual-link-not-spell-not-rush",
	 [](std::uint32_t t, bool rush) {
		 return !rush && is_ritual_monster_bits(t) && (t & kTypeLink) && !(t & kTypeSpell) &&
				!has_fusion_synchro_xyz(t);
	 },
	 Lands::Side, Lands::Extra, 2},
	// C2: a non-Rush Ritual Monster that is also Fusion, Synchro or Xyz (with
	// any Link/Spell/Trap bits). Builder: push_main refuses (:1585),
	// push_extra refuses a non-Rush Ritual Monster (:1615) -> Side.
	// Project: Fusion/Synchro/Xyz -> Extra. 7 x {Link, Spell, Trap} free.
	{"C2 ritual-fusion-synchro-xyz-not-rush",
	 [](std::uint32_t t, bool rush) {
		 return !rush && is_ritual_monster_bits(t) && has_fusion_synchro_xyz(t);
	 },
	 Lands::Side, Lands::Extra, 56},
	// D: not a Ritual Monster, with Link, Spell and at least one of
	// Fusion/Synchro/Xyz. Builder: push_main refuses (:1585), push_extra
	// takes its Link branch and refuses the Spell (:1617-1618) and never
	// reaches the Fusion/Synchro/Xyz test -> Side. Project:
	// Fusion/Synchro/Xyz -> Extra. 7 x 3 (Monster/Ritual not both) x
	// {Trap} free x 2 scopes.
	{"D link-spell-fusion-synchro-xyz-not-ritual-monster",
	 [](std::uint32_t t, bool) {
		 return !is_ritual_monster_bits(t) && (t & kTypeLink) && (t & kTypeSpell) &&
				has_fusion_synchro_xyz(t);
	 },
	 Lands::Side, Lands::Extra, 84},
};

constexpr int kRecordedDisagreements = 8 + 4 + 2 + 2 + 56 + 84;

} // namespace

EDOPRO_POLICY_TEST(deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies) {
	constexpr std::uint32_t kBits[] = {kTypeMonster, kTypeSpell,   kTypeTrap, kTypeFusion,
									   kTypeRitual,  kTypeSynchro, kTypeXyz,  kTypeLink};
	constexpr std::uint32_t kCombinations = 1u << 8;

	int checked = 0;
	int disagreements = 0;
	int family_cards[std::size(kRecordedFamilies)] = {};

	for(bool rush : {false, true}) {
		for(std::uint32_t mask = 0; mask < kCombinations; ++mask) {
			std::uint32_t type = 0;
			for(std::uint32_t bit = 0; bit < 8; ++bit)
				if(mask & (1u << bit))
					type |= kBits[bit];
			++checked;

			// The order the call sites try the pushes in does not matter, and
			// no card is accepted by both, so a card lands in exactly one
			// section or, if both refuse it, in Side.
			const Lands right_click = builder_lands(type, rush, true);
			const Lands middle_click = builder_lands(type, rush, false);
			EDOPRO_POLICY_CHECK_EQ(name_of(right_click), name_of(middle_click));
			EDOPRO_POLICY_CHECK(!(builder_push_main_accepts(type, rush) &&
								  builder_push_extra_accepts(type, rush)));

			const Lands ours =
				belongs_in_extra_deck(card(type, rush ? kScopeOcgTcg | kScopeRush : kScopeOcgTcg),
									  RitualPlacement::RushInExtra)
					? Lands::Extra
					: Lands::Main;

			int matching = 0;
			const RecordedFamily* family = nullptr;
			for(std::size_t i = 0; i < std::size(kRecordedFamilies); ++i) {
				if(kRecordedFamilies[i].matches(type, rush)) {
					++matching;
					family = &kRecordedFamilies[i];
					++family_cards[i];
				}
			}

			std::ostringstream label;
			label << "type 0x" << std::hex << type << (rush ? " (Rush scope)" : " (non-Rush scope)")
				  << ": deck builder -> " << name_of(right_click) << ", this project -> "
				  << name_of(ours);
			if(right_click != ours) {
				++disagreements;
				if(matching != 1) {
					edopro_next::policy::testing::report_failure(
						__FILE__, __LINE__,
						label.str() + " is in " + std::to_string(matching) +
							" recorded families, expected exactly 1");
				} else if(family->upstream_builder != right_click || family->this_project != ours) {
					edopro_next::policy::testing::report_failure(
						__FILE__, __LINE__,
						label.str() + " but family " + family->id + " records deck builder -> " +
							name_of(family->upstream_builder) + ", this project -> " +
							name_of(family->this_project));
				}
			} else if(matching != 0) {
				edopro_next::policy::testing::report_failure(
					__FILE__, __LINE__,
					label.str() + " agree, but " + std::to_string(matching) +
						" recorded family(ies) claim it");
			}
		}
	}

	// 2 scopes x 256 type combinations.
	EDOPRO_POLICY_CHECK_EQ(checked, 512);
	for(std::size_t i = 0; i < std::size(kRecordedFamilies); ++i) {
		const auto& family = kRecordedFamilies[i];
		EDOPRO_POLICY_CHECK_EQ(std::string(family.id) + ": " + std::to_string(family_cards[i]),
							   std::string(family.id) + ": " + std::to_string(family.cards));
	}
	EDOPRO_POLICY_CHECK_EQ(disagreements, kRecordedDisagreements);
}

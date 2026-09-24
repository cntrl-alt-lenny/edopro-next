// The one definition, in this project, of "does this card belong in the
// Extra Deck". See docs/architecture/deck-placement.md for the upstream
// source this reproduces, quoted at file and line, and
// docs/adr/0011-extra-deck-classification.md for why it lives here and how
// the Ritual and Rush cases are expressed.
//
// It reproduces the `is_extra_deck_card` lambda inside upstream's
// DeckManager::LoadDeck (gframe/deck_manager.cpp:335-348) exactly, plus the
// loop's token skip (:357) and a coarser version of its unresolved-code
// handling (see classify_card() for exactly how coarse). policy::validate_deck()
// reaches the same functions for its own zone checks (deck_validation.cpp), so
// validation and classification cannot drift apart;
// policy/tests/test_deck_placement.cpp pins both, and pins the card shapes on
// which upstream's deck builder (a different code path from the lambda) puts a
// card elsewhere (deck-placement.md §3.3).
//
// THE UI MUST NOT IMPLEMENT GAME RULES (AGENTS.md). This header carries no Qt
// type and no UI concept: a caller asks where a card goes and renders the
// answer.
#ifndef EDOPRO_NEXT_POLICY_DECK_PLACEMENT_H
#define EDOPRO_NEXT_POLICY_DECK_PLACEMENT_H

#include "edopro_next/data/card_code.h"
#include "edopro_next/data/card_database.h"
#include "edopro_next/data/card_record.h"

namespace edopro_next::policy {

// Mirrors gframe/deck.h:19-23's RITUAL_LOCATION, value for value, under names
// that say what each one does to a Ritual Monster:
//
//   RITUAL_LOCATION::DEFAULT -> RushInExtra   (Rush Ritual Monsters go to the
//                                              Extra Deck, every other Ritual
//                                              Monster to the Main Deck)
//   RITUAL_LOCATION::MAIN    -> Main          (every Ritual Monster: Main)
//   RITUAL_LOCATION::EXTRA   -> Extra         (every Ritual Monster: Extra)
//
// No default value is offered: every caller names the upstream mode it is
// reproducing, the same "no silently-picked ruleset" rule ValidationPolicy
// follows (validation_policy.h).
enum class RitualPlacement {
	RushInExtra,
	Main,
	Extra,
};

// Mirrors how upstream turns its resolved duel-rule boolean into a
// RITUAL_LOCATION everywhere it does so:
// `rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN`
// (gframe/generic_duel.cpp:423, gframe/deck_manager.cpp:407,
// gframe/menu_handler.cpp:315 and :812). Never yields RushInExtra: no
// upstream caller derives DEFAULT from a duel flag.
constexpr RitualPlacement ritual_placement_for(bool rituals_in_extra) noexcept {
	return rituals_in_extra ? RitualPlacement::Extra : RitualPlacement::Main;
}

// gframe/data_manager.h:92-94, CardDataC::isRitualMonster():
// `(type & (TYPE_MONSTER | TYPE_RITUAL)) == (TYPE_MONSTER | TYPE_RITUAL)`.
// A Ritual Spell is not a Ritual Monster.
bool is_ritual_monster(const data::CardRecord& record) noexcept;

// gframe/data_manager.h:96-98, CardDataC::isRush(): `ot & SCOPE_RUSH`.
bool is_rush(const data::CardRecord& record) noexcept;

// gframe/deck_manager.cpp:357: `cd->type & TYPE_TOKEN`. LoadDeck skips such a
// card before classifying it, in every section and every load mode.
bool is_token(const data::CardRecord& record) noexcept;

// The half of is_extra_deck_card that does not depend on RitualPlacement
// (gframe/deck_manager.cpp:336-339): a Fusion, Synchro or Xyz card, or a
// Link card that is also a Monster. See deck-placement.md for why upstream's
// literal Link line reduces to "Link and Monster".
bool is_extra_deck_type(const data::CardRecord& record) noexcept;

// gframe/deck_manager.cpp:335-348's is_extra_deck_card lambda, exactly, in
// its own order: Fusion/Synchro/Xyz, then Link Monster, then the Ritual
// Monster rule selected by `rituals`, else Main. Tokens are not special-cased
// here, because the lambda does not special-case them either - LoadDeck
// filters them out before asking; classify_card() below does the same.
bool belongs_in_extra_deck(const data::CardRecord& record, RitualPlacement rituals) noexcept;

// Where a card goes when it is placed by type. `Token` and `Unknown` are the
// cases with no placement: LoadDeck skips a token outright
// (deck_manager.cpp:357-358) and upstream's deck-builder search never lists
// one (deck_con.cpp:1192); an unresolved code has no type to classify by.
enum class CardPlacement {
	Main,
	Extra,
	Token,
	Unknown,
};

// Looks `code` up in `database` and classifies it: Unknown if it does not
// resolve, Token if it is a token, otherwise Main or Extra by
// belongs_in_extra_deck().
//
// What this reproduces of LoadDeck's per-code loop (deck_manager.cpp:349-364)
// and what it does not:
//  - Tokens: reproduced. The loop skips a token, after resolving it and before
//    asking the lambda, in every mode (:357); Token is that skip.
//  - Unresolved codes: only the part they share. The loop asks
//    gDataManager->GetCardData(code) and then GetDummyOrMappedCardData(code)
//    (:350-351); classify_card() asks only `database`, so a code `database`
//    lacks is Unknown even where upstream would resolve it through an
//    id-mapping file. For a code neither resolves, upstream does one of two
//    things depending on whether the load has an extra list: drops the code
//    and reports it as an error code (:352-354), or keeps a placeholder with
//    code 0 in the list it came from without classifying it (:359). Unknown
//    stands for both: "no classification". Neither the error code nor the
//    placeholder is modelled here.
//  - Side-list and Extra-list loops (:365-390) do not classify and are not
//    modelled.
CardPlacement classify_card(const data::CardDatabase& database, data::CardCode code,
							RitualPlacement rituals) noexcept;

} // namespace edopro_next::policy

#endif // EDOPRO_NEXT_POLICY_DECK_PLACEMENT_H

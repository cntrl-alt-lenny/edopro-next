// Presentation-independent, source-order-faithful deck validation. See
// docs/architecture/deck-legality.md for the exact upstream citation of
// every check this reproduces (gframe/deck_manager.cpp's CheckDeckSize/
// CheckDeckContent/CheckCards, and gframe/generic_duel.cpp's PlayerReady,
// which is what actually sequences deck-size before unknown-card before
// content validation - CheckDeckSize/CheckDeckContent never call each
// other or share that ordering themselves).
//
// THE RULES ENGINE MUST NOT BECOME THE UI, AND THE UI MUST NOT IMPLEMENT
// GAME RULES (CLAUDE.md). This module exists so that rule: QML must never
// count copies, inspect TYPE_* bits, read a banlist, or decide whether a
// deck is valid itself. It may only render the DeckValidationError this
// module returns.
#ifndef EDOPRO_NEXT_POLICY_DECK_VALIDATION_H
#define EDOPRO_NEXT_POLICY_DECK_VALIDATION_H

#include <cstdint>

#include "edopro_next/data/card_code.h"
#include "edopro_next/data/card_database.h"
#include "edopro_next/data/deck.h"
#include "edopro_next/policy/validation_policy.h"

namespace edopro_next::policy {

// Mirrors gframe/network.h's DeckError::DERR_TYPE enum: the subset this
// module reproduces, in the same names, for the same meanings - see
// docs/architecture/deck-legality.md for the exact upstream citation of
// each. `None` is the only value meaning "no error", matching
// DERR_TYPE::NONE. This module has no LFLIST-vs-collect-everything
// ambiguity to represent: it is a first-error result, not a findings list
// (see validate_deck()'s own doc comment for why that is deliberate).
enum class DeckErrorType {
	None,
	// From CheckDeckSize (gframe/deck_manager.cpp:238-258).
	MainCount,
	ExtraCount,
	SideCount,
	// From PlayerReady's own pre-CheckDeckContent short-circuit
	// (gframe/generic_duel.cpp:374-378) - see this header's own
	// "Unknown-card semantics" note below.
	UnknownCard,
	// From CheckDeckContent's own top-level checks
	// (gframe/deck_manager.cpp:206-215).
	ForbiddenType,
	TooManyLegends,
	TooManySkills,
	// From CheckCards's per-card checks (gframe/deck_manager.cpp:157-203).
	// Note EXTRACOUNT is reused by CheckDeckContent's own zone-placement
	// check (a card in the wrong section) in addition to CheckDeckSize's
	// use of the same name for a real Extra-count overflow - this module
	// keeps that same overload rather than inventing a fourth,
	// source-unfaithful distinction; a caller can always tell the two
	// apart by whether `card` is set (see DeckValidationError below).
	CardCount,
	// "This card is TCG-only" (found while OcgOnly is required) /
	// "this card is OCG-only" (found while TcgOnly is required) - matching
	// upstream's own, admittedly easy-to-misread naming exactly
	// (gframe/deck_manager.cpp:169,174: the *value* returned describes
	// what the card IS, not which mode rejected it).
	TcgOnly,
	OcgOnly,
	UnofficialCard,
	Lflist,
};

// Meaningful only for DeckErrorType::MainCount/ExtraCount/SideCount -
// mirrors gframe/network.h's DeckError::count triple exactly.
struct DeckSizeCount {
	std::uint32_t current = 0;
	std::uint32_t minimum = 0;
	std::uint32_t maximum = 0;

	friend bool operator==(const DeckSizeCount&, const DeckSizeCount&) = default;
};

// The result of validate_deck(). `type == DeckErrorType::None` means the
// deck passed every check this module performs, under the given policy -
// it is not a claim that upstream's *complete* rule set (which this module
// does not reproduce in full - see docs/architecture/deck-legality.md's
// own scope note) would also accept it.
struct DeckValidationError {
	DeckErrorType type = DeckErrorType::None;

	// Set only for MainCount/ExtraCount(-as-size-overflow)/SideCount.
	DeckSizeCount count;

	// The specific card code the failing check was evaluating - set for
	// every error type except None/MainCount/ExtraCount(-as-size-overflow)/
	// SideCount/TooManyLegends/TooManySkills, none of which are about one
	// specific card. For UnknownCard specifically, see this header's own
	// "Unknown-card semantics" note: this is this module's own
	// deterministically-chosen unknown code, not a claim of matching any
	// particular upstream load mode's `errorcode`.
	data::CardCode card = data::CardCode::None;

	explicit operator bool() const noexcept { return type != DeckErrorType::None; }

	friend bool operator==(const DeckValidationError&, const DeckValidationError&) = default;
};

// Validates `deck` against `policy`, resolving each card through `database`,
// and returns the single FIRST error found, in upstream's own precedence
// order - see docs/architecture/deck-legality.md for the exact citation of
// every step below:
//
//   1. Deck size (CheckDeckSize) - Main, then Extra, then Side.
//   2. Unknown-card condition, only when policy.content_checking_enabled.
//   3. Content validation (CheckDeckContent), in its own internal order:
//      forbidden types -> Legend monsters -> Legend spells -> Legend traps
//      -> Skill count -> Main cards in order -> Extra cards in order ->
//      Side cards in order, each card checked: allowed-card/scope ->
//      section-placement -> shared copy-count cap -> LFList/whitelist
//      limitation, in that order.
//
// This is intentionally the ONLY validation entry point this module
// offers: a single, source-ordered "first error" result, not an
// alternate "collect every problem" mode (see docs/adr/0007 for why that
// is deliberately deferred rather than added speculatively here).
//
// Unknown-card semantics: this project's edopro_next::data::Deck can
// legitimately contain a CardCode with no matching CardDatabase entry
// (see deck.h/ydk.h). Upstream's own UNKNOWNCARD is a byproduct of its
// loading/network path and legacy dummy-card machinery
// (DeckManager::GetDummyOrMappedCardData), not something CheckDeckContent
// computes itself - and which specific code upstream would report (if
// any) depends on which load mode produced the Deck in the first place
// (see docs/architecture/deck-legality.md#unknown-card for the full,
// source-verified account, including the asymmetry between upstream's
// `separated=true`/`false` load paths). This module does not recreate
// that machinery. It detects an unknown code directly through
// `database.find()`, and reports the FIRST one found scanning Main (in
// order), then Extra (in order), then Side (in order) - a deliberate,
// simple, fully-deterministic choice of this module's own, not a claim
// of matching any specific upstream load mode's `errorcode` value.
DeckValidationError validate_deck(const data::Deck& deck, const data::CardDatabase& database,
								   const ValidationPolicy& policy);

} // namespace edopro_next::policy

#endif // EDOPRO_NEXT_POLICY_DECK_VALIDATION_H

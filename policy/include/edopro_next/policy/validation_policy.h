// The explicit, source-equivalent inputs deck validation needs, that
// upstream itself receives from live host/session state
// (gframe/network.h's HostInfo) rather than from any fixed, universal
// ruleset. This project's UI has no networking/hosting session at this
// slice - see docs/adr/0007-deck-legality-policy-module.md - so there is
// deliberately no default constructor here that would silently invent one.
// A caller must choose every field explicitly.
#ifndef EDOPRO_NEXT_POLICY_VALIDATION_POLICY_H
#define EDOPRO_NEXT_POLICY_VALIDATION_POLICY_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "edopro_next/policy/lf_list.h"

namespace edopro_next::policy {

// Mirrors gframe/network.h's DeckSizes::Sizes exactly, including its
// inclusive comparison (network.h's own operator==(Sizes, size_t):
// `count < min` fails, `count <= max` is required) - reproduced by
// contains() below rather than the confusingly-named upstream
// `operator==`/`operator!=` pair.
struct SectionSizeRange {
	std::uint16_t min = 0;
	std::uint16_t max = 0;

	bool contains(std::size_t count) const noexcept { return count >= min && count <= max; }

	friend bool operator==(const SectionSizeRange&, const SectionSizeRange&) = default;
};

// Mirrors gframe/network.h's DeckSizes exactly - three independent
// min/max ranges, one per section.
struct DeckSizePolicy {
	SectionSizeRange main;
	SectionSizeRange extra;
	SectionSizeRange side;

	friend bool operator==(const DeckSizePolicy&, const DeckSizePolicy&) = default;
};

// Mirrors gframe/deck_manager.h's DuelAllowedCards enum exactly, in the
// same order, for the same five modes - see docs/architecture/
// deck-legality.md#scope for the exact per-mode check this drives
// (deck_validation.cpp), including the CHECK_UNOFFICIAL magnitude quirk
// (`scope > 0x3`) upstream applies to the first three modes and this
// module deliberately preserves rather than corrects.
enum class AllowedCardPool {
	OcgOnly,
	TcgOnly,
	OcgAndTcg,
	WithPrerelease,
	Any,
};

// The complete, explicit input to validate_deck() (deck_validation.h).
// There is intentionally no default value for any field and no factory
// function that would produce "the" default ruleset - see this header's
// own top comment and docs/adr/0007. A future UI/session layer may offer
// named convenience presets; that is explicitly out of scope for this
// module.
//
// This type is NOT an aggregate: it has one user-declared constructor,
// requiring every field as a positional argument with no default
// arguments at all. External review found the earlier plain-aggregate
// shape genuinely unsafe, not merely inconvenient: `ValidationPolicy
// policy;` (default-initialization, e.g. a local variable declared with
// no initializer) left `allowed_cards` - a plain enum with no in-class
// member initializer - holding an INDETERMINATE value, which
// validate_deck()'s own switch over it would then read as undefined
// behavior. `ValidationPolicy{}` (value-initialization) was safer but
// still silently selected the enum's first value (OcgOnly) and defaulted
// `lflist` to null - exactly the kind of unreviewed, silently-picked
// ruleset this type exists to make impossible. The mandatory constructor
// closes both paths: there is no way to construct a ValidationPolicy at
// all without a caller supplying every field by name, at every call site,
// checked at compile time.
struct ValidationPolicy {
	DeckSizePolicy deck_sizes;
	AllowedCardPool allowed_cards;

	// Mirrors gframe/network.h's HostInfo::forbiddentypes exactly: an
	// opaque TYPE_* bitmask, tested against CardRecord::type the same way
	// upstream's TypeCount() does (deck_validation.cpp) - this module never
	// interprets which bits exist, matching how CardRecord::type is left
	// opaque everywhere else in this project's data/ module.
	std::uint32_t forbidden_types;

	// Mirrors the resolved boolean gframe/generic_duel.cpp actually passes
	// to CheckDeckContent (`rituals_in_extra`, derived there from a
	// DUEL_EXTRA_DECK_RITUAL duel-rule bit) - NOT the three-state
	// RITUAL_LOCATION enum LoadDeck's own *classification* step uses. This
	// module validates a Deck whose Main/Extra/Side split already exists;
	// it does not classify one, so that three-state loader concept does
	// not apply here - see docs/architecture/deck-legality.md#ritual-policy
	// and docs/adr/0007.
	bool rituals_belong_in_extra;

	// Mirrors gframe/network.h's HostInfo::no_check_deck_content, inverted
	// for a positive name: when false, content validation (and the
	// unknown-card check that gates it - see deck_validation.h) is skipped
	// entirely, matching gframe/generic_duel.cpp's PlayerReady exactly;
	// only deck-size validation still runs.
	bool content_checking_enabled;

	// The banlist to validate against - or std::nullopt for upstream's
	// nullptr-LFList state. These are two genuinely different states, not
	// one collapsed into the other: gframe/deck_manager.cpp:217-218's
	// `if(!lflist) return ret;` causes CheckDeckContent to return
	// BEFORE any CheckCards pass ever runs, so a null list skips scope,
	// zone-placement, the three-copy cap, AND the banlist check entirely -
	// while a concrete LfList with empty `content` and `whitelist == false`
	// (upstream's synthetic "N/A" list) still runs every one of those
	// checks, simply finding no banlist-specific restriction for any card.
	// std::nullopt here reproduces the former; an LfList value (even a
	// default-constructed, empty one) reproduces the latter. See
	// deck_validation.h and docs/architecture/deck-legality.md#null-vs-na.
	std::optional<LfList> lflist;

	// The one and only way to construct a ValidationPolicy - see this
	// struct's own doc comment above. No default arguments: omitting any
	// field is a compile error, not a silently-guessed value.
	ValidationPolicy(DeckSizePolicy deck_sizes, AllowedCardPool allowed_cards,
					  std::uint32_t forbidden_types, bool rituals_belong_in_extra,
					  bool content_checking_enabled, std::optional<LfList> lflist)
		: deck_sizes(deck_sizes),
		  allowed_cards(allowed_cards),
		  forbidden_types(forbidden_types),
		  rituals_belong_in_extra(rituals_belong_in_extra),
		  content_checking_enabled(content_checking_enabled),
		  lflist(std::move(lflist)) {}
};

} // namespace edopro_next::policy

#endif // EDOPRO_NEXT_POLICY_VALIDATION_POLICY_H

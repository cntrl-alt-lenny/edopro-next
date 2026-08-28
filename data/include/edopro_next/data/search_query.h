// A structured, presentation-independent card search query. See
// docs/architecture/card-search.md for the source archaeology this is
// built from and docs/adr/0005-card-search-structured-query.md for why
// this replaces upstream's compact sigil mini-language (||, &&, *, $, $$,
// @, !!) rather than exposing it directly.
//
// This is a data-filtering boundary, not a legality boundary: there is no
// LFList/banlist field, no "legal" flag, no format/whitelist concept, and
// no deck-size awareness anywhere in this type. `scope` below is the raw
// `CardRecord::scope` bitmask, exposed for a caller that wants to filter
// on it as data - not a policy this type interprets for them.
#ifndef EDOPRO_NEXT_DATA_SEARCH_QUERY_H
#define EDOPRO_NEXT_DATA_SEARCH_QUERY_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "edopro_next/data/card_code.h"

namespace edopro_next::data {

// Which searchable field(s) `SearchQuery::text` is matched against. Only
// `name` and `text` (CardRecord::text, sourced from `texts.desc`) are ever
// searched - never the sixteen auxiliary strings, matching upstream's own
// deck search (DeckBuilder::CheckCardText only ever reads
// CardString::uppercase_name/uppercase_text, never CardString::desc[16] -
// see card-search.md).
enum class TextScope {
	Name,
	Text,
	NameOrText,
};

// A comparison against a single numeric CardRecord field. Deliberately
// three operators, not upstream's six-way per-field filter-type scheme
// (exact / at-least / at-most / at-most-excluding-"?" / at-least-
// excluding-"?" / exactly-"?" - gframe/deck_con.cpp's CheckCardProperties)
// - seeing whether a "?" stat (attack/defense == -1 or -2) satisfies a
// comparison is just ordinary signed-integer comparison against whichever
// threshold the caller picked, with no special-casing needed: `AtLeast`
// with a realistic positive threshold already excludes "?" cards without
// this type knowing what "?" means, exactly reproducing upstream's own
// observable filtering result for that case. A caller that specifically
// wants only "?" cards can use `EqualTo` with -1 or -2 directly. See
// card-search.md for the fuller comparison against upstream's scheme.
enum class NumericComparison {
	EqualTo,
	AtLeast,
	AtMost,
};

// `value` is intentionally the widest signed type here (not per-field
// int32_t/uint32_t/uint64_t) so one comparison shape covers every numeric
// CardRecord field this query supports. The comparison itself happens in
// `std::int64_t`: CardSearchIndex widens the stored field - signed
// (attack/defense) or unsigned (level/left_scale/right_scale) - into an
// `std::int64_t` and compares that directly against `value`, unchanged.
// It does not narrow `value` back down to the field's own native width
// first, and a query value never silently wraps merely because the
// stored field happens to be unsigned.
//
// This still reproduces `level`'s M3A-established uint32_t wraparound
// behaviour faithfully, because widening an unsigned value into a wider
// signed type preserves its numeric value (and therefore its relative
// order against any other such widened value) exactly - a card whose
// negative packed level decoded to the wrapped value 4294967289 widens
// to the int64_t 4294967289, not to -7, so `NumericFilter{4294967289,
// EqualTo}` matches it and `NumericFilter{-7, EqualTo}` does not -
// pinned directly by card_search_index.cpp's tests, not left implicit.
//
// `defense` on a Link monster never matches any NumericFilter at all,
// regardless of `value` or `comparison` - see card_search_index.h.
struct NumericFilter {
	std::int64_t value = 0;
	NumericComparison comparison = NumericComparison::EqualTo;
};

// "This field's bits must include every bit set in `require_all_bits`" -
// the same shape as upstream's own `(data.type & filter_type2) !=
// filter_type2` (for a Monster) and `(data.link_marker & filter_marks) !=
// filter_marks` checks (gframe/deck_con.cpp's CheckCardProperties), just
// named for what it does rather than left as an inline bitwise
// expression. Used for `type`, `link_marker`, and `scope` - see each
// field's own comment below for why `category` and `race` deliberately do
// NOT use this shape, despite also being CardRecord bitmask fields.
struct BitmaskFilter {
	std::uint32_t require_all_bits = 0;
};

// "This field's bits must include at least one bit set in `any_bits`" -
// upstream's own effect-category filter semantics, which are genuinely
// different from the "all bits" shape above: `filter_effect` is built by
// OR-ing together every checked category checkbox
// (BUTTON_CATEGORY_OK/CheckCardProperties, gframe/deck_con.cpp), and the
// actual check is `if(filter_effect && !(data.category & filter_effect))
// return false;` - reject only when *none* of the selected bits are
// present, i.e. keep a card matching *any* of them. Reproducing this as
// a "require all" filter would silently exclude a card that has only one
// of several selected effect categories, which upstream's own UI does
// not do.
struct AnyBitmaskFilter {
	std::uint32_t any_bits = 0;
};

struct SearchQuery {
	// Empty, or containing only whitespace, means "no text constraint" -
	// every entry passing the static filters below matches, with
	// MatchKind::NoTextQuery (see search_result.h). A caller that lets a
	// UI text field's live contents flow straight into this field (e.g.
	// a search box the user has cleared, or briefly left as only spaces)
	// gets the browse-everything behaviour, not "match nothing", without
	// needing to trim/empty-check the field itself first. Otherwise, text
	// is normalized once (normalize_search_text, text_normalize.h) and
	// split on whitespace into tokens, ALL of which must appear as
	// substrings - in any order - of the field(s) `text_scope` selects.
	// This is a deliberate departure from upstream's own token semantics
	// (Utils::ContainsSubstring requires tokens in left-to-right,
	// non-overlapping order - an artifact of how it is implemented with a
	// single forward-advancing find(), not a stated design choice) - see
	// card-search.md for the full reasoning.
	std::string text;
	TextScope text_scope = TextScope::NameOrText;

	// A ranking hint, not a filter: when the entry whose CardCode equals
	// this appears in the result set - because it independently passes
	// every filter below, `text` included - it is reported with
	// MatchKind::ExactCode (the highest-priority tier) instead of
	// whatever tier its text match would otherwise have earned. It does
	// not restrict the search to only this code, and it does not make a
	// card appear that would not otherwise have matched `text`/the other
	// filters. CardDatabase::find() already exists for an unconditional
	// exact-code lookup with no filtering at all; this field exists so a
	// caller building one structured query does not have to special-case
	// "did the user type a number" against `text` themselves - it is
	// explicit and typed, never parsed out of `text`.
	std::optional<CardCode> exact_code;

	std::optional<BitmaskFilter> type;
	// ANY of the selected category bits, not all of them - see
	// AnyBitmaskFilter's own comment for the exact source behaviour this
	// reproduces.
	std::optional<AnyBitmaskFilter> category;
	std::optional<std::uint32_t> attribute; // exact match, matching upstream's single-attribute selector
	// Exact equality against CardRecord::race, not "contains this bit" -
	// matching upstream's own single-race-selector check
	// (`data.race != filter_race`, CheckCardProperties). A card with
	// race == 0x3 does NOT match a query for race == 0x1, even though
	// bit 0x1 is set within it - upstream's own UI only ever offers
	// picking one race at a time (StartFilter's `filter_race = UINT64_C(1)
	// << (selected - 1)`), and this field intentionally does not offer a
	// "contains any/all of these race bits" mode upstream itself has no
	// equivalent for.
	std::optional<std::uint64_t> race;
	std::optional<BitmaskFilter> link_marker;
	std::optional<BitmaskFilter> scope; // raw CardRecord::scope bits - not a legality decision, see the file-level comment above

	std::optional<NumericFilter> attack;
	std::optional<NumericFilter> defense; // never matches a Link monster - see card_search_index.h
	std::optional<NumericFilter> level;
	std::optional<NumericFilter> left_scale;  // only ever matches a Pendulum monster - see card_search_index.h
	std::optional<NumericFilter> right_scale; // only ever matches a Pendulum monster - see card_search_index.h

	// "This card's setcodes (or, for an aliased card, its alias's
	// setcodes - see card-search.md#alias-and-setcode) include at least
	// one of these" - OR semantics, matching upstream's check_set_code
	// (gframe/deck_con.cpp). A numeric setcode filter only; resolving a
	// human-readable archetype name (e.g. a localized set-name string) to
	// a setcode is deliberately out of scope - see card-search.md#alias-
	// and-setcode for why that needs a data source this module does not
	// have.
	std::optional<std::vector<std::uint16_t>> setcodes;

	// Applied after ranking, not before - see card_search_index.h.
	std::optional<std::size_t> limit;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_SEARCH_QUERY_H

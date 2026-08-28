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
// `name` and `text` (CardRecord's `text1`, i.e. `texts.desc`) are ever
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
// CardRecord field this query supports; CardSearchIndex converts it to
// each field's real stored type before comparing, using that field's own
// comparison semantics (see card_search_index.h - this matters
// specifically for `level`, whose uint32_t wraparound representation for
// a negative packed value is source-faithful, not a bug - and for
// `defense` on a Link monster, which this comparison never matches at
// all, regardless of `value`).
struct NumericFilter {
	std::int64_t value = 0;
	NumericComparison comparison = NumericComparison::EqualTo;
};

// "This field's bits must include every bit set in `require_all_bits`" -
// the same shape as upstream's own `(data.type & filter) != filter` /
// `(data.link_marker & filter_marks) != filter_marks` checks
// (gframe/deck_con.cpp), just named for what it does rather than left as
// an inline bitwise expression. Works for any CardRecord bitmask field
// this query exposes (`type`, `category`, `attribute`, `race`,
// `link_marker`, `scope`) - it does not itself carry any meaning specific
// to one field.
struct BitmaskFilter {
	std::uint32_t require_all_bits = 0;
};

struct BitmaskFilter64 {
	std::uint64_t require_all_bits = 0;
};

struct SearchQuery {
	// Empty means "no text constraint" - every entry passing the static
	// filters below matches, with MatchKind::NoTextQuery (see
	// search_result.h). Non-empty text is normalized once
	// (normalize_search_text, text_normalize.h) and split on whitespace
	// into tokens, ALL of which must appear as substrings - in any order -
	// of the field(s) `text_scope` selects. This is a deliberate departure
	// from upstream's own token semantics (Utils::ContainsSubstring
	// requires tokens in left-to-right, non-overlapping order - an
	// artifact of how it is implemented with a single forward-advancing
	// find(), not a stated design choice) - see card-search.md for the
	// full reasoning.
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
	std::optional<BitmaskFilter> category;
	std::optional<std::uint32_t> attribute; // exact match, matching upstream's single-attribute selector
	std::optional<BitmaskFilter64> race;
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

// Turns the text a user types into a deck-search ATK, DEF, Level or Scale
// box into a NumericFilter, exactly as upstream's deck builder does. See
// docs/architecture/card-search.md §2.2 for the upstream source quoted at
// file and line, and data/tests/test_numeric_filter_text.cpp, which checks
// this against a transcription of upstream over every input form.
//
// Upstream, gframe/deck_con.cpp:21-51 (`parse_filter`) reads the first one
// or two characters to pick one of six comparisons, and BufferIO::GetVal
// (gframe/bufferio.h:240-249) reads the number after them. CheckCardProperties
// (gframe/deck_con.cpp:1202-1227) then applies the comparison. This header
// reproduces all three in one function, including the parts that surprise:
//
//  - text starting with anything but a digit, `=`, `>`, `<` or `?` (an empty
//    box, a leading space, a minus sign, a letter) sets no filter at all;
//  - after a recognised prefix, the number is read in unsigned 32-bit
//    arithmetic that wraps on overflow, and any character after the digits
//    makes the number 0 - so "1500a" means "equal to 0", not "no filter";
//  - "?" means ATK or DEF equal to -2 (not -1), and on Level or Scale it
//    matches no card at all;
//  - "<=" and "<" on ATK and DEF also reject a negative ("?") stat.
#ifndef EDOPRO_NEXT_DATA_NUMERIC_FILTER_TEXT_H
#define EDOPRO_NEXT_DATA_NUMERIC_FILTER_TEXT_H

#include <optional>
#include <string_view>

#include "edopro_next/data/search_query.h"

namespace edopro_next::data {

// Which box the text came from. It decides the number's width - upstream
// holds `filter_atk`/`filter_def` as int32_t and `filter_lv`/`filter_scl` as
// uint32_t (gframe/deck_con.h:115-122) - whether "at most" rejects negative
// stats, and what "?" means.
enum class NumericFilterField {
	Attack,
	Defense,
	Level,
	Scale,
};

// std::nullopt means "no filter", upstream's filter type 0. Otherwise the
// filter to put in SearchQuery::attack, ::defense, ::level or ::left_scale
// (upstream's scale box filters the left scale only, deck_con.cpp:1222-1224).
// `text` is UTF-8; only ASCII characters are ever recognised, as upstream
// only ever compares against ASCII characters.
std::optional<NumericFilter> parse_numeric_filter(std::string_view text, NumericFilterField field);

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_NUMERIC_FILTER_TEXT_H

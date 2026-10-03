// SPDX-License-Identifier: AGPL-3.0-or-later

#include "edopro_next/data/numeric_filter_text.h"

#include <cstdint>

namespace edopro_next::data {

namespace {

// gframe/bufferio.h:240-249, BufferIO::GetVal: digits accumulate in
// uint32_t (wrapping), and the result is 0 unless the digits run to the end
// of the string.
std::uint32_t get_val(std::string_view text) {
	std::uint32_t ret = 0;
	std::size_t pos = 0;
	while(pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
		ret = ret * 10u + static_cast<std::uint32_t>(text[pos] - '0');
		++pos;
	}
	if(pos == text.size())
		return ret;
	return 0;
}

// Upstream's parse_filter returns `int`, stored in an int32_t for ATK/DEF
// and a uint32_t for Level/Scale (deck_con.h:115-122): the same 32 bits,
// read as signed or unsigned.
std::int64_t as_field_value(std::uint32_t raw, NumericFilterField field) {
	if(field == NumericFilterField::Attack || field == NumericFilterField::Defense)
		return static_cast<std::int32_t>(raw);
	return raw;
}

bool is_stat(NumericFilterField field) {
	return field == NumericFilterField::Attack || field == NumericFilterField::Defense;
}

} // namespace

std::optional<NumericFilter> parse_numeric_filter(std::string_view text, NumericFilterField field) {
	// parse_filter's six filter types, deck_con.cpp:21-51. `text[0]` on an
	// empty string is upstream's terminating NUL, which matches no branch.
	enum class Form { None, Equal, AtLeast, Greater, AtMost, Less, Unknown };
	Form form = Form::None;
	std::string_view digits;
	if(text.empty()) {
		form = Form::None;
	} else if(text[0] == '=') {
		form = Form::Equal;
		digits = text.substr(1);
	} else if(text[0] >= '0' && text[0] <= '9') {
		form = Form::Equal;
		digits = text;
	} else if(text[0] == '>') {
		if(text.size() > 1 && text[1] == '=') {
			form = Form::AtLeast;
			digits = text.substr(2);
		} else {
			form = Form::Greater;
			digits = text.substr(1);
		}
	} else if(text[0] == '<') {
		if(text.size() > 1 && text[1] == '=') {
			form = Form::AtMost;
			digits = text.substr(2);
		} else {
			form = Form::Less;
			digits = text.substr(1);
		}
	} else if(text[0] == '?') {
		form = Form::Unknown;
	}

	const std::int64_t n = as_field_value(get_val(digits), field);
	// The comparisons in CheckCardProperties, deck_con.cpp:1202-1227, one
	// field at a time. A card is kept when none of the listed rejections
	// holds; each filter below keeps exactly the same cards.
	switch(form) {
	case Form::None:
		return std::nullopt;
	case Form::Equal: // type 1: reject `field != n`
		return NumericFilter{n, NumericComparison::EqualTo, false};
	case Form::AtLeast: // type 2: reject `field < n`
		return NumericFilter{n, NumericComparison::AtLeast, false};
	case Form::Greater: // type 3: reject `field <= n`, so keep field >= n + 1
		return NumericFilter{n + 1, NumericComparison::AtLeast, false};
	case Form::AtMost: // type 4: reject `field > n`, and on ATK/DEF `field < 0`
		return NumericFilter{n, NumericComparison::AtMost, is_stat(field)};
	case Form::Less: // type 5: reject `field >= n`, so keep field <= n - 1; ATK/DEF also `field < 0`
		return NumericFilter{n - 1, NumericComparison::AtMost, is_stat(field)};
	case Form::Unknown:
		// type 6: ATK/DEF reject `field != -2`; Level and Scale reject every
		// card (`|| filter_lvtype == 6`, `|| filter_scltype == 6`). The level
		// and scale fields are unsigned and CardSearchIndex compares them
		// widened to int64_t (search_query.h), so no stored value is ever -1.
		if(is_stat(field))
			return NumericFilter{-2, NumericComparison::EqualTo, false};
		return NumericFilter{-1, NumericComparison::EqualTo, false};
	}
	return std::nullopt;
}

} // namespace edopro_next::data

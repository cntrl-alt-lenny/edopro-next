#include "edopro_next/data/text_normalize.h"

namespace edopro_next::data {

namespace {

// Mirrors Utils::ToUpperChar's wchar_t table (gframe/utils.h) exactly,
// codepoint-for-codepoint - every range and every special-cased value
// below is that table, not a reinterpretation of it. The codepoints
// involved (161-252, plus U+0250, U+2C6F, U+2200) are all in the Basic
// Multilingual Plane and fit in a single UTF-16 code unit, which is why
// upstream's wchar_t-based table (16-bit on Windows, 32-bit on Linux) and
// this char32_t-based one agree on every value despite the different
// storage width - char32_t is chosen here specifically to sidestep that
// platform difference rather than reproduce it.
char32_t fold_codepoint(char32_t c) {
	const auto in_interval = [c](char32_t start, char32_t end) { return c >= start && c <= end; };
	if(in_interval(192, 197) || in_interval(224, 229) || c == 0x2c6f || c == 0x250 || c == 0x2200)
		return U'A';
	if(in_interval(200, 203) || in_interval(232, 235))
		return U'E';
	if(in_interval(204, 207) || in_interval(236, 239))
		return U'I';
	if(in_interval(210, 214) || in_interval(242, 246))
		return U'O';
	if(in_interval(217, 220) || in_interval(249, 252))
		return U'U';
	if(c == 209 || c == 241)
		return U'N';
	if(c == 161) // inverted exclamation mark
		return U'!';
	if(c == 191) // inverted question mark
		return U'?';
	if(c > 255)
		return c;
	// Upstream falls through to std::toupper(static_cast<int>(c)) here.
	// Verified empirically (default "C" locale, no setlocale call in this
	// codebase's search path): for the codepoints reaching this branch -
	// everything in [0, 255] not already handled above - std::toupper
	// changes only plain ASCII 'a'-'z', leaving every other Latin-1
	// character (e.g. AE, ss, eth, thorn) unchanged. Implementing that
	// directly, rather than calling std::toupper, reproduces the same
	// observable behaviour without this function's result ever depending
	// on the embedding process's global locale state - a deliberate,
	// harmless strengthening, not a behavioural difference.
	if(c >= U'a' && c <= U'z')
		return c - U'a' + U'A';
	return c;
}

// Decodes one UTF-8 codepoint starting at `pos`, advancing `pos` past it.
// Malformed input (a stray continuation byte, a truncated multi-byte
// sequence) is not rejected: the leading byte's raw value is returned as
// a one-byte "codepoint" and `pos` advances by exactly one, so this always
// makes forward progress - normalize_search_text never throws and never
// loops, on any input, which matters because it is a public boundary
// function, not one that can assume its input was already validated.
char32_t decode_utf8(std::string_view text, std::size_t& pos) {
	const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
	const unsigned char lead = byte(pos);
	const auto continuation = [&](std::size_t i) {
		return pos + i < text.size() && (byte(pos + i) & 0xC0) == 0x80;
	};
	if(lead < 0x80) {
		++pos;
		return lead;
	}
	if((lead & 0xE0) == 0xC0 && continuation(1)) {
		const char32_t cp = (static_cast<char32_t>(lead & 0x1F) << 6) | (byte(pos + 1) & 0x3F);
		pos += 2;
		return cp;
	}
	if((lead & 0xF0) == 0xE0 && continuation(1) && continuation(2)) {
		const char32_t cp = (static_cast<char32_t>(lead & 0x0F) << 12) |
							 (static_cast<char32_t>(byte(pos + 1) & 0x3F) << 6) |
							 (byte(pos + 2) & 0x3F);
		pos += 3;
		return cp;
	}
	if((lead & 0xF8) == 0xF0 && continuation(1) && continuation(2) && continuation(3)) {
		const char32_t cp = (static_cast<char32_t>(lead & 0x07) << 18) |
							 (static_cast<char32_t>(byte(pos + 1) & 0x3F) << 12) |
							 (static_cast<char32_t>(byte(pos + 2) & 0x3F) << 6) |
							 (byte(pos + 3) & 0x3F);
		pos += 4;
		return cp;
	}
	++pos;
	return lead;
}

void encode_utf8(char32_t cp, std::string& out) {
	if(cp < 0x80) {
		out += static_cast<char>(cp);
	} else if(cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else if(cp < 0x10000) {
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

} // namespace

std::string normalize_search_text(std::string_view utf8) {
	std::string out;
	out.reserve(utf8.size());
	std::size_t pos = 0;
	while(pos < utf8.size()) {
		const char32_t cp = decode_utf8(utf8, pos);
		encode_utf8(fold_codepoint(cp), out);
	}
	return out;
}

} // namespace edopro_next::data

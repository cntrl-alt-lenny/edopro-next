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

// Decodes one UTF-8 codepoint starting at `pos`, advancing `pos` past it
// and returning true - only when the bytes at `pos` are a strictly
// canonical, well-formed UTF-8 encoding (Unicode's own "Well-Formed UTF-8
// Byte Sequences" table): the right number of continuation bytes, each in
// 0x80..0xBF, AND the correct per-lead-byte restriction on the *second*
// byte's range that rules out overlong encodings (C0/C1 as leads; E0
// followed by 80..9F; F0 followed by 80..8F), UTF-16 surrogate codepoints
// (ED followed by A0..BF, i.e. U+D800..U+DFFF), and codepoints beyond
// Unicode's own ceiling (F4 followed by 90..BF; any lead F5..FF at all).
// Accepting a shape-only check (right byte count, each a generic
// continuation byte) is not enough - it would accept exactly these
// non-canonical sequences, whose decoded numeric value can still
// coincidentally land inside fold_codepoint()'s table (this bug shipped
// once already for truncated/stray bytes - see the git history - and a
// shape-only check reopens the same class of bug for overlong/surrogate/
// out-of-range sequences instead).
//
// On any sequence this does not accept, `pos` advances by exactly one
// byte (so this always makes forward progress - normalize_search_text's
// loop always terminates, on any input) and `cp` is left unset: the raw
// byte's numeric value is not a real codepoint and must never reach
// fold_codepoint() - see normalize_search_text, which copies such a byte
// through unchanged instead.
bool decode_utf8(std::string_view text, std::size_t& pos, char32_t& cp) {
	const auto byte_at = [&](std::size_t i) -> int {
		return pos + i < text.size() ? static_cast<unsigned char>(text[pos + i]) : -1;
	};
	const unsigned char lead = static_cast<unsigned char>(text[pos]);

	if(lead < 0x80) {
		cp = lead;
		++pos;
		return true;
	}

	// How many bytes this lead byte claims, and the valid range for the
	// second byte specifically - see the canonical ranges cited above.
	// Any lead not covered here (0x80..0xC1: a stray continuation byte or
	// an overlong two-byte lead; 0xF5..0xFF: beyond any valid lead) is
	// unconditionally malformed.
	int length = 0;
	int second_lo = 0x80;
	int second_hi = 0xBF;
	if(lead >= 0xC2 && lead <= 0xDF) {
		length = 2;
	} else if(lead == 0xE0) {
		length = 3;
		second_lo = 0xA0; // rules out the overlong E0 80..9F range
	} else if((lead >= 0xE1 && lead <= 0xEC) || (lead >= 0xEE && lead <= 0xEF)) {
		length = 3;
	} else if(lead == 0xED) {
		length = 3;
		second_hi = 0x9F; // rules out the UTF-16 surrogate range
	} else if(lead == 0xF0) {
		length = 4;
		second_lo = 0x90; // rules out the overlong F0 80..8F range
	} else if(lead >= 0xF1 && lead <= 0xF3) {
		length = 4;
	} else if(lead == 0xF4) {
		length = 4;
		second_hi = 0x8F; // rules out codepoints beyond U+10FFFF
	} else {
		++pos;
		return false;
	}

	const int second = byte_at(1);
	if(second < second_lo || second > second_hi) {
		++pos;
		return false;
	}
	int third = 0;
	if(length >= 3) {
		third = byte_at(2);
		if(third < 0x80 || third > 0xBF) {
			++pos;
			return false;
		}
	}
	int fourth = 0;
	if(length == 4) {
		fourth = byte_at(3);
		if(fourth < 0x80 || fourth > 0xBF) {
			++pos;
			return false;
		}
	}

	if(length == 2) {
		cp = (static_cast<char32_t>(lead & 0x1F) << 6) | static_cast<char32_t>(second & 0x3F);
	} else if(length == 3) {
		cp = (static_cast<char32_t>(lead & 0x0F) << 12) |
			 (static_cast<char32_t>(second & 0x3F) << 6) | static_cast<char32_t>(third & 0x3F);
	} else {
		cp = (static_cast<char32_t>(lead & 0x07) << 18) |
			 (static_cast<char32_t>(second & 0x3F) << 12) |
			 (static_cast<char32_t>(third & 0x3F) << 6) | static_cast<char32_t>(fourth & 0x3F);
	}
	pos += static_cast<std::size_t>(length);
	return true;
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
		const std::size_t start = pos;
		char32_t cp = 0;
		if(decode_utf8(utf8, pos, cp)) {
			encode_utf8(fold_codepoint(cp), out);
		} else {
			// A malformed byte is not a codepoint - copy it through
			// exactly as given, never folded, never re-encoded (which
			// would risk producing a *different* invalid byte sequence
			// than the one that came in).
			out += utf8[start];
		}
	}
	return out;
}

} // namespace edopro_next::data

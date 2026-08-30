// SPDX-License-Identifier: AGPL-3.0-or-later

#include "edopro_next/policy/lf_list.h"

#include <fstream>

namespace edopro_next::policy {

namespace {

// Verified against gframe/deck_manager.cpp:57 (the `!Name` handler) and
// :84-85 (the final flush) - upstream seeds every new section's hash with
// this exact literal, and treats a hash still equal to 0 as "no section has
// been opened yet" (never as a real, empty-but-started section - a real
// section's hash starts at this seed, which is nonzero).
constexpr std::uint32_t kHashSeed = 0x7dfcee6a;

constexpr std::string_view kWhitelistKey = "$whitelist";

// The fixed half of gframe/deck_manager.cpp:80's hash expression -
// `(code << 18) | (code >> 14)` - a true 32-bit rotate-left-by-18 (18+14
// == 32), with compile-time-constant shift amounts that are always within
// [0, 31] and therefore never undefined behavior, regardless of `code`.
constexpr std::uint32_t fixed_rotate_term(std::uint32_t code) {
	return (code << 18) | (code >> 14);
}

// The count-dependent half of the same expression -
// `(code << (27 + count)) | (code >> (5 - count))`. C++ requires each shift
// amount to be in [0, 31] for a 32-bit operand ([expr.shift]); solving
// `0 <= 27 + count <= 31` and `0 <= 5 - count <= 31` simultaneously gives
// count in [-26, 4] inclusive - the ONLY domain in which upstream's own
// expression is defined at all. See docs/architecture/deck-legality.md
// #hash-domain for the full derivation and the deliberate divergence this
// module uses outside that domain (a hash-unsafe line is dropped entirely,
// exactly like a malformed one - see parse_lflist() below). This function
// must never be called with a count outside that domain; callers check
// is_hash_safe_count() first.
constexpr std::int32_t kHashSafeCountMin = -26;
constexpr std::int32_t kHashSafeCountMax = 4;

constexpr bool is_hash_safe_count(std::int32_t count) {
	return count >= kHashSafeCountMin && count <= kHashSafeCountMax;
}

std::uint32_t count_dependent_term(std::uint32_t code, std::int32_t count) {
	const auto shift_left = static_cast<unsigned>(27 + count);
	const auto shift_right = static_cast<unsigned>(5 - count);
	return (code << shift_left) | (code >> shift_right);
}

// Splits `text` the same way `std::getline(stream, line)` would when
// repeatedly called until failure: a trailing '\n' does not produce a
// phantom empty final line, but a genuinely empty line between two '\n's
// (or the final segment of text with no trailing '\n' at all) is preserved.
// Verified against this behavior directly (see
// docs/architecture/deck-legality.md#line-splitting for the worked
// examples this reproduces) rather than assumed from memory.
std::vector<std::string_view> split_lines(std::string_view text) {
	std::vector<std::string_view> lines;
	std::size_t start = 0;
	while(start < text.size()) {
		const auto pos = text.find('\n', start);
		if(pos == std::string_view::npos) {
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, pos - start));
		start = pos + 1;
	}
	return lines;
}

} // namespace

LfListParse parse_lflist(std::string_view text) {
	LfListParse result;

	// Mirrors gframe/deck_manager.cpp:41-42's own `LFList lflist; lflist.hash
	// = 0;` local exactly, including its role as the "no section open yet"
	// sentinel - a `$whitelist` or content line seen before the first
	// `!Name` line acts on this not-yet-open section, but can never survive
	// to be emitted (see the `!` and end-of-input handling below), matching
	// upstream's own observable behavior precisely: such a line has no
	// effect on the final result.
	LfList current;
	current.hash = 0;

	std::size_t line_number = 0;
	for(auto raw_line : split_lines(text)) {
		++line_number;

		// gframe/deck_manager.cpp:46-48: truncate at the first '\r'
		// wherever it appears in the line, not merely a trailing CRLF
		// pair - an embedded '\r' anywhere discards everything from that
		// point on.
		std::string line(raw_line);
		if(const auto cr = line.find('\r'); cr != std::string::npos)
			line.erase(cr);

		if(line.empty() || line[0] == '#')
			continue;

		if(line[0] == '!') {
			if(current.hash != 0)
				result.lists.push_back(std::move(current));
			current = LfList{};
			current.name = line.substr(1);
			current.hash = kHashSeed;
			current.whitelist = false;
			continue;
		}

		// A *prefix* match (gframe/deck_manager.cpp:62:
		// `str.rfind(key.data(), 0, key.size()) == 0`), not exact equality -
		// "$whitelistanything" also matches. Runs unconditionally, even
		// before any `!Name` line has been seen (see `current`'s doc
		// comment above for why that has no observable effect).
		if(line.size() >= kWhitelistKey.size() &&
		   std::string_view(line).substr(0, kWhitelistKey.size()) == kWhitelistKey) {
			current.whitelist = true;
			continue;
		}

		// gframe/deck_manager.cpp:66-67: a content line before any `!Name`
		// header is dropped, exactly like a malformed one - it is not even
		// attempted to be parsed.
		if(current.hash == 0)
			continue;

		const auto space = line.find(' ');
		if(space == std::string::npos) {
			result.ignored.push_back({line_number, line, "no space separating code and count"});
			continue;
		}

		// gframe/deck_manager.cpp:71-73: the count field runs from the
		// space up to (but not including) the first character after it
		// that is not a digit or '-' - tolerating trailing text (e.g. a
		// comment) after a valid count, but NOT tolerating a second space:
		// a second space is itself "not a digit or '-'", so it immediately
		// ends the count field, leaving only the first space character
		// itself to be parsed as the count - which fails, dropping the
		// whole line. This is upstream's own behavior, reproduced exactly
		// by using the same find_first_not_of/substr sequence rather than
		// a more lenient split.
		auto count_end = line.find_first_not_of("-0123456789", space + 1);
		std::size_t count_len =
			count_end == std::string::npos ? std::string::npos : count_end - space;

		std::uint32_t code = 0;
		std::int32_t count = 0;
		bool parsed = false;
		bool is_code_zero = false;
		try {
			code = static_cast<std::uint32_t>(std::stoul(line.substr(0, space)));
			// gframe/deck_manager.cpp:76-77: code 0 short-circuits before
			// the count field is even parsed - a malformed count on a
			// code-0 line is therefore irrelevant, exactly as upstream
			// never attempts that parse either.
			if(code == 0) {
				is_code_zero = true;
			} else {
				count = static_cast<std::int32_t>(std::stol(line.substr(space, count_len)));
				parsed = true;
			}
		} catch(...) {
			parsed = false;
		}

		if(is_code_zero)
			continue;

		if(!parsed) {
			result.ignored.push_back({line_number, line, "malformed code or count"});
			continue;
		}

		if(!is_hash_safe_count(count)) {
			// Deliberate divergence: gframe/deck_manager.cpp:80's own hash
			// expression is undefined behavior for this count (see
			// is_hash_safe_count()'s doc comment above and
			// docs/architecture/deck-legality.md#hash-domain). Rather than
			// invent hash semantics upstream itself never defined for this
			// domain, this line is rejected outright - failing closed by
			// excluding it from BOTH the content map and the hash, exactly
			// as a syntactically malformed line is.
			result.ignored.push_back(
				{line_number, line,
				 "count outside the domain in which upstream's own hash expression is defined"});
			continue;
		}

		// gframe/deck_manager.cpp:79: `content[code] = count` overwrites on
		// a duplicate code - the final map holds only the LAST value seen
		// for that code.
		current.content[static_cast<data::CardCode>(code)] = count;
		// gframe/deck_manager.cpp:80: the hash update runs unconditionally
		// for every accepted line, including a duplicate code - so a
		// duplicate code's hash contribution is NOT overwritten the way its
		// content-map entry is; both occurrences are folded into the hash.
		current.hash = current.hash ^ fixed_rotate_term(code) ^ count_dependent_term(code, count);
	}

	if(current.hash != 0)
		result.lists.push_back(std::move(current));

	return result;
}

LfListLoadResult load_lflist(const std::filesystem::path& path) {
	LfListLoadResult result;

	std::ifstream file(path, std::ios::binary);
	if(!file) {
		result.error = "failed to open file: " + path.string();
		return result;
	}

	// Deliberately not `buffer << file.rdbuf()`: that inserter reads
	// directly from the streambuf, bypassing basic_istream::read()'s own
	// sentry/state-update machinery entirely, so a genuine mid-read I/O
	// error below the streambuf (e.g. `path` naming a directory on some
	// platforms) can leave `file` reporting good() with a silently
	// truncated or empty result - external review found this exact defect
	// here; data/src/ydk.cpp's load_ydk() already carries the fix and its
	// own empirical verification note for the identical operation. Reading
	// through file.read() in a sized loop goes through that machinery, so
	// a genuine read failure is distinguishable from a clean EOF via
	// file.bad() below.
	std::string content;
	char chunk[4096];
	while(file.read(chunk, sizeof(chunk)) || file.gcount() > 0)
		content.append(chunk, static_cast<std::size_t>(file.gcount()));
	if(file.bad()) {
		result.error = "failed to read file: " + path.string();
		return result;
	}

	const auto parsed = parse_lflist(content);
	result.ok = true;
	result.lists = parsed.lists;
	result.ignored = parsed.ignored;
	return result;
}

} // namespace edopro_next::policy

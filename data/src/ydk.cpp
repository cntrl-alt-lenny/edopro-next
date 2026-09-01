#include "edopro_next/data/ydk.h"

#include <fstream>
#include <system_error>
#include <stdexcept>

namespace edopro_next::data {

namespace {

// Mirrors LoadCardList's own pre-check (gframe/deck_manager.cpp:296,
// `str.find_first_of("0123456789") != std::string::npos`): a line is only
// ever attempted as a card entry if it contains at least one digit. A line
// with none never reaches std::stoul at all, so it is never reported as a
// "malformed" line either - it was never treated as a number in the first
// place, matching upstream's own control flow, not just its outcome.
bool has_digit(std::string_view line) {
	return line.find_first_of("0123456789") != std::string_view::npos;
}

// Mirrors LoadCardList's CR-strip (gframe/deck_manager.cpp:282-284):
// find the first of '\n' or '\r' in the line and truncate there. The
// caller here has already split on '\n' (see split_lines), so the
// extracted line can never itself contain one - this is equivalent to
// find('\r'), written to make that equivalence explicit rather than
// assumed.
std::string_view strip_cr(std::string_view line) {
	const auto pos = line.find_first_of("\n\r");
	return pos == std::string_view::npos ? line : line.substr(0, pos);
}

struct RawLine {
	std::size_t number;
	std::string_view text;
};

// Splits `text` on '\n', the same boundary std::getline(stream, line)
// uses against a stream opened in binary (untranslated) mode - which is
// what FileStream is on Linux (gframe/file_stream.h): no OS-level CRLF
// translation, so getline's splitting is purely a search for '\n' bytes.
// A trailing '\n' at the very end of `text` does not produce a spurious
// empty final line (getline stops at EOF without another read); text with
// no trailing '\n' still yields its last, unterminated line (getline's
// final read reaches EOF right after extracting it, and still returns
// true for that read).
std::vector<RawLine> split_lines(std::string_view text) {
	std::vector<RawLine> lines;
	std::size_t pos = 0;
	std::size_t number = 0;
	while(pos < text.size()) {
		const auto nl = text.find('\n', pos);
		++number;
		if(nl == std::string_view::npos) {
			lines.push_back({number, text.substr(pos)});
			break;
		}
		lines.push_back({number, text.substr(pos, nl - pos)});
		pos = nl + 1;
	}
	return lines;
}

// Strips '\n'/'\r' from a caller-supplied creator string before it is
// emitted. Without this, a creator string containing an embedded newline
// would not stay a single cosmetic comment line: text after the break
// would start a new line of its own, potentially matching "#extra"/"!..."
// or a bare card code and changing what a later parse_ydk() of this
// codec's own output actually contains - a caller-controlled string should
// never be able to inject deck content this way. Every other character is
// passed through unchanged; only the two characters that can create a new
// line are removed.
std::string sanitize_creator_line(std::string_view creator) {
	std::string out;
	out.reserve(creator.size());
	for(const char c : creator) {
		if(c != '\n' && c != '\r')
			out += c;
	}
	return out;
}

} // namespace

YdkParse parse_ydk(std::string_view text) {
	YdkParse result;
	// Equivalent to always calling LoadCardList with a non-null
	// `extralist` - see docs/architecture/deck-model.md#explicit-sections
	// for why this codec treats the file's own markers as authoritative
	// in every case, rather than reproducing LoadCardList's other mode
	// (extralist == nullptr) where "#extra" is inert and section
	// membership is decided later, by card type, in LoadDeck - a
	// CardDatabase-dependent step this codec deliberately does not
	// reimplement.
	bool is_extra = false;
	bool is_side = false;
	for(const auto& raw_line : split_lines(text)) {
		const std::string_view line = strip_cr(raw_line.text);
		if(line.empty())
			continue;
		if(line[0] == '#') {
			if(line == "#extra")
				is_extra = true;
			continue;
		}
		if(line[0] == '!') {
			is_side = true;
			continue;
		}
		if(!has_digit(line))
			continue;
		std::uint32_t code = 0;
		try {
			// std::stoul, exactly as LoadCardList calls it
			// (gframe/deck_manager.cpp:298): base 10, optional leading
			// whitespace and sign, stops at the first non-digit rather
			// than requiring the whole line to be numeric, and inherits
			// whatever width `unsigned long` has on the build platform -
			// see deck-model.md for the empirically-verified semantics
			// this depends on (leading sign wraps via unsigned
			// arithmetic, trailing garbage is silently ignored, genuine
			// non-numeric or out-of-range input throws).
			code = static_cast<std::uint32_t>(std::stoul(std::string(line)));
		} catch(const std::exception&) {
			result.ignored.push_back({raw_line.number, std::string(line), "malformed card code"});
			continue;
		}
		if(code == 0) {
			// Deliberate divergence from LoadCardList's raw behaviour,
			// which stores a 0 line structurally like any other code
			// (gframe/deck_manager.cpp:296-310 has no code == 0 check at
			// all - that check lives in the unrelated LFList/banlist
			// parser a few dozen lines earlier in the same file, not
			// here). Downstream, LoadDeck resolves an unrecognized code
			// (0 included, since no real .cdb row has id 0 - see
			// card_database.h) to a dummy CardDataC with code == 0,
			// which non-separated loads drop (errorcode, continue) and
			// separated/"loadalways" loads keep as an untyped Main-deck
			// placeholder - a CardDatabase-dependent policy this codec
			// does not reimplement. Since CardCode::None == 0 is this
			// project's own "not a real card" sentinel (card_code.h),
			// this codec excludes it here rather than storing it under
			// either upstream behaviour - see deck-model.md#card-code-0.
			result.ignored.push_back(
				{raw_line.number, std::string(line), "card code 0 is not a real card"});
			continue;
		}
		const auto card = static_cast<CardCode>(code);
		if(is_side)
			result.deck.side.push_back(card);
		else if(is_extra)
			result.deck.extra.push_back(card);
		else
			result.deck.main.push_back(card);
	}
	return result;
}

YdkLoadResult load_ydk(const std::filesystem::path& path) {
	YdkLoadResult result;
	std::error_code status_error;
	if(std::filesystem::is_directory(path, status_error)) {
		result.error = "failed to read file: " + path.string();
		return result;
	}
	if(status_error) {
		result.error = "failed to inspect file: " + path.string();
		return result;
	}

	std::ifstream file(path, std::ios::binary);
	if(!file) {
		result.error = "failed to open file: " + path.string();
		return result;
	}
	// Deliberately not `buffer << file.rdbuf()`: that inserter reads
	// directly from the streambuf, bypassing basic_istream::read()'s own
	// sentry/state-update machinery entirely, so a genuine mid-read I/O
	// error below the streambuf (short read, device error) can leave
	// `file` reporting good() with a silently truncated or empty result -
	// confirmed empirically (a streambuf-backed read of /proc/self/mem,
	// which opens successfully but fails on read, left file.bad() false
	// with zero bytes captured). A directory is rejected before opening:
	// libc++ can otherwise surface it as a clean EOF, indistinguishable
	// from a valid empty file. Reading through file.read() in a sized loop
	// goes through that machinery, so a genuine read failure is
	// distinguishable from a clean EOF via file.bad() below - verified
	// empirically for exact and non-exact chunk-boundary file sizes, and
	// for a zero-byte file.
	std::string content;
	char chunk[4096];
	while(file.read(chunk, sizeof(chunk)) || file.gcount() > 0)
		content.append(chunk, static_cast<std::size_t>(file.gcount()));
	if(file.bad()) {
		result.error = "failed to read file: " + path.string();
		return result;
	}
	auto parsed = parse_ydk(content);
	result.ok = true;
	result.deck = std::move(parsed.deck);
	result.ignored = std::move(parsed.ignored);
	return result;
}

std::string serialize_ydk(const Deck& deck, const YdkWriteOptions& options) {
	std::string out;
	// Matches DeckManager::SaveDeck (gframe/deck_manager.cpp:441): the
	// creator line, when present, precedes "#main" on its own line rather
	// than being folded into it.
	if(options.creator) {
		out += "#created by ";
		out += sanitize_creator_line(*options.creator);
		out += '\n';
	}
	auto write_section = [&out](std::string_view marker, const std::vector<CardCode>& cards) {
		out += marker;
		out += '\n';
		for(const auto card : cards) {
			out += std::to_string(to_number(card));
			out += '\n';
		}
	};
	// All three markers are written unconditionally, even for an empty
	// section - SaveDeck never guards them on the section being
	// non-empty, so a canonical file always has exactly this structure.
	write_section("#main", deck.main);
	write_section("#extra", deck.extra);
	write_section("!side", deck.side);
	return out;
}

YdkSaveResult save_ydk(const std::filesystem::path& path, const Deck& deck,
						const YdkWriteOptions& options) {
	YdkSaveResult result;
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if(!file) {
		result.error = "failed to open file for writing: " + path.string();
		return result;
	}
	const auto text = serialize_ydk(deck, options);
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	// write() can leave `file` reporting good() even though the data
	// never reached the destination: a full filesystem (verified
	// empirically against /dev/full) can accept the write() call into the
	// stream buffer and only fail once that buffer is actually flushed.
	// Flushing explicitly here, before checking `file`, surfaces that
	// failure at a precise point instead of leaving it for whatever
	// implicitly flushes next.
	file.flush();
	if(!file) {
		result.error = "failed to write file: " + path.string();
		return result;
	}
	// A successful flush() does not make close() a formality: close()
	// performs its own finalization (std::basic_fstream::close() sets
	// failbit if the underlying close fails, independently of any earlier
	// flush - e.g. a filesystem, network mount, or quota check that only
	// reports an error at close time, with an empty buffer and nothing
	// left to flush). Relying on the destructor to close - as the
	// original version of this function did - would let that failure
	// happen after `ok` had already been returned to the caller with no
	// way to report it, which is the same class of false-success this
	// function's flush() check was already added to close. The success
	// path is therefore write, then flush, then close, each checked in
	// turn, only setting `ok = true` once close() has itself succeeded.
	file.close();
	if(!file) {
		result.error = "failed to close file: " + path.string();
		return result;
	}
	result.ok = true;
	return result;
}

} // namespace edopro_next::data

// A presentation-independent representation and parser for upstream's
// "limitation/forbidden card list" (LFList / banlist) file format. See
// docs/architecture/deck-legality.md#lflist-grammar for the full,
// source-verified grammar this reproduces (gframe/deck_manager.cpp's
// DeckManager::LoadLFListSingle, gframe/deck_manager.h's LFList struct) and
// #hash-domain for the one deliberate divergence from it.
//
// This is a text/file <-> value boundary only, mirroring edopro_next::data's
// ydk.h split: parsing in-memory text is independently testable, and
// filesystem loading adds only filesystem failure semantics on top of it.
// It never consults a CardDatabase and never decides whether a deck is
// legal - see deck_validation.h for that.
#ifndef EDOPRO_NEXT_POLICY_LF_LIST_H
#define EDOPRO_NEXT_POLICY_LF_LIST_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "edopro_next/data/card_code.h"

namespace edopro_next::policy {

// One named banlist section - the text from one `!Name` line up to (but
// not including) the next `!` line or end of input. `hash` is a fingerprint
// of this section's own accepted content lines, computed exactly as
// upstream computes it (gframe/deck_manager.cpp:80) whenever a content
// line's count falls within the domain that expression is well-defined for
// - see docs/architecture/deck-legality.md#hash-domain for the domain and
// the deliberate divergence outside it (parse_lflist() below).
//
// `name` is plain UTF-8 (matching CardRecord::name/text's own convention -
// see card_record.h), not upstream's std::wstring: nothing in this module
// touches Irrlicht text rendering, so there is no reason to carry a wide
// string at all, and the underlying bytes are identical either way.
struct LfList {
	std::string name;
	std::uint32_t hash = 0;
	bool whitelist = false;
	// Card code -> limitation count. This module does not interpret what a
	// given count *means* (0/1/2/3 conventions are a Yu-Gi-Oh rules concept,
	// not a parsing one) - it only carries what the file says, exactly like
	// upstream's own banlist_content_t. A negative count is stored exactly
	// as parsed (see docs/architecture/deck-legality.md#hash-domain for the
	// one domain in which a parsed count is rejected outright).
	std::map<data::CardCode, std::int32_t> content;

	friend bool operator==(const LfList&, const LfList&) = default;
};

// One source line, within an already-open section, that looked like it was
// meant to be a `<code> <count>` content line but could not be accepted as
// one (no space found, a code/count parse failure, or a count outside the
// domain #hash-domain documents). `line_number` is 1-based.
//
// This has no upstream equivalent - LoadLFListSingle gives no diagnostic
// signal at all for a rejected line, silently `continue`-ing exactly the
// same way for this case as for a blank line, a `#` comment, a content
// line seen before any `!Name` header, or a code-0 line (all four of which
// this module also drops silently, with no entry here, matching upstream's
// own equally-silent treatment of them). This vector exists purely for
// this module's own testability/transparency and never affects the
// resulting LfLists.
struct LfListIgnoredLine {
	std::size_t line_number = 0;
	std::string text;
	std::string reason;
};

// The result of parse_lflist(). Parsing in-memory text has no filesystem
// dimension to fail on, and upstream's own grammar never rejects a file
// outright for its content (only individual lines are dropped) - so this
// always "succeeds" in the sense of producing a (possibly empty) list of
// LfLists; an input with no `!name` section header anywhere produces zero
// lists.
struct LfListParse {
	std::vector<LfList> lists;
	std::vector<LfListIgnoredLine> ignored;
};

// Parses LFList text already in memory, reproducing
// DeckManager::LoadLFListSingle's exact grammar - see
// docs/architecture/deck-legality.md#lflist-grammar for the full,
// line-by-line citation. In summary: `#` comments and blank lines are
// skipped; a `!Name` line starts a new named section (closing and emitting
// the previous one, if any, whose hash is nonzero); `$whitelist` (a
// *prefix* match, not exact equality - upstream tests with
// `str.rfind(key,0,key.size())==0`) marks the current section as a
// whitelist; any other line before the first `!Name` is dropped, exactly
// like a malformed one; a content line is `<code> <count>`, split on the
// first space, with the count read up to (but not including) the first
// character after it that is not a digit or `-` - so trailing text after a
// valid count (e.g. a comment) is tolerated, but a *second* space between
// code and count is not (it becomes part of what the count parser is asked
// to convert, which then fails - see docs/architecture/deck-legality.md's
// own worked example). A malformed code/count, or a code of 0, drops the
// line silently, matching upstream exactly.
LfListParse parse_lflist(std::string_view text);

// The result of load_lflist(). Unlike LfListParse, this has a genuine
// failure state: `ok` is false exactly when the file could not be
// opened/read, in which case `lists` and `ignored` are both left empty
// rather than partially populated.
struct LfListLoadResult {
	bool ok = false;
	std::string error;
	std::vector<LfList> lists;
	std::vector<LfListIgnoredLine> ignored;

	explicit operator bool() const noexcept { return ok; }
};

// Reads `path` and parses it. Never partially applies: on failure the
// returned lists/ignored are both empty, exactly like CardDatabase's
// LoadResult and edopro_next::data::load_ydk()'s YdkLoadResult.
LfListLoadResult load_lflist(const std::filesystem::path& path);

} // namespace edopro_next::policy

#endif // EDOPRO_NEXT_POLICY_LF_LIST_H

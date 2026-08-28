// A `.ydk` codec: reads and writes the Project Ignis deck-list text format
// into/from a Deck. See docs/architecture/deck-model.md for the exact
// source-verified grammar this reproduces (gframe/deck_manager.cpp's
// LoadCardList/SaveDeck/MakeYdkEntryString) and
// docs/adr/0004-deck-model-ydk-codec.md for the explicit-sections and
// code-0 decisions this codec implements.
//
// This is a text <-> Deck boundary only. It never opens a CardDatabase,
// never checks whether a code is a real card, and never classifies a code
// as Main/Extra by its type - see deck.h.
#ifndef EDOPRO_NEXT_DATA_YDK_H
#define EDOPRO_NEXT_DATA_YDK_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "edopro_next/data/deck.h"

namespace edopro_next::data {

// One source line that produced no card entry despite being processed as
// a candidate (that is: it was not blank, not a `#`/`!` marker or comment,
// and contained at least one digit) - either because it failed the same
// numeric parse upstream's LoadCardList performs, or because it parsed to
// card code 0, which this codec excludes as upstream's own "not a real
// card" sentinel (CardCode::None - see card_code.h). `line_number` is
// 1-based. Never affects the resulting Deck; purely informational, and
// callers that do not care may ignore it entirely.
struct YdkIgnoredLine {
	std::size_t line_number = 0;
	std::string text;
	std::string reason;
};

// The result of parse_ydk(). Parsing in-memory text has no filesystem
// dimension to fail on and no line upstream's own grammar treats as fatal
// (LoadCardList never rejects a file for its content, only for failing to
// open) - so this always "succeeds" in the sense of producing a Deck; an
// input with no recognisable card lines simply produces an empty one.
struct YdkParse {
	Deck deck;
	std::vector<YdkIgnoredLine> ignored;
};

// Parses `.ydk` text already in memory. Section membership is decided
// purely by the file's own markers - see docs/architecture/deck-model.md
// for the full grammar - never by card type, and never by consulting a
// CardDatabase: a syntactically valid, non-zero card code is stored as-is
// whether or not anything currently loaded recognises it.
YdkParse parse_ydk(std::string_view text);

// The result of load_ydk(). Unlike YdkParse, this has a genuine failure
// state: `ok` is false exactly when the file could not be opened/read, in
// which case `deck` and `ignored` are both left empty rather than
// partially populated - there is no in-place mutation of a caller-owned
// Deck for a failed load to leave half done.
struct YdkLoadResult {
	bool ok = false;
	std::string error;
	Deck deck;
	std::vector<YdkIgnoredLine> ignored;

	explicit operator bool() const noexcept { return ok; }
};

// Reads `path` and parses it. Never partially applies: on failure the
// returned Deck is empty, and the only way to observe a failed load is to
// check `ok`/`error` first, exactly like CardDatabase's LoadResult.
YdkLoadResult load_ydk(const std::filesystem::path& path);

// Optional, purely cosmetic metadata for serialize_ydk()/save_ydk(). Never
// affects parsing - see MATCH/DIVERGE notes in deck-model.md for why the
// legacy "# <CardName>" per-card comment (which needs a CardDatabase name
// lookup) is deliberately not offered here at all, and why creator is the
// only piece of upstream's optional writer metadata this codec exposes.
struct YdkWriteOptions {
	// When set, emitted verbatim as the file's first line: "#created by
	// <creator>\n", matching DeckManager::SaveDeck. When unset, that line
	// is omitted entirely - this codec has no notion of a "current
	// nickname" to default it to, by design (see deck-model.md).
	std::optional<std::string> creator;
};

// Serializes `deck` into canonical `.ydk` text: "#main\n", each main code,
// "#extra\n", each extra code, "!side\n", each side code - in that order,
// unconditionally, with a plain '\n' terminator throughout regardless of
// host platform. Deterministic: the same Deck and options always produce
// byte-identical output. Never touches a CardDatabase.
std::string serialize_ydk(const Deck& deck, const YdkWriteOptions& options = {});

// The result of save_ydk(). `ok` is false exactly when the file could not
// be written, matching CardDatabase's LoadResult in spirit (though not its
// exact shape - "rows_loaded" has no meaning for a write).
struct YdkSaveResult {
	bool ok = false;
	std::string error;

	explicit operator bool() const noexcept { return ok; }
};

// Serializes `deck` and writes it to `path`, overwriting any existing
// file. Writes in binary mode so the emitted '\n' bytes reach disk
// unchanged regardless of host platform text-mode translation - see
// deck-model.md for why byte-for-byte determinism is part of this
// codec's contract, not an incidental property.
YdkSaveResult save_ydk(const std::filesystem::path& path, const Deck& deck,
						const YdkWriteOptions& options = {});

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_YDK_H

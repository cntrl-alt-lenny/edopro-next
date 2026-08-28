// A fast, deterministic, presentation-independent search over a
// CardDatabase's currently loaded catalogue. See
// docs/architecture/card-search.md for the full design (snapshot
// lifecycle, normalization, ranking, filters, performance measurements)
// and docs/adr/0005-card-search-structured-query.md for the load-bearing
// decisions.
//
// This is a data-filtering boundary, not a legality boundary: no deck
// size, no three-copy limit, no LFList/banlist, no whitelist, no
// "is this card legal" concept anywhere in this class - see
// search_query.h's own file-level comment.
#ifndef EDOPRO_NEXT_DATA_CARD_SEARCH_INDEX_H
#define EDOPRO_NEXT_DATA_CARD_SEARCH_INDEX_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "edopro_next/data/card_code.h"
#include "edopro_next/data/card_database.h"
#include "edopro_next/data/search_query.h"
#include "edopro_next/data/search_result.h"

namespace edopro_next::data {

// Synchronous and single-threaded in contract: rebuild() and search() are
// ordinary member functions with no internal locking, and this class
// makes no guarantee about calling rebuild() concurrently with an
// in-progress search() (or two rebuild() calls concurrently) - that is
// the caller's responsibility to serialize, exactly like any other
// non-thread-safe C++ value type. A single already-built snapshot may be
// searched from multiple threads at once, since search() only reads
// entries_ and never mutates it. See docs/architecture/card-search.md
// #threading for why no stronger guarantee is provided in this slice.
class CardSearchIndex {
public:
	CardSearchIndex() = default;

	// Builds a private snapshot from `database`'s state at the moment of
	// this call - the currently active locale's effective name/text
	// (whatever CardDatabase::find() would currently return), and every
	// loaded card's static metadata. Replaces any previous snapshot
	// entirely; never mutates `database`.
	//
	// This index does not observe `database` afterward: a later
	// load_database(), load_locale(), or clear_locale() call is invisible
	// to search() until rebuild() runs again - see
	// docs/architecture/card-search.md#snapshot-and-rebuild for why this
	// simple, explicit lifecycle was chosen over an automatic observer.
	void rebuild(const CardDatabase& database);

	// Deterministic: the same built snapshot and the same query always
	// produce the same ordered result sequence - see
	// docs/architecture/card-search.md#ranking for the exact tier and
	// tie-break rules. Never touches `database`, only the snapshot
	// rebuild() last built. Results identify cards by CardCode only -
	// never a CardRecord*, never a pointer or iterator into this index's
	// own storage - so a result stays valid to read after this index is
	// rebuilt or destroyed; look the code up again via
	// CardDatabase::find() for its current CardRecord.
	std::vector<SearchResult> search(const SearchQuery& query) const;

	std::size_t size() const noexcept { return entries_.size(); }
	bool empty() const noexcept { return entries_.empty(); }

private:
	// Everything search() needs about one card, and nothing else - no
	// copy of the sixteen auxiliary strings (never searched, see
	// search_query.h's TextScope comment), no alias field kept for later
	// resolution (resolved once, into effective_setcodes, at rebuild()
	// time instead - see card_search_index.cpp and
	// docs/architecture/card-search.md#alias-and-setcode).
	struct Entry {
		CardCode code = CardCode::None;
		std::string normalized_name;
		std::string normalized_text;
		std::uint32_t scope = 0;
		std::uint32_t type = 0;
		std::uint32_t category = 0;
		std::uint32_t attribute = 0;
		std::uint64_t race = 0;
		std::uint32_t link_marker = 0;
		std::int32_t attack = 0;
		std::int32_t defense = 0;
		std::uint32_t level = 0;
		std::uint32_t left_scale = 0;
		std::uint32_t right_scale = 0;
		std::vector<std::uint16_t> effective_setcodes;
	};

	// Ascending by CardCode, inherited from CardDatabase's own ascending
	// iteration order (card_database.h) - rebuild() does not sort this
	// separately. Not load-bearing for search() (which always scans every
	// entry - see card_search_index.cpp), just a side effect of where
	// this snapshot's data comes from.
	std::vector<Entry> entries_;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_CARD_SEARCH_INDEX_H

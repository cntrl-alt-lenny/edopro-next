// The result of a CardSearchIndex query: a CardCode plus why it matched,
// never a pointer into index- or database-owned storage. See
// card_search_index.h for the full lifetime contract and
// docs/architecture/card-search.md#ranking for how MatchKind values are
// assigned and ordered.
#ifndef EDOPRO_NEXT_DATA_SEARCH_RESULT_H
#define EDOPRO_NEXT_DATA_SEARCH_RESULT_H

#include "edopro_next/data/card_code.h"

namespace edopro_next::data {

// Declared in rank-priority order - CardSearchIndex::search() sorts
// ascending by this enum's underlying value before its deterministic
// name/code tie-break, so a lower enum value is always a higher-priority
// match. This ordering is part of the type's contract, not an
// implementation accident: do not reorder these without updating
// card_search_index.cpp's ranking accordingly.
enum class MatchKind {
	// SearchQuery::exact_code was set and this entry is that code, and it
	// independently passed every other active filter (text included) -
	// the most specific possible match, reported in place of whatever
	// tier its text match would otherwise have earned. Not a bypass:
	// exact_code cannot make an entry appear that its own filters/text
	// would otherwise have excluded - see search_query.h.
	ExactCode,
	// SearchQuery::text, once normalized, equals the entry's normalized
	// name exactly.
	ExactName,
	// The entry's normalized name starts with the normalized query text.
	NamePrefix,
	// Every whitespace-separated token of the normalized query text
	// appears somewhere in the entry's normalized name (order-independent
	// - see search_query.h's own note on why this departs from
	// upstream's ordered token matching).
	NameMatch,
	// No name-tier match, but every token appears somewhere in the
	// entry's normalized text (rules text). Only reachable when
	// TextScope::Text or TextScope::NameOrText is in effect.
	TextMatch,
	// SearchQuery::text was empty: every entry passing the static filters
	// matches, with no text-based tier to distinguish between them - all
	// such results share this one kind and are ordered purely by the
	// deterministic name/code tie-break.
	NoTextQuery,
};

struct SearchResult {
	CardCode code;
	MatchKind match;

	friend bool operator==(const SearchResult&, const SearchResult&) = default;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_SEARCH_RESULT_H

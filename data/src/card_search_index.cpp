#include "edopro_next/data/card_search_index.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "edopro_next/data/text_normalize.h"

namespace edopro_next::data {

namespace {

// Verified against ocgcore's own public header
// (ocgcore/ocgapi_constants.h:56,58), matching the citation precedent
// already established for the Link bit in data/src/card_database.cpp - a
// card-schema constant, not a legacy-client one, so a single cited
// constant here is preferable to copying gframe headers or scattering an
// uncited literal.
constexpr std::uint32_t kTypeLinkBit = 0x4000000;
constexpr std::uint32_t kTypePendulumBit = 0x1000000;

bool passes_bitmask(const std::optional<BitmaskFilter>& filter, std::uint32_t field) {
	if(!filter)
		return true;
	return (field & filter->require_all_bits) == filter->require_all_bits;
}

bool passes_bitmask64(const std::optional<BitmaskFilter64>& filter, std::uint64_t field) {
	if(!filter)
		return true;
	return (field & filter->require_all_bits) == filter->require_all_bits;
}

bool passes_numeric(const NumericFilter& filter, std::int64_t field) {
	switch(filter.comparison) {
	case NumericComparison::EqualTo:
		return field == filter.value;
	case NumericComparison::AtLeast:
		return field >= filter.value;
	case NumericComparison::AtMost:
		return field <= filter.value;
	}
	return false;
}

// Splits on ASCII whitespace, dropping empty segments - " a  b\tc " ->
// {"a", "b", "c"}, not {"", "a", "", "b", "c", ""}. Deliberately simpler
// than upstream's Utils::TokenizeString (gframe/utils.h), which is a
// generic multi-separator splitter that can produce empty tokens for
// consecutive separators - this only ever needs to split query text into
// non-empty search tokens.
std::vector<std::string_view> whitespace_tokens(std::string_view text) {
	std::vector<std::string_view> tokens;
	std::size_t pos = 0;
	while(pos < text.size()) {
		while(pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' ||
									 text[pos] == '\r'))
			++pos;
		if(pos >= text.size())
			break;
		const std::size_t start = pos;
		while(pos < text.size() && text[pos] != ' ' && text[pos] != '\t' && text[pos] != '\n' &&
			  text[pos] != '\r')
			++pos;
		tokens.push_back(text.substr(start, pos - start));
	}
	return tokens;
}

// True iff every token in `tokens` appears somewhere in `haystack`, in
// any order, independently (tokens may overlap in the haystack). This is
// a deliberate departure from upstream's Utils::ContainsSubstring
// (gframe/utils.h/.cpp), whose doc comment says "all tokens are contained
// in the input" but whose actual implementation additionally requires
// them in left-to-right, non-overlapping order (each token's search
// starts from where the previous one's match ended) - an artifact of how
// it is written with a single forward-advancing find(), not a stated
// design choice, and not a rule this deliberately-order-independent
// search API reproduces. See docs/architecture/card-search.md#query-
// token-semantics.
bool contains_all_tokens(std::string_view haystack, const std::vector<std::string_view>& tokens) {
	for(const auto token : tokens) {
		if(haystack.find(token) == std::string_view::npos)
			return false;
	}
	return true;
}

// One ranked match, kept only for the duration of one search() call.
// `normalized_name` is a view into the CardSearchIndex::Entry it came
// from (entries_, which outlives this local vector for the whole call),
// used only to compute the deterministic tie-break - never exposed
// outside this function.
struct RankedMatch {
	CardCode code;
	MatchKind match;
	std::string_view normalized_name;
};

} // namespace

void CardSearchIndex::rebuild(const CardDatabase& database) {
	std::vector<Entry> entries;
	entries.reserve(database.size());
	for(const auto& [code, record] : database) {
		Entry entry;
		entry.code = code;
		entry.normalized_name = normalize_search_text(record.name);
		entry.normalized_text = normalize_search_text(record.text);
		entry.scope = record.scope;
		entry.type = record.type;
		entry.category = record.category;
		entry.attribute = record.attribute;
		entry.race = record.race;
		entry.link_marker = record.link_marker;
		entry.attack = record.attack;
		entry.defense = record.defense;
		entry.level = record.level;
		entry.left_scale = record.left_scale;
		entry.right_scale = record.right_scale;
		// Mirrors gframe/deck_con.cpp's CardSetcodes(): an aliased card's
		// setcode search uses the card it is an alias OF, when that card
		// is itself present in the database this snapshot is built from -
		// resolved once, here, rather than per-query.
		entry.effective_setcodes = record.setcodes;
		if(record.alias != CardCode::None) {
			if(const auto* aliased = database.find(record.alias))
				entry.effective_setcodes = aliased->setcodes;
		}
		entries.push_back(std::move(entry));
	}
	entries_ = std::move(entries);
}

std::vector<SearchResult> CardSearchIndex::search(const SearchQuery& query) const {
	const auto passes_filters = [&query](const Entry& e) {
		if(!passes_bitmask(query.type, e.type))
			return false;
		if(!passes_bitmask(query.category, e.category))
			return false;
		if(query.attribute && e.attribute != *query.attribute)
			return false;
		if(!passes_bitmask64(query.race, e.race))
			return false;
		if(!passes_bitmask(query.link_marker, e.link_marker))
			return false;
		if(!passes_bitmask(query.scope, e.scope))
			return false;
		if(query.attack && !passes_numeric(*query.attack, e.attack))
			return false;
		if(query.defense) {
			// A Link monster's `defense` column is not a defense value at
			// all (card_record.h) - a defense filter can never match one,
			// regardless of `value`, matching CheckCardProperties's own
			// unconditional `|| (data.type & TYPE_LINK)` exclusion
			// (gframe/deck_con.cpp).
			if(e.type & kTypeLinkBit)
				return false;
			if(!passes_numeric(*query.defense, e.defense))
				return false;
		}
		if(query.level && !passes_numeric(*query.level, e.level))
			return false;
		if(query.left_scale) {
			// A Pendulum scale filter can only match a Pendulum monster -
			// matching CheckCardProperties's own unconditional
			// `|| !(data.type & TYPE_PENDULUM)` exclusion.
			if(!(e.type & kTypePendulumBit))
				return false;
			if(!passes_numeric(*query.left_scale, e.left_scale))
				return false;
		}
		if(query.right_scale) {
			if(!(e.type & kTypePendulumBit))
				return false;
			if(!passes_numeric(*query.right_scale, e.right_scale))
				return false;
		}
		if(query.setcodes && !query.setcodes->empty()) {
			const bool any = std::any_of(
				query.setcodes->begin(), query.setcodes->end(), [&e](std::uint16_t setcode) {
					return std::find(e.effective_setcodes.begin(), e.effective_setcodes.end(),
									  setcode) != e.effective_setcodes.end();
				});
			if(!any)
				return false;
		}
		return true;
	};

	std::string normalized_query;
	std::vector<std::string_view> tokens;
	if(!query.text.empty()) {
		normalized_query = normalize_search_text(query.text);
		tokens = whitespace_tokens(normalized_query);
	}

	const auto classify_text_match = [&](const Entry& e) -> std::optional<MatchKind> {
		if(query.text_scope == TextScope::Name || query.text_scope == TextScope::NameOrText) {
			if(e.normalized_name == normalized_query)
				return MatchKind::ExactName;
			if(e.normalized_name.starts_with(normalized_query))
				return MatchKind::NamePrefix;
			if(contains_all_tokens(e.normalized_name, tokens))
				return MatchKind::NameMatch;
		}
		if(query.text_scope == TextScope::Text || query.text_scope == TextScope::NameOrText) {
			if(contains_all_tokens(e.normalized_text, tokens))
				return MatchKind::TextMatch;
		}
		return std::nullopt;
	};

	// A full scan even when exact_code is set: exact_code overrides the
	// *ranking* of whichever entry matches it (see search_query.h), it
	// does not restrict the search to that one entry - every other card
	// is still evaluated normally, and still needs to independently pass
	// `query`'s other filters and text to appear.
	std::vector<RankedMatch> matches;
	for(const auto& e : entries_) {
		if(!passes_filters(e))
			continue;
		MatchKind kind;
		if(query.text.empty()) {
			kind = MatchKind::NoTextQuery;
		} else {
			const auto classified = classify_text_match(e);
			if(!classified)
				continue;
			kind = *classified;
		}
		if(query.exact_code && e.code == *query.exact_code)
			kind = MatchKind::ExactCode;
		matches.push_back({e.code, kind, e.normalized_name});
	}

	std::sort(matches.begin(), matches.end(), [](const RankedMatch& a, const RankedMatch& b) {
		if(a.match != b.match)
			return a.match < b.match;
		if(a.normalized_name != b.normalized_name)
			return a.normalized_name < b.normalized_name;
		return a.code < b.code;
	});

	if(query.limit && matches.size() > *query.limit)
		matches.resize(*query.limit);

	std::vector<SearchResult> results;
	results.reserve(matches.size());
	for(const auto& m : matches)
		results.push_back(SearchResult{m.code, m.match});
	return results;
}

} // namespace edopro_next::data

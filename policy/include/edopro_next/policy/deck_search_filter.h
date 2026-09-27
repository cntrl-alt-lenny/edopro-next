// The parts of upstream's deck-builder search that depend on the selected
// banlist: which cards the search shows at all, and the "limit" filter
// (banned, limited, semi-limited, unlimited, and the card-pool categories
// upstream puts in the same list). See docs/architecture/card-search.md
// §2.3 for the upstream source quoted at file and line, and
// docs/adr/0012-deck-builder-search-filters.md for the decisions.
//
// Reproduces gframe/deck_con.cpp:1192-1193 (the cards hidden before any
// filter applies) and :1254-1322 (the `filter_lm` switch), and
// gframe/game.cpp:3412-3442 (Game::ReloadCBLimit, which choices the list
// offers). Everything else in CheckCardProperties is card data, and is
// data::SearchQuery's job; this header decides nothing a card's own fields
// do not already decide except through the banlist and the "show
// non-official cards" switch.
//
// No Qt type and no UI concept: the deck builder's adapter asks and
// renders the answer.
#ifndef EDOPRO_NEXT_POLICY_DECK_SEARCH_FILTER_H
#define EDOPRO_NEXT_POLICY_DECK_SEARCH_FILTER_H

#include <vector>

#include "edopro_next/data/card_record.h"
#include "edopro_next/policy/lf_list.h"

namespace edopro_next::policy {

// gframe/deck_con.h:20-38, `limitation_search_filters`, value for value.
enum class LimitationFilter {
	None,
	Banned,
	Limited,
	SemiLimited,
	Unlimited,
	Ocg,
	Tcg,
	TcgOcg,
	Prerelease,
	Speed,
	Rush,
	Legend,
	Anime,
	Illegal,
	VideoGame,
	Custom,
	All,
};

// The choices upstream's limit list offers, in its order - Game::ReloadCBLimit
// (gframe/game.cpp:3412-3442). With a whitelist: None (labelled for the
// whitelist), the four banlist counts, Legend, Illegal, Prerelease, All.
// Otherwise: None, the four banlist counts, OCG, TCG, TCG/OCG, Prerelease,
// Speed, Rush, Legend, and, only while non-official cards are shown, Anime,
// Illegal, Video game, Custom.
//
// `lflist` is the deck builder's selected banlist, or nullptr for this
// project's "No banlist" choice, which upstream does not have (it always
// has a list selected). With nullptr the four banlist counts are not
// offered: there is no list to count against. That is this project's
// decision, recorded in ADR 0012.
std::vector<LimitationFilter> limitation_filter_choices(const LfList* lflist, bool show_non_official);

// Whether the "show non-official cards" switch has any effect: upstream
// disables it while the selected list is a whitelist (game.cpp:3420-3436),
// because a whitelist shows non-official cards regardless (deck_con.cpp:1192).
bool non_official_switch_available(const LfList* lflist);

// Whether upstream's deck-builder search would list `record` at all, given
// the selected banlist, the "show non-official cards" switch and the limit
// filter. Every other search filter is applied separately, by
// data::CardSearchIndex. Exactly deck_con.cpp:1192-1193 and :1254-1322:
//
//  - a token (TYPE_TOKEN) or a hidden card (SCOPE_HIDDEN) is never listed;
//  - a card whose scope has any bit outside OCG, TCG and Prerelease is listed
//    only while non-official cards are shown or the list is a whitelist;
//  - Banned, Limited and SemiLimited keep a card whose count is exactly 0, 1
//    or 2; Unlimited keeps a count of 3 or more; a card with no entry counts
//    3, or -1 in a whitelist (deck_con.cpp:1256-1261);
//  - Ocg, Tcg, TcgOcg, Anime, Illegal and VideoGame keep a card whose scope
//    is exactly that value; Prerelease, Speed, Rush, Legend and Custom keep a
//    card whose scope has that bit;
//  - with a whitelist, a card not on it is dropped unless the filter is All,
//    whatever else the filter is (:1320-1321);
//  - None and All filter nothing else.
//
// With `lflist` nullptr, the list is treated as an empty, non-whitelist one,
// which is what every count query against it would give (each card counts
// 3). The deck builder never offers the banlist counts in that case
// (limitation_filter_choices), so this only defines the function everywhere.
bool deck_search_admits(const data::CardRecord& record, const LfList* lflist, bool show_non_official,
						LimitationFilter filter);

} // namespace edopro_next::policy

#endif // EDOPRO_NEXT_POLICY_DECK_SEARCH_FILTER_H

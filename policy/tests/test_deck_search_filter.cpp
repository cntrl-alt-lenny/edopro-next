// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins deck_search_admits() and limitation_filter_choices()
// (deck_search_filter.h) against an independent transcription of upstream:
// the hidden-card gate and the `filter_lm` switch in
// DeckBuilder::CheckCardProperties (gframe/deck_con.cpp:1192-1193,
// :1254-1322), LFList::GetLimitationIterator (gframe/deck_manager.h:23-30)
// and Game::ReloadCBLimit (gframe/game.cpp:3412-3442). Every combination of
// the scope bits upstream defines, token or not, every count a list can give
// a card (including through an alias), a blacklist, a whitelist and no list,
// both states of the non-official switch, and every filter.
//
// Synthetic records and lists only.
#include "edopro_next/policy/deck_search_filter.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::CardRecord;
using edopro_next::policy::deck_search_admits;
using edopro_next::policy::LfList;
using edopro_next::policy::limitation_filter_choices;
using edopro_next::policy::LimitationFilter;
using edopro_next::policy::non_official_switch_available;

namespace {

// ---- The transcription. gframe/data_manager.h:21-34 for SCOPE_*,
// ocgcore/ocgapi_constants.h:46 for TYPE_TOKEN, gframe/deck_con.h:20-38 for
// the filter values. ----
constexpr std::uint32_t SCOPE_OCG = 0x1;
constexpr std::uint32_t SCOPE_TCG = 0x2;
constexpr std::uint32_t SCOPE_ANIME = 0x4;
constexpr std::uint32_t SCOPE_ILLEGAL = 0x8;
constexpr std::uint32_t SCOPE_VIDEO_GAME = 0x10;
constexpr std::uint32_t SCOPE_CUSTOM = 0x20;
constexpr std::uint32_t SCOPE_SPEED = 0x40;
constexpr std::uint32_t SCOPE_PRERELEASE = 0x100;
constexpr std::uint32_t SCOPE_RUSH = 0x200;
constexpr std::uint32_t SCOPE_LEGEND = 0x400;
constexpr std::uint32_t SCOPE_HIDDEN = 0x1000;
constexpr std::uint32_t SCOPE_OCG_TCG = SCOPE_OCG | SCOPE_TCG;
constexpr std::uint32_t SCOPE_OFFICIAL = SCOPE_OCG | SCOPE_TCG | SCOPE_PRERELEASE;
constexpr std::uint32_t TYPE_TOKEN = 0x4000;

enum limitation_search_filters {
	LIMITATION_FILTER_NONE,
	LIMITATION_FILTER_BANNED,
	LIMITATION_FILTER_LIMITED,
	LIMITATION_FILTER_SEMI_LIMITED,
	LIMITATION_FILTER_UNLIMITED,
	LIMITATION_FILTER_OCG,
	LIMITATION_FILTER_TCG,
	LIMITATION_FILTER_TCG_OCG,
	LIMITATION_FILTER_PRERELEASE,
	LIMITATION_FILTER_SPEED,
	LIMITATION_FILTER_RUSH,
	LIMITATION_FILTER_LEGEND,
	LIMITATION_FILTER_ANIME,
	LIMITATION_FILTER_ILLEGAL,
	LIMITATION_FILTER_VIDEOGAME,
	LIMITATION_FILTER_CUSTOM,
	LIMITATION_FILTER_ALL
};

struct UpCard {
	std::uint32_t code;
	std::uint32_t alias;
	std::uint32_t type;
	std::uint32_t ot;
	bool IsInArtworkOffsetRange() const {
		if(alias == 0)
			return false;
		return (alias - code < 10 || code - alias < 10);
	}
};

struct UpList {
	std::unordered_map<std::uint32_t, int> content;
	bool whitelist;
	// gframe/deck_manager.h:23-30.
	auto GetLimitationIterator(const UpCard* pcard) const {
		auto flit = content.find(pcard->code);
		if(flit == content.end() && pcard->alias) {
			if(!whitelist || pcard->IsInArtworkOffsetRange())
				flit = content.find(pcard->alias);
		}
		return flit;
	}
};

// gframe/deck_con.cpp:1192-1193 and :1254-1322; `chkAnime` is `anime`.
bool upstream_admits(const UpCard& data, const UpList* filterList, bool anime, int filter_lm) {
	if(data.type & TYPE_TOKEN || data.ot & SCOPE_HIDDEN ||
	   ((data.ot & SCOPE_OFFICIAL) != data.ot && (!anime && !filterList->whitelist)))
		return false;
	if((filter_lm != LIMITATION_FILTER_NONE || filterList->whitelist) && filter_lm != LIMITATION_FILTER_ALL) {
		auto flit = filterList->GetLimitationIterator(&data);
		int count = 3;
		if(flit == filterList->content.end()) {
			if(filterList->whitelist)
				count = -1;
		} else
			count = flit->second;
		switch(filter_lm) {
		case LIMITATION_FILTER_BANNED:
		case LIMITATION_FILTER_LIMITED:
		case LIMITATION_FILTER_SEMI_LIMITED:
			if(count != filter_lm - 1)
				return false;
			break;
		case LIMITATION_FILTER_UNLIMITED:
			if(count < 3)
				return false;
			break;
		case LIMITATION_FILTER_OCG:
			if(data.ot != SCOPE_OCG)
				return false;
			break;
		case LIMITATION_FILTER_TCG:
			if(data.ot != SCOPE_TCG)
				return false;
			break;
		case LIMITATION_FILTER_TCG_OCG:
			if(data.ot != SCOPE_OCG_TCG)
				return false;
			break;
		case LIMITATION_FILTER_PRERELEASE:
			if(!(data.ot & SCOPE_PRERELEASE))
				return false;
			break;
		case LIMITATION_FILTER_SPEED:
			if(!(data.ot & SCOPE_SPEED))
				return false;
			break;
		case LIMITATION_FILTER_RUSH:
			if(!(data.ot & SCOPE_RUSH))
				return false;
			break;
		case LIMITATION_FILTER_LEGEND:
			if(!(data.ot & SCOPE_LEGEND))
				return false;
			break;
		case LIMITATION_FILTER_ANIME:
			if(data.ot != SCOPE_ANIME)
				return false;
			break;
		case LIMITATION_FILTER_ILLEGAL:
			if(data.ot != SCOPE_ILLEGAL)
				return false;
			break;
		case LIMITATION_FILTER_VIDEOGAME:
			if(data.ot != SCOPE_VIDEO_GAME)
				return false;
			break;
		case LIMITATION_FILTER_CUSTOM:
			if(!(data.ot & SCOPE_CUSTOM))
				return false;
			break;
		default:
			break;
		}
		if(filterList->whitelist && count < 0)
			return false;
	}
	return true;
}

// gframe/game.cpp:3412-3442, returning the item data in order.
std::vector<int> upstream_limit_choices(const UpList* filterList, bool anime) {
	bool white = filterList && filterList->whitelist;
	std::vector<int> items{LIMITATION_FILTER_NONE, LIMITATION_FILTER_BANNED, LIMITATION_FILTER_LIMITED,
						   LIMITATION_FILTER_SEMI_LIMITED, LIMITATION_FILTER_UNLIMITED};
	if(!white) {
		for(int f : {LIMITATION_FILTER_OCG, LIMITATION_FILTER_TCG, LIMITATION_FILTER_TCG_OCG,
					 LIMITATION_FILTER_PRERELEASE, LIMITATION_FILTER_SPEED, LIMITATION_FILTER_RUSH,
					 LIMITATION_FILTER_LEGEND})
			items.push_back(f);
		if(anime)
			for(int f : {LIMITATION_FILTER_ANIME, LIMITATION_FILTER_ILLEGAL, LIMITATION_FILTER_VIDEOGAME,
						 LIMITATION_FILTER_CUSTOM})
				items.push_back(f);
	} else {
		for(int f : {LIMITATION_FILTER_LEGEND, LIMITATION_FILTER_ILLEGAL, LIMITATION_FILTER_PRERELEASE,
					 LIMITATION_FILTER_ALL})
			items.push_back(f);
	}
	return items;
}

// ---- Fixtures ----

// Cards chosen to give every count a list can produce: an entry of 0, 1, 2,
// 3; no entry; an entry reached only through the alias, within and outside
// the artwork-offset window (only the latter differs in a whitelist).
struct Holder {
	std::uint32_t code;
	std::uint32_t alias;
};
constexpr Holder kHolders[] = {{100, 0}, {101, 0}, {102, 0}, {103, 0}, {104, 0}, {105, 101}, {200, 101}};

const std::vector<std::pair<std::uint32_t, int>> kEntries = {{100, 0}, {101, 1}, {102, 2}, {103, 3}};

UpList up_list(bool whitelist) {
	UpList list;
	list.whitelist = whitelist;
	for(auto [code, count] : kEntries)
		list.content.emplace(code, count);
	return list;
}

LfList our_list(bool whitelist) {
	LfList list;
	list.name = whitelist ? "white" : "black";
	list.whitelist = whitelist;
	for(auto [code, count] : kEntries)
		list.content.emplace(CardCode{code}, count);
	return list;
}

std::vector<std::uint32_t> all_scopes() {
	const std::uint32_t bits[] = {SCOPE_OCG,   SCOPE_TCG,        SCOPE_ANIME, SCOPE_ILLEGAL,
								  SCOPE_VIDEO_GAME, SCOPE_CUSTOM, SCOPE_SPEED, SCOPE_PRERELEASE,
								  SCOPE_RUSH,  SCOPE_LEGEND,     SCOPE_HIDDEN};
	std::vector<std::uint32_t> out;
	for(std::uint32_t mask = 0; mask < (1u << 11); ++mask) {
		std::uint32_t ot = 0;
		for(std::uint32_t i = 0; i < 11; ++i)
			if(mask & (1u << i))
				ot |= bits[i];
		out.push_back(ot);
	}
	return out;
}

} // namespace

EDOPRO_POLICY_TEST(deckSearchAdmitsExactlyTheCardsUpstreamsSearchLists) {
	const UpList up_black = up_list(false);
	const UpList up_white = up_list(true);
	// Upstream always has a list; this project's "No banlist" is compared
	// with an empty, non-whitelist list (deck_search_filter.h).
	const UpList up_empty{{}, false};
	const LfList black = our_list(false);
	const LfList white = our_list(true);

	struct ListCase {
		const UpList* upstream;
		const LfList* ours;
	};
	const ListCase lists[] = {{&up_black, &black}, {&up_white, &white}, {&up_empty, nullptr}};

	long compared = 0;
	long admitted = 0;
	for(const auto& list : lists) {
		for(std::uint32_t ot : all_scopes()) {
			for(std::uint32_t type : {0x1u, 0x1u | TYPE_TOKEN}) {
				for(const auto& holder : kHolders) {
					const UpCard up{holder.code, holder.alias, type, ot};
					CardRecord record;
					record.code = CardCode{holder.code};
					record.alias = CardCode{holder.alias};
					record.type = type;
					record.scope = ot;
					for(bool anime : {false, true}) {
						for(int f = LIMITATION_FILTER_NONE; f <= LIMITATION_FILTER_ALL; ++f) {
							const bool expected = upstream_admits(up, list.upstream, anime, f);
							const bool actual = deck_search_admits(record, list.ours, anime,
																	static_cast<LimitationFilter>(f));
							if(expected != actual) {
								edopro_next::policy::testing::report_failure(
									__FILE__, __LINE__,
									"code " + std::to_string(holder.code) + " ot 0x" + std::to_string(ot) +
										" type " + std::to_string(type) + " filter " + std::to_string(f) +
										" anime " + std::to_string(anime) + " list " +
										(list.ours ? list.ours->name : std::string("none")) +
										": upstream " + std::to_string(expected) + ", ours " +
										std::to_string(actual));
							}
							++compared;
							admitted += expected ? 1 : 0;
						}
					}
				}
			}
		}
	}
	// 3 lists x 2048 scopes x 2 types x 7 cards x 2 switch states x 17 filters.
	EDOPRO_POLICY_CHECK_EQ(compared, 3L * 2048 * 2 * 7 * 2 * 17);
	// Neither everything nor nothing.
	EDOPRO_POLICY_CHECK(admitted > 0 && admitted < compared);
}

EDOPRO_POLICY_TEST(limitChoicesMatchUpstreamsListForEveryListKind) {
	const UpList up_black = up_list(false);
	const UpList up_white = up_list(true);
	const LfList black = our_list(false);
	const LfList white = our_list(true);
	for(bool anime : {false, true}) {
		for(auto [up, ours] : {std::pair<const UpList*, const LfList*>{&up_black, &black}, {&up_white, &white}}) {
			std::vector<int> actual;
			for(auto f : limitation_filter_choices(ours, anime))
				actual.push_back(static_cast<int>(f));
			EDOPRO_POLICY_CHECK(actual == upstream_limit_choices(up, anime));
		}
		// No list: upstream's non-whitelist choices without the four counts.
		std::vector<int> expected = upstream_limit_choices(&up_black, anime);
		expected.erase(expected.begin() + 1, expected.begin() + 5);
		std::vector<int> actual;
		for(auto f : limitation_filter_choices(nullptr, anime))
			actual.push_back(static_cast<int>(f));
		EDOPRO_POLICY_CHECK(actual == expected);
	}
	EDOPRO_POLICY_CHECK(non_official_switch_available(nullptr));
	EDOPRO_POLICY_CHECK(non_official_switch_available(&black));
	EDOPRO_POLICY_CHECK(!non_official_switch_available(&white));
}

// SPDX-License-Identifier: AGPL-3.0-or-later

#include "edopro_next/policy/deck_search_filter.h"

#include <cstdint>

namespace edopro_next::policy {

namespace {

// gframe/data_manager.h:21-34 (SCOPE_*), and TYPE_TOKEN from
// ocgcore/ocgapi_constants.h:46.
constexpr std::uint32_t kScopeOcg = 0x1;
constexpr std::uint32_t kScopeTcg = 0x2;
constexpr std::uint32_t kScopeAnime = 0x4;
constexpr std::uint32_t kScopeIllegal = 0x8;
constexpr std::uint32_t kScopeVideoGame = 0x10;
constexpr std::uint32_t kScopeCustom = 0x20;
constexpr std::uint32_t kScopeSpeed = 0x40;
constexpr std::uint32_t kScopePrerelease = 0x100;
constexpr std::uint32_t kScopeRush = 0x200;
constexpr std::uint32_t kScopeLegend = 0x400;
constexpr std::uint32_t kScopeHidden = 0x1000;
constexpr std::uint32_t kScopeOcgTcg = kScopeOcg | kScopeTcg;
constexpr std::uint32_t kScopeOfficial = kScopeOcg | kScopeTcg | kScopePrerelease;
constexpr std::uint32_t kTypeToken = 0x4000;

} // namespace

std::vector<LimitationFilter> limitation_filter_choices(const LfList* lflist, bool show_non_official) {
	using L = LimitationFilter;
	// game.cpp:3413: `bool white = deckBuilder.filterList && deckBuilder.filterList->whitelist;`
	const bool white = lflist && lflist->whitelist;
	std::vector<L> out{L::None};
	// game.cpp:3416-3419. Not offered without a list (see the header).
	if(lflist)
		out.insert(out.end(), {L::Banned, L::Limited, L::SemiLimited, L::Unlimited});
	if(!white) {
		// game.cpp:3422-3428.
		out.insert(out.end(), {L::Ocg, L::Tcg, L::TcgOcg, L::Prerelease, L::Speed, L::Rush, L::Legend});
		// game.cpp:3429-3434.
		if(show_non_official)
			out.insert(out.end(), {L::Anime, L::Illegal, L::VideoGame, L::Custom});
	} else {
		// game.cpp:3437-3440.
		out.insert(out.end(), {L::Legend, L::Illegal, L::Prerelease, L::All});
	}
	return out;
}

bool non_official_switch_available(const LfList* lflist) {
	return !(lflist && lflist->whitelist);
}

bool deck_search_admits(const data::CardRecord& record, const LfList* lflist, bool show_non_official,
						LimitationFilter filter) {
	using L = LimitationFilter;
	const bool whitelist = lflist && lflist->whitelist;
	const std::uint32_t ot = record.scope;

	// deck_con.cpp:1192-1193.
	if((record.type & kTypeToken) || (ot & kScopeHidden) ||
	   ((ot & kScopeOfficial) != ot && (!show_non_official && !whitelist)))
		return false;

	// deck_con.cpp:1254.
	if(!((filter != L::None || whitelist) && filter != L::All))
		return true;

	// deck_con.cpp:1255-1261.
	int count = 3;
	const auto limit = lflist ? limitation_for(*lflist, record.code, record.alias) : std::nullopt;
	if(!limit) {
		if(whitelist)
			count = -1;
	} else {
		count = *limit;
	}

	// deck_con.cpp:1262-1319.
	switch(filter) {
	case L::Banned:
		if(count != 0)
			return false;
		break;
	case L::Limited:
		if(count != 1)
			return false;
		break;
	case L::SemiLimited:
		if(count != 2)
			return false;
		break;
	case L::Unlimited:
		if(count < 3)
			return false;
		break;
	case L::Ocg:
		if(ot != kScopeOcg)
			return false;
		break;
	case L::Tcg:
		if(ot != kScopeTcg)
			return false;
		break;
	case L::TcgOcg:
		if(ot != kScopeOcgTcg)
			return false;
		break;
	case L::Prerelease:
		if(!(ot & kScopePrerelease))
			return false;
		break;
	case L::Speed:
		if(!(ot & kScopeSpeed))
			return false;
		break;
	case L::Rush:
		if(!(ot & kScopeRush))
			return false;
		break;
	case L::Legend:
		if(!(ot & kScopeLegend))
			return false;
		break;
	case L::Anime:
		if(ot != kScopeAnime)
			return false;
		break;
	case L::Illegal:
		if(ot != kScopeIllegal)
			return false;
		break;
	case L::VideoGame:
		if(ot != kScopeVideoGame)
			return false;
		break;
	case L::Custom:
		if(!(ot & kScopeCustom))
			return false;
		break;
	case L::None:
	case L::All:
		break;
	}
	// deck_con.cpp:1320-1321.
	if(whitelist && count < 0)
		return false;
	return true;
}

} // namespace edopro_next::policy

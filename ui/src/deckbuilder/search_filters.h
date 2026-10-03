// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The deck builder's search filters: the choices each control offers, in
// upstream's order, and how the user's choices become a data::SearchQuery.
// Mirrors upstream's filter window (gframe/game.cpp:661-743, the lists at
// :3350-3462) and DeckBuilder::StartFilter (gframe/deck_con.cpp:1040-1058).
// See docs/architecture/card-search.md §2.2 and
// docs/adr/0012-deck-builder-search-filters.md.
//
// THE UI MUST NOT IMPLEMENT GAME RULES (AGENTS.md). Nothing here matches a
// card: parse_numeric_filter() (data/) turns box text into a comparison,
// CardSearchIndex (data/) matches, and policy::deck_search_admits()
// (policy/) decides anything that depends on the banlist. This file holds
// the option tables and copies choices into a query. QML only renders the
// labels and reports which index the user picked.

#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <cstdint>
#include <vector>

#include "edopro_next/data/search_query.h"
#include "edopro_next/policy/deck_search_filter.h"

namespace edopro_next::ui {

// Upstream's card-type list, game.cpp:3350-3362, in its order.
enum class CardTypeChoice {
    All,
    Monster,
    Spell,
    Trap,
    Skill,
};

struct LabelledValue {
    QString label;
    std::uint64_t value;
};

// Every choice the user has made. Indexes refer to the tables below.
struct SearchFilterState {
    CardTypeChoice cardType = CardTypeChoice::All;
    int subType = 0;
    int attribute = 0;
    int race = 0;
    QString attackText;
    QString defenseText;
    QString levelText;
    QString scaleText;
    std::uint32_t categoryMask = 0;
    std::uint32_t linkMarkerMask = 0;
    policy::LimitationFilter limitation = policy::LimitationFilter::None;
    bool showNonOfficial = false;

    friend bool operator==(const SearchFilterState&, const SearchFilterState&) = default;
};

QStringList cardTypeLabels();
// game.cpp:3363-3411: the sub-type list for a card type. Index 0 is "any".
// Values are upstream's item data: a set of type bits.
const std::vector<LabelledValue>& subTypeChoices(CardTypeChoice cardType);
// game.cpp:3443-3448: index 0 is "any", then ATTRIBUTE_EARTH .. DIVINE.
const std::vector<LabelledValue>& attributeChoices();
// game.cpp:3449-3462: index 0 is "any", then one race bit each.
const std::vector<LabelledValue>& raceChoices();
// game.cpp:721-724: 32 effect-category bits, 0x1 upward.
QStringList categoryLabels();
// game.cpp:734-741 and deck_con.cpp:447-463: the eight marker buttons in
// upstream's order, with the bit each sets.
struct LinkMarkerChoice {
    QString glyph;
    std::uint32_t bit;
};
const std::array<LinkMarkerChoice, 8>& linkMarkerChoices();
// The label for one limit choice. `whitelist` changes how None reads, since
// with a whitelist it shows only the cards on the list (deck_con.cpp:1320).
QString limitationLabel(policy::LimitationFilter filter, bool whitelist);

// Whether the Monster-only controls apply (upstream enables them only for
// Monster, deck_con.cpp:533-571, and reads them only then, :1044-1055).
bool monsterFiltersApply(const SearchFilterState& state);
// Whether the DEF box applies: upstream disables and clears it when the
// Monster sub-type is Link (deck_con.cpp:577-583).
bool defenseFilterApplies(const SearchFilterState& state);

// The data::SearchQuery for `state` and the search box `text`, with no
// result limit. StartFilter's reading of the controls
// (deck_con.cpp:1040-1055) and the card-type cases of CheckCardProperties
// (:1194-1249), turned into query fields. The banlist-dependent part is
// not in the query: apply policy::deck_search_admits() to its results.
data::SearchQuery buildSearchQuery(const SearchFilterState& state, const QString& text);

} // namespace edopro_next::ui

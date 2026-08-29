// SPDX-License-Identifier: AGPL-3.0-or-later

#include "card_entry.h"

namespace {

// Verified against ocgcore's own public header
// (ocgcore/ocgapi_constants.h:33,56,58), matching the citation precedent
// already established in data/src/card_search_index.cpp and
// data/src/card_database.cpp. Used *only* to decide which numeric fields
// this preview renders (a presentation decision - is an ATK/DEF/Level row
// meaningful for this printed card, is a Pendulum scale row worth showing)
// - never to decide deck-section membership, which stays entirely the
// user's own explicit choice via DeckController::addCard. This mirrors
// exactly how CardSearchIndex already uses the same two bits (Link,
// Pendulum) to decide which numeric filters can match a card, not to
// classify it into a deck section.
constexpr quint32 kTypeMonsterBit = 0x1;
constexpr quint32 kTypePendulumBit = 0x1000000;
constexpr quint32 kTypeLinkBit = 0x4000000;

} // namespace

CardEntry make_card_entry(edopro_next::data::CardCode code,
                           const edopro_next::data::CardDatabase& database) {
    CardEntry entry;
    entry.code = edopro_next::data::to_number(code);

    const auto* record = database.find(code);
    if (!record) {
        entry.known = false;
        return entry;
    }

    entry.known = true;
    entry.name = QString::fromStdString(record->name);
    entry.text = QString::fromStdString(record->text);
    entry.attack = record->attack;
    entry.defense = record->defense;
    entry.level = record->level;
    entry.leftScale = record->left_scale;
    entry.rightScale = record->right_scale;
    entry.linkMarker = record->link_marker;
    entry.attribute = record->attribute;
    entry.race = record->race;
    entry.type = record->type;

    entry.isMonster = (record->type & kTypeMonsterBit) != 0;
    entry.isPendulum = (record->type & kTypePendulumBit) != 0;
    entry.isLink = (record->type & kTypeLinkBit) != 0;

    return entry;
}

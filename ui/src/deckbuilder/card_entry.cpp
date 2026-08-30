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
// Verified against ocgcore/ocgapi_constants.h:55, the same authoritative
// header the three bits above already cite. Used only to pick the "Rank"
// label over "Level" for the one shared level/rank/link-rating magnitude
// (see card_entry.h's isXyz doc comment) - never for deck-section routing,
// exactly like the other type-bit checks in this file.
constexpr quint32 kTypeXyzBit = 0x800000;

// The eight LINK_MARKER_* bit values from ocgcore's own public header
// (ocgcore/ocgapi_constants.h:197-204 - octal literals there; decimal here,
// converted once and cited so a reader does not have to do octal-to-hex
// arithmetic to verify this table), in the exact order and with the exact
// direction-arrow glyphs gframe/data_manager.cpp's own
// DataManager::FormatLinkMarker() uses (gframe/data_manager.cpp:545-555):
// top-left, top, top-right, left, right, bottom-left, bottom, bottom-right.
// A tiny cited formatter here is preferred over either calling into
// DataManager from this new UI layer (a legacy-client dependency this
// module has no reason to take) or inventing a different notation upstream
// does not use.
struct LinkMarkerGlyph {
    quint32 bit;
    char16_t glyph;
};
constexpr LinkMarkerGlyph kLinkMarkerGlyphs[] = {
    {0x40, u'↖'},  // LINK_MARKER_TOP_LEFT     (octal 0100) -> "↖"
    {0x80, u'↑'},  // LINK_MARKER_TOP          (octal 0200) -> "↑"
    {0x100, u'↗'}, // LINK_MARKER_TOP_RIGHT    (octal 0400) -> "↗"
    {0x8, u'←'},   // LINK_MARKER_LEFT         (octal 0010) -> "←"
    {0x20, u'→'},  // LINK_MARKER_RIGHT        (octal 0040) -> "→"
    {0x1, u'↙'},   // LINK_MARKER_BOTTOM_LEFT  (octal 0001) -> "↙"
    {0x2, u'↓'},   // LINK_MARKER_BOTTOM       (octal 0002) -> "↓"
    {0x4, u'↘'},   // LINK_MARKER_BOTTOM_RIGHT (octal 0004) -> "↘"
};

// Mirrors DataManager::FormatLinkMarker() exactly: each set bit renders as
// "[<arrow>]" in the fixed order above, and an unset bit contributes
// nothing - not a placeholder dash, an omission - so a 0 mask (no markers
// at all) renders as an empty string, matching upstream's own
// FormatLinkMarker(0) result. `link_marker` is already 0 for every
// non-Link card (CardRecord's own documented invariant), so calling this
// unconditionally for any CardRecord is harmless; CardPreview additionally
// gates the whole Link Markers row on `isLink` so this string is never
// shown for a non-Link card regardless.
QString format_link_marker(quint32 link_marker) {
    QString result;
    for (const auto& marker : kLinkMarkerGlyphs) {
        if (link_marker & marker.bit)
            result += QStringLiteral("[%1]").arg(QChar(marker.glyph));
    }
    return result;
}

// Mirrors Game::ShowCardInfo's own combat-stat formatting (gframe/game.cpp:
// the `cd->attack < 0`/`cd->defense < 0` checks in both its Link and
// non-Link branches) exactly: any negative value - not just the -1/-2
// CardRecord's own doc comment names as the values currently in use -
// renders as a literal "?", never the negative number itself.
// CardRecord::attack/defense already preserve these values faithfully
// (data/'s own documented "not sentinels this module strips" contract);
// this is the one place that turns them into the honest display string.
// Both CardPreview.qml and SearchResultsModel::build_summary() read the
// resulting attackDisplay/defenseDisplay off the same CardEntry rather than
// each recomputing this rule themselves, so the two presentations can never
// disagree with each other.
QString format_combat_stat(qint32 value) {
    return value < 0 ? QStringLiteral("?") : QString::number(value);
}

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
    entry.attackDisplay = format_combat_stat(record->attack);
    entry.defenseDisplay = format_combat_stat(record->defense);
    entry.level = record->level;
    entry.leftScale = record->left_scale;
    entry.rightScale = record->right_scale;
    entry.linkMarker = record->link_marker;
    entry.linkMarkerDisplay = format_link_marker(record->link_marker);
    entry.attribute = record->attribute;
    entry.race = record->race;
    // QString::number() operates on the qulonglong directly - no double
    // round-trip, so this is exact for every representable race bitmask,
    // unlike concatenating `race` itself into a QML string expression
    // (see card_entry.h). Hex, not decimal: race is a bitmask of RACE_*
    // constants, and no human-readable race-name table exists yet.
    entry.raceDisplay = QStringLiteral("0x%1").arg(record->race, 0, 16);
    entry.type = record->type;

    entry.isMonster = (record->type & kTypeMonsterBit) != 0;
    entry.isPendulum = (record->type & kTypePendulumBit) != 0;
    entry.isLink = (record->type & kTypeLinkBit) != 0;
    entry.isXyz = (record->type & kTypeXyzBit) != 0;

    return entry;
}

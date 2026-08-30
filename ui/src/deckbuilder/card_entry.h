// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A QML-facing snapshot of one card's data, built once per lookup from the
// Qt-free edopro_next::data::CardRecord this project's data/ module already
// owns. This is the only path a card's data takes into QML: nothing in this
// UI layer holds a CardRecord*, and QML never touches data/ types directly.

#pragma once

#include <QMetaType>
#include <QString>

#include "edopro_next/data/card_code.h"
#include "edopro_next/data/card_database.h"

// A plain value type (Q_GADGET, not QObject): copied by value into and out
// of QML, exactly like the CardRecord it is built from - no ownership, no
// lifetime coupling to the CardDatabase that produced it. Returned from
// Q_INVOKABLE methods/Q_PROPERTYs; QML reads its Q_PROPERTYs by dotted
// access with no separate registration needed for a plain gadget used this
// way.
class CardEntry {
    Q_GADGET

    Q_PROPERTY(quint32 code MEMBER code)
    Q_PROPERTY(bool known MEMBER known)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString text MEMBER text)

    // Only meaningful when `isMonster` is true - see card_entry.cpp for why
    // these classifications (and no others) are made here rather than left
    // as raw, uninterpreted data, and why none of them ever influences which
    // deck section a card can be added to.
    //
    // `level` is the single stored magnitude upstream's own `cd->level`
    // is (gframe/game.cpp's card-info formatting - see card_entry.cpp): a
    // Level monster's Level, an Xyz's Rank, and a Link's Link Rating all
    // live in this one field, exactly as `CardRecord::level` documents.
    // `isXyz`/`isLink` exist so the presentation layer (CardPreview.qml)
    // can label that one magnitude correctly instead of calling it "Level"
    // unconditionally - which was a real bug (see docs/architecture/
    // deck-builder-ui.md#10.7) and is not repeated here, deliberately: no
    // separate `levelLabel`/`rank`/`linkRating` fields are added, so there
    // is exactly one place to keep in sync with `level`'s real meaning.
    Q_PROPERTY(bool isMonster MEMBER isMonster)
    Q_PROPERTY(bool isXyz MEMBER isXyz)
    Q_PROPERTY(qint32 attack MEMBER attack)
    // Only meaningful when `isMonster && !isLink` - a Link monster's
    // `defense` is always 0 (CardRecord::defense's own documented Link
    // exception; the real value lives in `linkMarker` instead). CardPreview
    // must not render this as an ordinary DEF stat for a Link monster.
    Q_PROPERTY(qint32 defense MEMBER defense)
    Q_PROPERTY(quint32 level MEMBER level)
    Q_PROPERTY(bool isPendulum MEMBER isPendulum)
    Q_PROPERTY(quint32 leftScale MEMBER leftScale)
    Q_PROPERTY(quint32 rightScale MEMBER rightScale)
    Q_PROPERTY(bool isLink MEMBER isLink)
    Q_PROPERTY(quint32 linkMarker MEMBER linkMarker)
    // A ready-to-display rendering of `linkMarker`, computed once in C++ -
    // see card_entry.cpp's format_link_marker(), cited directly against
    // gframe/data_manager.cpp's DataManager::FormatLinkMarker(). Keeps the
    // eight LINK_MARKER_* bit constants and their arrow glyphs out of QML,
    // matching the existing raceDisplay precedent below. Empty for any
    // non-Link card, and for a Link card with no marker bits set at all -
    // the same "absent means omitted, not a placeholder" behaviour upstream
    // itself has for a zero mask.
    Q_PROPERTY(QString linkMarkerDisplay MEMBER linkMarkerDisplay)

    Q_PROPERTY(quint32 attribute MEMBER attribute)
    Q_PROPERTY(qulonglong race MEMBER race)
    // A ready-to-display exact rendering of `race`, computed in C++ - see
    // card_entry.cpp. `race` is a 64-bit bitmask (Project Ignis's RACE_*
    // constants run up to bit 62, e.g. RACE_YOKAI = 0x4000000000000000),
    // and QML/JavaScript numbers are IEEE-754 doubles: concatenating a
    // qulonglong straight into a QML string expression (`"" + entry.race`)
    // forces a double round-trip whose *default* decimal string form does
    // not reliably reproduce the original digits for values in this range,
    // even when the double itself is bit-exact - confirmed empirically
    // against this project's real Qt 6.8.3 build (see
    // docs/architecture/deck-builder-ui.md#race-value-precision). QML must
    // read raceDisplay for anything shown to a user, never `race` itself.
    Q_PROPERTY(QString raceDisplay MEMBER raceDisplay)
    Q_PROPERTY(quint32 type MEMBER type)

public:
    quint32 code = 0;
    bool known = false;
    QString name;
    QString text;

    bool isMonster = false;
    bool isXyz = false;
    qint32 attack = 0;
    qint32 defense = 0;
    quint32 level = 0;
    bool isPendulum = false;
    quint32 leftScale = 0;
    quint32 rightScale = 0;
    bool isLink = false;
    quint32 linkMarker = 0;
    QString linkMarkerDisplay;

    quint32 attribute = 0;
    qulonglong race = 0;
    QString raceDisplay = QStringLiteral("0x0");
    quint32 type = 0;
};
Q_DECLARE_METATYPE(CardEntry)

// Builds the QML-facing snapshot for `code` from `database`'s *current*
// state. `known` is false, and every other field left at its default, when
// `database` does not currently recognize `code` - the caller (a search
// result, or a deck entry) is responsible for still representing the code
// itself; this function never invents data for an unknown card.
CardEntry make_card_entry(edopro_next::data::CardCode code,
                           const edopro_next::data::CardDatabase& database);

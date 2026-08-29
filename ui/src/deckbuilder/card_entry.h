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
    // that one classification (and no other) is made here rather than left
    // as raw, uninterpreted data, and why it never influences which deck
    // section a card can be added to.
    Q_PROPERTY(bool isMonster MEMBER isMonster)
    Q_PROPERTY(qint32 attack MEMBER attack)
    Q_PROPERTY(qint32 defense MEMBER defense)
    Q_PROPERTY(quint32 level MEMBER level)
    Q_PROPERTY(bool isPendulum MEMBER isPendulum)
    Q_PROPERTY(quint32 leftScale MEMBER leftScale)
    Q_PROPERTY(quint32 rightScale MEMBER rightScale)
    Q_PROPERTY(bool isLink MEMBER isLink)
    Q_PROPERTY(quint32 linkMarker MEMBER linkMarker)

    Q_PROPERTY(quint32 attribute MEMBER attribute)
    Q_PROPERTY(qulonglong race MEMBER race)
    Q_PROPERTY(quint32 type MEMBER type)

public:
    quint32 code = 0;
    bool known = false;
    QString name;
    QString text;

    bool isMonster = false;
    qint32 attack = 0;
    qint32 defense = 0;
    quint32 level = 0;
    bool isPendulum = false;
    quint32 leftScale = 0;
    quint32 rightScale = 0;
    bool isLink = false;
    quint32 linkMarker = 0;

    quint32 attribute = 0;
    qulonglong race = 0;
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

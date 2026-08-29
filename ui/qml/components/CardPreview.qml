// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A textual/metadata-driven card preview. No artwork - this project fetches
// card images at runtime and does not bundle or cache them (CLAUDE.md), and
// building that pipeline is explicitly out of scope for this slice (see
// docs/architecture/deck-builder-ui.md#artwork). Shows raw stored fields;
// see card_entry.cpp for the one deliberate presentation classification
// (isMonster) this preview relies on, and why it never affects deck
// editing.

import QtQuick
import QtQuick.Layouts
import EdoproNext

Flickable {
    id: root
    property var entry: ({})

    contentWidth: width
    contentHeight: column.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    ColumnLayout {
        id: column
        width: root.width
        spacing: Theme.space3

        Text {
            Layout.fillWidth: true
            text: root.entry.known ? root.entry.name : "Unknown card"
            font.family: Theme.fontFamily
            font.pointSize: Theme.textTitle
            font.weight: Theme.weightBold
            color: root.entry.known ? Theme.textPrimary : Theme.textTertiary
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: "Card code " + root.entry.code
            elide: Text.ElideRight
            font.family: Theme.fontFamilyMono
            font.pointSize: Theme.textCaption
            color: Theme.textTertiary
        }

        Text {
            Layout.fillWidth: true
            visible: !root.entry.known
            text: "This code is not present in the currently loaded card database. It is "
                + "kept exactly as loaded and will be preserved if this deck is saved again."
            font.family: Theme.fontFamily
            font.pointSize: Theme.textCaption
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.entry.known
            height: 1
            color: Theme.border
        }

        GridLayout {
            Layout.fillWidth: true
            visible: root.entry.known && root.entry.isMonster
            columns: 2
            rowSpacing: Theme.space1
            columnSpacing: Theme.space4

            Text {
                text: "ATK / DEF"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                text: root.entry.isLink
                    ? (root.entry.attack + " / LINK")
                    : (root.entry.attack + " / " + root.entry.defense)
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                text: "Level"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                text: String(root.entry.level)
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                visible: root.entry.isPendulum
                text: "Pendulum scale"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                visible: root.entry.isPendulum
                text: root.entry.leftScale + " / " + root.entry.rightScale
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                text: "Attribute / Race"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                // raceDisplay, never the raw `race` value - `race` is a
                // 64-bit bitmask and QML numbers are doubles; concatenating
                // it directly does not reliably reproduce its exact digits
                // for high-bit values (confirmed empirically - see
                // card_entry.h). raceDisplay is formatted once in C++ from
                // the real uint64_t, with no double round-trip.
                text: root.entry.attribute + " / " + root.entry.raceDisplay
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.entry.known
            text: root.entry.text
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.45
        }
    }
}

// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A textual/metadata-driven card preview. No artwork - this project fetches
// card images at runtime and does not bundle or cache them (CLAUDE.md), and
// building that pipeline is explicitly out of scope for this slice (see
// docs/architecture/deck-builder-ui.md#artwork). Shows raw stored fields;
// see card_entry.cpp for the presentation classifications this preview
// relies on (isMonster/isXyz/isLink/isPendulum) and why none of them ever
// affects deck editing.
//
// Level/Rank/Link Rating are one shared stored magnitude
// (`CardEntry::level`, sourced from `CardRecord::level` - see
// card_entry.h), labelled here according to the same distinction upstream's
// own card-info panel makes (gframe/game.cpp - see docs/architecture/
// deck-builder-ui.md#10.7): "Level" for an ordinary Level monster, "Rank"
// for an Xyz, "Link Rating" for a Link. A Link monster additionally has no
// real DEF (`CardRecord::defense` is always 0 for one - see card_entry.h)
// and instead shows its link markers, rendered by `entry.linkMarkerDisplay`
// (card_entry.cpp, cited against gframe/data_manager.cpp's
// FormatLinkMarker()). ATK/DEF are read from `entry.attackDisplay`/
// `entry.defenseDisplay`, never `entry.attack`/`entry.defense` directly -
// a negative stored value is a real "varies" sentinel, not an error, and
// renders as "?" exactly as upstream's own card-info panel does (see
// card_entry.h). This is presentation of static card metadata only - no
// legality, no rules evaluation.

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
                text: "ATK"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                objectName: "atkValueText"
                text: root.entry.attackDisplay
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                // A Link monster has no real DEF - CardRecord::defense is
                // always 0 for one, and the stored `def` column value is
                // its link markers instead (see card_entry.h). Hiding this
                // whole row for a Link, rather than showing a fake "0",
                // is the honest choice.
                objectName: "defRowLabel"
                visible: !root.entry.isLink
                text: "DEF"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                objectName: "defRowValue"
                visible: !root.entry.isLink
                text: root.entry.defenseDisplay
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                // See this file's own header comment and docs/architecture/
                // deck-builder-ui.md#10.7: Level/Rank/Link Rating are one
                // shared stored magnitude, labelled by isLink/isXyz.
                objectName: "levelRankLabelText"
                text: root.entry.isLink ? "Link Rating" : (root.entry.isXyz ? "Rank" : "Level")
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                objectName: "levelRankValueText"
                text: String(root.entry.level)
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
            }

            Text {
                objectName: "linkMarkerRowLabel"
                visible: root.entry.isLink
                text: "Link Markers"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                objectName: "linkMarkerValueText"
                visible: root.entry.isLink
                text: root.entry.linkMarkerDisplay
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

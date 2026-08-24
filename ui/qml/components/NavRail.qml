// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The navigation rail collapses to icons below Theme.breakpointCompact so the
// layout is driven by available width, not by a fixed resolution.

import QtQuick
import QtQuick.Layouts
import EdoproNext

Rectangle {
    id: root

    property int currentIndex: 0
    property bool compact: false
    signal navigate(int index)

    color: Theme.surface
    implicitWidth: compact ? Theme.navRailCompactWidth : Theme.navRailWidth
    Behavior on implicitWidth {
        NumberAnimation {
            duration: Theme.durNormal * Theme.motionScale
            easing.type: Theme.easeStandard
        }
    }

    // Hairline separator rather than a shadow: cheaper, and quieter.
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        // Wordmark
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                spacing: Theme.space3
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 4; height: 20; radius: 2
                    color: Theme.accent
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.compact
                    text: "edopro-next"
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.textHeading
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
            }
        }

        SectionHeading {
            visible: !root.compact
            Layout.leftMargin: Theme.space3
            Layout.topMargin: Theme.space2
            text: "Play"
        }

        Repeater {
            model: [
                { label: "Home",     glyph: "◆" },
                { label: "Decks",    glyph: "▤" },
                { label: "Duel",     glyph: "⚔" },
                { label: "Replays",  glyph: "▶" }
            ]
            NavButton {
                Layout.fillWidth: true
                label: modelData.label
                glyph: modelData.glyph
                compact: root.compact
                active: root.currentIndex === index
                onActivated: root.navigate(index)
            }
        }

        Item { Layout.fillHeight: true }

        NavButton {
            Layout.fillWidth: true
            label: "Settings"
            glyph: "⚙"
            compact: root.compact
            active: root.currentIndex === 4
            onActivated: root.navigate(4)
        }
    }
}

// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A navigation entry. Focus is explicit and visible: this must be operable by
// keyboard and, later, by controller. The focus ring is not decoration.

import QtQuick
import QtQuick.Controls
import EdoproNext

Item {
    id: root

    property string label: ""
    property string glyph: ""
    property bool active: false
    property bool compact: false
    signal activated()

    implicitHeight: 40
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: label

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: root.active ? Theme.accentSubtle
                           : (hover.hovered ? Theme.surfaceHover : "transparent")
        Behavior on color {
            ColorAnimation { duration: Theme.durFast * Theme.motionScale }
        }

        // Active indicator: a short rule, not a filled block.
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: 0
            width: 2
            height: root.active ? 18 : 0
            radius: 1
            color: Theme.accent
            Behavior on height {
                NumberAnimation {
                    duration: Theme.durNormal * Theme.motionScale
                    easing.type: Theme.easeEmphasis
                }
            }
        }
    }

    // Focus ring, drawn outside the fill so it never shifts layout.
    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.accent
        visible: root.activeFocus
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        spacing: Theme.space3

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.glyph
            font.family: Theme.fontFamily
            font.pixelSize: 15
            color: root.active ? Theme.accent : Theme.textSecondary
            Behavior on color {
                ColorAnimation { duration: Theme.durFast * Theme.motionScale }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: !root.compact
            text: root.label
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            font.weight: root.active ? Theme.weightMedium : Theme.weightRegular
            color: root.active ? Theme.textPrimary : Theme.textSecondary
            Behavior on color {
                ColorAnimation { duration: Theme.durFast * Theme.motionScale }
            }
        }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.activated() }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                || event.key === Qt.Key_Space) {
            root.activated();
            event.accepted = true;
        }
    }
}

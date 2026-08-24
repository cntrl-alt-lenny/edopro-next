// SPDX-License-Identifier: AGPL-3.0-or-later
//
// An honest empty state. It explains what will live here and why it is not
// here yet, rather than showing a fake or disabled-looking interface.

import QtQuick
import QtQuick.Layouts
import EdoproNext

Item {
    id: root
    property string screenName: ""
    property string intent: ""
    property string blockedBy: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space7 * 2, 520)
        spacing: Theme.space4

        SectionHeading { text: root.screenName }

        Text {
            Layout.fillWidth: true
            text: "Not implemented yet"
            font.family: Theme.fontFamily
            font.pointSize: Theme.textTitle
            font.weight: Theme.weightBold
            color: Theme.textPrimary
        }

        Text {
            Layout.fillWidth: true
            text: root.intent
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.45
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            height: blockedText.implicitHeight + Theme.space4 * 2
            radius: Theme.radiusMd
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            Text {
                id: blockedText
                anchors.fill: parent
                anchors.margins: Theme.space4
                text: root.blockedBy
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
                lineHeight: 1.4
            }
        }
    }
}

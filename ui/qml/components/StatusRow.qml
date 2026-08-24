// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Reports a real project status. `state` is one of: "working", "progress",
// "planned". Nothing here may claim functionality that does not exist.

import QtQuick
import QtQuick.Layouts
import EdoproNext

RowLayout {
    id: root
    property string title: ""
    property string detail: ""
    property string status: "planned"

    spacing: Theme.space3

    readonly property color statusColor: status === "working" ? Theme.success
                                       : status === "progress" ? Theme.warning
                                       : Theme.textTertiary

    Rectangle {
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 5
        width: 6; height: 6; radius: 3
        color: root.statusColor
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Text {
            text: root.title
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            font.weight: Theme.weightMedium
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            text: root.detail
            font.family: Theme.fontFamily
            font.pointSize: Theme.textCaption
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }

    Text {
        Layout.alignment: Qt.AlignTop
        text: root.status === "working" ? "working"
            : root.status === "progress" ? "in progress" : "planned"
        font.family: Theme.fontFamily
        font.pointSize: Theme.textCaption
        color: root.statusColor
    }
}

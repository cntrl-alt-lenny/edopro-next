// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Reports one roadmap milestone's status, in the roadmap's own vocabulary:
// `status` is one of "done", "in progress", "not started" (docs/ROADMAP.md).
// The caller states the status and tools/check_home_status.py checks it
// against the roadmap, so nothing here may claim functionality that does not
// exist.

import QtQuick
import QtQuick.Layouts
import EdoproNext

RowLayout {
    id: root
    property string milestone: ""
    property string title: ""
    property string detail: ""
    property string status: "not started"

    spacing: Theme.space3

    readonly property color statusColor: status === "done" ? Theme.success
                                       : status === "in progress" ? Theme.warning
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
        RowLayout {
            spacing: Theme.space2
            Text {
                visible: root.milestone !== ""
                Layout.preferredWidth: Theme.space5
                text: root.milestone
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
            }
            Text {
                text: root.title
                font.family: Theme.fontFamily
                font.pointSize: Theme.textBody
                font.weight: Theme.weightMedium
                color: Theme.textPrimary
            }
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
        text: root.status
        font.family: Theme.fontFamily
        font.pointSize: Theme.textCaption
        color: root.statusColor
    }
}

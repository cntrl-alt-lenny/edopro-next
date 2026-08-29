// SPDX-License-Identifier: AGPL-3.0-or-later
//
// One deck section (Main/Extra/Side) as a keyboard-selectable list. Reads
// directly from a DeckSectionModel supplied by DeckController - this
// component holds no deck data of its own. An unknown card code (present
// in the loaded deck but not the loaded catalogue) is shown honestly, with
// its numeric code, rather than hidden or silently dropped - see
// docs/architecture/deck-builder-ui.md#unknown-card-handling.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EdoproNext

ColumnLayout {
    id: root

    property string title: ""
    property int count: 0
    property var sectionModel: null
    property int section: -1
    signal entryActivated(int row)

    spacing: Theme.space1

    SectionHeading { text: root.title + " (" + root.count + ")" }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Theme.radiusMd
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        ListView {
            id: listView
            anchors.fill: parent
            anchors.margins: Theme.space1
            clip: true
            model: root.sectionModel
            activeFocusOnTab: true
            ScrollBar.vertical: ScrollBar {}

            onCurrentIndexChanged: root.entryActivated(currentIndex)

            // Keys has no dedicated onBackspacePressed handler (unlike
            // onDeletePressed) - Backspace is handled through the generic
            // onPressed dispatch instead, the same pattern NavButton.qml
            // already uses for Return/Enter/Space.
            Keys.onDeletePressed: {
                if (currentIndex >= 0)
                    deckController.removeAt(root.section, currentIndex);
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Backspace && currentIndex >= 0) {
                    deckController.removeAt(root.section, currentIndex);
                    event.accepted = true;
                }
            }

            delegate: ItemDelegate {
                width: listView.width
                highlighted: ListView.isCurrentItem
                onClicked: listView.currentIndex = index

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: parent.highlighted ? Theme.accentSubtle
                         : (parent.hovered ? Theme.surfaceHover : "transparent")
                }

                contentItem: RowLayout {
                    spacing: Theme.space2
                    Text {
                        Layout.fillWidth: true
                        text: model.name
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.textBody
                        font.italic: !model.known
                        color: model.known ? Theme.textPrimary : Theme.textTertiary
                    }
                    Text {
                        visible: !model.known
                        text: "#" + model.cardCode
                        font.family: Theme.fontFamilyMono
                        font.pointSize: Theme.textCaption
                        color: Theme.warning
                    }
                }

                Keys.onReturnPressed: listView.currentIndex = index
            }
        }
    }
}

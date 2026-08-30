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
    // A presentation-level request only - this component never calls
    // deckController itself. The owning screen performs the actual
    // removal and is the only thing that knows how to clean up selection/
    // preview state afterward (docs/architecture/deck-builder-ui.md#7.1) -
    // duplicating that cleanup here would be exactly the kind of
    // mouse-path/keyboard-path divergence external review found: the
    // original Keys.onDeletePressed/Backspace handlers called
    // deckController.removeAt() directly, bypassing
    // DeckBuilderScreen.removeSelectedDeckEntry()'s clearAllSelection()
    // entirely and leaving stale selectedSection/selectedDeckRow/
    // hasPreview/previewCode behind.
    signal removeRequested(int row)

    // A plain alias (not a one-way declarative binding) so the owning
    // screen can imperatively clear or set this list's visual selection -
    // e.g. to deselect this section when another section or the search
    // list is selected instead - without fighting a bound expression: a
    // `currentIndex: someExpression` binding declared elsewhere would be
    // permanently broken the moment a click below assigns to it directly,
    // which is exactly the class of bug that let three independently
    // selectable lists show contradictory highlights at once.
    property alias currentIndex: listView.currentIndex

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
            // ListView's own default is 0, not -1, the moment a non-empty
            // model is set - without this, the first row silently becomes
            // "selected" the instant a deck is loaded or a card is added,
            // with no user interaction at all (found via visual
            // verification: an unrelated screenshot showed an unselected
            // deck's first entry already highlighted, "Remove selected"
            // already enabled, and the preview pane already populated).
            currentIndex: -1
            ScrollBar.vertical: ScrollBar {}

            onCurrentIndexChanged: root.entryActivated(currentIndex)

            // Keys has no dedicated onBackspacePressed handler (unlike
            // onDeletePressed) - Backspace is handled through the generic
            // onPressed dispatch instead, the same pattern NavButton.qml
            // already uses for Return/Enter/Space. Both just request -
            // never mutate deckController or this list's own currentIndex
            // themselves.
            Keys.onDeletePressed: {
                if (currentIndex >= 0)
                    root.removeRequested(currentIndex);
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Backspace && currentIndex >= 0) {
                    root.removeRequested(currentIndex);
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

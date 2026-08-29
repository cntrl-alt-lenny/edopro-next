// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The functional deck-builder core (M3D1): search the loaded card pool,
// inspect a card, add it explicitly to Main/Extra/Side, remove entries,
// and open/save a .ydk. This screen owns no deck data itself - every list
// here is a direct view over `deckController`'s canonical Deck
// (deck_controller.h) or `cardCatalog`'s search index
// (search_results_model.h); this file only renders and forwards user
// intent. See docs/architecture/deck-builder-ui.md.
//
// Deliberately not implemented here: legality of any kind, deck-size or
// copy-count limits, automatic Main/Extra classification, artwork,
// archetype-name search, controller navigation.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import EdoproNext

Item {
    id: root

    readonly property bool hasCatalog: cardCatalog.loaded

    // The card currently shown in the preview pane - whichever of the two
    // lists below was most recently interacted with. Not persisted
    // anywhere else; purely this screen's own transient presentation
    // state (ui/qml owns visual selection, never deck contents - see
    // docs/architecture/deck-builder-ui.md#what-qml-owns).
    property int previewCode: -1
    property bool previewKnown: false

    property int selectedResultRow: -1
    property int selectedSection: -1 // DeckController.Main/.Extra/.Side, or -1
    property int selectedDeckRow: -1

    function sectionModel(section) {
        if (section === DeckController.Main) return deckController.mainModel;
        if (section === DeckController.Extra) return deckController.extraModel;
        if (section === DeckController.Side) return deckController.sideModel;
        return null;
    }

    function selectDeckEntry(section, row) {
        selectedSection = section;
        selectedDeckRow = row;
        const model = sectionModel(section);
        if (model && row >= 0 && row < model.rowCount()) {
            previewCode = model.data(model.index(row, 0), DeckSectionModel.CardCodeRole);
            previewKnown = model.data(model.index(row, 0), DeckSectionModel.KnownRole);
        }
    }

    function addSelectedResultTo(section) {
        if (selectedResultRow < 0 || selectedResultRow >= searchResults.resultCount)
            return;
        deckController.addCard(searchResults.cardCodeAt(selectedResultRow), section);
    }

    function removeSelectedDeckEntry() {
        if (selectedSection < 0 || selectedDeckRow < 0)
            return;
        deckController.removeAt(selectedSection, selectedDeckRow);
        selectedDeckRow = -1;
    }

    function confirmThen(action) {
        if (deckController.dirty) {
            pendingAction = action;
            discardDialog.open();
        } else {
            action();
        }
    }
    property var pendingAction: null

    SearchResultsModel {
        id: searchResults
        catalog: cardCatalog
        queryText: searchField.text
    }

    // ---- Keyboard access (core, not final parity) --------------------
    // Only active while this screen is the one actually showing - these
    // Shortcut items exist for the whole app lifetime (StackLayout keeps
    // every page instantiated), so without this guard Ctrl+S would fire
    // even while looking at Home.
    readonly property bool isActiveScreen: StackLayout.isCurrentItem

    Shortcut {
        sequence: "Ctrl+F"
        enabled: root.isActiveScreen
        onActivated: searchField.forceActiveFocus()
    }
    Shortcut {
        sequence: "Ctrl+O"
        enabled: root.isActiveScreen
        onActivated: root.confirmThen(function() { openDialog.open(); })
    }
    Shortcut {
        sequence: "Ctrl+S"
        enabled: root.isActiveScreen
        onActivated: {
            if (!deckController.saveDeck())
                saveAsDialog.open();
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: root.isActiveScreen
        onActivated: saveAsDialog.open()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.isActiveScreen
        onActivated: {
            selectedResultRow = -1;
            selectedDeckRow = -1;
            selectedSection = -1;
        }
    }

    FileDialog {
        id: openDialog
        title: "Open deck"
        nameFilters: ["Yu-Gi-Oh! deck (*.ydk)", "All files (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: deckController.loadDeck(selectedFile)
    }
    FileDialog {
        id: saveAsDialog
        title: "Save deck as"
        nameFilters: ["Yu-Gi-Oh! deck (*.ydk)", "All files (*)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ydk"
        onAccepted: deckController.saveDeckAs(selectedFile)
    }

    Dialog {
        id: discardDialog
        // An explicit width, rather than letting the dialog's implicitWidth
        // derive from contentItem's own width (which the wrapped Text
        // below would otherwise need to derive from the dialog's width in
        // turn) - that circular shape is exactly what produced a real
        // "Binding loop detected for property implicitWidth" warning here.
        width: 360
        modal: true
        anchors.centerIn: parent
        title: "Discard unsaved changes?"
        standardButtons: Dialog.Discard | Dialog.Cancel
        onAccepted: {
            if (root.pendingAction) {
                const action = root.pendingAction;
                root.pendingAction = null;
                action();
            }
        }
        onRejected: root.pendingAction = null

        contentItem: Text {
            text: "The current deck has unsaved changes. Discard them?"
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }

    // ---- No catalog loaded: an honest empty state, not a broken screen -
    ColumnLayout {
        anchors.centerIn: parent
        visible: !root.hasCatalog
        width: Math.min(parent.width - Theme.space7 * 2, 520)
        spacing: Theme.space4

        SectionHeading { text: "Decks" }
        Text {
            Layout.fillWidth: true
            text: "No card database loaded"
            font.family: Theme.fontFamily
            font.pointSize: Theme.textTitle
            font.weight: Theme.weightBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            text: "Start edopro-next with one or more --card-db <path> options pointing at a "
                + "Project Ignis-compatible .cdb file to search and build decks. No database "
                + "is bundled with this application."
            font.family: Theme.fontFamily
            font.pointSize: Theme.textBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.45
        }
        Text {
            Layout.fillWidth: true
            visible: cardCatalog.lastError.length > 0
            text: cardCatalog.lastError
            font.family: Theme.fontFamilyMono
            font.pointSize: Theme.textCaption
            color: Theme.danger
            wrapMode: Text.WordWrap
        }
    }

    // ---- The real screen -----------------------------------------------
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space4
        visible: root.hasCatalog

        // -- Search pane --
        ColumnLayout {
            Layout.preferredWidth: parent.width * 0.34
            Layout.fillHeight: true
            spacing: Theme.space3

            SectionHeading { text: "Search (" + cardCatalog.cardCount + " cards loaded)" }

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search by name or text…"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textBody
                color: Theme.textPrimary
                Keys.onDownPressed: resultsList.forceActiveFocus()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    clip: true
                    model: searchResults
                    activeFocusOnTab: true
                    currentIndex: root.selectedResultRow
                    onCurrentIndexChanged: {
                        root.selectedResultRow = currentIndex;
                        if (currentIndex >= 0) {
                            root.previewCode = searchResults.cardCodeAt(currentIndex);
                            root.previewKnown = true;
                        }
                    }
                    ScrollBar.vertical: ScrollBar {}

                    delegate: ItemDelegate {
                        width: resultsList.width
                        highlighted: ListView.isCurrentItem
                        onClicked: resultsList.currentIndex = index

                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: parent.highlighted ? Theme.accentSubtle
                                 : (parent.hovered ? Theme.surfaceHover : "transparent")
                        }

                        contentItem: ColumnLayout {
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: model.name
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.textBody
                                font.weight: Theme.weightMedium
                                color: Theme.textPrimary
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: model.summary.length > 0
                                text: model.summary
                                elide: Text.ElideRight
                                font.family: Theme.fontFamilyMono
                                font.pointSize: Theme.textCaption
                                color: Theme.textTertiary
                            }
                        }

                        Keys.onReturnPressed: resultsList.currentIndex = index
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                enabled: root.selectedResultRow >= 0

                Button {
                    text: "Add to Main"
                    onClicked: root.addSelectedResultTo(DeckController.Main)
                }
                Button {
                    text: "Extra"
                    onClicked: root.addSelectedResultTo(DeckController.Extra)
                }
                Button {
                    text: "Side"
                    onClicked: root.addSelectedResultTo(DeckController.Side)
                }
            }
        }

        Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }

        // -- Deck pane --
        ColumnLayout {
            Layout.preferredWidth: parent.width * 0.36
            Layout.fillHeight: true
            spacing: Theme.space3

            ColumnLayout {
                // Two rows rather than one, so the current filename and the
                // three file actions never compete for the same horizontal
                // space - at the 960px minimum width, a single RowLayout
                // squeezed the SectionHeading down to a couple of pixels
                // and the (opaque) New button painted over what was left of
                // its text instead of the text eliding or wrapping.
                Layout.fillWidth: true
                spacing: Theme.space2

                SectionHeading {
                    Layout.fillWidth: true
                    text: deckController.currentFileName.length > 0
                        ? deckController.currentFileName + (deckController.dirty ? " • unsaved" : "")
                        : "New deck" + (deckController.dirty ? " • unsaved" : "")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Button {
                        Layout.fillWidth: true
                        text: "New"
                        onClicked: root.confirmThen(function() { deckController.newDeck(); })
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Open"
                        onClicked: root.confirmThen(function() { openDialog.open(); })
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Save"
                        onClicked: {
                            if (!deckController.saveDeck())
                                saveAsDialog.open();
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: deckController.lastError.length > 0
                text: deckController.lastError
                font.family: Theme.fontFamilyMono
                font.pointSize: Theme.textCaption
                color: Theme.danger
                wrapMode: Text.WordWrap
            }

            DeckSectionList {
                id: mainList
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Main"
                count: deckController.mainCount
                sectionModel: deckController.mainModel
                section: DeckController.Main
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Main, row); }
            }
            DeckSectionList {
                id: extraList
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Extra"
                count: deckController.extraCount
                sectionModel: deckController.extraModel
                section: DeckController.Extra
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Extra, row); }
            }
            DeckSectionList {
                id: sideList
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Side"
                count: deckController.sideCount
                sectionModel: deckController.sideModel
                section: DeckController.Side
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Side, row); }
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text: "Remove selected"
                enabled: root.selectedSection >= 0 && root.selectedDeckRow >= 0
                onClicked: root.removeSelectedDeckEntry()
            }
        }

        Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }

        // -- Preview pane --
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space3

            SectionHeading { text: "Card details" }

            CardPreview {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.previewCode >= 0
                // Always a real CardEntry, never a placeholder {} - a
                // plain JS object has no `known`/`isMonster`/etc.
                // properties at all, so binding CardPreview's strongly
                // typed bool/QString properties to (say) `({}).known`
                // assigns `undefined`, which QML reports as a real
                // warning ("Unable to assign [undefined] to bool").
                // CardCode 0 is guaranteed by data/'s own invariant
                // (card_code.h) never to be a real loaded card, so
                // cardDetails(0) reliably returns known: false with every
                // other field at its ordinary default - never undefined.
                entry: cardCatalog.cardDetails(Math.max(root.previewCode, 0))
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.previewCode < 0
                text: "Select a search result or a deck entry to see its details here."
                font.family: Theme.fontFamily
                font.pointSize: Theme.textBody
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignTop
            }
        }
    }
}

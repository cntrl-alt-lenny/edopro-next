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
    // previewCode is a card code, which can be any nonzero uint32 - a
    // plain QML `int` is signed 32-bit and cannot represent values above
    // 2^31-1 without wraparound, so "no selection" is tracked with its own
    // boolean rather than a negative-value sentinel on previewCode itself.
    property double previewCode: 0
    property bool hasPreview: false
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

    // ---- Selection: one coherent notion of "what is selected", shared
    // across the search results list and all three deck-section lists.
    //
    // Each DeckSectionList's ListView.currentIndex (exposed via its
    // `currentIndex` alias) and resultsList's own currentIndex are all
    // driven imperatively from here - never from a declarative binding
    // back to selectedSection/selectedDeckRow/selectedResultRow. A
    // declarative `currentIndex: root.selectedResultRow`-style binding is
    // permanently broken the first time a click imperatively assigns to
    // that same currentIndex (a real, established QML behaviour: writing
    // to a property removes its earlier binding), so relying on one here
    // would silently stop clearing a list's visual highlight after the
    // first click - exactly the kind of stale-highlight bug this contract
    // exists to prevent. Selecting anything always clears every other
    // selection first, so at most one row is ever highlighted at a time.
    function clearAllSelection() {
        selectedResultRow = -1;
        selectedSection = -1;
        selectedDeckRow = -1;
        hasPreview = false;
        resultsList.currentIndex = -1;
        mainList.currentIndex = -1;
        extraList.currentIndex = -1;
        sideList.currentIndex = -1;
    }

    function selectDeckEntry(section, row) {
        // A list's own currentIndex can become -1 on its own (this screen
        // clearing it programmatically to deselect that list, or a model
        // reset leaving nothing current) - that is not a new selection to
        // act on, just the list reporting its own now-empty state; ignore
        // it here rather than clobbering whatever this screen just set.
        if (row < 0)
            return;
        const model = sectionModel(section);
        if (!model || row >= model.rowCount())
            return;
        selectedResultRow = -1;
        resultsList.currentIndex = -1;
        selectedSection = section;
        selectedDeckRow = row;
        if (section !== DeckController.Main) mainList.currentIndex = -1;
        if (section !== DeckController.Extra) extraList.currentIndex = -1;
        if (section !== DeckController.Side) sideList.currentIndex = -1;
        previewCode = model.data(model.index(row, 0), DeckSectionModel.CardCodeRole);
        previewKnown = model.data(model.index(row, 0), DeckSectionModel.KnownRole);
        hasPreview = true;
    }

    function addSelectedResultTo(section) {
        if (selectedResultRow < 0 || selectedResultRow >= searchResults.resultCount)
            return;
        deckController.addCard(searchResults.cardCodeAt(selectedResultRow), section);
    }

    // The one and only removal path - both the "Remove selected" button
    // and each DeckSectionList's Delete/Backspace request funnel through
    // here, so mouse and keyboard can never diverge in what cleanup
    // happens afterward (a real bug external review found: the keyboard
    // path originally called deckController.removeAt() directly, leaving
    // stale selection/preview state behind that only the button path
    // cleared).
    function removeDeckEntry(section, row) {
        const model = sectionModel(section);
        if (!model || row < 0 || row >= model.rowCount())
            return;
        deckController.removeAt(section, row);
        // Rather than guess a "next" row to land on (which, after a
        // removal, may not even refer to a sensible neighbour - e.g. the
        // section can now be empty, or a different card has shifted into
        // the same numeric index), clear selection completely: a
        // deterministic, unambiguous state that can never show a preview
        // silently mismatched with what is actually still selected.
        clearAllSelection();
    }

    function removeSelectedDeckEntry() {
        if (selectedSection < 0 || selectedDeckRow < 0)
            return;
        removeDeckEntry(selectedSection, selectedDeckRow);
    }

    function saveOrSaveAs() {
        // saveDeck() returning false means two different things -
        // "there is no current path yet" and "there is a path, but
        // writing it genuinely failed" - and only the first should ever
        // prompt Save As. Conflating them would turn a real disk/
        // permission error into a confusing, unexplained Save As dialog
        // instead of the actual error deckController.lastError already
        // surfaces in the deck pane.
        if (deckController.currentPath.length === 0) {
            saveAsDialog.open();
        } else {
            deckController.saveDeck();
        }
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

    // SearchResultsModel::refresh() (search_results_model.cpp) always
    // does a full beginResetModel()/endResetModel() - for a new query, a
    // catalog reload, or both - and emits resultsChanged() unconditionally
    // once it finishes. resultsList.onCurrentIndexChanged (below) is not
    // enough on its own to keep a stale selection from surviving that:
    // Qt Quick's ListView does not reliably re-fire currentIndexChanged
    // just because a reset replaced the *data* at an unchanged numeric
    // index (only when the index number itself changes) - so a selected
    // result could survive a query change or a catalog reload as the same
    // currentIndex, now silently pointing at a completely different card,
    // with the old card's preview still showing. Every refresh() - for
    // any reason - must invalidate any live search-result selection.
    // Deliberately does not touch a deck-section selection
    // (selectedSection/selectedDeckRow) - a search refresh has nothing to
    // do with those, and clearing them here would be exactly the kind of
    // "accidentally clear a valid deck-section selection" the search/deck
    // separation this screen owns must not do.
    Connections {
        target: searchResults
        function onResultsChanged() {
            if (root.selectedResultRow >= 0) {
                root.selectedResultRow = -1;
                resultsList.currentIndex = -1;
                root.hasPreview = false;
            }
        }
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
        onActivated: root.saveOrSaveAs()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: root.isActiveScreen
        onActivated: saveAsDialog.open()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.isActiveScreen
        onActivated: root.clearAllSelection()
    }

    FileDialog {
        id: openDialog
        title: "Open deck"
        nameFilters: ["Yu-Gi-Oh! deck (*.ydk)", "All files (*)"]
        fileMode: FileDialog.OpenFile
        // Only on a successful load - a failure leaves the current deck
        // (and therefore its selection/preview) genuinely unchanged, so
        // there is nothing stale to clear.
        onAccepted: {
            if (deckController.loadDeck(selectedFile))
                root.clearAllSelection();
        }
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
        // Dialog.Discard carries Qt's DestructiveRole, which fires
        // discarded() - not accepted() - confirmed against this project's
        // own Qt 6.8.3 build (a standalone offscreen QML case that clicked
        // the button and logged which signal fired). Using onAccepted here
        // meant clicking Discard closed the dialog but silently dropped
        // pendingAction, so New/Open could never actually proceed past the
        // confirmation once a deck had unsaved edits.
        onDiscarded: {
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

    // ---- The real screen - always visible, even with no catalog loaded.
    // Only card search/resolution degrades without one: the deck pane and
    // preview pane stay fully functional regardless of hasCatalog, since
    // data/'s own Deck and .ydk codec never depend on a CardDatabase
    // either (docs/architecture/deck-builder-ui.md#no-catalog-editing).
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space4

        // -- Search pane --
        ColumnLayout {
            // 0.32/0.34, not the original 0.34/0.36, and the preview pane
            // below carries an explicit Layout.minimumWidth - found via
            // visual verification: at the default 1280x800 window, this
            // RowLayout's own available width is the *shell's* width
            // (1064px, after the nav rail's own share), not the full
            // window width, so the original percentages left the preview
            // pane only ~138px wide - enough for "Card code " (no
            // wrapMode, so it silently overflowed the window's own right
            // edge) and to force-wrap a card title into single overflowing
            // words rather than comfortably wrapped lines.
            Layout.preferredWidth: parent.width * 0.32
            Layout.fillHeight: true
            spacing: Theme.space3

            SectionHeading {
                text: root.hasCatalog
                    ? ("Search (" + cardCatalog.cardCount + " cards loaded)")
                    : "Search"
            }

            // No catalog: an honest message in place of the search UI, not
            // a broken or hidden screen - the deck pane and preview pane
            // beside this one remain fully usable regardless.
            ColumnLayout {
                objectName: "noCatalogMessage"
                visible: !root.hasCatalog
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space4

                Text {
                    Layout.fillWidth: true
                    text: "No card database loaded"
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.textTitle
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    text: "Start edopro-next with one or more --card-db <path> options "
                        + "pointing at a Project Ignis-compatible .cdb file to search for "
                        + "cards. No database is bundled with this application. A deck can "
                        + "still be opened, edited and saved without one - card names simply "
                        + "will not resolve until a database is loaded."
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
                Item { Layout.fillWidth: true; Layout.fillHeight: true }
            }

            // Has a catalog: the real search UI.
            ColumnLayout {
                visible: root.hasCatalog
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space3

                // Surfaced here too, not only in the no-catalog message
                // above: with multiple --card-db paths, one can fail
                // while another still succeeds, leaving hasCatalog true -
                // lastError must stay visible or a partial load failure
                // is silently lost (CLAUDE.md's honesty rules).
                Text {
                    Layout.fillWidth: true
                    visible: cardCatalog.lastError.length > 0
                    text: cardCatalog.lastError
                    font.family: Theme.fontFamilyMono
                    font.pointSize: Theme.textCaption
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                }

                TextField {
                    id: searchField
                    // Set (only) so visual/interaction verification tooling
                    // can locate this real instance via findChild() - not
                    // read by any production code.
                    objectName: "searchField"
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
                        // Set (only) so ui/tests/test_deckbuilder_screen.cpp
                        // can locate this real instance via findChild() -
                        // not read or relied upon by any production code.
                        objectName: "resultsList"
                        anchors.fill: parent
                        anchors.margins: Theme.space1
                        clip: true
                        model: searchResults
                        activeFocusOnTab: true
                        // ListView's own default is 0, not -1, the instant
                        // a non-empty model is set - without this, typing a
                        // query (or even the initial, unfiltered "show
                        // everything" result set) would silently
                        // auto-select and preview the first result with no
                        // user interaction at all (found via visual
                        // verification - see the identical fix and comment
                        // on DeckSectionList.qml's own internal ListView).
                        // Not a binding to root.selectedResultRow (see the
                        // selection-contract comment above sectionModel())
                        // - currentIndex is driven purely imperatively from
                        // there on, by user clicks below and by this
                        // screen's own clearAllSelection()/selectDeckEntry().
                        currentIndex: -1
                        onCurrentIndexChanged: {
                            root.selectedResultRow = currentIndex;
                            if (currentIndex >= 0) {
                                root.previewCode = searchResults.cardCodeAt(currentIndex);
                                root.previewKnown = true;
                                root.hasPreview = true;
                                root.selectedSection = -1;
                                root.selectedDeckRow = -1;
                                mainList.currentIndex = -1;
                                extraList.currentIndex = -1;
                                sideList.currentIndex = -1;
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
        }

        Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }

        // -- Deck pane --
        ColumnLayout {
            Layout.preferredWidth: parent.width * 0.34 // see the search pane's own comment above
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
                        onClicked: root.confirmThen(function() {
                            deckController.newDeck();
                            root.clearAllSelection();
                        })
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Open"
                        onClicked: root.confirmThen(function() { openDialog.open(); })
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Save"
                        onClicked: root.saveOrSaveAs()
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
                // Set (only) so ui/tests/test_deckbuilder_screen.cpp can
                // locate this real instance via findChild() - not read or
                // relied upon by any production code.
                objectName: "mainList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Main"
                count: deckController.mainCount
                sectionModel: deckController.mainModel
                section: DeckController.Main
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Main, row); }
                onRemoveRequested: function(row) { root.removeDeckEntry(DeckController.Main, row); }
            }
            DeckSectionList {
                id: extraList
                objectName: "extraList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Extra"
                count: deckController.extraCount
                sectionModel: deckController.extraModel
                section: DeckController.Extra
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Extra, row); }
                onRemoveRequested: function(row) { root.removeDeckEntry(DeckController.Extra, row); }
            }
            DeckSectionList {
                id: sideList
                objectName: "sideList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Side"
                count: deckController.sideCount
                sectionModel: deckController.sideModel
                section: DeckController.Side
                onEntryActivated: function(row) { root.selectDeckEntry(DeckController.Side, row); }
                onRemoveRequested: function(row) { root.removeDeckEntry(DeckController.Side, row); }
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
            // A hard floor, not just "whatever's left over" - found via
            // visual verification that the plain fillWidth share could
            // shrink to ~138px at the default 1280x800 window (see the
            // search pane's own comment above), nowhere near enough to
            // show a card title or the ATK/DEF grid without text
            // overflowing this pane's own right edge. Qt Quick Layouts
            // treats minimumWidth as a harder constraint than a sibling's
            // preferredWidth, so the search/deck panes shrink first if
            // there is ever genuinely not enough room for all three.
            Layout.minimumWidth: 260
            spacing: Theme.space3

            SectionHeading { text: "Card details" }

            CardPreview {
                objectName: "cardPreview"
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.hasPreview
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
                // previewCode itself is never negative (it holds a card
                // code, tracked as a `double` so codes above 2^31-1 are
                // representable - see its declaration above), so no
                // clamping is needed here the way an int sentinel required.
                //
                // cardCatalog.cardCount is read here purely for its
                // dependency-tracking effect, not its value: cardDetails()
                // is a plain method call, which QML's binding engine has
                // no notify signal to react to on its own, so without an
                // explicit property read this binding would keep showing
                // whatever cardDetails() returned at the time of the last
                // *selection* change and never notice a later catalog
                // reload for the same still-selected code. cardCount is a
                // real Q_PROPERTY with NOTIFY loadedChanged (card_catalog.h),
                // so reading it here forces a re-evaluation on every reload.
                entry: {
                    cardCatalog.cardCount;
                    return cardCatalog.cardDetails(root.hasPreview ? root.previewCode : 0);
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !root.hasPreview
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

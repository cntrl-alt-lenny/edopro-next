// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Test-only host for ui/tests/test_deckbuilder_screen.cpp - not shipped in
// edopro_next_shell (a separate qt_add_qml_module target, see
// CMakeLists.txt). Puts a real DeckBuilderScreen inside an actual
// StackLayout as the current page, matching Main.qml's own shape closely
// enough that StackLayout.isCurrentItem resolves to true the same way it
// does for real use - without this, DeckBuilderScreen's `isActiveScreen`
// property (and everything gated on it) would never see a StackLayout
// ancestor at all.

import QtQuick
import QtQuick.Layouts
import EdoproNext

Window {
    id: window
    width: 1280
    height: 800
    visible: true

    StackLayout {
        anchors.fill: parent
        currentIndex: 0

        DeckBuilderScreen {
            id: screen
            objectName: "screen"
        }
    }
}

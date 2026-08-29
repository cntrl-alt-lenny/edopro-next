// SPDX-License-Identifier: AGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EdoproNext

ApplicationWindow {
    id: window

    width: 1280
    height: 800          // Steam Deck's native size, deliberately the default
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "edopro-next"
    color: Theme.bg

    // Layout responds to available width, never to a fixed resolution.
    readonly property bool compact: width < Theme.breakpointCompact

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            id: rail
            Layout.fillHeight: true
            compact: window.compact
            currentIndex: stack.currentIndex
            onNavigate: function(index) { stack.currentIndex = index; }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            // startScreenIndex is a launch-time convenience for visual
            // verification/screenshots only (main.cpp's --start-screen) -
            // normal use always reaches every screen through the nav rail
            // or the Ctrl+<N> shortcuts below, which is why it seeds the
            // initial index rather than becoming a routing property this
            // layout reacts to afterward.
            currentIndex: typeof startScreenIndex !== "undefined" ? startScreenIndex : 0

            HomeScreen {}

            DeckBuilderScreen {}

            NotImplementedScreen {
                screenName: "Duel"
                intent: "The duel field: clear chain visualisation, obvious targeting, "
                      + "elegant prompts, and motion that communicates rules state "
                      + "rather than decorating it."
                blockedBy: "Deliberately last. Requires the semantic client model, and a "
                         + "way to prove a change altered presentation and not duel "
                         + "behaviour. Upstream's duel handling currently fuses protocol "
                         + "decode, state, animation and widgets in one function."
            }

            NotImplementedScreen {
                screenName: "Replays"
                intent: "Browse, inspect and step through saved duels."
                blockedBy: "Upstream's replay handling is already free of UI dependencies, "
                         + "making this a low-risk early migration once the shell matures."
            }

            NotImplementedScreen {
                screenName: "Settings"
                intent: "One coherent settings surface, with strong defaults, that does "
                      + "not require understanding the engine to navigate."
                blockedBy: "Waiting on the configuration model."
            }
        }
    }

    // Global keyboard navigation. Controller support will map onto the same
    // focus model rather than emulating a mouse.
    Shortcut {
        sequences: ["Ctrl+1", "Ctrl+2", "Ctrl+3", "Ctrl+4", "Ctrl+5"]
        onActivated: function() {}
    }
    Shortcut { sequence: "Ctrl+1"; onActivated: stack.currentIndex = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: stack.currentIndex = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: stack.currentIndex = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: stack.currentIndex = 3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: stack.currentIndex = 4 }
}

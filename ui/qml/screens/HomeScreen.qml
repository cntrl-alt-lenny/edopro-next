// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The home screen states honestly what this project is and what actually
// works. It does not display fabricated decks, cards or duel history.

import QtQuick
import QtQuick.Layouts
import EdoproNext

Flickable {
    id: root
    contentWidth: width
    contentHeight: column.implicitHeight + Theme.space8 * 2
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    ColumnLayout {
        id: column
        width: Math.min(root.width - Theme.space7 * 2, Theme.contentMaxWidth)
        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.space8
        spacing: Theme.space6

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3

            Text {
                text: "A modern client for EDOPro"
                font.family: Theme.fontFamily
                font.pointSize: Theme.textDisplay
                font.weight: Theme.weightBold
                font.letterSpacing: Theme.trackingDisplay
                color: Theme.textPrimary
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                text: "Preserving Project Ignis's duel engine and card scripts, while replacing the presentation layer it inherited from old YGOPro."
                font.family: Theme.fontFamily
                font.pointSize: Theme.textBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.45
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space4

            SectionHeading { text: "Status" }

            Text {
                Layout.fillWidth: true
                text: "This shell is an architectural proof, not a playable client. Nothing below is dressed up as finished."
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
            }

            StatusRow {
                Layout.fillWidth: true
                title: "Upstream baseline builds"
                detail: "Untouched upstream EDOPro compiles and runs; see docs/BASELINE.md."
                status: "working"
            }
            StatusRow {
                Layout.fillWidth: true
                title: "Qt 6 / QML shell"
                detail: "This window. Design tokens, responsive rail, keyboard focus."
                status: "working"
            }
            StatusRow {
                Layout.fillWidth: true
                title: "Semantic client model"
                detail: "Presentation-free duel state, so game state can be reasoned about without a renderer. Not started."
                status: "planned"
            }
            StatusRow {
                Layout.fillWidth: true
                title: "Deck builder"
                detail: "The first screen to migrate: deck files and card data are already presentation-independent upstream."
                status: "planned"
            }
            StatusRow {
                Layout.fillWidth: true
                title: "Duel field"
                detail: "Deliberately last. Highest risk; needs the semantic model first."
                status: "planned"
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            SectionHeading { text: "Build" }
            GridLayout {
                columns: 2
                rowSpacing: Theme.space2
                columnSpacing: Theme.space5

                Text {
                    text: "Shell version"
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.textCaption
                    color: Theme.textTertiary
                }
                Text {
                    text: AppContext.appVersion + "  ·  " + AppContext.gitSha
                    font.family: Theme.fontFamilyMono
                    font.pointSize: Theme.textCaption
                    color: Theme.textSecondary
                }
                Text {
                    text: "Qt"
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.textCaption
                    color: Theme.textTertiary
                }
                Text {
                    text: AppContext.qtVersion
                    font.family: Theme.fontFamilyMono
                    font.pointSize: Theme.textCaption
                    color: Theme.textSecondary
                }
                Text {
                    text: "Platform"
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.textCaption
                    color: Theme.textTertiary
                }
                Text {
                    text: AppContext.platform
                    font.family: Theme.fontFamilyMono
                    font.pointSize: Theme.textCaption
                    color: Theme.textSecondary
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space4
            text: "EDOPro is free software under the GNU AGPL v3 or later, developed by Project Ignis. This project is an independent fork and is not affiliated with or endorsed by Project Ignis, Konami or Shueisha."
            font.family: Theme.fontFamily
            font.pointSize: Theme.textCaption
            color: Theme.textTertiary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }
    }
}

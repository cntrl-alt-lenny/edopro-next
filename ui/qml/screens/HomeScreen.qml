// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The home screen states honestly what this project is and how far along it
// is. Its status rows mirror docs/ROADMAP.md and are checked against it by
// tools/check_home_status.py. It does not display fabricated decks, cards or
// duel history.

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
                text: "This shell is not a playable client. To duel today, use upstream EDOPro. Each status below is the state of the matching milestone in docs/ROADMAP.md, and tools/check_home_status.py fails the test suite if they disagree."
                font.family: Theme.fontFamily
                font.pointSize: Theme.textCaption
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
            }

            // One row per roadmap milestone. `milestone`, `title` and `status`
            // are checked against docs/ROADMAP.md by tools/check_home_status.py
            // (run by the Python test suite in CI): change a status only when
            // the roadmap changes, and change it to match. `detail` says what
            // the milestone covers and must not state a status of its own.
            StatusRow {
                Layout.fillWidth: true
                milestone: "M0"
                title: "Foundation"
                detail: "Upstream baseline build, architecture survey, and this Qt 6 / QML shell with its design tokens."
                status: "done"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M1"
                title: "Make change provable"
                detail: "Recorded-protocol regression baseline, for showing that a change altered presentation and not duel behaviour."
                status: "in progress"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M2"
                title: "Semantic client model"
                detail: "Presentation-free duel state decoded from the message stream, so it can be reasoned about without a renderer."
                status: "done"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M3"
                title: "Deck and card data"
                detail: "Card database, deck files, search and deck legality, and the deck builder screen that uses them."
                status: "in progress"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M4"
                title: "Low-risk screens"
                detail: "Settings, replay browser, and lobby and network screens."
                status: "not started"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M5"
                title: "Duel field"
                detail: "Deliberately last: the highest-risk screen, and it depends on everything above."
                status: "not started"
            }
            StatusRow {
                Layout.fillWidth: true
                milestone: "M6"
                title: "Platform and input"
                detail: "Windows and macOS builds and CI, controller navigation, Steam Deck, and an accessibility pass."
                status: "in progress"
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

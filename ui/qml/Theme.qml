// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Design tokens.
//
// Every spacing, colour, radius, duration and type size in the application
// resolves through this file. No screen should contain a raw hex colour or a
// magic pixel value; if something is missing here, add it here.
//
// The palette is deliberately restrained. This is a card game: the artwork is
// the product, and the chrome exists to stay out of its way. No gradients, no
// glassmorphism, no neon.

pragma Singleton
import QtQuick

QtObject {
    id: theme

    // ---- Colour -----------------------------------------------------------
    // Dark-first. Near-neutral surfaces with a very slight cool cast so warm
    // card art reads correctly against them.

    readonly property color bg:            "#0E0F12"
    readonly property color surface:       "#16181D"
    readonly property color surfaceRaised: "#1E2128"
    readonly property color surfaceHover:  "#242832"
    readonly property color border:        "#2A2E37"
    readonly property color borderStrong:  "#3A404C"

    readonly property color textPrimary:   "#E8EAED"
    readonly property color textSecondary: "#9AA0AA"
    readonly property color textTertiary:  "#6B7280"

    // A single muted gold accent, used sparingly - selection, focus, the
    // active nav item. Never as a fill for large areas.
    readonly property color accent:        "#C9A227"
    readonly property color accentSubtle:  "#3A3218"
    readonly property color accentText:    "#0E0F12"

    readonly property color success:       "#5B9E63"
    readonly property color warning:       "#C08A3E"
    readonly property color danger:        "#B4544A"

    // ---- Spacing ----------------------------------------------------------
    // A 4px base scale. Layouts compose these rather than inventing values.

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int space7: 48
    readonly property int space8: 64

    // ---- Radii ------------------------------------------------------------

    readonly property int radiusSm: 4
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12

    // ---- Typography -------------------------------------------------------
    // Sizes are in points so they track the platform's DPI scaling.

    readonly property string fontFamily: "Inter, Segoe UI, Noto Sans, DejaVu Sans, sans-serif"
    readonly property string fontFamilyMono: "JetBrains Mono, Cascadia Mono, DejaVu Sans Mono, monospace"

    readonly property int textDisplay: 30
    readonly property int textTitle:   19
    readonly property int textHeading: 14
    readonly property int textBody:    12
    readonly property int textCaption: 10

    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium:  Font.Medium
    readonly property int weightBold:    Font.DemiBold

    // Tracking. Small caps headings get positive tracking; display text gets
    // slightly negative, which is what makes large type look deliberate.
    readonly property real trackingDisplay: -0.5
    readonly property real trackingHeading: 1.2

    // ---- Motion -----------------------------------------------------------
    // Motion communicates state change. It is never decorative.
    // Honour reduced-motion by setting `enableMotion` false; every animation
    // in the app multiplies its duration by `motionScale`.

    property bool enableMotion: true
    readonly property real motionScale: enableMotion ? 1.0 : 0.0

    readonly property int durFast:   120
    readonly property int durNormal: 180
    readonly property int durSlow:   260

    readonly property int easeStandard: Easing.OutCubic
    readonly property int easeEmphasis: Easing.OutQuint

    // ---- Layout -----------------------------------------------------------
    // Breakpoints, not fixed resolutions. 1280x720 is the minimum sensible
    // desktop target; 1280x800 is the Steam Deck.

    readonly property int breakpointCompact: 1100
    readonly property int navRailWidth: 216
    readonly property int navRailCompactWidth: 64
    readonly property int contentMaxWidth: 1080
}

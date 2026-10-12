# Platform fonts

Typography is presentation only. The engine, semantic model, card data and
legality policy neither select nor depend on fonts.

## Cause and resolution

The offscreen macOS platform supplies a default `Sans Serif` family that is
not installed. Even controls without an explicit Theme family therefore
trigger Qt alias population. Fixing Theme alone does not fix the class.

Theme also previously used comma-separated family strings. In Qt 6.5.3,
[`QQuickFontValueType::setFamily`](https://github.com/qt/qtdeclarative/blob/v6.5.3/src/quick/util/qquickvaluetypes.cpp)
calls `v.setFamily(family)`, and
[`QFont::setFamily`](https://github.com/qt/qtbase/blob/v6.5.3/src/gui/text/qfont.cpp)
calls `setFamilies(QStringList(family))`. This is one literal family name,
not a CSS fallback list. The QString QFont constructor is a different API;
its comma splitting does not establish QML setter semantics. The dedicated
QML regression reads back the font family list to establish this distinction.

`PlatformFonts` chooses an installed Latin family in the previous preference
order, then common platform alternatives (Helvetica/Arial/Liberation Sans;
Menlo/Consolas/Liberation Mono). Every candidate must have the requested
fixed-pitch classification. If these are absent, the platform general/fixed
family is used only when actually installed; otherwise an available public
family with the requested classification is selected. An environment with
no suitable font fails explicitly rather than pretending to render correctly.
The resolved strings are cached for the process, not persisted in settings.

After constructing QGuiApplication and before constructing any QML engine or
control, startup replaces only the default font family with the resolved
proportional family. Theme uses the same resolver for proportional and mono
text. Point sizes, weights, tracking and layout tokens are unchanged.
Changing installed fonts during a running session requires restarting.
Qt's ordinary glyph fallback remains enabled; this is not a promise of every
language's glyph coverage. No fonts are installed, bundled or downloaded;
there is no diagnostic interception or filtering.

## Compatibility and evidence

The helper uses public QFontDatabase families(Latin), isFixedPitch,
isPrivateFamily and systemFont, QFont::setFamily, QGuiApplication::setFont,
and QML_ELEMENT/QML_SINGLETON registration available at the Qt 6.5 floor.
The pinned Qt 6.5.3 source above establishes setter compatibility. Source
compatibility and actual execution are separate: recorded macOS execution
uses Qt 6.11.1; Linux CI uses its existing Qt 6.8.3 job. Qt 6.5 execution and
Windows are not established by those runs.

Run `ctest --test-dir ui/build --output-on-failure` after configuring UI
Debug/WERROR/UI_TESTS. The dedicated font suite checks installed family
membership, actual QFontInfo pitch/resolution, unthemed controls, popup
delegates and the real Main/DeckBuilder presentation at both window sizes
with synthetic cards. With EDOPRO_FOCUS_CAPTURES pointing at an existing
directory, it saves preview/focus and popup captures alongside the existing
screen suite's captures. Assets remain untracked. This test has a separate
CMake directory because two modules with the same URI in one directory can
silently substitute the wrong qmldir, even with distinct output directories.

The real executable separately must pass both unmodified workflow smoke
assertions: survival to timeout and empty stderr. Headless font/screenshots
and replay tests do not establish native navigation reliability or unchanged
duel behaviour. Batch 27 owns native navigation evidence.

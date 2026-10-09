# EDOPro Next · batch 29-platform-font-resolution · Brain review

## Done

Reviewed exact delivery `fbbfcacba9f2221dd0bc9067261423916a2bc69c`, against
default `1068ee31`. Normal path. Inspected resolver, startup, Theme, registration,
dedicated tests, architecture and retained failures. No blocking implementation
defect found. Prepared PR #50 for owner approval; no merge has occurred.

The resolver selects installed Latin proportional/fixed-pitch families without
suppression or installation. Startup initializes the default before QML; Theme
uses the same resolver. Sizes/weights/tracking and layout tokens are unchanged.
Separate test-module scope avoids the prior registration collision. Existing
navigation harness/driver and search adapters are untouched.

Disposition: **accepted for owner-approved merge; PR checks are green at this
exact delivery**. Search/navigation still require integration
and new-head verification; no old review is carried over blindly.

## Checked

Brain reconfigured UI Debug/WERROR/UI_TESTS, rebuilt and ran
`ctest --test-dir ui/build --output-on-failure`: exit 0; deckbuilder,
deckbuilder_screen and platform_fonts all passed. Local macOS arm64 / Qt 6.11.1.
Independent real-shell offscreen smoke: survived 20 seconds (normalized status
124), survival assertion exit 0, empty-stderr assertion exit 0, zero stderr
bytes, wrapper exit 0. No filtering or altered assertions.

Reran dedicated real-shell typography rows at both sizes with fresh captures:
exit 0. Inspected minimum-size preview/focus and Effects popup: title/body wrap,
result elision, numeric mono text, focused search outline and popup text/bounds
are readable. This is headless real-QML evidence, not native-input verification.

Final-head Linux CI runs `37938111899` (branch) and `37939170538` (PR #50)
succeeded at the delivery SHA, including all required jobs and literal strict
shell smoke. PR #50 reports CLEAN. Linux uses the existing Release configuration;
Linux Debug execution is not inferred.

Re-read Qt 6.5.3's primary
[QML setter](https://github.com/qt/qtdeclarative/blob/v6.5.3/src/quick/util/qquickvaluetypes.cpp)
and [QFont setter](https://github.com/qt/qtbase/blob/v6.5.3/src/gui/text/qfont.cpp):
the former calls setFamily, which stores one family string. The recorded literal
comma-string defect and supported setter semantics are accurate. No private API,
engine change, new dependency or softened CI gate was introduced.

## Not checked

Windows, Qt-floor execution, Steam Deck, every language glyph, dynamic font
installation/reselection, physical keyboard or native navigation reliability.
Families are cached until restart. Environments lacking a suitable installed
Latin proportional/monospace font fail explicitly; this limit is documented.

## Failed or blocked

No remaining product-font failure observed at this head. The original screen
test executable still uses its old default-font bootstrap; its macOS diagnostic
is retained and is distinct from the now-clean real-shell smoke. Batch 27 owns
any screen-driver bootstrap integration. Owner approval remains the merge gate;
recheck the exact head/checks immediately before merging. No setting or assertion
waiver is requested.

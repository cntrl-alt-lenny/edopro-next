<!-- fw-report
round: 024-scroll-focus-visibility
role: verifier
branch: verifier/024-scroll-focus-visibility-2
head: 6b9c661ccc5107e2bdd96124e95ac05f9623a5d0
os: macOS 27.0
python: 3.9.6
written: 2026-10-03T20:36:28Z
-->
Reviewed commit: `6b9c661ccc5107e2bdd96124e95ac05f9623a5d0`.
Fresh isolated clone starting on master; exact expected SHA selected by the tool.
Review branch: `verifier/024-scroll-focus-visibility-2`. The earlier blocked report
is on a separate branch and was not continued. Delivery branch naming does not
change the Builder's role. Combined production, tests and documentation diff
reviewed against `origin/master` (`a1306f93883eff04ffc721bcaacc92cd88b54680`);
round 024's correction isolated against round 023's production SHA
`f4b9493626b93b387175418dc31d6f9bb06fbb25`.

Independent first pass included source/diff reading, builds, Python checks,
key traversal, eight captures, both diagnostic regressions and restoration,
native-plugin test and CI checks before opening round 024's Builder report.
Pass two read that report and produced six supplemental full-shell captures.
No production or test files changed in the review checkout. Diagnostic changes
were confined to a disposable source copy; only this report is committed.

## Findings

No blocking implementation finding.

1. **[NOTE] `ui/qml/screens/DeckBuilderScreen.qml:603-606` — stale hint-fit
   source comment.** It still says about 80 pixels is enough for `">=1500, ?"`.
   The default-state full-shell 1280x800 capture elides ATK/DEF to `">=1500..."`.
   Section 15.1 now correctly records that. Align the comment with that wording;
   this does not change editing or bounds. The supplemental enabled-Monster
   capture has slightly different column sizing and shows more of the hint;
   neither observation supports the unconditional source claim.
2. **[NOTE] `docs/architecture/deck-builder-ui.md:1125-1131` — native traversal
   limitation remains, accurately documented.** Independently reproduced Cocoa's
   minimum-forward missing-control assertion below. Its cause is undetermined.
   Offscreen success establishes this correction's scoped key-event behavior,
   not native physical input or full keyboard parity. No platform setting was
   changed and this round explicitly excludes navigation-policy changes.

## Verified

### Seat and environment

`git clone https://github.com/cntrl-alt-lenny/edopro-next.git <fresh-checkout>`:
exit 0. First project command:

```sh
python3 tools/fw.py start --role verifier --round 024-scroll-focus-visibility --review origin/brain/024-scroll-focus-visibility
```

Exit 0:

```text
seat ok: verifier, round 024-scroll-focus-visibility, branch verifier/024-scroll-focus-visibility-2 at 6b9c661ccc51
  brief: docs/rounds/024-scroll-focus-visibility/brief.md
  reviewing exactly 6b9c661ccc5107e2bdd96124e95ac05f9623a5d0 from origin/brain/024-scroll-focus-visibility
```

Immediately next, `git submodule update --init`, exit 0:
`Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'`.
`git submodule status`, exit 0:

```text
 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
```

Read AGENTS, framework, Verifier card, round brief and inherited round 022 brief.
`sw_vers`, exit 0: macOS 27.0, build 26A428.
`clang++ --version`, exit 0: Apple clang 21.0.0 (clang-2100.3.34.2),
arm64-apple-darwin27.0.0. `qtpaths --qt-version`, exit 0: 6.11.1;
CI pins 6.8.3. `python3 --version`, exit 0: 3.9.6 for seat commands.
Workspace dependency Python `--version`, exit 0: 3.12.14; called `PY312`
below. All suite/generator/golden commands use that supported interpreter,
without retrying the known Python 3.9 suite failure.

### Required builds and checks at the reviewed SHA

For S=ui, data and policy, ran each command in sequence:

```sh
cmake -S <S> -B <S>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON
cmake --build <S>/build --parallel
ctest --test-dir <S>/build --output-on-failure
```

UI configure additionally used `-DEDOPRO_NEXT_UI_TESTS=ON`. All nine commands
returned 0. UI fresh build: `[104/104] Linking CXX executable tests/test_deckbuilder_screen`.
Configure/build logs include QTP0004, deprecated SQLite imported-target notices,
optional Vulkan-header discovery messages and duplicate-library linker warnings;
warning-free logs are not claimed.

```text
UI:
1/2 Test #1: deckbuilder ...................... Passed 0.46 sec
2/2 Test #2: deckbuilder_screen ............... Passed 8.56 sec
100% tests passed out of 2
Total Test time (real) = 9.02 sec
Data:
1/4 Test #1: card_database .................... Passed 0.37 sec
2/4 Test #2: deck_ydk ......................... Passed 0.14 sec
3/4 Test #3: card_search ...................... Passed 0.30 sec
4/4 Test #4: numeric_filter_text .............. Passed 0.21 sec
100% tests passed out of 4
Total Test time (real) = 1.03 sec
Policy:
1/4 Test #1: lf_list .......................... Passed 0.17 sec
2/4 Test #2: deck_validation .................. Passed 0.33 sec
3/4 Test #3: deck_placement ................... Passed 0.16 sec
4/4 Test #4: deck_search_filter ............... Passed 0.31 sec
100% tests passed out of 4
Total Test time (real) = 0.98 sec
```

The local offscreen shell smoke used Python `subprocess.run` with environment
`QT_QPA_PLATFORM=offscreen`, `capture_output=True`, `timeout=20`. TimeoutExpired
kills/waits for the child; this adapts Linux's `timeout 20` survival assertion,
not a literal process exit 124. Collecting wrapper exit 0, separate outcomes:

```text
SURVIVAL PASS timeout at 20s (Python adaptation; process killed)
EMPTY STDERR False
qt.qpa.fonts: Populating font family aliases took 57 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost.
```

The diagnostic had a trailing space and newline. There were no QML diagnostics.
The local empty-stderr assertion FAILS; it is not renamed a pass. Linux CI's
strict survival and empty-stderr assertions passed at the exact reviewed SHA.

Each following command returned 0:

| Command | Actual output |
|---|---|
| `"$PY312" tools/generate_messages.py --check` | `message table up to date (96 ids)` |
| `"$PY312" tools/generate_protocol_constants.py --check` | `protocol constants up to date (187 values)` |
| `"$PY312" tools/generate_readme_status.py --check` | `README status block is up to date` |
| `"$PY312" -m unittest discover -s tests -v` | `Ran 133 tests in 12.049s`, `OK (skipped=11)` |
| `"$PY312" tests/test_replay_trace.py --update` | wrote `duel-chains-battle-yrpX.trace`, `duel-extended-yrpX.trace`, `duel-chains-battle-yrp.trace` |
| `git diff --exit-code -- tests/golden` | empty |
| `python3 tools/fw.py check` | `0 error(s), 0 warning(s)`; supported-Python rerun also recorded below |
| `git diff --check` | empty |

Each suite skip is named below. The ACL test requires Windows; the other ten
require a fresh client semantic-trace binary, which this checkout did not build.
No push-guard skip occurred. Structural replay reproduction is not evidence of
unchanged duel behavior.

- test_unreadable_source_tree_fails_closed (test_semantic_trace.TestBinaryFreshness.test_unreadable_source_tree_fails_closed) ... skipped 'Windows ACL denial is required for this enumeration test'

- test_fixtures_exist (test_semantic_trace.TestSemanticGoldens.test_fixtures_exist) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_rendering_is_deterministic (test_semantic_trace.TestSemanticGoldens.test_rendering_is_deterministic) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_traces_match_golden (test_semantic_trace.TestSemanticGoldens.test_traces_match_golden) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_committed_fixtures_are_semantically_complete (test_semantic_trace.TestSemanticQuality.test_committed_fixtures_are_semantically_complete) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_coverage_accounts_for_every_packet (test_semantic_trace.TestSemanticQuality.test_coverage_accounts_for_every_packet) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_model_invariants_hold_at_the_end (test_semantic_trace.TestSemanticQuality.test_model_invariants_hold_at_the_end) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_no_environmental_leakage (test_semantic_trace.TestSemanticQuality.test_no_environmental_leakage) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_no_packet_is_malformed_or_unknown (test_semantic_trace.TestSemanticQuality.test_no_packet_is_malformed_or_unknown) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_query_stream_coverage_is_real_and_clean (test_semantic_trace.TestSemanticQuality.test_query_stream_coverage_is_real_and_clean) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

- test_something_is_actually_decoded (test_semantic_trace.TestSemanticQuality.test_something_is_actually_decoded) ... skipped 'no semantic-trace binary is present and newer than every client source file; configure and build client/, or set EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary'

### Actual traversal, bounds and operation

```sh
QT_QPA_PLATFORM=offscreen EDOPRO_FOCUS_CAPTURES=<captures> ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible
```

Exit 0: `Totals: 6 passed, 0 failed, 0 skipped, 0 blacklisted, 7221ms`.
All four rows (minimum-forward, minimum-reverse, default-forward, default-reverse)
logged `main reached 15 of 15`, `categories reached 33 of 33`,
`markers reached 9 of 9`, `lower category Space changed model selection`, and
both popups' `Done/Space close, Space reopen, Tab visible, Escape close`.

Source inspection confirms actual events:
`QTest::keyClick(window, Qt::Key_Tab, reverse ? Qt::ShiftModifier : Qt::NoModifier)`.
Only starting Search, popup triggers, and starting Done are explicitly focused;
individual traversal destinations are not. Monster is enabled by its actual
combo's activated signal, not a parallel model. Main expected set:
searchField, filtersToggle, clearFiltersButton, cardTypeCombo, subTypeCombo,
attributeCombo, raceCombo, attackField, defenseField, levelField, scaleField,
limitationCombo, nonOfficialCheck, categoriesButton, linkMarkersButton.
Each is asserted enabled. Effects expects categoryCheck0..31 and categoriesDone;
markers expects markerButton0..7 and markersDone; the disabled center is excluded.
The sets must equal the reached sets after at most 100 key events. Every reached
expected control is checked after layout settles against the named surface
intersected with every clipping ancestor, on both axes, with 0.5-pixel tolerance.
This is stricter than testing the ScrollView's outer frame alone.

Actual bounds examples (all four category rows agree):

```text
categories Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
categories Shift+Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
categories Tab categoryCheck12 bounds 28,304 196x40 viewport 28,80 396x264
categories Shift+Tab categoryCheck12 bounds 28,80 196x40 viewport 28,80 396x264
main Tab linkMarkersButton bounds 142,320 130x40 viewport 16,148 266x212
```

The main example is minimum-forward. Category 32 ends exactly at viewport y=344;
reverse traversal also restores upper categories to the viewport. Space changes
both checked state and `categorySelected(31)`; this is model operation evidence.
Done is reached again by Tab, Space closes, Space on a seeded trigger reopens,
next Tab reaches a visible popup control, and Escape closes with no popup control
active. Markers get the same lifecycle and every-marker visibility checks;
individual marker toggles are covered by existing adapter/screen tests, rather
than claimed as new exhaustive physical operation evidence.

Surface inventory: main filter grid and Effects grid both use FocusScrollView;
Link-marker popup has fixed contents and no scrollable surface. Search results
are output, not filters. Qt-owned ComboBox option popups are separate: installed
Basic/ComboBox.qml:100-105 reads `clip: true`,
`currentIndex: control.highlightedIndex`, `highlightMoveDuration: 0`.
Highlight-following is source-only inference here; not exhaustive option traversal.
The correction maps focused descendants into content, clamps vertical contentY,
and rechecks focus/content/view/control geometry after layout. It implements
presentation behavior, not matching or any game rule.

### Failing controls and restoration

A disposable copy of ui/data/policy excluded build directories. Only its QML was
mutated for these checks. Configure/build returned 0. The reviewed checkout
remained at its original SHA with no production changes.

1. Replace scratch DeckBuilderScreen.qml with the literal result of
   `git show f4b9493626b93b387175418dc31d6f9bb06fbb25:ui/qml/screens/DeckBuilderScreen.qml`.
   Rebuild target test_deckbuilder_screen, exit 0; run
   `QT_QPA_PLATFORM=offscreen <scratch>/ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible`, exit 4:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) 'isVisible(active, clippingSurface)' returned FALSE. (categoryCheck9 bounds 250,260 175x40 viewport 28,80 396x264)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) 'isVisible(active, clippingSurface)' returned FALSE. (categoryCheck31 bounds 250,755 218x40 viewport 28,80 396x264)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-forward) 'isVisible(active, clippingSurface)' returned FALSE. (categoryCheck9 bounds 250,260 175x40 viewport 28,80 396x264)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse) 'isVisible(active, clippingSurface)' returned FALSE. (categoryCheck31 bounds 250,755 218x40 viewport 28,80 396x264)
Totals: 2 passed, 4 failed, 0 skipped, 0 blacklisted, 64900ms
```

Each failing row additionally printed the guard failure
`'!QTest::currentTestFailed()' returned FALSE. ()`. Forward fails earlier on
horizontal clipping (right edge 425 versus viewport 424); reverse directly
reproduces the original invisible Category 32, vertically and horizontally.
The test's runtime naming of old unnamed Done permits accounting without altering
original QML behavior. This is a test of old production QML at the final test SHA,
not reliance on round 023's earlier review.

2. Restore delivered QML, then insert exactly
   `return; // verifier diagnostic disables visibility` at the beginning of
   shared `function revealFocused(item)`. Rebuild exit 0; run
   `QT_QPA_PLATFORM=offscreen <scratch>/ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:minimum-forward`, exit 1:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) 'isVisible(active, clippingSurface)' returned FALSE. (attackField bounds 142,324 130x40 viewport 16,148 266x212)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) '!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 16031ms
```

3. Copy the reviewed QML back, `diff -q` against reviewed source exit 0 with
   no output; rebuild exit 0; rerun all four rows, exit 0:
   `Totals: 6 passed, 0 failed, 0 skipped, 0 blacklisted, 7423ms`.
   Reached sets again logged 15/15, 33/33, 9/9 for every row. No mutation committed.

The missing-control failure mechanism was also independently exercised by:

```sh
QT_QPA_PLATFORM=cocoa ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:minimum-forward
```

Exit 1:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) 'reached == expected' returned FALSE. (main missing: attributeCombo, cardTypeCombo, categoriesButton, clearFiltersButton, filtersToggle, limitationCombo, linkMarkersButton, nonOfficialCheck, raceCombo, subTypeCombo)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) '!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 3701ms
```

This real native-plugin event test proves a loop missing controls fails independently
of clipping. It reaches Search and four numeric fields; it stops before popup tests.
It is not a physical-keyboard test or a universal conclusion about macOS.

### Captures inspected independently

A disposable synthetic database has eleven datas columns and texts id/name/desc
plus str1..16. Forty Effect Monsters have ids 1..40, scope 3, attack 1000+id,
defense 1000, level 4, race/attribute/category 1, names Synthetic card 01..40.
No real database, image, artwork or script was committed.

For each (w,h)=(960,600),(1280,800), normal full-shell capture command, exit 0:

```sh
QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell --card-db <synthetic.cdb> --start-screen decks --capture <captures>/shell-<w>.png --capture-width <w> --capture-height <h>
```

`sips -g pixelWidth -g pixelHeight <captures>/*.png`, exit 0, measured shell
960x600 and 1280x800. The targeted key-event capture command above additionally
produced main-lower, effects-lower and markers images at each harness screen area,
896x600 and 1064x800, measured with sips. Those six screen-only images omit the nav
rail and have a different window palette. They show actual reached destinations,
not forced-focus substitutes for traversal. I viewed all eight images.

After pass two, supplemental six full-shell images were made from the restored
scratch source. Production QML compares byte-for-byte equal (`diff -q`, exit 0).
Only scratch main.cpp's capture driver was instrumented: at 500ms select Monster
via activated(1); main state explicitly focuses Link markers; Effects opens the
popup and 300ms later explicitly focuses Category 32; markers opens its popup.
Capture waits 1800ms. These are visual-state setup, not traversal evidence.
Scratch shell build exit 0. For all six combinations:

```sh
QT_QPA_PLATFORM=offscreen EDOPRO_CAPTURE_STATE=<main|effects|markers> <scratch>/ui/build/edopro_next_shell --card-db <synthetic.cdb> --start-screen decks --capture <captures>/full-<w>-<state>.png --capture-width <w> --capture-height <h>
```

All returned 0; sips measured three 960x600 and three 1280x800 images. I viewed
all six myself. Controls remain inside their columns. At minimum the scrolled
main grid exposes Level/Scale/Limit and both popup buttons; two complete result
rows plus part of a third remain. Default size shows several complete results.
Both full-shell Effects popups show complete Categories 23..32 and Done within
bounds, including the lower target; partially visible unfocused rows at the top
are ordinary scroll clipping. Both marker popups show every button and Done.
Popup overlays intentionally cover underlying panes. Default ATK/DEF hints and
Ruleset/Banlist text can elide/truncate inside their controls; no column overlap.
The screenshots establish presentation only, not native input or duel behavior.

### Combined semantic review and primary-source rereading

The combined data/parser, policy lookup/search, adapter/query wiring, QML and tests
were read at this SHA. Round 024 changes only QML, its screen test, architecture
text and round documents; the semantic implementation is inherited. data/policy
remain Qt-free; QML holds choices and renders model outputs. The moved limitation
lookup retains exact code-first/conditional-alias ordering. No new dependency or
engine patch. No diff in client, gframe, integration, ocgcore, workflows, tools,
framework or state. Historical reports are unchanged; milestone remains open.

Re-read authoritative preserved source, not earlier reports as a substitute:

- `gframe/deck_con.cpp:1192`: `data._data.type & TYPE_TOKEN || data._data.ot & SCOPE_HIDDEN`
  and the nonofficial/whitelist clause; `:1196` rejects
  `(data._data.type & filter_type2) != filter_type2`; `:1233`, `:1240` reject
  `filter_type2 && data._data.type != filter_type2`. Monster subset versus
  Spell/Trap exact type is preserved.
- `:1204-1205` rejects `(data._data.attack > filter_atk || data._data.attack < 0)`
  for at-most and `filter_atktype == 6 && data._data.attack != -2` for question mark.
  DEF analogues and Link exclusion are at `:1210-1212`. Level/Scale question mark
  rejects every card; Scale compares `data._data.lscale` and requires Pendulum
  (`:1216-1225`). Header `gframe/deck_con.h:115-122` declares
  `DECLARE_WITH_CACHE(int32_t, filter_atk)` and
  `DECLARE_WITH_CACHE(uint32_t, filter_lv)` / filter_scl.
- `gframe/bufferio.h:240-249`: `uint32_t ret = 0;`,
  `ret = ret * 10 + (*pstr - L'0');`, `if(*pstr == 0) return ret; return 0;`.
  The parser's wrapping/malformed-suffix behavior and strict comparison mapping
  agree. `gframe/deck_con.cpp:21-51` sets types 1..6 and otherwise type 0.
- `gframe/data_manager.cpp:146-153`: `cd.level = -(level & 0xff);`,
  `cd.lscale = (level >> 24) & 0xff;`, `cd.rscale = (level >> 16) & 0xff;`.
  Numeric tests transcribe that decode and distinguish left/right wiring.
- `gframe/deck_con.cpp:1250-1253`: `!(data._data.category & filter_effect)`
  versus `(data._data.link_marker & filter_marks) != filter_marks`.
  Any category/all selected markers remain distinct.
- `gframe/deck_manager.h:23-30`: `auto flit = content.find(pcard->code);`,
  `if(flit == content.end() && pcard->alias)`,
  `if(!whitelist || pcard->IsInArtworkOffsetRange()) flit = content.find(pcard->alias);`.
  `gframe/data_manager.h:81-85` uses the same two unsigned differences for artwork
  range. `deck_con.cpp:1254-1322` distinguishes exact scope and bit scope filters,
  starts count at 3, uses -1 for absent whitelist entries, and rejects
  `filterList->whitelist && count < 0`. Re-read Game::ReloadCBLimit at
  `gframe/game.cpp:3412-3442` for choices/disabled switch.
- Corrected ADR timing: `gframe/deck_con.cpp:338-341` Search calls `StartFilter();`;
  `:346-354` category OK ends with `HideElement(mainGame->wCategories); break;`
  and no StartFilter. `:446-466` markers OK calls `StartFilter(true);`.
  `:471-481` Enter includes all five fields. `:486-501` calls
  `StartFilterIfLongerThan(2);` for ATK/DEF/name and `StartFilter();` for Level/Scale.
  Combo/list/switch handlers at `:512-617` rerun filters. Clear at `:1363-1397`
  includes `results.clear();` and leaves nonofficial alone. These support the ADR's
  deliberate live-update/Clear divergences, not unchanged duel behavior.
- Re-read option tables in `gframe/game.cpp:3350-3462`, race constants in
  `ocgcore/ocgapi_constants.h:71-103`, and StartFilter `:1040-1058`.
  Generic category labels and the higher-race/resource limitation are documented.

### Documentation and expectation corrections

Against the round 023 production source, the only replaced documentation sentences
are in deck-builder-ui.md; line wrapping normalized here:

1. Removed: "Every filter control takes focus with Tab (`focusPolicy` includes
   `Qt.TabFocus`, pinned by `filterControlsReachTheSearchModel`)."
   Replacement: "Every filter control declares Tab focus capability (`focusPolicy`
   includes `Qt.TabFocus`, pinned by `filterControlsReachTheSearchModel`)."
   Added: "Capability alone does not establish traversal or visibility." and
   "Round 024's real-screen key-event coverage and native platform limits are
   described in §15.2." Full-parity-open sentence remains.
2. Removed: "At 300 pixels four columns leave each control about 80 pixels, enough
   for the ">=1500, ?" hint (measured in the 1280x800 capture, where the column is
   about 303 pixels and four columns are used)."
   Replacement: "At 300 pixels four columns leave each numeric control about
   80 pixels; the ATK/DEF hints are elided in the full-shell 1280x800 capture."
   Added: "The fields remain editable and inside their column."
   The two-column width sentence is unchanged.
3. Removed: "A `Flickable` does not follow keyboard focus, so when Tab moves focus
   to a filter control below or above the visible rows, the screen scrolls it into view."
   Replacement: "The main filter grid follows focus above or below the visible rows."
   Added: "Round 023's handler did not cover the separate Effects scroll area;
   round 024 replaces it with the shared mechanism described in §15.2."
4. Removed: "Every control keeps its Tab focus."
   Replacement: "Every control keeps its Tab focus capability."

Section 15.2 is new. No state sentence changed. Inherited combined documentation
changes were also inspected against master; they introduce filters, supersede
prior token visibility and numeric/type simplification statements, preserve all
remaining milestone gaps, and record divergences. These inherited replacements
were not altered by round 024.

No existing assertion/expectation was relaxed by round 024. Its only removed test
code is the item helper's direct return, replaced with QObject lookup plus actual
window visual-tree fallback; its removed comment "Keyboard: every filter control
takes focus by Tab (Qt::TabFocus bit)." becomes "Focus capability only; actual
traversal is checked separately below." Existing column bounds, multiple-result
row check, policy/semantics and initial forced-focus tests remain. Forced-focus
checks are not counted as actual traversal evidence.

### Pass two and CI at the literal delivery

Builder and independent results agree on offscreen reached sets, Category 32
bounds/operation, original-QML reverse failure, disabled-visibility failure,
restored passing result, Cocoa missing controls and font diagnostic. My original
QML run additionally checks every size/direction and catches earlier horizontal
clipping in forward traversal. I repeated standalone data/policy cycles as assigned.
I did not treat the Builder's older implementation SHA or historical round 022/023
reviews as correctness at this delivered SHA. No material contradictory claim found.

`gh api repos/cntrl-alt-lenny/edopro-next/commits/6b9c661ccc5107e2bdd96124e95ac05f9623a5d0/check-runs`,
exit 0. All five deterministic checks completed success in both runs:
Regression harness (3.10), Regression harness (3.12), Semantic client model,
Card and deck data, Qt 6 shell (Linux).
[Push run](https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37150160654)
skips upstream-baseline; [PR run](https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37150163552)
additionally has success for observer=false and observer=true baseline jobs.
Those are CI observations, not local engine-equivalence evidence.
Verifier report-commit CI is unknown until observed after push.

### Acceptance criteria

1. Met by measured full-shell captures at both exact sizes, plus inspected scrolled
   main/Effects/markers states, existing column/result-row test and harness captures.
2. Met for scoped offscreen tests: every expected enabled main control and every
   category reached by actual forward/reverse keys at both sizes, strict intersected
   clipping bounds and complete accounting. Native limitation remains explicit.
3. Met for scoped offscreen operation: lower category Space changes model, both
   popups close/reopen and restore visible key focus; markers all reached.
   Inventory and source-only ComboBox inference stated separately.
4. Met: original QML fails all four rows; disabling shared revelation fails;
   restored QML matches reviewed source and all rows pass.
5. Met: architecture accurately distinguishes capability/traversal/native scope,
   fixes elided hint wording, introduces no parity or duel-equivalence claim.
   Stale QML comment is the nonblocking note above.
6. Required local checks passed except the explicitly platform-specific strict
   empty-stderr assertion. Diagnostic is literal, separately reported; required
   Linux CI checks passed at the delivered commit. Supported Python used for suite.

## Not verified

- Physical native keyboard/mouse traversal, native popup-category operation on this
  final SHA, native default-size/reverse traversal, Windows navigation, controllers,
  or the cause of Cocoa skipping non-text controls. Cocoa minimum-forward failed
  and stopped before its popup test. No OS keyboard preference changed.
- Local empty stderr; it fails on the quoted platform diagnostic. No local Linux
  Qt 6.8.3 run; its strict smoke evidence is CI only.
- Exhaustive Qt ComboBox option navigation, arbitrary fonts/DPI/translated labels,
  every window size, live-resize focused geometry behavior, or visual comparison
  against upstream GUI. Captures are offscreen; harness and diagnostic forced
  capture focus are identified separately from key-event evidence.
- A new client semantic binary, the ten skipped semantic tests and Windows ACL
  skip. No local upstream baseline/observer equivalence or engine-loaded duel
  test. Structural golden checks do not establish unchanged duel behavior.
- New benchmark before/after comparison, inherited search-performance mutation
  controls, or repetition of prior round's numeric-wiring mutations. This round's
  required data/policy/UI tests were rerun at the final commit and source wiring
  reread; round 024 does not change matching or the search path.
- Report-commit CI before it exists. No review of a later-moving delivery branch.

## Verdict

No blocking defect found in this correction at the exact reviewed SHA. The prior
Effects focus-visibility failure is corrected across both scrollable filter
surfaces, and the regression demonstrably detects old QML, disabled revelation
and missed controls. High confidence in the scoped offscreen behavior, supported
by both-size visuals, forward/reverse key events, popup operation, standalone
module checks and exact-SHA CI. Native parity remains unestablished and the local
strict stderr assertion fails as documented; neither is silently rounded up.
This is evidence for Brain's adjudication, not acceptance or merge authority.

<!-- fw-report
round: 024-scroll-focus-visibility
role: builder
branch: brain/024-scroll-focus-visibility
head: 762e938db016d30105b281d63c3ccc57a0a20147
os: macOS 27.0
python: 3.9.6
written: 2026-10-03T20:02:18Z
-->
Implementation/evidence commit: `762e938db016d30105b281d63c3ccc57a0a20147`.
Builder delivery only; no acceptance or merge. Draft PR: https://github.com/cntrl-alt-lenny/edopro-next/pull/42.
The branch name retained by `fw.py start` is `brain/024-scroll-focus-visibility`;
the seat is Builder, not Brain. Historical rounds 022/023 reports are untouched.

## Verified

### Start and environment

`python3 tools/fw.py start --role builder --round 024-scroll-focus-visibility` — exit 0:

```text
seat ok: builder, round 024-scroll-focus-visibility, branch brain/024-scroll-focus-visibility at 5273e32e4eba
  (branch name chosen by your tool rather than builder/024-scroll-focus-visibility; that is fine -- your report records it)
  brief: docs/rounds/024-scroll-focus-visibility/brief.md
  finish with: write docs/rounds/024-scroll-focus-visibility/builder.md, then python3 tools/fw.py report --role builder --round 024-scroll-focus-visibility --push
```

Immediately next, `git submodule update --init` — exit 0, no output.
`git submodule status` — exit 0:

```text
 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
```

Environment commands, each exit 0: `sw_vers` → macOS 27.0, build 26A428;
`clang++ --version` → Apple clang 21.0.0 (clang-2100.3.34.2), target
arm64-apple-darwin27.0.0; `qtpaths --qt-version` → 6.11.1;
`cmake --version` → 4.4.3. Local Qt differs from CI's pinned 6.8.3.
`python3 --version` initially returned 3.9.6. Workspace dependencies located an
available Python 3.12.14 (`--version`, exit 0). Below `PY312` denotes that
executable, with no personal path; the complete Python suite and generator/golden
commands used it. No known system-Python 3.9 suite failure was retried as product evidence.

### UI cycle and restored regression

Commands at the stated implementation/evidence commit:

- `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON` — exit 0, `-- Generating done (0.1s)`, build files written to `ui/build`.
- `cmake --build ui/build --parallel` — exit 0, `[30/30] Linking CXX executable edopro_next_shell` on the final configured tree. The initial complete build also returned 0, `[104/104] Linking CXX executable tests/test_deckbuilder_screen`.
- `ctest --test-dir ui/build --output-on-failure` — exit 0:

```text
    Start 1: deckbuilder
1/2 Test #1: deckbuilder ......................   Passed    0.41 sec
    Start 2: deckbuilder_screen
2/2 Test #2: deckbuilder_screen ...............   Passed    8.61 sec

100% tests passed out of 2

Total Test time (real) =   9.03 sec
```

CMake emitted QTP0004, deprecated SQLite imported-target notices and an optional
Vulkan-header discovery message. Link steps emitted duplicate-library warnings.
These were not build failures; warning-free logs are not claimed.

`QT_QPA_PLATFORM=offscreen ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible`
— exit 0 after restoring both diagnostic mutations:

```text
PASS   : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse)
PASS   : TestDeckBuilderScreen::cleanupTestCase()
Totals: 6 passed, 0 failed, 0 skipped, 0 blacklisted, 7303ms
```

The four data rows are minimum-forward, minimum-reverse, default-forward and
default-reverse. Literal reached-control accounting and lower-category bounds:

```text
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) main reached 15 of 15
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) categories Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) categories reached 33 of 33
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) markers reached 9 of 9
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) main reached 15 of 15
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) categories Shift+Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) categories reached 33 of 33
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) markers reached 9 of 9
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-forward) main reached 15 of 15
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-forward) categories Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-forward) categories reached 33 of 33
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-forward) markers reached 9 of 9
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse) main reached 15 of 15
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse) categories Shift+Tab categoryCheck31 bounds 229,300 195x44 viewport 28,80 396x264
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse) categories reached 33 of 33
QINFO  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(default-reverse) markers reached 9 of 9
```

Actual events are `QTest::keyClick(window, Qt::Key_Tab, reverse ? Qt::ShiftModifier : Qt::NoModifier)`.
The main expected set is all 15 `kSearchColumnControls`, asserted enabled after
selecting Monster: searchField, filtersToggle, clearFiltersButton, cardTypeCombo,
subTypeCombo, attributeCombo, raceCombo, attackField, defenseField, levelField,
scaleField, limitationCombo, nonOfficialCheck, categoriesButton, linkMarkersButton.
The Effects set is categoryCheck0..31 and categoriesDone; the marker set is
markerButton0..7 and markersDone (the disabled centre is excluded).
Each reached named control is checked against the surface rectangle intersected
with every clipping ancestor. Missing any expected item after the bounded event
loop fails separately. The search field and each popup's Done are only traversal
seeds; destinations are not individually forced. Popup triggers are initially
focused explicitly and opened with Space. Lower-category operation and Done
closing are reached again with Tab. Each size/direction logged:

```text
lower category Space changed model selection
categories Done/Space close, Space reopen, Tab visible, Escape close
markers Done/Space close, Space reopen, Tab visible, Escape close
```

Space on Category 32 changes both its checked state and `categorySelected(31)`.
Done is reached with keys and closed with Space; the same trigger reopens with
Space, automatic restored named-control focus (if any) and the next Tab focus
must be visible; Escape closes and leaves no popup control active.

### Regression sensitivity

Saved the original QML before editing. `git show f4b9493626b93b387175418dc31d6f9bb06fbb25:ui/qml/screens/DeckBuilderScreen.qml`
produced the round-023 source; `cmp` against the saved original — exit 0, no output.
For the original-QML probe, copied that byte-identical QML into the build input,
rebuilt `test_deckbuilder_screen` (exit 0), and ran:

`QT_QPA_PLATFORM=offscreen ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:minimum-reverse`
— exit 1:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) 'isVisible(active, clippingSurface)' returned FALSE. (categoryCheck31 bounds 250,755 218x40 viewport 28,80 396x264)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-reverse) '!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 16531ms
```

The test can label the old popup's unnamed Done in the test's runtime object tree
for accounting; it does not modify the original production QML or its behavior.

For the disabled-visibility probe, inserted this sole diagnostic statement at the
start of the shared `revealFocused(item)`:

```qml
return; // diagnostic mutation: disable all focus visibility
```

Rebuilt `test_deckbuilder_screen` (exit 0), then ran
`QT_QPA_PLATFORM=offscreen ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:minimum-forward`
— exit 1:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) 'isVisible(active, clippingSurface)' returned FALSE. (attackField bounds 142,324 130x40 viewport 16,148 266x212)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) '!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 16326ms
```

Restored the saved fixed QML, `cmp` — exit 0, and rebuilt the normal UI.
The restored passing targeted command and complete CTest output are above.
No diagnostic mutation is committed. During development the new strict test also
found horizontal category clipping and reverse-traversal geometry settling after
label wrapping; both are addressed in the shared view/category cell sizing. A
test fixture lookup was corrected to find Repeater delegates through the real
window's visual tree as well as QObject ownership; no assertion was weakened.

### Offscreen smoke and native-plugin diagnostic

The workflow's two assertions were run separately through a Python subprocess
wrapper: `QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell`, `timeout=20`.
A `TimeoutExpired` kills/waits for the child and maps survival to status 124,
matching Linux `timeout`; this is the macOS adaptation, not a literal shell exit
124. The wrapper reports the outcomes independently:

```text
survival assertion: 124 PASS
empty-stderr assertion: FAIL
qt.qpa.fonts: Populating font family aliases took 54 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost. 

```

The collecting wrapper exits 0; that does not make the empty-stderr assertion
pass. This host's platform font diagnostic is preserved literally and is separate
from CI's strict Linux survival/empty-stderr assertion. No QML, TypeError or
ReferenceError appeared in this launch.

`QT_QPA_PLATFORM=cocoa ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:minimum-forward`
— exit 1. Search and ATK/DEF/Level/Scale were reached; literal failure:

```text
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) 'reached == expected' returned FALSE. (main missing: attributeCombo, cardTypeCombo, categoriesButton, clearFiltersButton, filtersToggle, limitationCombo, linkMarkersButton, nonOfficialCheck, raceCombo, subTypeCombo)
FAIL!  : TestDeckBuilderScreen::filterTabTraversalStaysVisible(minimum-forward) '!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 4060ms
```

This programmatic native-plugin result demonstrates that the accounting assertion
fails when traversal misses controls. It supports the earlier Verifier's attributed
native limitation without establishing its cause, testing physical keyboard input,
or drawing a universal macOS conclusion. No keyboard preferences were changed.

### Captures inspected

All databases and captures are disposable, uncommitted and synthetic. The default
full-shell database has the eleven `datas` columns and `texts.id/name/desc/str1..16`,
40 synthetic Effect Monsters with ids 1..40, scope 3, ATK 1500, DEF 1000, Level 4,
race 1, attribute 1, category 1, named `Synthetic card 01`..`40`.
The committed test builds its own 40 synthetic Monster cards using the existing
fixture helper; it does not require this disposable database.

For (w,h)=(960,600),(1280,800):

```sh
QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell --card-db /tmp/edopro-024/synthetic.cdb   --start-screen decks --capture /tmp/edopro-024/shell-<w>.png   --capture-width <w> --capture-height <h>
sips -g pixelWidth -g pixelHeight /tmp/edopro-024/*.png
QT_QPA_PLATFORM=offscreen EDOPRO_FOCUS_CAPTURES=/tmp/edopro-024   ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible
```

Both normal capture commands returned 0. `sips` returned 0: shell-960 is 960x600,
shell-1280 is 1280x800. The test capture command returned 0, `Totals: 6 passed,
0 failed, 0 skipped`. Its six images (main-lower, effects-lower, markers for each
screen area) are 896x600 and 1064x800. These are screen-only harness images, not
full-shell captures: the harness has different window palette/nav composition.
They are saved at actual event destinations (Link markers, Category 32, marker Done).

Additionally built a disposable copy of `ui/`, `data/`, `policy/` with tests disabled
(configure and build both exit 0). Its production DeckBuilderScreen QML compares
byte-for-byte equal with the delivered QML (`cmp`, exit 0). Only its `main.cpp`
capture driver was instrumented: at 600 ms select Monster via `activated(1)`;
for main-lower focus Link markers; for effects-lower open Effects and after another
300 ms focus Category 32 via the window visual tree; for markers open the marker
popup; capture at 1800 ms. No shell/QML instrumentation is committed.
For each size and each state, command:

```sh
QT_QPA_PLATFORM=offscreen EDOPRO_CAPTURE_STATE=<state>   /tmp/edopro-024/scratch/ui/build/edopro_next_shell   --card-db /tmp/edopro-024/synthetic.cdb --start-screen decks   --capture /tmp/edopro-024/full-<w>-<state>.png --capture-width <w> --capture-height <h>
```

All six commands returned 0; `sips` measured three 960x600 and three 1280x800
full-shell images. Those supplemental images deliberately seed focus/open popups;
they are visual evidence, not traversal evidence.
I opened and inspected all fourteen images myself. Main controls stayed within
columns, the scrolled minimum-size filters show Level/Scale/Limit and both popup
buttons, and more than one result row remains visible (two complete rows and part
of a third at the minimum). Effects shows Categories 23..32 with wrapped labels
and Done inside its popup, including the complete lower focused category in the
key-event harness. Marker captures show all eight buttons and Done without clipping.
Popups intentionally overlay underlying panes. Default shell ATK/DEF hints are
elided in their columns; Ruleset/Banlist truncation remains inside the controls.
This is visual inspection of presentation, not native physical input or duel evidence.

Scrollable-surface inventory: main filter grid and Effects category grid use the
shared reveal mechanism, both covered by strict forward/reverse real-screen tests.
Markers have no scrollable contents and are covered by the same key-event/bounds
checks. The result ListView is search output, not a filter surface. ComboBox option
popups are Qt-owned ListViews: installed Basic/ComboBox.qml lines 100-105 show
`clip: true`, `currentIndex: control.highlightedIndex`, `highlightMoveDuration: 0`.
Their highlight-following is a source-only inference here; exhaustive traversal of
every drop-down option is not claimed or replaced by our ScrollView handler.

### Python and repository checks

The supported interpreter was used for every command below; exits are 0:

| Command | Actual relevant output |
|---|---|
| `"$PY312" tools/generate_messages.py --check` | `message table up to date (96 ids)` |
| `"$PY312" tools/generate_protocol_constants.py --check` | `protocol constants up to date (187 values)` |
| `"$PY312" tools/generate_readme_status.py --check` | `README status block is up to date` |
| `"$PY312" -m unittest discover -s tests -v` | `Ran 133 tests in 13.072s`, `OK (skipped=11)` |
| `"$PY312" tests/test_replay_trace.py --update` | wrote `duel-chains-battle-yrpX.trace`, `duel-extended-yrpX.trace`, `duel-chains-battle-yrp.trace` |
| `git diff --exit-code -- tests/golden` | no output |
| `"$PY312" tools/fw.py check` | `0 error(s), 0 warning(s)` |
| `git diff --check` | no output |
| `python3 tools/check_pr_evidence.py --file <disposable PR body>` | `PR body uses rerunnable evidence commands and contains no measured figures.` |

Each Python skip named (no push-guard skip):

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

The Windows ACL test needs Windows denial semantics; the other ten need a fresh
client semantic-trace executable, absent here. Structural replay reproduction is
not evidence of unchanged duel behavior. Python checks ran on this round's final
code content before the focused commits; the UI cycle/targeted regression were
also rerun after those commits at the literal implementation/evidence SHA.

### CI

`gh run view 37149744465 --json jobs,status,conclusion` — exit 0:
completed, conclusion success at `762e938db016d30105b281d63c3ccc57a0a20147`.
All five deterministic checks succeeded: Semantic client model, Qt 6 shell
(Linux), Regression harness (3.10), Regression harness (3.12), Card and deck data.
The push run skips upstream-baseline as designed. Run:
https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37149744465.
`gh run view 37149781007 --json jobs,status,conclusion` — exit 0:
completed, conclusion success at the same literal head; all five deterministic
checks and both upstream-baseline observer variants succeeded in the PR run.
Run: https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37149781007.
These CI results do not establish unchanged duel behavior.
Report-commit CI is unknown until observed after `fw.py report --push`; no result
from an older commit is substituted for it. This report will not be rewritten
merely to claim its own future checks already passed.

## Not verified

- Native physical keyboard/mouse traversal, native parity on all platforms, or the
  cause of Cocoa skipping non-text controls. The Cocoa programmatic probe fails;
  offscreen key events passing do not resolve that limitation.
- Local strict empty stderr: it fails on the quoted macOS font-alias diagnostic.
- Exhaustive keyboard traversal of every Qt-owned drop-down option; the supplied
  Qt source was inspected separately from main filter-control traversal evidence.
- No local standalone data/policy cycle was repeated: they are untouched by this
  correction. The final Verifier is explicitly assigned their WERROR/build/CTest
  cycles against the combined inherited delivery. CI's data/policy evidence is
  recorded only at its actual SHA.
- No engine/baseline/observer behavior equivalence or unchanged duel behavior is
  claimed; no engine/integration/legacy code was changed. No real-card database,
  artwork, scripts or native controller testing.
- Report-commit CI remains unknown in this document until observed externally.

## Changed

- `ui/qml/screens/DeckBuilderScreen.qml`: one inline FocusScrollView for both
  filter scroll areas, layout-aware reveal and bounded vertical offset; Effects
  cells wrap inside available width; stable Done object names for test accounting.
  No search choices, matching, numeric wiring, parsing or policy changed.
- `ui/tests/test_deckbuilder_screen.cpp`: add four size/direction rows for real
  key traversal, strict complete-control accounting, clipping bounds, lower-category
  operation and popup lifecycle. Visual-tree fallback finds overlay/Repeater items;
  an optional uncommitted capture directory provides diagnostic frames.
  Existing assertions/expectations are unchanged. Only the old test comment changed
  from "Keyboard: every filter control takes focus by Tab (Qt::TabFocus bit)." to
  "Focus capability only; actual traversal is checked separately below."
  No column, result-row, semantics or forced-focus expectation was relaxed.
- `docs/architecture/deck-builder-ui.md`: correct capability/hint wording and add
  §15.2 explaining coverage, visual evidence, the failure class and native limits.
- This round's Builder report. No earlier reports, framework files, state, or
  out-of-scope production modules changed.

Every removed documentation sentence and replacement (full sentences quoted,
line wrapping normalized; unchanged neighboring sentences are not removals):

1. Removed: "Every filter control takes focus with Tab (`focusPolicy` includes
   `Qt.TabFocus`, pinned by `filterControlsReachTheSearchModel`)."
   Replaced: "Every filter control declares Tab focus capability (`focusPolicy`
   includes `Qt.TabFocus`, pinned by `filterControlsReachTheSearchModel`)."
   Added: "Capability alone does not establish traversal or visibility." and
   "Round 024's real-screen key-event coverage and native platform limits are
   described in §15.2." The following full-parity-open sentence remains.
2. Removed: "At 300 pixels four columns leave each control about 80 pixels, enough
   for the ">=1500, ?" hint (measured in the 1280x800 capture, where the column is
   about 303 pixels and four columns are used)."
   Replaced: "At 300 pixels four columns leave each numeric control about 80 pixels;
   the ATK/DEF hints are elided in the full-shell 1280x800 capture." Added: "The
   fields remain editable and inside their column." The two-column width sentence
   remains unchanged.
3. Removed: "A `Flickable` does not follow keyboard focus, so when Tab moves focus
   to a filter control below or above the visible rows, the screen scrolls it into view."
   Replaced: "The main filter grid follows focus above or below the visible rows."
   Added: "Round 023's handler did not cover the separate Effects scroll area;
   round 024 replaces it with the shared mechanism described in §15.2."
4. Removed: "Every control keeps its Tab focus."
   Replaced: "Every control keeps its Tab focus capability."

§15.2 is wholly new text, including the attributed round-023 native observation
and this seat's separate programmatic Cocoa result. No earlier round report was
rewritten and `docs/state.md` was not changed.

## Open questions

The native non-text Tab-traversal cause remains unresolved, within the brief's
explicit platform-navigation non-goal. Independent verification and Brain
adjudication are next; this Builder neither accepts nor merges the delivery.

<!-- fw-report
round: 023-filter-layout-correction
role: builder
branch: builder/023-filter-layout-correction
head: 72d27c2218cf2dc7ea2218e2cc20a84e8a0c78eb
os: macOS 27.0
python: 3.9.6
written: 2026-09-28T16:03:18Z
-->
Builder report for round 023-filter-layout-correction. Code and records end at `72d27c22`;
`fw.py report` commits this file on top of it and changes no code. Work was done on macOS in a
linked worktree of the project (`.worktrees/builder-023`). Below, `<worktree>` stands for that
folder's absolute path and `<scratchpad>` for a session scratch folder outside the repository.

## Verified

### Seat start (required evidence 1)

- `python3 tools/fw.py start --role builder --round 023-filter-layout-correction` → exit 0
  ```
  seat ok: builder, round 023-filter-layout-correction, branch builder/023-filter-layout-correction at 92db3a892c06
    brief: docs/rounds/023-filter-layout-correction/brief.md
    finish with: write docs/rounds/023-filter-layout-correction/builder.md, then python3 tools/fw.py report --role builder --round 023-filter-layout-correction --push
  ```
- `git submodule update --init` (right after it) → exit 0
  ```
  Cloning into '<worktree>/ocgcore'...
  Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
  ```
- `git submodule status` → ` 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)`
- Environment, each from a command: `sw_vers` → `ProductName: macOS`, `ProductVersion: 27.0`,
  `BuildVersion: 26A428`; `uname -m` → `arm64`; `c++ --version` → `Apple clang version 21.0.0
  (clang-2100.3.34.2)`; `qtpaths6 --qt-version` → `6.11.1` (Homebrew; **CI pins Qt 6.8.3**);
  `cmake --version` → 4.4.3; `ninja --version` → 1.13.2; `sqlite3 --version` → 3.54.0;
  Python for the suite `/opt/homebrew/bin/python3.13 --version` → `Python 3.13.15`.

### The `ui/` cycle (required evidence 2), fresh `rm -rf ui/build`, tree of `a4f3d805`

`a4f3d805` and `72d27c22` differ only in one sentence of `docs/capabilities.md`, so this cycle
covers the final code.

```
$ cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
-- Build files have been written to: <worktree>/ui/build
cfg-exit=0
$ cmake --build ui/build
[104/104] Linking CXX executable tests/test_deckbuilder_screen
ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'
build-exit=0
$ ctest --test-dir ui/build --output-on-failure
1/2 Test #1: deckbuilder ......................   Passed    0.62 sec
2/2 Test #2: deckbuilder_screen ...............   Passed    2.24 sec
100% tests passed out of 2
ctest-exit=0
```
The `ld` warning is the pre-existing one earlier rounds report.

Offscreen clean-QML-load (CI's step; macOS has no `timeout`, so a 20 s `alarm` stands in):
`perl -e 'alarm 20; exec @ARGV' env QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`
→ status 142 (killed by the alarm, so still running at 20 s: CI's first assertion holds).
stderr, one line:
```
qt.qpa.fonts: Populating font family aliases took 154 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost.
```
`grep -ciE 'qml|TypeError|ReferenceError'` on that stderr → `0`. CI's second assertion (empty
stderr) would not pass verbatim on macOS because of this font notice, as rounds 019, 020 and
022 found; on Linux, CI's "Qt 6 shell (Linux)" job passed (below). The same launch with the
synthetic database, banlist and `--start-screen decks` for 10 s printed nothing but that notice.

### Captures (required evidence 3)

Synthetic database built outside the repository in `<scratchpad>/synth.cdb` with `sqlite3`: 30
cards named "Synthetic Card 1".."Synthetic Card 30" (Normal monsters, some Spells and Traps),
and a three-entry banlist `<scratchpad>/synth.lflist.conf`. Neither is committed
(`git status --short` empty).

Command, run once for each size (`w`,`h` = 960,600 and 1280,800), at the round's start
(before) and on the final QML (final):
```
QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell --card-db <scratchpad>/synth.cdb \
  --lflist <scratchpad>/synth.lflist.conf --start-screen decks \
  --capture <scratchpad>/<name>-<w>x<h>.png --capture-width <w> --capture-height <h>
```
→ exit 0 each. `sips -g pixelWidth -g pixelHeight` → `before-960x600.png` 960×600,
`before-1280x800.png` 1280×800, `final-960x600.png` 960×600, `final-1280x800.png` 1280×800.

What I looked at in each, by eye:
- **before-960x600** (round 022's QML): reproduces the Verifier's finding. The search box, Clear,
  the right-hand filter column (Sub-type, Type/race, DEF, Scale), the Limit list and "Link
  markers…" cross the divider into the deck column; Clear touches "Ruleset"; the results show one
  row ("Synthetic Card 1").
- **final-960x600**: every search-column control ends left of the divider. The filters are two
  columns (label, control) in a scroll area showing Type to ATK, with a vertical bar in its own
  gutter beside the controls. The results show three rows (Synthetic Card 1, 10, 11). Add to
  Main / Add to Side sit below, inside the column. The deck column and preview are unchanged in
  shape; Ruleset and Banlist boxes cut their text short ("Standar", "No ban") exactly as in the
  before capture - truncation inside the box, not overlap.
- **final-1280x800**: four filter columns, all controls inside the column, no scroll bar (the
  grid fits), the non-official check box label wraps onto a second line ("…)") instead of
  widening the grid, Effects… and Link markers… side by side, five result rows visible. The
  ATK/DEF hint text is elided to ">=1500…" in their boxes (80-pixel controls); ">=5" and
  "4, <=3" show whole.
- **before-1280x800**: four columns, no overlap (matches round 022's capture).

Not checked visually: the scrolled states (the lower filter rows at 960×600 after scrolling or
tabbing), the Effects and Link-marker popups at either size, any control in a non-default state,
a selected result, Qt 6.8.3 rendering, and any window size other than these two. The scrolled
state and keyboard reveal are checked by the screen test below, not by eye.

### The layout test, and that it can fail

`searchPaneFitsItsColumnAtMinimumAndDefaultSizes` (`ui/tests/test_deckbuilder_screen.cpp`) sizes
the harness window to the screen area 960×600 and 1280×800 leave beside the nav rail (896×600,
1064×800), and requires: the search column ends before the deck column; each of the 15 search
column controls lies within the column; `resultsList.indexAt(1, height-1) >= 1` (a second row
shows); focusing "Link markers…" and then the card-type box leaves each inside the scroll area's
viewport. `QT_QPA_PLATFORM=offscreen ui/build/tests/test_deckbuilder_screen
searchPaneFitsItsColumnAtMinimumAndDefaultSizes` → `Totals: 3 passed, 0 failed`.

Each run below changed only what is named, rebuilt, ran the one test, and restored the file
(`git status --short` showed only my uncommitted work before and after):
1. Round 022's `DeckBuilderScreen.qml` (`git show HEAD:…` at the brief commit) plus only the two
   `objectName`s `searchPane`/`deckPane` the test locates:
   ```
   FAIL!  : TestDeckBuilderScreen::searchPaneFitsItsColumnAtMinimumAndDefaultSizes() 'r.left() >= paneRect.left() - 0.5 && r.right() <= paneRect.right() + 0.5' returned FALSE. (960x600 searchField 16,44 314x40 in pane 16..293)
   ```
2. Filters not allowed to shrink, no floor for results
   (`Layout.minimumHeight: Math.min(filterGrid.implicitHeight, 96)` → `filterGrid.implicitHeight`;
   `Layout.minimumHeight: 160` → `0`):
   ```
   FAIL!  : TestDeckBuilderScreen::searchPaneFitsItsColumnAtMinimumAndDefaultSizes() 'bottomRow >= 1' returned FALSE. (960x600 bottom row -1)
   ```
3. Focus handling disabled (`filterScroll.revealFocused(root.Window.window.activeFocusItem);` →
   `{}`):
   ```
   FAIL!  : TestDeckBuilderScreen::searchPaneFitsItsColumnAtMinimumAndDefaultSizes() 'r.top() >= viewport.top() - 0.5 && r.bottom() <= viewport.bottom() + 0.5' returned FALSE. (960x600 linkMarkersButton at 592..632, viewport 148..360)
   ```

### Acceptance criterion 2: wrong wiring fails a `ui/` test (required evidence 4)

New test `numberBoxesReadTheFieldUpstreamReads` (`ui/tests/test_deckbuilder.cpp`). Each change
applied to `ui/src/deckbuilder/search_filters.cpp`, rebuilt, `ui/build/tests/test_deckbuilder`
run, then `git checkout -- ui/src/deckbuilder/search_filters.cpp`:

- Scale to the right scale:
  ```
  -        query.left_scale = data::parse_numeric_filter(state.scaleText.toStdString(), NumericFilterField::Scale);
  +        query.right_scale = data::parse_numeric_filter(state.scaleText.toStdString(), NumericFilterField::Scale);
  ```
  → test binary exit 1; ctest `50% tests passed, 1 tests failed out of 2`
  ```
  FAIL!  : TestDeckBuilder::numberBoxesReadTheFieldUpstreamReads() Compared lists have different sizes.
     Actual   (resultCodes(model)) size: 0
     Expected ((L{604})) size: 1
     Loc: [<worktree>/ui/tests/test_deckbuilder.cpp(1669)]
  ```
  (line 1669 is Scale "2" against the card with left scale 2, right scale 7).
- Level parsed as ATK:
  ```
  -        query.level = data::parse_numeric_filter(state.levelText.toStdString(), NumericFilterField::Level);
  +        query.level = data::parse_numeric_filter(state.levelText.toStdString(), NumericFilterField::Attack);
  ```
  → ctest `50% tests passed, 1 tests failed out of 2`
  ```
  FAIL!  : TestDeckBuilder::numberBoxesReadTheFieldUpstreamReads() Compared lists have different sizes.
     Actual   (resultCodes(model)) size: 0
     Expected ((L{603})) size: 1
     Loc: [<worktree>/ui/tests/test_deckbuilder.cpp(1661)]
  ```
  (line 1661 is Level "4294967041" against the card whose packed level -1 upstream stores as
  that unsigned value).

Upstream read for this test, at the file and line (`gframe/`): `deck_con.h:115-122` declares
`filter_atk`/`filter_def` `int32_t` and `filter_lv`/`filter_scl` `uint32_t`;
`deck_con.cpp:1222-1224` compares `data._data.lscale` only, and rejects
`!(data._data.type & TYPE_PENDULUM)`; `data_manager.cpp:146-153` reads `if(level < 0) cd.level =
-(level & 0xff);`, `cd.lscale = (level >> 24) & 0xff;`, `cd.rscale = (level >> 16) & 0xff;`.

### Python, generators, framework check (required evidence 5), at `72d27c22`

- `/opt/homebrew/bin/python3.13 tools/generate_messages.py --check` → exit 0, `message table up to
  date (96 ids)`
- `/opt/homebrew/bin/python3.13 tools/generate_protocol_constants.py --check` → exit 0, `protocol
  constants up to date (187 values)`
- `/opt/homebrew/bin/python3.13 tools/generate_readme_status.py --check` → exit 0 (run at
  `a4f3d805`; no README-feeding file changed after)
- `/opt/homebrew/bin/python3.13 -m unittest discover -s tests -v` → exit 0, `Ran 133 tests`,
  `OK (skipped=11)`. Skips: `test_semantic_trace.TestBinaryFreshness.test_unreadable_source_tree_fails_closed`
  ("Windows ACL denial is required for this enumeration test"), and ten needing a built,
  fresh `client/` semantic-trace binary: `TestSemanticGoldens.test_fixtures_exist`,
  `test_rendering_is_deterministic`, `test_traces_match_golden`,
  `TestSemanticQuality.test_committed_fixtures_are_semantically_complete`,
  `test_coverage_accounts_for_every_packet`, `test_model_invariants_hold_at_the_end`,
  `test_no_environmental_leakage`, `test_no_packet_is_malformed_or_unknown`,
  `test_query_stream_coverage_is_real_and_clean`, `test_something_is_actually_decoded`.
  At `a4f3d805` the same suite failed once: `test_readme_standard.LinkTest` found my new link to
  this report, which does not exist until this report is committed. `72d27c22` removes that
  link (see Changed); the suite is then clean.
- `python3 tools/fw.py check` → exit 0, `0 error(s), 0 warning(s)`.

### Record changes (required evidence 6): every removed sentence and its replacement

1. `docs/adr/0012-deck-builder-search-filters.md`, Decision 6. Removed: "Upstream runs it on
   Enter, on a combo box change, on the category and marker OK buttons, and on typing in ATK/DEF
   only once the text is longer than two characters (`deck_con.cpp:471-516`)." Replaced by:
   upstream runs it on its Search button (`:338-341`); on Enter in the name, ATK, DEF, Level or
   Scale box (`:471-481`); on every change to Level or Scale (`:498-501`); on a change to ATK,
   DEF or the name box only once longer than two characters (`:486-497`); on a change of card
   type, sub-type, attribute, race, limit or banlist (`:512-515`, `:525-586`); on the
   non-official switch (`:607-612`); and on the link markers' OK (`:446-466`); the categories'
   OK only records the categories and closes the window, applying at the next search
   (`:346-354`). "Here every change re-runs it." became "Here every change re-runs it, the
   categories included." "The cards found for a given set of choices are the same." is kept.
   I read each cited range in `gframe/deck_con.cpp` at this commit (`StartFilter` at `:339`,
   `:465`, `:478`, `:489`, `:500`, `:515`, `:572`, `:585`, `:612`; none in `:346-354`).
2. `docs/architecture/deck-builder-ui.md` §15, "Tests" bullet. Removed: "`limitFilterFollowsTheSelectedBanlist`
   and `cardTypeChangeResetsTheMonsterControlsAsUpstreamDoes` (`ui/tests/test_deckbuilder.cpp`)
   drive every control through the adapter against a synthetic database;" Replaced by the same
   list plus `numberBoxesReadTheFieldUpstreamReads`, and a clause saying it has cards that tell
   the four boxes apart so Scale-to-right-scale or Level-as-ATK fails. The rest of the bullet
   is unchanged.
3. `docs/architecture/deck-builder-ui.md` §15, "Visual check" bullet. Its sentence "a `--capture`
   of the Decks screen with a synthetic database and banlist, at the default 1280x800 window,
   showing the filter grid in its default state." is kept; added: "It did not look at the
   960x600 minimum, where round 022's grid did not fit; §15.1 records the correction." New
   §15.1 (added only) argues the layout: what was wrong, the four changes, alternatives not
   taken, how it is checked, and what is not changed.
4. `docs/capabilities.md`, "Windows / macOS builds" row. Removed: "`ui/` on macOS has no recorded
   evidence, because the machine had no Qt". Replaced by: `ui/` on macOS 27 (arm64) with
   Homebrew's Qt 6.11.1, not CI's pinned 6.8.3, configures with `-DEDOPRO_NEXT_UI_TESTS=ON`,
   builds, passes CTest and loads offscreen with no QML diagnostic (one macOS font-alias notice
   on stderr), recorded by the Builders of rounds 019 (Release), 020 and 022 (Debug, `WERROR`)
   and independently by those rounds' Verifiers, with links. I opened each cited report and
   found its `ui/` configure, build, ctest 2/2 and offscreen result. The citations to brief 007
   and `state-history.md` are kept for `client/`/`data/`/`policy/`.

### CI (required evidence 7)

- `gh run view 36447720568` (push of `72d27c2218cf2dc7ea2218e2cc20a84e8a0c78eb`) → `success`:
  Semantic client model success; Qt 6 shell (Linux) success; Regression harness (3.12) success;
  Card and deck data success; Regression harness (3.10) success; Upstream EDOPro baseline
  skipped (not triggered on a branch push; nothing under `gframe/`/`integration/` changed).
- The earlier `a4f3d805` run (36447414578) failed both Regression harness jobs on the link to
  this not-yet-written report, and passed the other three; fixed by `72d27c22`.

## Not verified

- **CI on the report commit** that `fw.py report` makes on top of `72d27c22`: it adds only this
  file; its run starts after this report is written.
- **Qt 6.8.3, Linux and Windows locally.** CI's Linux job builds `ui/`, runs both ctest suites
  (including the new tests) and the offscreen check with Qt 6.8.3, and passed; I did not see
  Qt 6.8.3's rendering. Font metrics differ by platform; the four-column threshold (300 pixels)
  was chosen from the macOS capture.
- **Visual states** listed under Captures as not checked by eye.
- **Real keyboard Tab traversal.** The test moves focus with `forceActiveFocus(Qt::TabFocusReason)`
  and checks the scroll follows; it does not press Tab through the screen, and offscreen has no
  window manager. That every filter control accepts Tab focus is pinned by the existing
  `filterControlsReachTheSearchModel`, not re-derived by me.
- **Window sizes other than 960×600 and 1280×800**, and resizing while filters are scrolled.
- **Level vs Scale field kinds.** Swapping `NumericFilterField::Level` and `::Scale` cannot be
  caught by any test: `parse_numeric_filter` treats the two identically (both unsigned, neither
  a stat, `data/src/numeric_filter_text.cpp`). The Scale box parsed as ATK is caught by the new
  test's `">=2147483648"` check (keeps the Pendulum card if read signed); I did not run that
  mutation.
- **`client/`, `data/`, `policy/`, `gframe/`, golden reproduction**: untouched by this round, not
  run. `tools/check_pr_evidence.py`: not run, no PR opened by me.
- The Python suite under the system Python 3.9.6: not rerun (rounds 021-022 record one known
  3.9 failure in `tools/generate_readme_status.py`, unrelated).

## Changed

- `ui/qml/screens/DeckBuilderScreen.qml` - the filter grid now sits in a `ScrollView`
  (`filterScroll`) that is at most the grid's height and may shrink to about two rows; the grid
  is the scroll area's width, four columns at 300 pixels and above, two below; the limit list,
  check box and two buttons span according to the column count; the check box fills its row and
  its label wraps (a Basic-style-shaped `contentItem` in `FilterCheck`); the results frame has a
  160-pixel floor; focus moving into the grid scrolls the focused control into view; four
  `objectName`s (`searchPane`, `deckPane`, `resultsFrame`, `filterScroll`) for tests and captures.
  No binding to a search result, no card inspection, no matching added.
- `ui/tests/test_deckbuilder_screen.cpp` - new `searchPaneFitsItsColumnAtMinimumAndDefaultSizes`;
  new `initTestCase()` that sets the Basic style before any engine is created. **This changes
  the environment of every existing screen test**: before, they ran in the platform's default
  style (macOS style here, Fusion on Linux); now in Basic, the style `main.cpp:99` sets for the
  shell. No existing assertion was edited or removed; all 21 earlier screen tests pass in Basic
  (ctest above; `test_deckbuilder_screen` → `Totals: 24 passed, 0 failed`, 22 tests plus
  `initTestCase`/`cleanupTestCase`). Without it, on macOS, the new check box `contentItem` produced "The current
  style does not support customization of this control" warnings in the test.
- `ui/tests/test_deckbuilder.cpp` - new `numberBoxesReadTheFieldUpstreamReads` with its own
  four-card fixture. `filterFixture()` and every existing expectation are unchanged.
- `docs/adr/0012-deck-builder-search-filters.md` - Decision 6's trigger sentence, as above.
- `docs/architecture/deck-builder-ui.md` - §15 bullets and new §15.1, as above.
- `docs/capabilities.md` - the macOS `ui/` evidence, as above.
- `docs/state.md` - not changed.

## Open questions

1. **`docs/ROADMAP.md` M6** still lists macOS local builds as `client/`, `data/` and `policy/`
   only (line 244). It is not wrong - it does not deny `ui/` - but it is now narrower than
   `docs/capabilities.md`. ROADMAP is outside this brief's scope, so I left it.
2. **The screen tests' style.** Setting Basic in `initTestCase` makes the tests match the shipped
   shell; if Brain prefers the platform default for the older tests, the layout test alone would
   need its own process or harness, since `QQuickStyle::setStyle` applies per process.
3. **The check box label** wraps its last "…)" onto a second line at 1280×800. It is readable
   and in its column; a shorter English label would avoid the wrap, but the wording is round
   022's and I did not change it.

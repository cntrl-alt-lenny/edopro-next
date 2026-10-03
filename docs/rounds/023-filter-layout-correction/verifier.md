<!-- fw-report
round: 023-filter-layout-correction
role: verifier
branch: verifier/023-filter-layout-correction
head: f4b9493626b93b387175418dc31d6f9bb06fbb25
os: macOS 27.0
python: 3.9.6
written: 2026-10-03T19:32:03Z
-->
Reviewed commit: `f4b9493626b93b387175418dc31d6f9bb06fbb25`.
Independent review in a fresh isolated clone; no implementation changes in the review checkout.
The combined diff was reviewed against `origin/master` (`a1306f93883eff04ffc721bcaacc92cd88b54680`),
and the correction separately against `origin/brain/023-filter-layout-correction`
(`92db3a892c06475f35a28d2a31d212256437a13a`). Round 022's review is not treated as evidence at this SHA.

The independent first pass, including source reading, builds, captures, Tab probes and both
wiring mutations, finished before opening either round's agent reports. Pass two then read
round 023's Builder report, round 022's Builder and Verifier reports, and the historical
reports cited by the capabilities row.

## Findings

1. **[BLOCKER] `ui/qml/screens/DeckBuilderScreen.qml:724-750` — the Effects popup leaves
   keyboard focus on invisible categories.** The new reveal handler at lines 546-557 only
   handles descendants of `filterGrid`; the categories live in a different clipped
   `ScrollView`. On the real screen in the offscreen test harness, open Effects and press
   Tab through the checkboxes. Category 13 already extends below the visible category area;
   Category 32 (`categoryCheck31`) receives actual Tab focus at scene y=755..795 while the
   entire popup occupies y=44..404. The view still displays Categories 1..12. This happens
   at both screen areas corresponding to 960x600 and 1280x800. A screenshot at Category 32
   confirms the popup has not scrolled. A failing geometry assertion after actual Tab
   events demonstrates the defect (reproduction below). The control can be toggled blindly,
   but its label and selection state are invisible, so it is not usable by keyboard as the
   brief requires. This popup was introduced by round 022 and remains in the combined diff;
   round 023 corrects the same failure class only in the outer grid. Fix focus visibility
   for every scrollable filter surface and test real key traversal plus visible bounds,
   including the category popup, rather than adding another forced-focus assertion.

2. **[NOTE] `docs/architecture/deck-builder-ui.md:1060-1062` — the hint-fit explanation
   overstates the capture.** It says roughly 80 pixels are enough for the `">=1500, ?"`
   hint. My full-shell 1280x800 capture elides the ATK/DEF hints (`">=1500..."`), as the
   Builder's report also explicitly observes. Controls remain inside their column and are
   editable; this is a wording correction, not another overlap defect.

3. **[NOTE] `docs/architecture/deck-builder-ui.md:1026-1028` and
   `ui/tests/test_deckbuilder_screen.cpp:881-890` — Tab capability is not universal traversal
   evidence.** The committed check asserts a `focusPolicy` bit; the new layout test at
   lines 977-989 forces focus onto only the first and last filters. Neither sends a Tab
   key. My independent offscreen key-event probe does reach all main filters, but native
   macOS traversal on this host does not: after selecting Monster with the actual combo,
   Tab from it goes to ATK, then DEF, Level, Scale, then Search, skipping the remaining
   combos, checkboxes and buttons. In the native Effects popup, Tab did not focus a checkbox.
   I did not change system keyboard-navigation preferences or establish the cause of this
   platform difference. Qualify the documentation and preserve this limitation; the
   offscreen result alone is not proof of native traversal on every supported platform.

## Verified

### Seat and environment

`python3 tools/fw.py start --role verifier --round 023-filter-layout-correction --review origin/builder/023-filter-layout-correction`
returned exit 0:

```text
seat ok: verifier, round 023-filter-layout-correction, branch verifier/023-filter-layout-correction at f4b9493626b9
  brief: docs/rounds/023-filter-layout-correction/brief.md
  reviewing exactly f4b9493626b93b387175418dc31d6f9bb06fbb25 from origin/builder/023-filter-layout-correction
  do not open docs/rounds/023-filter-layout-correction/builder.md until your first pass is finished
  finish with: write docs/rounds/023-filter-layout-correction/verifier.md, then python3 tools/fw.py report --role verifier --round 023-filter-layout-correction --push
```

Immediately afterwards, `git submodule update --init` returned exit 0:
`Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'`.
`git submodule status` returned exit 0:
` 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)`.

Commands: `sw_vers` returned macOS 27.0, build 26A428;
`clang++ --version` returned Apple clang 21.0.0 (clang-2100.3.34.2), target
arm64-apple-darwin27.0.0; `qtpaths --qt-version` returned 6.11.1.
`python3 --version` returned 3.9.6; the available bundled Python runtime returned
3.12.14. All environment commands succeeded except the initial lookup for an unversioned
Homebrew Python executable, which was absent. Qt is Homebrew 6.11.1, different from CI's
6.8.3. Below `PY312` denotes the located Python 3.12.14 executable, without a personal path.

### Builds and checks at the reviewed tree

These were independent fresh build directories, not reused from another seat.

| Command | Exit and real output |
|---|---|
| `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON` | 0, configure and generate completed |
| `cmake --build ui/build --parallel` | 0, `[104/104] Linking CXX executable tests/test_deckbuilder_screen` |
| `ctest --test-dir ui/build --output-on-failure` | 0, deckbuilder Passed; deckbuilder_screen Passed; `100% tests passed out of 2` |
| `cmake -S data -B data/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON` and `cmake --build data/build --parallel` | both 0 |
| `ctest --test-dir data/build --output-on-failure` | 0, card_database, deck_ydk, card_search, numeric_filter_text Passed; `100% tests passed out of 4` |
| `cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON` and `cmake --build policy/build --parallel` | both 0 |
| `ctest --test-dir policy/build --output-on-failure` | 0, lf_list, deck_validation, deck_placement, deck_search_filter Passed; `100% tests passed out of 4` |
| `python3 tools/generate_messages.py --check` | 0, `message table up to date (96 ids)` |
| `python3 tools/generate_protocol_constants.py --check` | 0, `protocol constants up to date (187 values)` |
| `"$PY312" tools/generate_readme_status.py --check` | 0, `README status block is up to date` |
| `python3 tools/fw.py check` | 0, `0 error(s), 0 warning(s)` |
| `"$PY312" -m unittest discover -s tests -v` | 0, `Ran 133 tests`, `OK (skipped=11)` |
| `"$PY312" tests/test_replay_trace.py --update` | 0, wrote all three structural goldens |
| `git diff --exit-code -- tests/golden` | 0, empty output |
| `git diff --check` | 0, empty output |

CMake emitted QTP0004 and SQLite imported-target deprecation notices and could not find
optional Vulkan headers. The UI link emitted duplicate-library warnings. These did not
fail configuration or compilation; no claim that all logs were warning-free.

The 11 Python skips, each named:

- `TestBinaryFreshness.test_unreadable_source_tree_fails_closed`: Windows ACL denial required.
- `TestSemanticGoldens.test_fixtures_exist`
- `TestSemanticGoldens.test_rendering_is_deterministic`
- `TestSemanticGoldens.test_traces_match_golden`
- `TestSemanticQuality.test_committed_fixtures_are_semantically_complete`
- `TestSemanticQuality.test_coverage_accounts_for_every_packet`
- `TestSemanticQuality.test_model_invariants_hold_at_the_end`
- `TestSemanticQuality.test_no_environmental_leakage`
- `TestSemanticQuality.test_no_packet_is_malformed_or_unknown`
- `TestSemanticQuality.test_query_stream_coverage_is_real_and_clean`
- `TestSemanticQuality.test_something_is_actually_decoded`

The last ten need a fresh client semantic-trace binary, absent in this clone. No push-guard
skip occurred. The initial system-Python run returned 1: `Ran 133 tests`,
`FAILED (failures=1, skipped=11)`, failing
`test_readme_status.CommandLineTest.test_check_fails_on_a_stale_copy_and_update_repairs_it`.
Its single-test rerun also returned 1. Direct reproduction exposed
`TypeError: write_text() got an unexpected keyword argument 'newline'` at
`tools/generate_readme_status.py:283`. Structural golden update under system Python also
returned 1 for the same argument at `tests/test_replay_trace.py:234`. Python 3.12.14 reruns
above passed. Neither failure was silently omitted or fixed by this Verifier.

Offscreen launch: a Python `subprocess.run` wrapper ran
`QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell` with a 20-second timeout.
`TimeoutExpired` proved the process survived the full interval; the wrapper then killed it.
The captured stderr was exactly one line:

```text
qt.qpa.fonts: Populating font family aliases took 70 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost.
```

No QML/TypeError/ReferenceError diagnostic appeared. **CI's literal empty-stderr assertion
fails on this host**; surviving the interval is verified, a completely clean stderr is not.
The wrapper exited 0 because it was collecting both outcomes, not because the strict check
passed. CI's Linux check is green independently.

### Captures and keyboard probes

Synthetic data only: a temporary SQLite `.cdb` with the usual eleven `datas` columns and
`texts.id/name/desc/str1..str16`; 40 Effect Monsters, ids 1..40, scope 3, ATK 1000+id,
DEF 1000, Level 4, race 1, attribute 1, category 1, names `Synthetic card 01`..`40`.
No database, artwork or capture is committed. My first fixture omitted str1..str16 and
failed to load (`no such column: texts.str1`); those captures were discarded and regenerated
with the full schema. That was fixture setup error, not a product finding.

Commands, for `(w,h)=(960,600)` and `(1280,800)`:

```sh
QT_QPA_PLATFORM=offscreen ui/build/edopro_next_shell --card-db /tmp/023-synthetic.cdb \
  --start-screen decks --capture /tmp/023-<w>.png --capture-width <w> --capture-height <h>
sips -g pixelWidth -g pixelHeight /tmp/023-960.png /tmp/023-1280.png
```

Both capture launches returned 0; `sips` returned 0 and respectively 960x600 and 1280x800.
I opened and inspected both images myself. At 960x600 the main filters are two columns,
inside the divider, with an independent scroll gutter; two entire result rows and the
start of a third remain visible. At 1280x800 all main filter rows fit in four columns,
inside the divider, with multiple result rows. Clear and the search box remain inside
both columns. Deck and preview panes do not overlap; Ruleset/Banlist text truncation stays
inside the controls. ATK/DEF hints at 1280x800 are elided (finding 2).

For scrolled filters and open popups I used a disposable source copy outside the review
checkout, compiling a temporary slot in `test_deckbuilder_screen.cpp` against the **unchanged
production QML**. The existing `Harness` loads the actual screen. Window areas were
896x600 and 1064x800, the portions of the two shell sizes after their 64/216-pixel rails.
It loaded 40 synthetic monsters, enabled Monster controls, requested activation, initially
focused Search, then sent `QTest::keyClick(window, Qt::Key_Tab)` 35 times per size with
20 ms event settling and logged `activeFocusItem()->objectName()` each time. This initial
focus placement is explicit; subsequent traversal is actual Tab events, not forced focus.
Command: `QT_QPA_PLATFORM=offscreen <scratch>/ui/build/tests/test_deckbuilder_screen verifierProbe`.
Initial traversal probe returned 0, `Totals: 3 passed, 0 failed, 0 skipped`.

At both sizes the sequence from Search was:
`filtersToggle`, `clearFiltersButton`, `cardTypeCombo`, `subTypeCombo`, `attributeCombo`,
`raceCombo`, `attackField`, `defenseField`, `levelField`, `scaleField`, `limitationCombo`,
`nonOfficialCheck`, `categoriesButton`, `linkMarkersButton`, `resultsList`.
The probe asserted every enabled named main filter appeared. The main scroll area followed
focus to the lower rows. Supplemental images deliberately forced focus to Link markers to
hold the lower state for capture; those images alone are **not** traversal evidence.

I viewed six supplemental images: scrolled filters, Effects open, and Link markers open,
at each size. Their pixel sizes are the harness areas, not full-shell captures. Lower main
filters remained inside their column, the result rows remained visible, Effects showed
its initial category rows and Done inside a 420x360 popup, and Link markers showed all eight
buttons and Done without clipping. Popups intentionally overlay the underlying columns.
The plain Window harness does not reproduce Main.qml's complete palette/nav rail; I do not
claim its colors or full-shell composition match native rendering.

Then I sent actual Tab events inside each open popup (no forced focus onto its controls).
Link-marker traversal reached every marker and Done. Effects traversal reached all 32
checkboxes and Done, **but did not reveal clipped rows** (finding 1). Relevant literal output:

```text
QSize(896, 600) categoriesPopup TAB 12 "categoryCheck12" QPointF(28,350) QSizeF(181, 40)
QSize(896, 600) categoriesPopup TAB 31 "categoryCheck31" QPointF(250,755) QSizeF(218, 40)
QSize(1064, 800) categoriesPopup TAB 30 "categoryCheck31" QPointF(250,755) QSizeF(218, 40)
```

Both screenshots taken with Category 32 focused still displayed only Categories 1..12.
A subsequent version added a failing assertion after the Tab focus reached Category 32:

```cpp
// Existing Harness, 896x600, synthetic catalog, Monster enabled.
QMetaObject::invokeMethod(h.child("categoriesPopup"), "open");
// Send Tab repeatedly and inspect window->activeFocusItem(), rather than force category focus.
// When objectName() == "categoryCheck31":
const qreal bottom = h.child("categoriesPopup")->property("y").toReal()
                   + h.child("categoriesPopup")->property("height").toReal()
                   + item(h, "searchField")->mapToScene(QPointF(0, 0)).y();
QVERIFY2(f->mapToScene(QPointF(0, f->height())).y() <= bottom,
         "actual Tab focus is below even the entire Effects popup");
```

For this harness popup top/bottom are 44/404; focused control bottom is 795. This is a
conservative assertion against the whole popup, not merely its smaller clipped viewport.
Rebuild returned 0; the same probe command returned **1**:

```text
FAIL!  : TestDeckBuilderScreen::verifierProbe() 'f->mapToScene(QPointF(0,f->height())).y() <= bottom' returned FALSE. (actual Tab focus is below even the entire Effects popup)
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 2783ms
```

Native macOS inspection used the exact built shell binary in a temporary app bundle, with
arguments for the synthetic database and Decks screen. Through native UI automation I
selected Monster with its combo, pressed real Tab keys, opened both popups with their
buttons, pressed Tab in Effects, closed with Escape, and mouse-scrolled Effects down.
The default native window was inspected; both popups and lower category rows rendered
without overlap there. The native Tab limitation is described in finding 3. An attempted
native resize drag did not change dimensions; I therefore do not call those native images
960x600 verification. Exact-size full-shell verification came from the offscreen captures.

### Both wiring mutations fail the new test

Mutations ran only in the disposable source copy; the review checkout stayed at the exact
reviewed production tree. Each rebuilt `test_deckbuilder` successfully and ran:
`QT_QPA_PLATFORM=offscreen <scratch>/ui/build/tests/test_deckbuilder numberBoxesReadTheFieldUpstreamReads`.

Scale mutation:

```diff
- query.left_scale = data::parse_numeric_filter(state.scaleText.toStdString(), NumericFilterField::Scale);
+ query.right_scale = data::parse_numeric_filter(state.scaleText.toStdString(), NumericFilterField::Scale);
```

Exit 1:

```text
FAIL!  : TestDeckBuilder::numberBoxesReadTheFieldUpstreamReads() Compared lists have different sizes.
   Actual   (resultCodes(model)) size: 0
   Expected ((L{604})) size: 1
   Loc: [<scratch>/ui/tests/test_deckbuilder.cpp(1669)]
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 17ms
```

Restored Scale before applying the independent Level mutation:

```diff
- query.level = data::parse_numeric_filter(state.levelText.toStdString(), NumericFilterField::Level);
+ query.level = data::parse_numeric_filter(state.levelText.toStdString(), NumericFilterField::Attack);
```

Exit 1:

```text
FAIL!  : TestDeckBuilder::numberBoxesReadTheFieldUpstreamReads() Compared lists have different sizes.
   Actual   (resultCodes(model)) size: 0
   Expected ((L{603})) size: 1
   Loc: [<scratch>/ui/tests/test_deckbuilder.cpp(1661)]
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted, 27ms
```

After restoring the source and rebuilding, the same test returned 0,
`Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted, 18ms`.
The real reviewed source already uses `NumericFilterField::Level`; the correction diff
against the Brain branch changes tests, not this production line. No existing expectation
changed in round 023. Round 022's existing test helper gained race/attribute/category
columns; its original expectations remain intact.

### Upstream re-read and quoted

Primary source was the preserved local upstream `gframe/`, unchanged against master.
I read `deck_con.cpp:330-354`, `:446-617`, `:1191-1324`, `deck_con.h:115-122`,
`data_manager.cpp:137-156`, `bufferio.h:240-249`, the type/race/limit tables in
`game.cpp:3350-3462`, and the parser at `deck_con.cpp:21-51` directly.

ADR 0012 Decision 6's corrected triggers agree with that source:

- Search: `case BUTTON_START_FILTER: { StartFilter(); break; }` (`deck_con.cpp:338-341`).
- Category OK: `filter_effect = 0;` then the 32 checked bits, followed by
  `mainGame->HideElement(mainGame->wCategories); break;` (`:346-354`). There is no
  `StartFilter` call in this case.
- Marker OK: `mainGame->HideElement(mainGame->wLinkMarks); StartFilter(true); break;`
  (`:464-466`).
- Enter: the cases `EDITBOX_ATTACK`, `EDITBOX_DEFENSE`, `EDITBOX_STAR`, `EDITBOX_SCALE`,
  `EDITBOX_KEYWORD` share `StartFilter();` (`:471-481`).
- Changed text: `if (filter.size() > length) StartFilter();` (`:486-490`), and ATK,
  DEF, KEYWORD share `StartFilterIfLongerThan(2);` (`:493-497`); STAR and SCALE share
  `StartFilter();` (`:498-501`).
- Banlist calls `ReloadCBLimit(); StartFilter(true);` (`:512-516`); main type resets the
  monster controls then calls `StartFilter(true);` (`:525-573`); subtype/other selectors
  also call `StartFilter(true);` (`:575-587`).
- Non-official checkbox calls `ReloadCBLimit()`, conditionally preserves the prior index,
  then `StartFilter(true);` (`:607-613`).

The asymmetric-scale and unsigned-Level expectations are also supported directly:
`DECLARE_WITH_CACHE(uint32_t, filter_lv)` (`deck_con.h:120`), compared with
`DECLARE_WITH_CACHE(int32_t, filter_atk)` (`:116`);
`(filter_scltype == 1 && data._data.lscale != filter_scl)` and
`|| !(data._data.type & TYPE_PENDULUM)` (`deck_con.cpp:1222-1225`);
`if(level < 0) cd.level = -(level & 0xff);` and
`cd.lscale = (level >> 24) & 0xff; cd.rscale = (level >> 16) & 0xff;`
(`data_manager.cpp:146-153`). The new fixture's left=2/right=7 and packed level=-1
therefore distinguish precisely the two wrong wirings. Data parser tests compare kept sets
against upstream transcriptions, while policy tests exercise the hidden-card and limit
branches. Passing those tests is evidence about search matching, not unchanged duel behavior.

### Acceptance and invariants

1. **Captures / more than one row: met** at both requested full-shell sizes, inspected by me.
   **Every filter usable by keyboard: not met**, due to finding 1. Main-grid offscreen
   traversal is established separately from committed forced-focus tests.
2. **Wrong Scale and Level wiring caught: met**, two independent failed mutations above.
3. **ADR and capabilities: met**, except the separate hint-fit note in the architecture
   rationale. Capability sources were checked after the blind pass: Builder 019 records
   Release configure/build, CTest 2/2 and the font notice; Builder 020 and 022 record Debug
   with WERROR, CTest 2/2 and the same notice; their Verifiers each independently record
   UI builds/CTest and offscreen survival. Verifier 020 records both Release and Debug.
   The row correctly attributes historical evidence and does not cite a nonexistent report.
4. **Build/CTest, supported Python, generators, fw check and final-SHA CI: met**. The
   strict empty-stderr smoke assertion is **not met locally**; Linux CI satisfies it.

The full diff changes no `gframe/`, `integration/`, `client/`, `ocgcore/`, workflow,
framework or `docs/state.md` content. Matching lives in data and policy; QML collects
choices and renders results. No new dependency or real card-data asset was added. Round
023's correction is limited to the QML, two UI tests and three documentation files plus
its Builder report. No test expectation was silently relaxed. No merge or implementation
repair was performed by this seat.

### Record removals and replacements, correction against the Brain branch

I compared all removals in the three scoped documents directly, not just their description:

- ADR 0012 Decision 6 removed the sentence saying upstream runs on Enter, combo changes,
  **category and marker OK**, and ATK/DEF text longer than two characters. It is replaced
  by the explicit Search/Enter/Level/Scale/name/ATK/DEF/selector/checkbox/marker trigger
  list and a sentence explaining category OK only records and closes. The sentence
  `Here every change re-runs it.` becomes `Here every change re-runs it, the categories
  included.` The same-cards sentence remains.
- Architecture section 15's Tests sentence replaces the existing list ending in
  `limitFilterFollowsTheSelectedBanlist` and `cardTypeChangeResetsTheMonsterControlsAsUpstreamDoes`
  with that list plus `numberBoxesReadTheFieldUpstreamReads`, and explains its distinct
  fields/field kinds. The screen-test sentence remains. The Visual check sentence remains
  and gains the explicit missing-960x600 explanation. Section 15.1 is added, with no
  earlier section removed; its width/height strategy agrees with the QML, subject to
  finding 2 and the keyboard qualification above.
- Capabilities' macOS sentence formerly joined client/data/policy builds to
  `ui/ on macOS has no recorded evidence, because the machine had no Qt` and the brief
  007/state-history citations. The client/data/policy claim and those citations remain;
  the no-UI-evidence clause is replaced by the macOS 27 arm64 / Qt 6.11.1 UI configure,
  build, CTest and offscreen-with-font-notice statement and six existing reports from
  rounds 019/020/022. Windows and CI status sentences remain. No round-023 evidence is
  retroactively attributed to those earlier reports.

### Pass two and final review-SHA CI

The Builder's outcomes agree with my fresh UI cycle, captures, wiring mutations, trigger
reading and capabilities-source checks. Its stated limitation, `Real keyboard Tab
traversal`, is accurate; I extended that evidence and found the popup defect. The Builder
also explicitly did not visually inspect scrolled filters or either popup. This is new
review evidence, not a contradiction of a claimed popup inspection. Round 022's reports
likewise left popup keyboard use unverified. Its earlier `No BLOCKER` verdict applies
only to its earlier SHA and cannot resolve this finding.

`gh api repos/cntrl-alt-lenny/edopro-next/commits/f4b9493626b93b387175418dc31d6f9bb06fbb25/check-runs`
returned exit 0. Run [36448227420](https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/36448227420)
records completed **success** for Regression harness (3.10), Regression harness (3.12),
Semantic client model, Card and deck data, and Qt 6 shell (Linux); upstream-baseline is
completed **skipped**, as expected for a branch push. These are observations at the
literal reviewed SHA, not a claim about the later Verifier report commit's CI.

## Not verified

- Native all-control keyboard traversal on macOS, Linux or Windows: only the precise native
  limitation and offscreen traversal above were observed. System navigation settings were
  not changed. Offscreen events do not establish physical keyboard behavior everywhere.
- Native exact 960x600 popup rendering: resize attempt did not succeed. Exact-size
  full-shell base captures and matching-size real-screen harness popups were inspected;
  the harness has a different root palette and omits the nav rail.
- Visible keyboard use of all Effects categories fails, as finding 1 establishes. I did
  not count merely acquiring focus on hidden controls as usable reachability.
- Qt 6.8.3 rendering locally, other window sizes, all banlist/non-official combinations
  visually, every dropdown option by keyboard, and resizing while scrolled.
- Strict empty-stderr offscreen launch on macOS: font alias notice remains; Linux CI is
  separate evidence. No font configuration was changed to make this check appear clean.
- Upstream GUI execution, duel behavior, observer fixture equivalence, and upstream
  baseline build locally. Those source trees are unchanged. No replay result is cited
  as evidence of unchanged duel behavior.
- `client/` build and ten semantic-trace tests, Windows ACL test, real Project Ignis card
  databases and upstream resource labels. Historical report evidence is not a new
  execution of those old builds.
- Round 022's prior custom fuzzing and benchmark measurements were not independently
  repeated at this SHA. The combined source and current data/policy/UI suites were
  reviewed and rerun; the brief explicitly does not reopen search semantics.
- CI on the report commit created by `fw.py report`: it does not exist when this report
  is written. No PR was opened/updated, so no PR-body evidence check was required.
- Temporary probe source/captures are uncommitted. Reproduction operations and the
  decisive code/output are recorded above; only this report is committed.

## Verdict

The main-column layout correction, asymmetric Scale regression test, unsigned Level
regression test and corrected upstream trigger record are well supported at the exact
reviewed SHA. Required local builds, supported-Python checks and the five deterministic
CI checks passed, with the macOS smoke diagnostic explicitly qualified. I recommend
returning this combined change for correction before acceptance: the Effects popup still
lets Tab move focus into invisible categories, violating the brief's keyboard usability
requirement. Confidence is high in that failure and in the main-grid geometry/wiring
results; native platform keyboard behavior remains qualified as described above. This is
a Verifier finding, not acceptance or a merge decision.

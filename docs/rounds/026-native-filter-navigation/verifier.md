<!-- fw-report
round: 026-native-filter-navigation
role: verifier
branch: verifier/026-native-filter-navigation
head: 97ff057d4ffca0cfffeb109d62cf4d8ed029cccc
os: macOS 27.0.1
python: 3.9.6
written: 2026-10-05T11:14:27Z
-->
# Verifier report: native filter navigation

Reviewed commit: `97ff057d4ffca0cfffeb109d62cf4d8ed029cccc`.
All results below were obtained independently in the isolated Verifier checkout
on macOS 27.0.1 (26A434), Apple clang 21.0.0, Qt 6.11.1 and Basic style.
The framework commands used Python 3.9.6; the required suite used Python
3.13.15 (`python3.13`, available on PATH). No production files were edited.

## Findings

No BLOCKER or SHOULD FIX findings. The delivered investigation supports the
narrow focus-policy conclusion and preserves the unresolved native gates.

- [NOTE] `docs/rounds/026-native-filter-navigation/attachments/research.md:209`
  — the required macOS empty-stderr assertion fails independently here too.
  The shell survives the full 20-second timeout, but emits the font diagnostic
  quoted below. This is accurately reported, not a clean-load pass. Production
  source bytes are unchanged and the exact delivered SHA's Linux Qt check is
  successful; the macOS assertion nevertheless remains failed.
- [NOTE] `docs/architecture/deck-builder-ui.md:1163` — popup failures remain
  intermittent. My first full-shell all-controls comparison reached all 15 main
  controls then failed at null popup focus. My later sequential matrix passed
  all 24 all-controls rows. These observations support the stated unresolved
  reliability limitation; the successful repeat does not explain the failure.

### Independent first pass and acceptance criteria

I read the real diff, required production/test sources and diagnostic sources,
built both baseline and disposable probes, ran baseline rows and a controlled
policy comparison, and examined native desktop traversal before opening
`builder.md`. The first comparison already reproduced the load-bearing result:
runtime policy 3 excludes ten non-text main controls, while all-controls reaches
15/15 with the existing clipping assertions. Only then did I read Builder's
report and research narrative. Historical Round 024 reports were not used as
independent confirmation.

1. Met: unchanged production bytes match starting master
   `812f5a38214b377fa09efac9a556334cac54cf3b`; four offscreen rows pass and four
   Cocoa rows independently fail on the exact same missing ten controls. Native
   process policy/style/activation were inspected in the disposable comparisons.
2. Met for the investigation: at 960x600 and 1280x800, desktop-delivered forward
   and reverse keys reproduce the five-field cycle. Input, runtime policy and
   focus logs distinguish the real Main shell from the screen-only harness.
3. Met with stated gaps: both independently seeded popups exclude children under
   default desktop policy. Diagnostic all-controls rows exercise all 33 Effects
   controls and all 9 marker controls, Category 32 model operation and close/reopen
   assertions. This is not default-policy end-to-end reachability.
4. Met: version-matched primary source and default/all/default comparisons
   support policy as the cause of main non-text exclusion. Activation/popup
   failure causes are explicitly left unresolved, not attributed to policy alone.
5. Met: literal default failures, the earlier null-focus failure and restored
   default failures demonstrate live failure mechanisms. Destination sets,
   ancestor clipping, lower-category model checks and operation assertions are
   unchanged. No native expectation was weakened or skipped.
6. Met: recommendation requires a later product decision and API compatibility
   assessment; no production fix, hardware/controller parity or duel claim.
7. Met for diagnostic/build/regression evidence and independent reproduction.
   The macOS empty-stderr assertion is a disclosed failure, not a passing check.

### Commands and literal results obtained independently

`python3 tools/fw.py start --role verifier --round 026-native-filter-navigation`
exited 0 and selected `97ff057d4ffca0cfffeb109d62cf4d8ed029cccc`.
Immediately `git submodule update --init` exited 0. `git submodule status`
exited 0 and reported `46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57` for ocgcore.
`git rev-parse HEAD`, `sw_vers`, `clang++ --version`, `python3 --version`,
`python3.13 --version` and `qmake -query QT_VERSION` exited 0 with the identity
and versions above. Initial tool discovery confirmed CMake/Ninja/Qt availability.

These commands each exited 0:

```text
cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
cmake --build ui/build --parallel
ctest --test-dir ui/build --output-on-failure
```

CTest output: `1/2 Test #1: deckbuilder ... Passed`,
`2/2 Test #2: deckbuilder_screen ... Passed`,
`100% tests passed, 0 tests failed out of 2`.

Each row used `QT_QPA_PLATFORM=<platform> ui/build/tests/test_deckbuilder_screen
filterTabTraversalStaysVisible:<row>` as a separate process:

| Row | Offscreen exit | Cocoa exit |
| --- | --- | --- |
| minimum-forward | 0 | 1 |
| minimum-reverse | 0 | 1 |
| default-forward | 0 | 1 |
| default-reverse | 0 | 1 |

Each offscreen row reported `main reached 15 of 15`, `categories reached 33 of 33`,
`markers reached 9 of 9`, `lower category Space changed model selection`, and
`Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted`.
All four Cocoa rows independently reported:

```text
'reached == expected' returned FALSE. (main missing: attributeCombo, cardTypeCombo, categoriesButton, clearFiltersButton, filtersToggle, limitationCombo, linkMarkersButton, nonOfficialCheck, raceCombo, subTypeCombo)
'!QTest::currentTestFailed()' returned FALSE. ()
Totals: 2 passed, 1 failed, 0 skipped, 0 blacklisted
```

Baseline harness areas are 896x600 and 1064x800. Full-shell probe dimensions
are 960x600 and 1280x800. Do not equate the two.

Disposable generator command:
`python3 docs/rounds/026-native-filter-navigation/attachments/make_probe.py . <probe-source>`
exited 0: `Disposable instrumentation created; all production QML bytes unchanged.`
Ordinary UI configure/build commands with the same Debug/WERROR/UI_TESTS flags,
using disposable source/build directories, exited 0.
An independent byte comparison found only the three advertised altered copies:
`ui/src/main.cpp`, `ui/tests/CMakeLists.txt`, `ui/tests/test_deckbuilder_screen.cpp`.
The added observer header is separate. All copied QML/data/policy bytes match.
The generator adds test-only Main resources/AppContext, selects the host,
resizes it, logs policy/activation, optionally sets process-local all-controls,
and labels isolated popup seeds. It also logs null-focus missing names. The shell
copy adds the passive observer and optional diagnostic resize; it does not set
policy or focus. No shipped switch is introduced.

First comparison used the probe binary with `QT_QPA_PLATFORM=cocoa`,
`EDOPRO_DIAG_FULL_SHELL=1`, row `minimum-forward`, fresh default/all/default
processes, adding only `EDOPRO_DIAG_POLICY=all` for the middle process.
Exits were 1/1/1. Both default runs had effective policy 3 and ten missing
non-text controls at null focus; all-controls reported `main reached 15 of 15`
then failed `'active' returned FALSE` in Effects. This was retained as a failure.

The subsequent complete sequential matrix used
`python3.13 docs/rounds/026-native-filter-navigation/attachments/run_diagnostics.py <probe-binary> <untracked-log-directory>`.
No desktop interaction ran concurrently. Every default-policy row exited 1;
every all-controls row exited 0. The wrapper's exit 0 is not the row verdict.
Each passing row preserved the original assertions, including Category 32
Space/model change and popup close/reopen behavior when applicable.


Independent matrix exits, each entry a separate fresh process:

| Host | Policy | Surface | minimum-forward | minimum-reverse | default-forward | default-reverse |
| --- | --- | --- | --- | --- | --- | --- |
| screen | default | main | 1 | 1 | 1 | 1 |
| screen | default | categories | 1 | 1 | 1 | 1 |
| screen | default | markers | 1 | 1 | 1 | 1 |
| screen | all | main | 0 | 0 | 0 | 0 |
| screen | all | categories | 0 | 0 | 0 | 0 |
| screen | all | markers | 0 | 0 | 0 | 0 |
| shell | default | main | 1 | 1 | 1 | 1 |
| shell | default | categories | 1 | 1 | 1 | 1 |
| shell | default | markers | 1 | 1 | 1 | 1 |
| shell | all | main | 0 | 0 | 0 | 0 |
| shell | all | categories | 0 | 0 | 0 | 0 |
| shell | all | markers | 0 | 0 | 0 | 0 |

A fresh full-shell `minimum-reverse` default/all/default repeat exited 1/0/1.
The middle run completed every strict main/popup assertion; both default runs
failed with missing non-text controls. This restoration demonstrates a literal
failure, successful controlled comparison and restored failure without changing
production QML, test expectations or system settings.

### Primary source re-derivation

`git ls-remote https://github.com/qt/qtbase.git refs/tags/v6.11.1 refs/tags/v6.11.1^{}`
and the corresponding qtdeclarative command exited 0. Peeled revisions match
the delivery: qtbase `59c81a3c2247b821b9b84b4eb8d939b77e07e276`,
qtdeclarative `a02bed441965ee1f18f856352c7d5ee5ba35d795`.
I fetched the raw files at these exact revisions using Python urllib, exit 0,
and inspected literal source lines, not a report paraphrase:

- qtbase `src/plugins/platforms/cocoa/qcocoatheme.mm:440-442` queries
  `[[NSApplication sharedApplication] isFullKeyboardAccessEnabled]` and returns
  all-controls or text/list flags. Source URL:
  `https://github.com/qt/qtbase/blob/59c81a3c2247b821b9b84b4eb8d939b77e07e276/src/plugins/platforms/cocoa/qcocoatheme.mm#L440`.
- qtdeclarative `src/quick/items/qquickitem.cpp:2462-2487` reads runtime policy;
  `if (tabFocus == Qt::TabFocusAllControls) return true;` precedes the
  editability/text/accessibility-role restrictions. The same candidate check is
  used by traversal at line 2716. Source URL:
  `https://github.com/qt/qtdeclarative/blob/a02bed441965ee1f18f856352c7d5ee5ba35d795/src/quick/items/qquickitem.cpp#L2462`.
- qtbase `src/gui/kernel/qstylehints.cpp:574-595` reads an instance override
  before theme hints. Setter documentation says `\internal`; its body changes
  the instance value and emits the signal. Installed QtGui framework header
  `qstylehints.h:96` declares `setTabFocusBehavior`, confirmed by `rg`, exit 0.
  Source URL:
  `https://github.com/qt/qtbase/blob/59c81a3c2247b821b9b84b4eb8d939b77e07e276/src/gui/kernel/qstylehints.cpp#L574`.

An initial installed-header lookup used a nonexistent include layout and failed;
`rg --files -L` located the actual framework header. Official documentation
`https://doc.qt.io/qt-6/qstylehints.html#tabFocusBehavior-prop` currently renders
Qt 6.12.0, so it is supplemental, not version-matched source evidence.
`defaults read -g AppleKeyboardUIMode` and
`defaults read com.apple.universalaccess fullKeyboardAccessEnabled` each exited 1
with key-not-found output. No preference or permission was changed. Missing
keys do not establish effective policy; runtime reads and the comparison do.

### Desktop-delivered native shell observations

`python3 attachments/synthetic_cards.py <untracked-synthetic.cdb>` (with the full
round attachment prefix) exited 0: `40 synthetic Monster cards created`.
Native shell entry was `QT_QPA_PLATFORM=cocoa EDOPRO_DIAG_WIDTH=<width>
<probe-build>/edopro_next_shell --card-db <synthetic.cdb> --start-screen decks`.
A disposable app bundle symlinked this same executable for CUA discovery; its
plist contained executable, bundle identity/name and application type. No source
change was made for bundling. Direct executable discovery through CUA failed;
bundle selection succeeded. Each owned session was terminated before the next
session or sequential test matrix (SIGINT cleanup exit 130, not a test result).

`cua.getApp(<bundle>)`, `App.click(<fresh AX index>)`,
`App.pressKey('Tab')`, `App.pressKey('shift+Tab')`, `App.pressKey('Escape')`,
`App.pressKey('space')`, `App.getAXState()` and `App.getScreenshot()` were used.
Type was selected as Monster by click; Search was explicitly clicked once to
seed main traversal. At each size I sent ten forward and ten reverse keys.
The passive log shows the forward cycle
`attackField, defenseField, levelField, scaleField, searchField` repeated twice;
reverse is `scaleField, levelField, defenseField, attackField, searchField`
repeated twice. Ten non-text controls from the unchanged 15-control inventory
are absent; a clicked Type or popup trigger is not counted as reached by Tab.
Every logged key delivery had `active true` and `spontaneous true`. Startup
activation was true at minimum and false at default size; key-time activation
was true at both. Runtime policy was `Qt::TabFocusBehavior(3)` and style Basic.

Both popup triggers were separately clicked at both sizes because Tab did not
reach them. Tab and Shift+Tab left `Popup` active, with zero named child
destinations out of the expected 33 Effects and 9 markers controls. Escape
closed both at minimum and restored the initial Search field. At default size,
Space did not operate the container; Done-click closed, click reopened, and
Escape closed each popup, restoring Search. Restoration to Search rather than
Builder's Scale is consistent with each session's distinct prior focused field.
No default-policy lower-category keyboard operation was established.

Inspected app-only screenshots: minimum 1920x1264 and default 2560x1664 pixels
including chrome at 2x scale. Search's blue focus ring is visible and contained;
minimum screenshots show Effects and Link popup frames and Done within the
window. Logs show numeric destinations visible within clipped bounds after
scrolling. I did not visually inspect all 32 Effects rows or each native numeric
focus ring. Screenshots alone are not traversal evidence; images/databases stay
untracked. Native desktop automation is distinct from QTest Cocoa injection and
physical hardware input.

### Remaining required checks and diff/CI review

The Python subprocess smoke wrapper launched the unchanged offscreen shell,
waited 20 seconds and observed TimeoutExpired while alive, then terminated it.
Survival PASS; empty-stderr FAIL. The collecting wrapper exited 0; its synthetic
timeout status 124 is an adaptation marker, not a shell exit. Literal stderr:

```text
qt.qpa.fonts: Populating font family aliases took 53 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost.
```

Commands below each exited 0:

```text
python3.13 tools/generate_messages.py --check
message table up to date (96 ids)
python3.13 tools/generate_protocol_constants.py --check
protocol constants up to date (187 values)
python3.13 tools/generate_readme_status.py --check
README status block is up to date
python3.13 -m unittest discover -s tests -v
Ran 133 tests
OK (skipped=11)
python3.13 tests/test_replay_trace.py --update
wrote tests/golden/duel-chains-battle-yrpX.trace
wrote tests/golden/duel-extended-yrpX.trace
wrote tests/golden/duel-chains-battle-yrp.trace
git diff --exit-code -- tests/golden
python3 tools/fw.py check
0 error(s), 0 warning(s)
git diff --check
git diff --check origin/master...HEAD
```

Every skip: `TestBinaryFreshness.test_unreadable_source_tree_fails_closed`
requires Windows ACL denial; missing fresh semantic binary skips
`TestSemanticGoldens.test_fixtures_exist`, `test_rendering_is_deterministic`,
`test_traces_match_golden`, and `TestSemanticQuality.test_committed_fixtures_are_semantically_complete`,
`test_coverage_accounts_for_every_packet`, `test_model_invariants_hold_at_the_end`,
`test_no_environmental_leakage`, `test_no_packet_is_malformed_or_unknown`,
`test_query_stream_coverage_is_real_and_clean`, `test_something_is_actually_decoded`.
No push-guard test skipped. Golden reproduction is not duel equivalence.

`git diff --exit-code <starting-master> HEAD -- ui data policy client gframe
integration ocgcore` exited 0 with no output. `docs/state.md` and framework files
are unchanged. Changed test expectations: None. The sole architecture replacement
attributes Round 024's historical result and points to the investigation; every
added architecture sentence in section 15.3 was checked against source/probes and
its exact enumeration in `attachments/documentation-changes.txt`. Added research
and log records are investigation evidence, not shipped feature claims. A search
for home paths/emails in attachments/report found no matches.

`gh api repos/cntrl-alt-lenny/edopro-next/commits/97ff057d4ffca0cfffeb109d62cf4d8ed029cccc/check-runs`
exited 0: Regression harness (3.10), Regression harness (3.12), Semantic client
model, Card and deck data and Qt 6 shell (Linux) are completed/success at the
literal reviewed SHA. Upstream baseline is completed/skipped, consistent with
this documentation-only branch and no legacy/duel changes. The separate legacy
status endpoint returned pending; it is not a substitute for these check-run
results. No PR was opened or updated; PR-body evidence checking was not triggered.

## Not verified

- Physical keyboard, controllers, Windows, real card assets, duel equivalence,
  full accessibility behavior and broad native parity.
- A production-policy path to popup children or Category 32 operation; observed
  default navigation excludes them. Diagnostic successes are not a product fix.
- Cause or frequency of intermittent popup/null-focus/close failures. The
  independent first comparison failed; later complete comparisons passed.
- Stable production API compatibility across the supported Qt floor. The
  experimental setter's internal documentation remains a follow-up constraint.
- Clean macOS stderr: explicitly failed on the platform font diagnostic.
- A separate baseline user-session check without the passive observer, all
  Effects rows visually, and every numeric focus ring visually. Production
  QML bytes and strict geometry assertions were independently checked.
- CI for the forthcoming Verifier report-only commit until observed. Exact
  Builder delivery checks above do not carry forward automatically.

## Verdict

No blocking review finding at the exact delivered SHA. High confidence in the
narrow finding that effective Cocoa text/list Tab policy causes the main
non-text exclusion in this environment: source, live policy, native desktop
keys, strict Cocoa failures and process-local counterfactual agree. The delivery
honestly bounds default popup reachability, intermittent native reliability,
font diagnostics and API compatibility. It is a supported investigation and
recommendation for Brain to adjudicate, not a shipped fix, acceptance or merge.

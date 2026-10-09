# Native filter navigation investigation

## Reproduction identity and inputs

Production starting master: `812f5a38214b377fa09efac9a556334cac54cf3b`.
Seat start selected Brain brief `88a169c5ce8a9339f19fe4e317c6a9a1b54af3c5`;
production UI bytes are identical to starting master. Environment commands and
literal results are in environment.txt. In committed text logs, trailing spaces
are represented as `\x20` and trailing tabs as `\t`, and terminal blank lines
are trimmed so `git diff --check` can pass; diagnostic text is otherwise retained
apart from private-path redaction. The initial final-diff check caught log
whitespace (exit 2); this encoding corrected it without suppressing diagnostics. macOS 27.0.1/26A434, Apple clang
21.0.0, Qt runtime/QtTest 6.11.1, Basic Controls style, Python suite 3.13.15.
Default Python is 3.9.6 and was used only for compatible framework commands.

Baseline UI configure/build/CTest commands are those in the brief. They exit
0; configure-baseline.txt, build-baseline.txt and ctest-baseline.txt retain
outputs. The two UI CTest tests passed. The individual unchanged traversal
rows are named by QPA platform and data row in this directory. Four offscreen
rows exit 0; four Cocoa rows exit 1, at the null active-focus assertion before
complete missing accounting. These observations are not a successful native
suite. Neither the test expectations nor production bytes were edited.

## Primary toolkit source

Queried `git ls-remote https://github.com/qt/qtbase refs/tags/v6.11.1 refs/tags/v6.11.1^{}`
and the same command for qtdeclarative; both exit 0. Peeled identities:

- qtbase: `59c81a3c2247b821b9b84b4eb8d939b77e07e276`.
- qtdeclarative: `a02bed441965ee1f18f856352c7d5ee5ba35d795`.

Fetched the three files with `curl -L -s https://raw.githubusercontent.com/qt/<repository>/v6.11.1/<file>`;
all fetch commands exit 0. The revision above pins each tag used for retrieval.
These sources match the installed runtime version, not an unversioned dev branch.

1. [Cocoa theme](https://github.com/qt/qtbase/blob/59c81a3c2247b821b9b84b4eb8d939b77e07e276/src/plugins/platforms/cocoa/qcocoatheme.mm#L440),
   lines 440-442: `[[NSApplication sharedApplication] isFullKeyboardAccessEnabled]`.
   Its true branch returns TabFocusAllControls; false returns text/list flags.
   This is the actual toolkit query, distinct from similarly named accessibility
   settings. No global/user preference was written.
2. [Qt Quick candidate filtering](https://github.com/qt/qtdeclarative/blob/a02bed441965ee1f18f856352c7d5ee5ba35d795/src/quick/items/qquickitem.cpp#L2462),
   lines 2462-2487: `if (tabFocus == Qt::TabFocusAllControls) return true;`.
   Otherwise candidate acceptance checks editability/text/accessibility role.
   The note at 7213-7216 says focus can be restricted on macOS by system settings.
   A QML TabFocus bit therefore expresses capability, not effective eligibility.
3. [Style hints](https://github.com/qt/qtbase/blob/59c81a3c2247b821b9b84b4eb8d939b77e07e276/src/gui/kernel/qstylehints.cpp#L574),
   lines 574-595: getter reads the instance override before platform hints;
   setter writes that instance value and emits its signal. Setter documentation
   includes `\internal`. The installed public header declares setTabFocusBehavior
   at line 96. The [official getter documentation](https://doc.qt.io/qt-6/qstylehints.html#tabFocusBehavior-prop)
   describes a read-only property; this experiment is not a promise that the
   undocumented setter is a stable supported production API on every Qt version.

Targeted reads `defaults read -g AppleKeyboardUIMode` and
`defaults read com.apple.universalaccess fullKeyboardAccessEnabled` each exit 1,
with key-not-found output in environment.txt. Absent keys do not mean the
runtime policy is enabled or disabled. The runtime getter is the load-bearing
read: Cocoa reports flags 3 (text/list) in both diagnostic hosts and both shell
sizes. No cause is inferred merely from an OS default or focus-capability bit.

## Disposable probes and controlled comparison

Rerunnable sources: make_probe.py, focus_observer.h, synthetic_cards.py,
run_diagnostics.py. They do not add a shipped configuration switch. Example
commands from the repository, using non-personal placeholders for disposable
outputs:

```
python3.13 docs/rounds/026-native-filter-navigation/attachments/make_probe.py . <probe-source>
cmake -S <probe-source>/ui -B <probe-build> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
cmake --build <probe-build> --parallel
python3.13 docs/rounds/026-native-filter-navigation/attachments/run_diagnostics.py <probe-build>/tests/test_deckbuilder_screen <log-directory>
python3.13 docs/rounds/026-native-filter-navigation/attachments/synthetic_cards.py <untracked-synthetic.cdb>
```

Actual probe builds live under the ignored UI build tree. Production-bytes.txt
lists the only three altered copies: ui/src/main.cpp, ui/tests/CMakeLists.txt,
ui/tests/test_deckbuilder_screen.cpp. All production QML files and all data and
policy sources compare byte-for-byte; production QML SHA-256 values are listed.
The generator adds AppContext and existing full-shell resources to the test
binary; chooses TestHarness or Main based on a diagnostic environment variable;
sets startScreenIndex=1 for Main; chooses screen/deckBuilderScreen accordingly;
resizes Main to full-shell dimensions; logs policy/style/activation; optionally
sets only process-local all-controls policy; permits a separately labelled popup
seed; and prints missing names if focus becomes null. The original traverse,
visibility/intersected-clipping, destination counts and operation assertions
remain. No expected set, skip or production focus code changes.

Main diagnostic copies add only an observer call after load and its header.
It records keys, spontaneous delivery, focus bounds/clipping, an immediate and
60 ms later focus sample, and startup policy/style/activation, plus window activation at key delivery. The only optional
action is resize via EDOPRO_DIAG_WIDTH. It does not change policy or force focus.
Immediate geometry samples can precede scrolling; they are not claimed as settled
visibility failures. AppContext build labels in the diagnostic test are explicitly
"diagnostic", not a product build claim.

The initial 48-row matrix overlaps desktop interactions and is retained as
exploratory data (diag-*.txt directly in this folder). The second matrix in
controlled/ ran without concurrent desktop automation. Every row starts a fresh
process, keeping initial policy 3. Default versus `EDOPRO_DIAG_POLICY=all` changes
one policy value; native plugin, style, QML and assertions stay the same. The
screen host has 896x600/1064x800 areas; Main has 960x600/1280x800 windows.
See matrix-summary.txt for every independent row and its exact exit.

In the controlled matrix all 24 default-policy rows fail. All-controls rows
reach all 15 main controls in both hosts/directions/sizes. The combined
minimum-forward shell run then fails in Effects with null focus; it is not a
passing end-to-end row. All-controls independently seeded popups mostly pass,
including Category 32 Space/model selection and Done/Space, reopen, visible Tab
and Escape checks, but screen/default-forward Effects has a null-focus failure
and screen/default-forward markers reaches 9/9 then fails Done close. Those
failures remain unresolved, including under the controlled run; concurrency
alone does not explain them. Window activation at startup is logged independently
and can be false in a few runs, so no universal activation guarantee follows.

The default policy consistently excludes ten enabled main controls. Successful
counterfactual runs remove that exclusion without changing QML or preferences;
this supports policy as the cause of that class of omission, not a complete
explanation of every native focus/close failure. policy-restoration.txt runs
full-shell minimum-reverse with default/all/default in fresh processes: the
literal default failure, passing override and restored default failure retain
live accounting/clipping checks. A collecting wrapper returning 0 is never a
row verdict. No fault injection is needed to demonstrate missing-control failure.

## Desktop-delivered native input

Native baseline entry was `QT_QPA_PLATFORM=cocoa ui/build/edopro_next_shell
--card-db <synthetic.cdb> --start-screen decks`. A temporary app bundle points
to the same built executable; its plist declares only a bundle identifier/name,
executable, application type and high-resolution capability. Launch Services
`open -n <bundle> --args --card-db <synthetic.cdb> --start-screen decks` keeps
that executable alive. Raw Popen launch ended when the command tool cleaned its
process group; that launch is not used as session evidence.

In the uninstrumented default shell, desktop automation clicked Type, sent
Down/Return to select Monster, clicked Search once as the initial seed, then
sent seven Tab and six Shift+Tab events. Native accessibility focus read after
each key showed the five-field cycle. Screenshot: 2560x1664 pixels, corresponding
to 1280x800 content at 2x scale plus window chrome. Blue numeric focus was visible.
This corroborates that the real shell shares the exclusion, not just the harness.

For passive instrumented full-shell sessions, launch command was:

```
open -n <observer-bundle> --env QT_QPA_PLATFORM=cocoa --env EDOPRO_DIAG_WIDTH=960 --stderr <native-log> --args --card-db <synthetic.cdb> --start-screen decks
```

Repeat with width=1280; this changes only window dimensions. `cua.getApp(<bundle>)`
selects the native surface. `App.click(<current AX index>)`,
`App.pressKey('Tab')`, `App.pressKey('shift+Tab')`, `App.pressKey('Escape')`
and `App.getAXState()` delivered and observed input. No CGEvent/AppleScript path
or accessibility preference change was used. The observer records native key
messages as spontaneous=true, unlike direct QTest delivery. These are desktop
automation events, not physical hardware presses.

Both shell sizes: after Monster activation and an initial Search click,
six forward destinations were attackField, defenseField, levelField, scaleField,
searchField, attackField. Six reverse destinations were scaleField, levelField,
defenseField, attackField, searchField, scaleField. Enabled expected controls
are the test's unchanged 15-control inventory. The ten absent controls are
filtersToggle, clearFiltersButton, cardTypeCombo, subTypeCombo, attributeCombo,
raceCombo, limitationCombo, nonOfficialCheck, categoriesButton, linkMarkersButton.
Do not count a mouse-seeded combo or trigger as reached by Tab. Repeated cycles
plus strict QTest accounting support the exclusion; six events are not a claim
of an exhaustive desktop focus-chain survey.

Both popups were opened independently by click because the main Tab path did
not reach their triggers. At minimum size, a scroll action revealed the triggers.
Effects and markers each received Tab and Shift+Tab; native AX focus stayed on
the containing window and passive logs identify Popup focus, with no child
Tab destination. Their expected inventories remain categoryCheck0..31 plus
categoriesDone (33), and markerButton0..7 plus markersDone (9). Effects/marker
mouse toggles at minimum size are labelled seeds, not keyboard operation evidence.
The Category 32 keyboard operation is established only by passing diagnostic
all-controls QTest rows, not by the default native desktop session.

Escape closed each popup at both observed sizes and restored the prior Scale
field; clicks reopened each and Done-click closed each. Reopen focused Popup,
not a visible category/marker. No successful Space/Done keyboard close or
keyboard reopening is claimed under production policy. Passive native logs are
in desktop-minimum.txt and desktop-default.txt. A fresh passive-observer activation
sample at each size clicked Search and delivered Tab with Type still Any;
activation-minimum.txt and activation-default.txt both show active=true at startup
and key delivery, policy 3, and spontaneous=true. This small activation sample is
separate from the earlier Monster traversal and does not expand its reached set.
Launch Services also emits the literal sandbox-extension diagnostic retained in
those logs; it did not prevent an active window or observed key delivery.

A preliminary attempt to select the original default shell while the observer
shell also existed produced an AX/screenshot mismatch and ineffective Escape.
Those default-popup attempts are excluded from conclusions. The final default
observer session ran after both earlier owned processes were terminated, with
only one shell. Its popup screenshots, AX and passive logs agree. Minimum-shell
native logs agree with its AX and captures; the original default shell's earlier
main cycle was recorded before the observer existed.

## Visual checks and limits

Inspected native app-only screenshots of main numeric focus and both seeded
popups at both content sizes. Minimum capture is 1920x1264 pixels (960x600 at
2x plus chrome), default is 2560x1664 (1280x800 at 2x plus chrome). Captures
come from `App.getScreenshot()` and remain uncommitted. Main focus ring is visible;
minimum filter scrolling reveals the numeric field. Popup frames/Done and
visible rows fit within the window. Offscreen rows and diagnostic comparisons
check full reached-control bounds against clipping ancestors. Screenshots
alone do not establish traversal or inspect all 32 category rows. No screenshot,
card database or unrelated desktop data is committed.

The offscreen 20-second startup survival assertion passes. The independent
empty-stderr assertion fails on the literal platform font diagnostic in
smoke.txt. Python subprocess timeout replaces unavailable GNU timeout; termination
returncode -15 is cleanup, not exit 124. The survival result is recorded from
TimeoutExpired while the process was still alive. No diagnostic is suppressed.

## Recommendation and outstanding questions

The next product brief should first choose whether the Basic-style client
intentionally includes all enabled filter controls in keyboard traversal on
macOS, overriding text/list platform preference, or retains that preference and
provides an explicit keyboard route to the excluded surfaces. Consistent
all-controls traversal is the most direct candidate supported by this comparison,
but it is a product compatibility decision, not a fix delivered here. Do not
change a user's OS setting. Record any deliberate override in an ADR/architecture
record, assess the API across the supported Qt floor, and rerun native operation
and activation tests before shipping. The setter experiment alone does not
justify a production dependency on an internally documented API.

Before a native correctness claim, independently isolate the intermittent
QTest null-focus/Done-close failures: delivery to QQuickWindow, native activation,
popup focus restoration and timing remain competing explanations. Do not
soften reached sets or clipping checks. Stable native user-session/hardware
operation with an all-controls product candidate remains unverified; Windows,
controllers, accessibility permission changes and duel behavior are outside scope.

# Batch 27 navigation evidence — incomplete native reliability gate

Implementation source is `54ed9d8d9ebcd91e0ccafea9496135cf7173e9d1`, base
`1068ee315006c5d3599e5c2100435aa8b9441967`. Subsequent delivery commits contain
only documents/evidence. Earlier stages are explicitly experimental working-tree
variants, not this final source. Round 026 is preserved verbatim under
`docs/rounds/026-native-filter-navigation/`, from the verifier branch; its
failures and internal-setter experiments remain historical, not shipped fixes.

## Environment and boundaries

macOS 27.0.1 (26A434), arm64; Apple clang 21.0.0; Qt 6.11.1 Cocoa/offscreen,
Basic style; CMake 4.4.3; Ninja 1.13.2; Python 3.13.15 (general checks),
Python 3.9.6 (framework). ocgcore initialized at
`46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57`; it was not modified.
Production startup, app-context, CMake, adapters, engine/model/search semantics,
settings and system keyboard preferences are unchanged. Runtime Tab policy was
text/list flags 3, including successful native observations.

Absolute checkout paths are replaced by `<repository>` / `<primary-checkout>`.
Diagnostic trailing spaces are encoded as `\x20`; messages are not removed.
Local databases, build outputs and screenshots are untracked. Copied diagnostic
scripts are evidence, not new test registration or product input-loop code.

## Rerun the required checks

Run in the worker checkout; native tests take foreground focus. Do not perform
concurrent desktop interaction during native rows. Read results individually;
a passing later run never invalidates a recorded failure.

```sh
cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
cmake --build ui/build --parallel
ctest --test-dir ui/build --output-on-failure
mkdir -p ui/build/b27-evidence
python3 docs/batches/27-native-filter-navigation-evidence/b27-final-checks.py
python3 docs/batches/27-native-filter-navigation-evidence/b27-general-checks.py
```

The final-check script separately invokes every named traversal row under each
platform and the unavailable-controls check. Its historical SOURCE marker names
the recorded source, not an automatic attestation of a future rerun; record
`git rev-parse HEAD` alongside any new output. General commands/output/exits are
in `general-0.txt` through `general-7.txt`: both generators `--check`, README
status, unittest discovery, replay `--update`, unchanged golden diff, framework
and whitespace checks. All exited 0. Discovery reported 133 tests, 11 skips:
one Windows ACL denial requirement, ten absent/freshness-qualified semantic-trace
checks. Exact names/reasons are preserved. Push-guard tests ran. No semantic or
replay result is claimed as unchanged duel behavior.

Configure/build succeeded; `final-ctest.txt` has two passing CTest targets.
Final offscreen traversal rows and unavailable-control checks passed. Final
Cocoa minimum-forward failed when the window deactivated after Clear; the other
three Cocoa rows passed. See `final-cocoa-minimum-forward.txt`, including
`null focus; window active false`. This is a required-row failure, not excluded
from the result. Do not summarize the final screen matrix as green.

### Strict smoke

`final-smoke.txt` records the separate workflow-equivalent assertions. On this
host the process was still running after 20 seconds: survival assertion exit 0.
Empty stderr assertion exit 1: Qt emitted the missing `Sans Serif` alias warning.
Cleanup SIGTERM exit -15 is neither a successful smoke exit nor a timeout code.
The smoke wrapper exited 1. There was no suppression or font substitution.
On hosts with `timeout`, use the workflow's literal survival/empty-stderr checks:

```sh
QT_QPA_PLATFORM=offscreen timeout 20 ./ui/build/edopro_next_shell >ui/build/smoke.stdout 2>ui/build/smoke.stderr
```

Capture status immediately, require 124 independently, then require
`test ! -s ui/build/smoke.stderr` independently. On macOS without GNU timeout,
launch with subprocess, wait 20 seconds, assert `poll() is None`, separately
assert stderr bytes are empty, then terminate/reap. Do not count cleanup as
survival. The production workflow command and semantics are authoritative.

## Failures and investigation sequence

Each file prefix retains its literal output. No unsuccessful attempt was removed.

| Prefix | Observation / consequence |
| --- | --- |
| baseline | Unchanged source: offscreen passes; every Cocoa row misses ten non-text main controls. |
| explicit-chain | Links alone do not normalize Shift+Tab; reverse and Cocoa popup failures. |
| backtab / keyencoding | KeyNavigation.BeforeItem dispatches Tab forward before a later Keys handler; normalize both backward encodings before attached navigation. |
| synchronized | Shared handler/opening/restoration improved operation; Cocoa reverse still misses lower four controls at implicit Results boundary. |
| listrole | Accessible.List repairs that boundary; all traversal rows pass. Initial skip fixture lacked a catalog and disabled everything; preserved `skips-offscreen.txt`, then supplied a synthetic catalog. |
| strict | Newly added unconditional visualFocus-property assertion fails TextField, which derives TextInput. Retain activeFocus for every item; require visualFocus wherever exposed. |
| focusproperty | Corrected assertions retain a native category traversal failure with window inactive/null focus. |
| ls | LaunchServices test launch fails qWaitForWindowActive; open exit 0 is not test success. |
| appactive | Exposure/application/window activation checks still see Cocoa key-window resignation mid-traversal. |
| eventloop | ApplicationWindow plus qExec scheduled inside app.exec passes; this alone is not causal evidence. |
| controlled-loop-* | Full two-by-two event-loop/window-type comparison: every combination passes. Does not isolate cause or establish a cure. Restore build succeeded. |
| final-* | Exact implementation: required screen Cocoa minimum-forward fails again. Successful full-shell rows remain a separate result. |

Qt 6.11.1 Cocoa notification logs show `NSWindowDidResignKeyNotification` before
null focus and failed window-active assertions, without a preceding new popup
window creation/hide event in those logs. They do not identify the initiating
OS/application event. No claim that unrelated apps caused the failure is made.
Qt's requestActivateWindow only makes its responder/window key; startup activation
and manual-event processing differ from a running GUI event loop, but controlled
runs do not establish either as the cause. Relevant primary source:

- [Cocoa window requestActivateWindow](https://github.com/qt/qtbase/blob/v6.11.1/src/plugins/platforms/cocoa/qcocoawindow.mm)
- [Cocoa application delegate activation](https://github.com/qt/qtbase/blob/v6.11.1/src/plugins/platforms/cocoa/qcocoaapplicationdelegate.mm)
- [Cocoa event dispatcher](https://github.com/qt/qtbase/blob/v6.11.1/src/plugins/platforms/cocoa/qcocoaeventdispatcher.mm)

The exact shell implementation behaves correctly in the observed active native
session. Reliable automatic native activation remains unresolved; Brain must
adjudicate this incomplete gate. No private API or OS-setting workaround was added.

## Regression mechanisms

`b27-popup-regression.py` temporarily removes popup `onOpened` focus, then
separately removes Results' Accessible.List declaration. The offscreen forward
row fails the actual first-control assertion; Cocoa reverse fails strict lower
filter accounting respectively. Both processes exit 1, builds succeed, restoration
build exits 0. These exercise mechanisms that fail, not lists of successful cases.
Run mutation/comparison scripts only in an otherwise clean disposable checkout:
they temporarily edit owned sources and restore in `finally`.

## Real-shell probe and native interaction

Use the carried `attachments/make_probe.py` in a fresh ignored output directory.
It asserts every production QML byte matches, adding test-only full-shell
registration and passive main.cpp observation to a disposable copy. Build its
`ui/` with the same Debug/WERROR/UI_TESTS flags and SQLite/Qt environment. No
tracked CMake file is changed. Run every row separately with
`EDOPRO_DIAG_FULL_SHELL=1 QT_QPA_PLATFORM=offscreen` and then `cocoa`.
**Leave EDOPRO_DIAG_POLICY and EDOPRO_DIAG_SURFACE unset.** The historical generator
contains a conditional diagnostic internal setter; that branch was not executed
in batch 27's final shell or native observations. All eight final full-shell rows
passed (`final-shell-*`), at 960x600 and 1280x800; build/configure outputs retained.

Native desktop session used the same byte-matched production QML and unchanged
startup with passive focus/input logging. A local `Batch27Navigation.app` bundle
hosted the disposable executable, launched with Cocoa, diagnostic size and
`--card-db` / `--start-screen decks`. Database generation uses the historical
synthetic-card script: 40 synthetic Monster cards, no licensed runtime assets.
The computer-use app handle explicitly targeted that EDOPro bundle.

At each size: click Type once, Down/Return to Monster; click Search once as the
initial seed. Tab through every main control and Results; Shift+Tab through the
reverse route. From Search, backward navigation reaches Effects without another
mouse seed. Space opens to Category 1; traverse all categories plus Done both
ways. Operate Category 32 with Space: checkbox becomes selected and Effects(1)
reflects the model. Done via keyboard closes to trigger; Space reopens at first
control; Escape restores trigger. Tab to markers, Space opens first marker;
cycle all eight plus Done both ways, operate first marker with Space (selected),
then keyboard Done, reopen, Escape. No popup or restored-trigger mouse seed.

`native-minimum.txt` and `native-default.txt` record actual delivered spontaneous
keys, active key window at every delivered event, all category/marker destinations,
Done and restored focus. Each has 201 delivered key events including modifier
presses. These are desktop automation inputs, not physical keyboard verification.
Final null focus is after explicitly quitting the probe, not mid-traversal.

Inspected native images at each size show Search/Effects focus and full Effects
and marker popup bounds, including focused/selected Category 32 and first marker.
Screenshots are local/untracked: 1920x1264 and 2560x1664 pixels with display scale
and chrome. Visual samples do not prove every ring's appearance. Strict geometry
assertions separately cover focused controls and ancestor clipping. QTest captures
remain in `ui/build/b27-captures`; CI's existing `ui-screenshots` artifact is unchanged.

## Delivery

No broad keyboard/controller/accessibility parity, Windows or engine claim.
No production changes in `ui/src`, CMake, `client`, `data`, `policy`, `gframe`,
`integration` or ocgcore. Brain review is required; Builder never self-accepts or
merges. CI must be read at the final pushed SHA and reported separately from
this fixed-source local evidence. Successful CI cannot resolve Cocoa or strict
macOS stderr failures recorded here.

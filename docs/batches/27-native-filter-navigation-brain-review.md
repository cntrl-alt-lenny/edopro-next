# EDOPro Next · batch 27-native-filter-navigation · Brain review

## Done

Reviewed exact delivery `c78b1485d40f5ed73da405eabd78a9c9536e6977`, against
default branch `1068ee31`. Normal path. Inspected production QML, harness,
screen-test changes, architecture/ADR and retained failure evidence. Changes
use public QML focus/navigation APIs; no private setter, engine modification,
search semantics change or batch 28 ownership collision was found.

Disposition: **return for native reliability diagnosis; do not accept or merge**.
The implementation and stricter assertions are useful, but required native
evidence remains red. PR #48 stays draft.

## Checked

Brain reconfigured Debug/WERROR/UI_TESTS, rebuilt and ran
`ctest --test-dir ui/build --output-on-failure`: exit 0, both targets passed.
Required PR CI passed at the delivery SHA. Local macOS arm64 / Qt 6.11.1.

Fresh separate Cocoa row processes at that exact source:

| Row | Exit | Observation |
|---|---|---|
| minimum-forward | 0 | Popup entry, traversal, operation and restoration passed |
| minimum-reverse | 1 | Effects traversal lost activation and active focus |
| default-forward | 1 | Effects traversal lost activation and active focus |
| default-reverse | 1 | Effects traversal lost activation and active focus |
| unavailable controls | 0 | Disabled/hidden controls skipped |

Every failed row reaches `QVERIFY(active)` at screen-test line 1095 after
`null focus; window active false`; the real failure is preserved. A successful
minimum-forward repeat does not resolve the Builder's recorded failure.

Independent 20-second offscreen smoke: survival assertion exit 0, empty-stderr
assertion exit 1, wrapper exit 1. Actual diagnostic: missing `Sans Serif`
font-family alias. Reproduced on batch 28 and on unchanged baseline production
sources, so this is a shared existing gate, not a navigation regression claim.
Inspected the stored minimum-size Effects capture: lower focused category is
visible with a focus outline. See the adjacent Brain evidence directory.

Coordination addendum: Brain's two offscreen smoke processes were terminated
and reaped before launching the three failing native rows. Brain launched no
competing Cocoa peer during that matrix, but recorded no foreground PID
history; other desktop activity remains unknown. Later follow-up diagnostic
files reproduce the failure signature with a deliberate Cocoa peer launch.
That establishes a possible cause class, not attribution of historical failures
or acceptance of the still-in-flight follow-up source.

## Not checked

Physical keyboard, fresh desktop-delivered shell interaction, controller,
Windows, Qt-floor execution, all visual states or engine behaviour. The cause
of native window resignation is not established; no external-app explanation
is inferred. Linux CI does not close a Cocoa gate.

## Failed or blocked

**P1: native reliability.** Establish whether the failure originates in
application, test activation/event delivery or an external event, with controlled
evidence that can fail. Correct the causal class. Preserve destination,
visibility, operation and window-active assertions; do not hide failures by
retries or unproven startup changes.

**P1: strict smoke.** Separate Normal batch 29 owns font-resolution/startup
work. Native diagnosis can proceed immediately without that batch merging.
Coordinate any shared bootstrap/helper requirement through Brain.

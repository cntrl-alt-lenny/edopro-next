# EDOPro Next · batch 27-native-filter-navigation · integrated Brain review

## Done

Reviewed exact integrated head `08f4e630d83918de41659d7b8c5e4667e2edbdb4` against
merged font/default head `af334154`. Normal path. Production navigation,
TestHarness and screen driver are byte-identical to reviewed `f96ac235`;
font sources match default. No new implementation was introduced by integration.

**Return for correction; PR #48 stays draft.** Strict font smoke is now clear,
but native activation loss still occurs at this integrated head. The new
failure is current evidence, not merely an unresolved historical report.

## Checked

Fresh macOS arm64 / Qt 6.11.2 Debug/WERROR/UI_TESTS configure/build and full
CTest: exit 0; all three targets passed. Real-shell offscreen smoke survived
20 seconds with empty stderr; independent assertions and wrapper exited 0.
Required branch/PR CI (runs `38216314108`, `38216318162`) passed at this head,
including Linux Qt 6.8.3 strict smoke. Linux captures at both harness sizes
show the focused lower Effects control visible within its popup viewport.

Separate preselected Cocoa rows, no retries:

| Passive tracing | Row | Exit |
|---|---|---|
| Enabled | minimum/default forward/reverse; unavailable controls | All 0 |
| Disabled | minimum-reverse | 0 |
| Disabled | default-forward | **1** |

The failing row reaches all Effects controls, then loses active focus/window
activation while navigating to the lower category. `session.keyClick(Tab)`
fails at screen-test line 1237. Foreground observer changes from test PID
33178 to Claude PID 1057 at epoch `1791790520.6034322`. The initiating action
is not established by that transition alone; event logging was disabled in
this preselected row. No competing shell was launched by Brain in the matrix.

A separate, labelled peer-launch negative control at the same head exits 1
with the activation-loss assertion; its expected-failure predicate exits 0.
This confirms guard sensitivity and does not explain or erase the quiet-row
failure. Test/peer processes were terminated and reaped before other work.

## Not checked

Failure initiation, whether passive tracing affects timing, physical keyboard,
controller, Windows, minimum-Qt execution, every visual state or duel behaviour.
macOS Qt 6.8.3 link failure from the installed SDK's missing AGL is retained.

## Failed or blocked

**P1: current native reliability.** Builder must diagnose this exact-head
untraced failure and correct the causal class without weakening the latch,
destinations, clipping or model-operation assertions. The foreground sequence
is a lead, not attribution. Preserve every failed attempt and avoid
retry-until-green. Search #49 and independent label work do not depend on this.
See `27-integration-brain-evidence/` and the new continuation brief.

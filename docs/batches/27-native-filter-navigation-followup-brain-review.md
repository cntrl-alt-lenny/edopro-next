# EDOPro Next · batch 27-native-filter-navigation · follow-up Brain review

## Done

Reviewed exact delivery `f96ac2354f255b0d830b1219e316a14b458e6ea2`, including
test-source commit `4d1a7e2c30965e31ee85d2e550a48f5480fa02a0`. Normal path.
Compared the follow-up against previously reviewed `c78b1485`. Production
navigation is unchanged. Passive tracing consumes no input; the activation
latch covers every navigation key and final completion without reactivating
the window. Existing destination, clipping and model-operation checks remain.

No blocking defect found in the follow-up itself. **Batch acceptance remains
pending font integration and exact-new-head verification; PR #48 stays draft.**
Historical activation initiation remains unknown. Neither this review nor a
future integrated acceptance may claim a retrospective cure or attribution.

## Checked

Brain rebuilt Debug/WERROR/UI_TESTS and ran offscreen CTest: exit 0, both
targets passed. Native Cocoa processes ran sequentially with a foreground-PID
monitor: minimum/default forward/reverse and unavailable-control rows all
exited 0. Repeated minimum-reverse and default-forward with passive tracing
disabled: both exited 0. These were preselected original failure cases, not
retry-until-green. The session latch remains enabled in both modes.

Inspected actual outputs and foreground timestamps. Traced rows retain native
application/window/key events; separate untraced runs avoid relying on tracing
timing. Foreground transitions at launch/teardown are retained, including the
brief loginwindow transition between untraced rows. This is evidence of these
runs, not a general desktop-history guarantee. Local macOS arm64 / Qt 6.11.1.
Commands, real output and exits are in the adjacent follow-up Brain evidence.

Read Builder's controlled peer-launch failures and foreground attribution:
each injected activation loss fails the latch. Popup-entry-focus and
category-model mutations fail separate product assertions. The retained
Builder-caused unavailable-control launch conflict is explicitly attributed;
its isolated successor passes. No failure was deleted or recast as success.

Required PR CI run `37938789003` and branch run `37938782698` passed at the
exact delivery. Existing font diagnostics still appear in this branch's
screen harness; batch 29 fixes real-shell startup independently.

## Not checked

Historical initiating event; physical keyboard/controller; Windows; execution
at the Qt floor; every visual state; engine behaviour. Quiet QTest passes and
desktop automation do not prove uninterrupted activation in all situations.
No competing foreground product was launched by Brain during these rows.

## Failed or blocked

The original missing-font strict-smoke failure remains open on this branch.
Integrate the accepted font delivery, then rerun strict smoke, full UI checks
and native rows at the resulting SHA before Brain adjudicates final acceptance.
Keep the historical evidence gap explicit. Search batch 28 has no dependency
on resolving that gap. Both Workers have finished and released the native slot;
no further diagnostic task is assigned merely to keep them active.

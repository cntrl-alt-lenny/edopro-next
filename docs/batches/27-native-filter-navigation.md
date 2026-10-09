# Batch 27: native filter navigation

## Done

Retained the public QML navigation implementation and all historical evidence.
Follow-up adds passive timestamped native application/window/input tracing and
an uninterrupted-activation latch across every navigation key, including popup
operation/restoration. Later reacquisition cannot hide a loss. No production
navigation, startup/font/Theme, CMake, adapter, engine or system changes in this
follow-up. Brain should keep shared status incomplete until historical activation
initiation is established; PR #48 remains draft.

## Checked

Exact follow-up test source: `4d1a7e2c30965e31ee85d2e550a48f5480fa02a0`, continuing
`c78b1485d40f5ed73da405eabd78a9c9536e6977`. Subsequent commits are documentation
and evidence. [Follow-up commands, outputs and limits](27-native-filter-navigation-followup-evidence/README.md)
retain actual exits, skips and failed attempts.

UI Debug/WERROR/UI_TESTS configure/build and offscreen CTest passed. Every
traversal row ran separately offscreen/Cocoa and passed, as did byte-matched
full-shell rows. Unavailable controls passed offscreen; the initial Cocoa run
failed an attributed diagnostic launch conflict, preserved verbatim. A separate
isolated Cocoa run passed after removing that known conflict.

Controlled peer Cocoa launch during Effects switches macOS foreground PID,
then resigns the native key window and fails every row. The new latch fails
all corresponding trials. Removing popup entry focus or category-model update
still fails distinct strict product assertions; restoration build passed.
Actual desktop-delivered main/popup routes, operations and restoration were
observed at both sizes. Inspected native focus/selected popup states and four
QTest focus captures; no broad visual-parity claim.

Generators, README check, Python discovery, golden reproduction/unchanged diff,
framework and whitespace passed. Discovery keeps Windows ACL/semantic-trace
skips. PR-evidence check passed with the real/proposed body after an empty-stdin
invocation failed. Replay does not establish engine behaviour. Delivery CI is
reported separately at the pushed SHA.

## Not checked

The initiating event in the earlier delivery/Brain failures: foreground history
was absent. Neither quiet passes nor injected peer launches identify it.
Physical keyboard, controller, Windows, actual Qt-floor execution, every visual
state and engine behaviour remain unverified. Targeted desktop automation does
not establish uninterrupted native activation.

## Failed or blocked

**STOPPED, unresolved historical cause.** The controlled external-launch class
is established and activation-precondition enforcement strengthened, but no
product/environment change is claimed to cure the uninstrumented historical
failures. Brain confirms historical foreground activity is unknown. A new
Builder-caused launch overlap is preserved with direct PID attribution; native
processes/desktop interaction are finished and the slot released through Brain.

Strict smoke: survival exit 0, empty-stderr exit 1, wrapper exit 1 for the missing
Sans Serif alias; batch 29 owns that gate. Setup errors and unsuccessful controls
remain recorded. No retries-until-green, softened assertions, self-acceptance or
merge.

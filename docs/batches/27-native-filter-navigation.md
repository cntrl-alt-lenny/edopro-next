# Batch 27: native filter navigation

## Done

Implemented public QML navigation across enabled filters and popup controls,
both backward key encodings, unavailable-control skipping, first-control popup
entry and trigger restoration. Strengthened strict screen assertions and aligned
the test window/event loop with GUI startup. Added proposed ADR 0013 and
architecture limits. Preserved round 026 verbatim and unsuccessful batch 27
attempts. No batch 28 boundary changes were needed. Brain should update shared
status only after adjudicating this incomplete native reliability gate.

## Checked

Implementation source: `54ed9d8d9ebcd91e0ccafea9496135cf7173e9d1`, based on
`1068ee315006c5d3599e5c2100435aa8b9441967`. Documentation commits add no code.
[Evidence and rerunnable commands](27-native-filter-navigation-evidence/README.md)
record output, exits, environments, skips and failed attempts.

UI Debug/WERROR/UI_TESTS configure/build and CTest succeeded. Every traversal
row ran separately offscreen and Cocoa; full-shell probe rows passed on both.
Actual native desktop-delivered Tab/Shift+Tab, popup Space/Done/Escape,
close/reopen and model operation passed at both sizes with default policy.
Inspected focus and popup captures; databases/captures remain untracked.
Removing popup opening focus or the Results accessibility role makes relevant
strict assertions fail. Generators, README check, Python discovery, golden
reproduction/unchanged diff and framework/diff checks passed. Discovery retains
Windows ACL and absent semantic-trace skips; replay is not duel evidence.

## Not checked

Physical keyboard hardware, controllers, Windows, actual Qt 6.5/6.8.3 execution,
all controls' visual appearance and broad native/accessibility parity. Public
API source compatibility was inspected at floor/CI versions. Engine behavior
was not tested or changed. CI at the pushed delivery SHA is reported in the
handoff; it cannot replace native evidence.

## Failed or blocked

STOPPED for Brain review: final screen-harness Cocoa minimum-forward lost native
activation after Clear and failed strict traversal. Other final rows passed;
full-shell and controlled startup comparisons also passed, which does not
explain the intermittent failure. Native key-window resignation precedes null
focus, but its initiating cause remains unresolved. Startup changes are not
claimed as a causal fix.

Strict smoke survival passed; independent empty-stderr assertion failed on Qt's
missing Sans Serif alias warning. No suppression or system-setting change.
The useful implementation is pushed for exact review, not self-accepted.

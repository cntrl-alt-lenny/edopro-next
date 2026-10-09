# Parallel Builder dispatch

Owner direction, 2026-10-09: plan two active Builders where useful independent
work exists. This is a product-progress pilot using framework 4.0.1, not a
new orchestration layer. Both batches are Normal under AGENTS.md's UI rule;
Brain independently reviews/re-runs evidence, owner approves merges. If a
batch needs a Checked boundary, stop that expansion and ask Brain to arrange
the independent review.

| Batch | Useful outcome | Exclusive ownership |
|---|---|---|
| [27-native-filter-navigation](../batches/27-native-filter-navigation-brief.md) | Reliable native macOS Tab/Shift+Tab across enabled filters and popups | Existing QML/navigation/startup files; screen tests; deck-builder UI architecture; round 026 evidence; ADR 0013 if needed; own summary |
| [28-card-code-search](../batches/28-card-code-search-brief.md) | Find a known card by its decimal code through the existing search box | Search-results/input adapters; dedicated new tests; scoped UI CMake registration; card-code architecture; ADR 0014; own summary |

Brain owns shared status pages, scope/semantic decisions, build-file handoffs,
integration order and acceptance. Neither Builder edits the other's files or
merges branches. Keep independent worktrees/build directories. Native desktop
interaction belongs to batch 27; batch 28 uses headless/offscreen evidence
and does not compete for keyboard/focus.

## Genuine dependencies

Both can begin from the same current default branch. Neither implementation
depends on the other, on review of historical round 026, or on this planning
PR merging. Read the pushed briefs through `git show` when they are not yet
on the default branch; do not merge the planning branch into product branches
just to read it. Read round 026's evidence without first merging its obsolete
framework/rules.

Batch 28 owns the CMake seam because it needs a new dedicated test target.
Batch 27 can use existing files/targets. If its real solution needs new source
registration, Brain coordinates that specific change with batch 28 or revises
ownership. Only the affected build/validation step then waits; unrelated
implementation continues. Any new QML control must join batch 27's screen
layout/keyboard coverage before acceptance.

Delivery order is not prescribed: review a finished batch while the other
continues. Integrate independently accepted PRs serially under owner-approves.
A changed default branch requires updating the remaining branch and checking
its new exact head; no old review is carried over blindly. Do not weaken a
check or treat a successful repeated run as an explanation of earlier failure.

## Judge the pilot

At the two-week scorecard, record accepted user-visible progress, owner relay
and coordination effort, corrective batches and review/approval waits. Do not
count unmerged implementation as shipped or token use as value. Retain two
Builders only while their independent work is reducing the delivery queue;
choose a single Builder when decomposition would create churn or idle tasks.

## Follow-up after delivery review, 2026-10-09

Brain reviewed both deliveries; neither is accepted. Native activation loss
was reproduced across multiple Cocoa rows. Card-code implementation review
found no blocking code defect, but the strict macOS font-smoke gate is red
on both deliveries and unchanged baseline production sources.

| Active Worker | Ownership now | Entry point |
|---|---|---|
| Navigation Worker | Continue batch 27: navigation QML, screen tests/TestHarness, causal activation diagnosis | [Follow-up brief](../batches/27-native-filter-navigation-followup-brief.md) |
| Previous search Worker | New batch 29: Theme typography, font startup/helper, dedicated new font test and scoped CMake registration | [Font-resolution brief](../batches/29-platform-font-resolution-brief.md) |

This transfers font-related Theme/startup ownership from the original broad
navigation allocation to batch 29; navigation tests remain with batch 27.
Brain coordinates any shared bootstrap/helper requirement. Batch 28 stays
frozen in draft PR #49. Its acceptance depends on an accepted font fix and
new-head checks, not on completing native navigation. Both follow-ups can
start now. Only integration/revalidation waits for the relevant delivery.

### Shared macOS desktop sequence

Batch 27 takes the first exclusive Cocoa/native interaction slot for its
controlled activation diagnosis and shell traversal. Batch 29 continues
compilation, explicitly offscreen tests/smoke/captures and inspection of saved
images. It defers foreground Cocoa launches and desktop automation until
batch 27 releases the slot. Then batch 29 can perform any necessary native
font/visual check while batch 27 returns to headless work.

Before native runs, identify other project GUI processes and their actual
platform; record unexpected overlap rather than killing another Worker's
process. Release the slot after owned GUI processes exit and input/capture
work finishes, preserving both passing and failing output. Brain coordinates
the handoff through the owner or explicitly authorised chat messages.

A simultaneous shell was observed during diagnosis; it was no longer running
when Brain checked. Overlap is a possible confound, not an established cause
of historical failures. Controlled interference can explain a failure
signature without proving what initiated an earlier window resignation.
Assertions and acceptance requirements remain intact.

### Delivery review completed, 2026-10-09

Both follow-ups have finished. Font batch 29 is independently accepted for
owner-approved merge in PR #50, with green required checks and a clean strict
macOS real-shell smoke. Navigation follow-up review found no blocking defect
in the new tracing/activation latch; controlled native rows pass, including
two preselected rows without passive tracing. Historical activation initiation
remains unknown and must stay explicit.

The next genuine integration dependency is the font delivery: search #49 and
navigation #48 need it and checks at their new heads before acceptance. Search
does not wait for native historical attribution. The planning PR need not merge
first. Brain owns integration order, exact-head reviews and owner merge cards;
Workers have released the desktop slot. No extra task is assigned solely to
keep two seats occupied. See the independent batch 29 and batch 27 follow-up
Brain reviews for checked outcomes and remaining limits.

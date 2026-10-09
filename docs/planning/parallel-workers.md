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

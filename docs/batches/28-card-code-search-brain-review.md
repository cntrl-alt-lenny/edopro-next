# EDOPro Next · batch 28-card-code-search · Brain review

## Done

Reviewed exact delivery `c647e663233705425aeea8fd70199792a0615361`, against
default branch `1068ee31`. Normal path. No blocking implementation defect
found: checked ASCII uint32 parsing, leading zeroes, catalogue existence,
retained structured/visibility filters, original-text fallback, result ranking
and cap, and refresh signals match the dispatch contract. ADR 0005 remains
unchanged. Dedicated tests and CMake registration respect assigned ownership.

Disposition: **implementation review clear; acceptance blocked by strict
macOS smoke**. Brain prepared draft PR #49; no merge or acceptance occurred.
This delivery does not depend on completing batch 27's navigation work.

## Checked

Brain reconfigured Debug/WERROR/UI_TESTS, rebuilt and ran
`ctest --test-dir ui/build --output-on-failure`: exit 0, all three targets
passed. Dedicated test executable: 17 passed, no failures/skips. Required
branch CI succeeded at the delivery SHA; the newly opened PR's CI remains
a separately checked merge gate. Local macOS arm64 / Qt 6.11.1.

Independent negative control at pre-feature test commit
`3da9b9d1b5bbb4c851a9470909e17e46d32b9789`: configured/built the dedicated
target, then ran it. Exit 3, with known-code, filter and catalogue-reload
assertions failing; 14 passed, three failed. This proves regression sensitivity.

Re-read `gframe/bufferio.h`'s `GetVal` and `deck_con.cpp`'s numeric shortcut
and later property-filter loop; source quotations match. Checked overflow,
untrimmed upstream input and filter bypass are explicitly distinguished in
the architecture/ADR. Verified catalogue reload rebuilds the snapshot before
emitting the refresh signal; no stale pointer is introduced.

Independent 20-second offscreen smoke: survival assertion exit 0, empty-stderr
assertion exit 1, wrapper exit 1. Actual diagnostic: missing `Sans Serif`
font-family alias. The same smoke failed at the pre-feature commit, whose
production UI sources match the default branch. See adjacent Brain evidence.
`fw.py check` and whitespace checks passed.

## Not checked

Real card database, native interaction, performance guarantee, Windows or
full legacy grammar. No duel-behaviour claim. Headless regression success
does not waive the separate empty-stderr requirement.

## Failed or blocked

**P1: shared strict smoke gate.** Batch 29 owns font resolution. Keep this
feature unchanged while that independent fix proceeds. After integration,
update/review the new exact head and rerun the dedicated regressions, full
UI evidence and required CI. Do not wait for navigation acceptance.

The reported model-tester warning is retained; assertions pass, and the new
input adapter introduces no new role/type contract. No additional blocking
finding follows from that diagnostic alone.

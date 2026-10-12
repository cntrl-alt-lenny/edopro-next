# EDOPro Next · batch 28-card-code-search · integrated Brain review

## Done

Reviewed exact integrated head `7af65e44e39ee438aecd554d23c69aeb95d83d18` against
merged font/default head `af334154`. Owner approved only font PR #50; search
PR #49 remains unmerged. Normal path. **Accepted for owner-approved merge.**

Compared against the previously reviewed `c647e663`: adapter and dedicated
regressions are byte-identical. Font/startup/Theme/dedicated font-test sources
match default. Brain resolved the CMake append conflict by retaining the
original dedicated search target and separate font-test subdirectory. No new
search rule, UI control, dependency or framework change was introduced.

## Checked

Fresh macOS arm64 / Qt 6.11.2 configure/build under Debug/WERROR/UI_TESTS:
exit 0. Full CTest: exit 0, all four targets passed. Dedicated card-code
regressions: exit 0; all 17 cases passed without skips. Strict real-shell
offscreen smoke survived 20 seconds and had empty stderr; both independent
assertions and wrapper exited 0. Fonts/real-shell typography tests passed.

Required branch and PR CI are green at this head (runs `38216360188` and
`38216365472`), including strict Linux Qt 6.8.3 smoke. PR reports CLEAN.
Inspected fresh Linux CI minimum/default real-shell preview captures:
readable title/body wrapping, result elision, numeric text and search focus.
Fresh local typography captures were generated; no broader appearance claim.

The previous pre-feature negative control remains relevant because the
feature/regression sources are unchanged; it is not presented as a new-head
execution. New-head full tests and strict smoke close the original shared
font blocker independently of navigation acceptance.

## Not checked

Real card database, native search interaction, Windows runtime, minimum-Qt
execution, complete legacy grammar or engine behaviour. macOS Qt 6.8.3 did
not link against the installed SDK: the official Qt package requires AGL.
That failed attempt is retained, not counted as successful platform evidence.

## Failed or blocked

No remaining search blocker found. Owner approval is required for #49;
recheck head, base and CI immediately before merging. The navigation failure
is separate and does not hold this feature. Setup failures and evidence
sanitization correction are recorded in `integration-2026-10-12-setup.txt`.
Commands, real output and exits are in `28-integration-brain-evidence/`.

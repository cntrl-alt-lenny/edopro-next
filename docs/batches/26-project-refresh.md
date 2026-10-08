# Project and framework refresh, 2026-10-08

Small path; Brain's own housekeeping. No production behavior change.

## Done

Fetched the default branch and read its current rules. Adopted released
framework 4.0.1 from tag `v4.0.1` at
`06a7d45e45751c40809d6644ea4503ec61cef709`; dry-run and update exited 0.
Only the framework manifest pin changed; copied framework files matched.
Corrected the state/roadmap's stale outstanding structured-filter claim,
clarified capabilities' historical build evidence and prepared batch 27.

Round 026 at `f610dcb30bae8b22d43c474bd55db6fa4990d09f` has both reports,
but is unmerged. Read its brief, reports and actual diff. It is an investigation,
not a shipped navigation fix. Required macOS empty stderr failed; native popup
reliability remains unresolved. Its evidence must carry into the continuation;
this refresh does not accept or merge that round. Round 024's extra unmerged
branch adds a historical failed-start report, not a product change. Existing
seat checkouts and branches were preserved.

Recorded the independently reproduced fictitious "EADME" batch in
`docs/framework-feedback/2026-10-08-legacy-round-status.md`. No framework patch
was made here. Brain recommends app-local all-controls keyboard navigation
using supported APIs; this is a proposal, not an owner-approved policy record.

## Checked

At refresh source commit `d233fa2fd8eb63bc65aaa330a899c5ed4155c930`, macOS:

| Command | Literal result | Exit |
| --- | --- | --- |
| `python3.13 tools/generate_messages.py --check` | `message table up to date (96 ids)` | 0 |
| `python3.13 tools/generate_protocol_constants.py --check` | `protocol constants up to date (187 values)` | 0 |
| `python3.13 tools/generate_readme_status.py --check` | `README status block is up to date` | 0 |
| `python3.13 -m unittest discover -s tests -v` | `Ran 133 tests in 2.038s`; `OK (skipped=11)` | 0 |
| `python3.13 tests/test_replay_trace.py --update` | wrote the three existing structural traces | 0 |
| `git diff --exit-code -- tests/golden` | empty | 0 |
| `python3 tools/fw.py check` | `0 error(s), 0 warning(s)` | 0 |
| `git diff --check`; `git diff --check origin/master...HEAD` | empty | 0 |
| `git diff --exit-code origin/master HEAD -- client data policy ui gframe integration ocgcore tools/fw.py docs/agents/FRAMEWORK.md docs/agents/roles tests/test_framework.py` | empty | 0 |

Skipped: `TestBinaryFreshness.test_unreadable_source_tree_fails_closed` requires
Windows ACL denial. Missing fresh semantic binary skipped
`TestSemanticGoldens.{test_fixtures_exist,test_rendering_is_deterministic,test_traces_match_golden}`
and `TestSemanticQuality.{test_committed_fixtures_are_semantically_complete,test_coverage_accounts_for_every_packet,test_model_invariants_hold_at_the_end,test_no_environmental_leakage,test_no_packet_is_malformed_or_unknown,test_query_stream_coverage_is_real_and_clean,test_something_is_actually_decoded}`.
No push-guard test skipped. Structural goldens do not prove duel behavior.

Re-ran the existing Verifier checkout's unchanged screen binary for
`filterTabTraversalStaysVisible:minimum-forward`: offscreen exited 0,
`Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted`; Cocoa exited 1,
`main missing: attributeCombo, cardTypeCombo, categoriesButton, clearFiltersButton, filtersToggle, limitationCombo, linkMarkersButton, nonOfficialCheck, raceCombo, subTypeCombo`.
This corroboration uses the existing build, not a fresh UI build or physical
keyboard. Production source trees match current master.

Re-read pinned Qt Cocoa policy, Quick candidate filtering and the experimental
setter's internal documentation from the primary sources cited by round 026.
Live GitHub check runs were successful at round 026's exact report tip;
upstream baseline skipped. Protection still requires PRs and strict checks,
enforces admins and disallows force-push/deletion.

## Not checked

No new UI/engine build, live native shell session, physical keyboard,
controller, Windows, Steam Deck, artwork or engine re-simulation. Final
report-commit CI must be checked live before merging. The scorecard's first
two-week interval is not due yet.

## Failed or blocked

The Cocoa reproduction failed as expected; it is not a passing product test.
Framework status gives the wrong batch name. The upstream remote is absent
in this clone; no remote or repository setting was changed. Merge remains
subject to owner approval under AGENTS.md.

Brief-ID: 012-2026-09-21-evidence-freshness-reland

Status: accepted

## MODE: IMPLEMENTATION

## Goal

Re-land Brief 009 as its own reviewed round on the current `master`, and
close the two evidence-freshness defects it names. Brief 009's full problem
statement, investigation questions and acceptance criteria are in
[`archive/009-2026-09-01-evidence-freshness.md`](009-2026-09-01-evidence-freshness.md)
and are **incorporated here by reference**. Read that file first; this brief
states only what has changed since it was written.

The two defects, in one line each:

- **A.** A PR body's quoted evidence figures silently stop describing the
  range that will merge whenever `strict: true` forces a branch update.
- **B.** `tests/test_semantic_trace.py`'s `find_binary()` silently prefers a
  stale `edopro_next_semantic_trace` binary, with no freshness check.

## Why this is next

Brief 011 is merged, and this was queued behind it. Brief 009's work was
delivered as PR #26 on `meta/evidence-freshness` at `1438f934`, based on
`9005f950`. It was closed unmerged because it collided with the framework
adoption (PR #27), and **it was never independently reviewed**. Treat it as
prior art to evaluate, not as accepted work.

## What changed since Brief 009 was written

- `master` has moved from `9005f950` to the adopted framework and Brief 011.
  `AGENTS.md` in particular was substantially rewritten by the adoption, so
  the delivered `AGENTS.md` change cannot be applied as-is.
- The shared framework's canonical files under `docs/agents/`, and
  `tools/checkout.py` and `tools/report.py`, are now installed and must stay
  byte-identical to the framework version this repository adopted.
  Brief 009's scope permitted documentation changes "in `AGENTS.md` and/or
  `docs/agents/`". That permission now covers only `AGENTS.md` and the
  project-owned files under `docs/agents/`: `model-notes.md`, `launching.md`
  and `worktree-mechanism.md`. If defect A's mechanism needs a change to a
  canonical framework file, write it as a proposal in your report and do not
  make it.
- The agent loop now runs on the owner's Windows 11 machine (MSVC 19.44, Qt
  6.8.3, vcpkg). macOS is not available.
- `python` is 3.12 on this machine. From PowerShell, 8 `test_push_guard` cases
  fail because `bash` resolves to the WSL launcher. That is a known,
  pre-existing issue and not this round's to fix; run the Python suite from
  Git Bash, and report the PowerShell result if you run it.

## Base and branch

Work on branch `meta/evidence-freshness-reland`. Its first commit is Brain's
close-out of round 011, which also queues this brief; it touches only
`docs/briefs/`, `docs/state.md` and `docs/agents/model-notes.md`. Continue on
top of it. Do not rebase or force-push. Do not reuse, rebase or delete
`meta/evidence-freshness`; it stays as the record of what PR #26 delivered.
Whether you carry any of `1438f934` forward, and how, is your decision.
Explain it in the report.

## Scope, non-scope, invariants

As in Brief 009, with the amendment above to what `docs/agents/` may be
changed. In particular: no `.github/workflows/` or repository-setting change;
no production code in `client/`, `data/`, `policy/`, `ui/`, `gframe/` or
`ocgcore/`; no weakened, skipped or deleted test; archived briefs' recorded
evidence is not rewritten. The semantic-trace tests must still skip cleanly
when `client/` is not built, and freshness must fail closed.

Also in scope: Brain's close-out commit at the base of this branch is part of
the reviewed range. If you find an error in it, report it; do not rewrite it.

## Acceptance criteria

Brief 009's acceptance criteria, unchanged, plus:

- The round's evidence is produced on this branch and at its final head, on
  this machine. Figures copied from PR #26 are not evidence.
- The defect-A mechanism is demonstrated against PR #19's historical case, as
  Brief 009 requires. It is also demonstrated against this round's own PR
  after a branch update, if one happens during the round.
- `python -m unittest discover -s tests -v` and both generator `--check`
  commands are green from Git Bash.
- Canonical framework files and `tools/checkout.py` / `tools/report.py` are
  unchanged, shown by an empty
  `git diff origin/master -- tools/checkout.py tools/report.py` together with
  the list of `docs/agents/` files changed.
- The PR body carries `DO NOT MERGE — under review`, and the PR is not
  merged.

## Required evidence

Brief 009's required evidence, produced fresh, plus the platform, exact base
and head SHAs, and the check-run conclusions at the final head. Say what you
did not run. Do not cite the replay harness as evidence about duel
behaviour; this round does not touch it.

## Completion-report schema

The standard report in
[`docs/agents/roles/worker.md`](../agents/roles/worker.md), plus Brief 009's
three additional fields, plus what you did with `1438f934` and why.

## Reopened corrections E1-E3

E1 — the PR-body checker (`tools/check_pr_evidence.py`) misses the forms measured evidence actually takes in this repository's PR bodies, and flags ordinary text. Observed by Brain at 80f8465f, each run as a body alongside a valid command: these passed with exit 0 — prose hard-wrapped as "the unittest run reported 112" / "tests, all green."; a markdown table row "| unittest | 112 |"; "ctest reported 3/3" / "passing."; and "data 3/3, policy 2/2, CI 12 green". "Run the tests with Python 3.12." was rejected with exit 1. The Verifier also got "Evidence: 69" / "tests passed." through. This project hard-wraps its PR bodies, so a line-local check misses its own house style. Required outcome: the mechanism catches measured-evidence figures in the forms this repository's PR bodies actually use, and does not flag commit SHAs, PR, brief or correction identifiers, or version numbers. Use the real bodies of this repository's past PRs (`gh pr view <n> --json body`) as a corpus, and report what the mechanism does on them. Deliberately disguised numerals (Roman numerals, number words such as "dozen", hexadecimal) are out of scope: the risk is honest drift, not evasion. Say so in the documentation as a stated limit. If a line-by-line pattern cannot meet this, change the mechanism's shape rather than growing a pattern list, and explain why. New tests must fail at 80f8465f.

E2 — the semantic-trace freshness check fails open when part of `client/` cannot be enumerated. Observed by Brain at 80f8465f on Windows, with a real permission denial (`icacls /deny (OI)(CI)(RD)`) on a subdirectory holding a newer source file: `binary_is_fresh` returned True. Python's `Path.rglob` skips an unreadable directory without raising. That contradicts Brief 009's protected invariant ("If freshness cannot be established, the answer is 'not fresh'") and the completion report's fail-closed claim. Required outcome: a source tree that cannot be fully enumerated or stat'ed makes the binary not fresh, with a test that fails at 80f8465f. Where the test cannot set up an unreadable directory (for example, running with administrator or root rights), it must skip with a visible reason rather than pass.

E3 — `docs/state.md`'s macOS section still says a stale `client/build/edopro_next_semantic_trace` "is silently preferred" and that `find_binary()` has "no freshness check". After this round that is no longer true. Make it describe the behaviour after your change, including its named residual limit (a newer binary built from a different commit can still pass).

## Reopened corrections E4-E6

E4 — a PR body carrying a measured figure still passes the checker. Observed by Brain at e29ff3d2: PR #15's body passes with exit 0 although it pastes `git diff --stat` output ("docs/architecture/deck-builder-legality.md | 523 ++++++++"). A standalone body of per-file diff-stat lines plus a valid command also passes. Diff-stat output is the exact historical form Brief 009 exists to catch (PR #19's rejected figure was a diff stat). The Verifier also found decimal counts the checker does not flag, in bodies it rejects for other reasons: PR #3 lines 191 and 284, PR #11 lines 113-114, and PR #12 lines 182-183.

E5 — false alarm. Observed by the Verifier and confirmed by Brain: the pasted diagnostic line `load_ydk(directory):      ok=1  error=""` (PR #23 line 50) is rejected as a measured figure. It is a status observation, not a count or timing that goes stale.

E6 — the round's acceptance keeps moving because the checker is judged against examples chosen one review at a time. Required outcome: commit the real bodies of this repository's PRs #1-#28 (from gh pr view <n> --json body) as test fixtures, each with an expected verdict. Add line-level expectations for every figure and false-alarm line named in E4 and E5, and for every other measured figure you find in the corpus. The test must fail at e29ff3d2. The correct outcome is this: no corpus body that contains a measured figure passes; no body fails solely because of a line that is not a measured figure; and the expected verdicts are your judgement, listed in your report so review can challenge them. State in AGENTS.md that the checker is a heuristic, what it is known not to catch (including the declared disguised-numeral limit), and that it runs only when invoked, since no CI change is in scope.


---

## Outcome — accepted and merged 2026-09-21

Accepted by Brain and merged as PR #28 (merge `8bad984e`) at head
`27ec4157a17401f4ebebbba5e18e2fe9beeac6a4`, base
`823fa67907e8d8462fbc9005ec85311fe00aabbe`. Four delivered heads were
reviewed: `80f8465f`, `e29ff3d2` and `27ec4157` by name, plus the round's
first delivery. The first three passes ran in Claude Code. The last pass, for
both Builder and Verifier, ran in **Antigravity** (Gemini 3.8 Flash, High),
the first use of that tool in any seat.

**Acceptance conditions, confirmed at the literal head.** The Verifier
reviewed `27ec4157` itself (task `012-verify-evidence-freshness-reland-3`).
Brain independently checked every BLOCKER and UNPROVEN CLAIM from every pass.
All required checks were green (12 success, 1 conditional matrix entry
skipped; merge state CLEAN). The change is inside routine scope: the PR-body
checker is a local tool, and no required check or setting changed.

**How the passes were adjudicated.**

- `80f8465f` — both Verifier BLOCKERs were upheld. Brain reproduced the
  fail-open freshness scan with a real Windows ACL denial. It upheld the
  checker finding on realistic grounds: hard-wrapped prose, tables and `N/M`
  counts passed, and "Python 3.12" was falsely flagged. It overruled the
  argument based on disguised numerals, since the risk is honest drift, not
  evasion.
- `e29ff3d2` — the Verifier's BLOCKER named misses inside bodies the checker
  already rejects, and Brain overruled its severity on that basis. Brain's own
  sweep of all 28 past PR bodies then found PR #15 passing while it pasted
  `git diff --stat` output, the exact historical form. E6 replaced
  review-by-examples with a committed corpus of real bodies and fixed
  expected verdicts.
- `27ec4157` — no BLOCKER. The Verifier's SHOULD FIX was confirmed by Brain
  against the live bodies. PR #10's and PR #16's fixtures were committed
  double-spaced (`\r\n` became `\n\n`), and their tests pin line numbers in
  the doubled files. The pass or fail verdict is unchanged for all 28 bodies.
  The other 26 fixtures match the live bodies. Accepted with this recorded as
  a known defect to fix in a later round. It fails loudly, not silently: a
  refresh from the live bodies breaks the pinned line numbers.

**Unsupported claims in the final Builder report**, caught by the Verifier
and confirmed by Brain; neither was merged: "normalized with LF line
endings" (false for two fixtures), and PR #1 "contains measured figures
(lines 41, 70, 71)" (the checker flags none of them). Both Antigravity seats
reported the OS as `10.0.26100`; this machine is `10.0.26200.9457`.

**Brain's own error in this round.** The first Verifier prompt gave a
full-length base SHA written from memory, which did not exist. Its
instruction to fall back to `git rev-parse 823fa679` caught it, and the
Verifier reviewed the correct base. Brain now copies every SHA in a prompt
from command output.

**Not established.** macOS and native Linux were not run locally; Linux CI
ran at every head. The checker is a heuristic that runs only when invoked.
`AGENTS.md` states its limits.

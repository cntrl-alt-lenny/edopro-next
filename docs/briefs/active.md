Brief-ID: 012-2026-09-21-evidence-freshness-reland

Status: active

## MODE: IMPLEMENTATION

## Goal

Re-land Brief 009 as its own reviewed round on the current `master`, and
close the two evidence-freshness defects it names. Brief 009's full problem
statement, investigation questions and acceptance criteria are in
[`delivered/009-2026-09-01-evidence-freshness.md`](delivered/009-2026-09-01-evidence-freshness.md)
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

# Brief 009 — evidence that describes the thing it claims to describe

Status: queued

One brief lives here at a time. On delivery it moves to
[`delivered/`](delivered/), and only on adjudication to
[`archive/`](archive/) — see [`README.md`](README.md). Template and field
meanings: [`docs/roles/builder.md`](../roles/builder.md).

---

## MODE: IMPLEMENTATION

## Goal

Two mechanisms in this repository produce an evidence artifact that **silently
stops describing what it names**, with nothing that can fail when it does.
Close both, and close them as one class.

**A. A PR body's evidence figures rot when the branch is updated.**
`master` is protected with `strict: true`, so whenever `master` moves an open
PR must be updated before it can merge. That produces a new head — and every
number already quoted in the PR body (`git diff --stat`, test counts, timings)
silently stops describing the range that will actually merge. Nothing warns.

This is not hypothetical. Both of PR #19's UNPROVEN CLAIMs were exactly this,
and both were true when written:

| Claim in the body | True of | False of the merged range |
|---|---|---|
| "seven files, 173 insertions / 100 deletions" | `90194888..6d3aadbf` | 10 files, 620/279 |
| "56 ran, 0 failures" | the same earlier range | 69 tests |

`AGENTS.md` already says a Verifier review does not survive a new head. It says
nothing about the body's own numbers, which have the same failure mode and no
discipline attached at all.

**B. `tests/test_semantic_trace.py` silently prefers a stale binary.**
`find_binary()` searches `client/build`, `build/client`, `build` in order and
takes the first file it finds, with **no freshness check against the sources**.
On 2026-09-01 a binary four days old was picked up on a clean tree and four
tests failed against it. The failure was the lucky direction: the dangerous one
is a C++ edit that is never compiled, tested against the stale binary, and
reported green.

## Why this is next

It is the same defect class this project keeps producing, and the one
`AGENTS.md` names first: **an artifact that reads as checked while the property
it names is unverified.** Both instances above were found by accident rather
than by any mechanism.

It is also entirely Python, shell and documentation, so it is verifiable on
any of this project's machines.

**Machine note, corrected 2026-09-01.** An earlier draft of this brief said the
round would run on macOS, where `ui/` "cannot be built at all". Work has moved
back to the **Windows** host, and that claim does not describe it: MSVC
19.44.35228, CMake 3.31.6, Ninja 1.13.2, Python 3.12.10, Qt 6.8.3
`msvc2022_64` and vcpkg are all present, and `ui/` was measured this session to
configure, build 86/86 and pass `ctest` 2/2. Say which host you actually ran
on; do not copy either machine's limits from `docs/state.md` without checking
them.

It does **not** depend on brief 008, which is delivered but unadjudicated
(PR #24). Keep it that way: if your work starts needing 008's outcome, stop and
say so rather than building on an unreviewed round.

## Base SHA

Branch from `origin/master` as `meta/evidence-freshness`. Record the SHA with
`git log -1`.

**This brief itself lives on `meta/round-5-queue` (PR #25).** Read it there;
do not branch from it. That is the same arrangement brief 006 used.

## Scope

- A mechanism that makes **A** detectable rather than a matter of care.
- A fix for **B** that cannot be satisfied by a stale artifact.
- Whatever documentation change the mechanisms imply, in `AGENTS.md` and/or
  `docs/agents/`.

## Non-scope

- **Do not change `.github/workflows/` or any repository setting**, including
  branch protection. If your answer to A needs CI or a protection change, that
  is a proposal for Brain and the owner — write it down, do not make it.
- Do not touch `gframe/`, `ocgcore/`, `client/`, `data/`, `policy/` or `ui/`
  production code. B is a test-harness fix.
- Do not weaken, skip or delete any existing test.
- Do not rewrite the archived briefs' recorded evidence to match reality. Those
  are a historical record of what was claimed; correcting them retroactively
  destroys exactly the trail this brief exists to protect.

## Protected invariants

- **Failing closed.** If freshness cannot be established, the answer is "not
  fresh", never "assume fine". B's current bug is precisely a fail-open.
- **`tests/` must keep running with no cmake, no Qt and no built `client/`.**
  The semantic-trace tests *skip* when `client/` is not built, and that must
  remain a skip — not an error, and not a silent pass.
- A mechanism that only works on one platform, or only in CI, is not a fix for
  a defect that appears on developer machines.

## Required investigation

1. **For B: what is the correct freshness relation?** Newer-than-sources is one
   answer; a build-system-provided path (`EDOPRO_NEXT_SEMANTIC_TRACE`, which CI
   already sets) is another; refusing to search at all is a third. Say what you
   considered. Note that "newest mtime" has its own failure mode — a binary
   built from a *different* commit can still be newer than the sources.
2. **For A: what can actually fail?** Options include a CI check that compares
   figures in the PR body against the real range, a template that forbids
   quoting figures at all in favour of a command a reader can re-run, or a
   documented discipline with no automation.

   **Brain's ruling, 2026-09-01: a mechanism that can fail is REQUIRED.** An
   earlier draft of this brief also permitted "a documented discipline with no
   automation, if you argue it". That permission is **withdrawn**, because it
   contradicted this brief's own acceptance criterion demanding a demonstration
   against PR #19's historical case — a Builder taking the permitted route
   could not have satisfied it.

   The reason a mechanism is affordable here is an asymmetry worth stating:
   *verifying* that a body's figures match the real range means parsing
   free-form prose and is brittle, but *prohibiting* figures in the body — and
   requiring instead that it name a command a reader can re-run — is a cheap
   check that fails loudly. **A prohibition is a mechanism.** That is a hint at
   a tractable shape, not a specification; if you find a better mechanism,
   build it and say why it beats the prohibition. `AGENTS.md` is explicit that
   "a list of cases you tried is not coverage" and that a claim that matters
   wants a mechanism that can fail.
3. **Is this class present anywhere else?** Look for other places where a
   recorded number, path or artifact is trusted without being tied to the thing
   it describes. Report what you find; do not fix beyond the two named
   instances without saying why.

## Acceptance criteria

- **B fails before your fix and passes after it**, demonstrated: place a
  deliberately stale binary where `find_binary()` would take it, and show the
  suite refusing it. A narrative description is not sufficient.
- The semantic-trace tests still *skip* cleanly with no `client/` build, and
  still pass with a fresh one.
- `python3 -m unittest discover -s tests` green.
- Both generator `--check`s green.
- Whatever mechanism you build for A, demonstrate it catching PR #19's actual
  historical case — the figures and ranges are in the table above.
- CI green at the head SHA, queried rather than assumed.

## Required evidence

- The stale-binary reproduction, both directions, as real output.
- The A mechanism demonstrated against the PR #19 case, as real output.
- `python3 -m unittest discover -s tests -v` (tail is fine).
- `python3 tools/generate_messages.py --check` and
  `python3 tools/generate_protocol_constants.py --check`.
- `git diff` against your base SHA.
- CI check-run conclusions at the exact head SHA.
- What you did **not** run, explicitly. Do not assert that `ui/` was skipped
  "because Qt is absent" without checking — on the Windows host Qt is present
  and `ui/` builds. State the real reason, which for this brief is simply that
  `ui/` is out of scope.

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../roles/builder.md), plus:

- **The freshness relation you chose for B**, and the failure mode it still
  has. Every choice here has one; name yours.
- **Your answer to A**, including the case for doing less than automation if
  that is your conclusion.
- Anything from investigation 3 you found and deliberately left alone.

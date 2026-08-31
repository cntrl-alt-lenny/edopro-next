# Model notes

A dated log of what has actually been observed running each seat on each
model. Kept out of `AGENTS.md` so that file stays lean and does not accumulate
model trivia that goes stale.

All three roles are explicitly model-agnostic (see `AGENTS.md`). This file is
supporting evidence for that design, **not a ranking and not a requirement**.
Brain's review standard — independently re-check everything — does not change
based on which model ran a round.

**Only record what was actually observed in a real round.** Do not
pre-populate with generic advice this project has not earned.

## Seat characteristics, for choosing on merit

Not model recommendations — a description of what each seat's work actually
demands, so the choice can be made deliberately.

- **Brain** — holds upstream source, our source, an ADR, a Verifier finding
  and CI state simultaneously, and compares them. Wants context headroom and
  a willingness to reject work it commissioned itself.
- **Builder** — one coherent problem, its own investigation, real code and
  real tests. Wants sustained engineering over a single task rather than
  breadth.
- **Verifier** — fresh context, adversarial, re-derives upstream semantics.
  **Prefer a different model family from Brain and Builder**; the seat's whole
  value is not sharing their blind spots.

## Mechanics observed in this environment

Checked against **Claude Code 2.1.181** on 2026-08-30/31. Recheck if the
version changes rather than inheriting these as permanent facts.

- **Established:** `effortLevel` (`low` / `medium` / `high` / `xhigh`) and
  `ultracode` are settings fields. Method: read directly out of the installed
  binary's own embedded settings schema, which contains verbatim
  `effortLevel:E.enum(["low","medium","high","xhigh"]).optional()…describe(
  "Persisted effort level for supported models.")`. Because
  `.claude/settings.local.json` is gitignored and per-checkout, each worktree
  can pin a different effort — Brain's, Builder's and Verifier's need not
  match.
- **Do not use `json.schemastore.org/claude-code-settings.json` to check
  this.** Round 1's Verifier tried, and that third-party schema both missed
  `effortLevel` and missed `hooks`, a field this repository already uses. It
  lags the product. The installed binary is the authority.
- **Open, not established:** whether reasoning effort is absent from
  agent-file frontmatter. It was inferred from finding no such key in the
  binary, which is absence of evidence; the Agent tool's own description can
  be read the other way. Settle it by setting an `effort:` key in a role file
  and observing whether it takes effect, then record the result here.
- Agent frontmatter does carry `name`, `description`, `tools` and `model`.
  This project's role files deliberately omit `model:` so each seat inherits
  whatever the owner launched; `verifier.md` sets `tools:` to keep that seat
  read-only apart from `Bash`.
- Multi-agent orchestration can pin model and effort per dispatched agent,
  which is the one mechanism that sets both declaratively.

## Round log

Record each round as: date, seat, brief, model and effort actually used,
outcome, and what was specifically observed — including operational mishaps,
which are usually more useful than impressions of quality.

**Framework install (2026-08-30) — Brain seat, Claude Opus 5.** Reviewed both
sibling frameworks (`gx-spirit-caller`, `edopro-retro-formats`), this
repository's state, CI and merged PR history, and authored the framework on
`meta/agentic-framework` (PR #14). Not a Builder round and not evidence about
the Builder or Verifier seats. Owner's stated intent is to vary Brain's model
with available capacity rather than pin it.

**Round 1 (2026-08-31) — Builder seat, Claude Sonnet 5 at High effort.**
Brief 001, `UPSTREAM ARCHAEOLOGY`, the deck-builder legality boundary — see
[`../briefs/`](../briefs/) for where that brief currently sits and what its
status is; do not assume from here. Produced PR #15, a 523-line source-cited
document. Observations:

- **Did not anchor.** The brief deliberately posed "does upstream's deck
  editor validate the whole deck at all?" as an open question while Brain
  already had a preliminary answer. Builder investigated independently and
  returned a substantially deeper answer than Brain's — three distinct editor
  mechanisms, their bypasses, and the total absence of checks on the save and
  import paths. Keeping the answer out of the brief was the right call and is
  worth repeating.
- **Flagged rather than fixed.** Found that `deck-builder-ui.md` §1 claims
  more than upstream source supports, and recorded it quoted-both-ways in the
  new document instead of silently editing an out-of-scope file. Correct
  discipline.
- **Respected the negative evidence rule.** Did not run the cmake/ctest
  cycles for a docs-only change, and said so explicitly rather than reporting
  an unfalsifiable green.
- **Calibrated its own confidence** — high on the descriptive sections,
  medium on the recommendation — and named the single fact that would most
  change it. That is the behaviour the report schema is trying to elicit.

**Round 1 (2026-08-31) — Verifier seat, Claude Sonnet 5 at High effort.**
Reviewed PR #14 (the framework itself), base `b6315df1`, head `fa881423`.
Outcome: **two BLOCKERs upheld**, and the round paid for the seat on its first
outing. Observations:

- **Found real defects in Brain's own work**, in the one piece of actual code
  in that PR: two ways to reach `master` while the push guard reported
  success. Both reproduced exactly as described.
- **Correctly identified the class**, and its verdict framed the problem
  precisely — a control advertised as unconditional that an ordinary shell
  habit defeats.
- **But under-counted the extent.** Brain's follow-up sweep found **seven**
  fail-open cases, not two (also `bash -c`, `(…)`, `$(…)`,
  `+refs/heads/master`, `git -C . push`), plus one fail-closed case Verifier
  did not look for at all. Finding the class but not the boundary is the
  useful thing to know about this seat at this tier: it justified the fix, but
  a Brain that had patched only the two reported cases would still have
  shipped five bypasses.
- **Filed an UNPROVEN CLAIM that was, in fact, true** (the `effortLevel`
  settings field). This is a *correct* use of the class — the claim was
  unverifiable with the tools Verifier had, and it said so rather than
  guessing. The defect was Brain's: the doc asserted "verified" without
  recording the method, leaving nothing to re-check. Fixed by stating the
  method.
- **Distinguished honestly between what it checked and what it could not**,
  and independently reproduced every evidence command in the PR body.

**Round 1 verification, second pass (2026-08-31) — Verifier seat, Claude
Sonnet 5 at High effort.** Reviewed PR #14 at its final head `152014ac`
(base `b6315df1`). Outcome: **zero BLOCKERs, seven SHOULD FIX and five NOTE**,
and Brain merged on the strength of it. Observations:

- **It answered the calibration note from its previous round.** Told that its
  earlier pass found the defect class but not its extent, it ran six
  independent adversarial investigations with a refutation pass on each
  candidate finding, and reported that one investigation returned a broken
  placeholder result which it then redid by hand. That self-report is itself
  the useful behaviour.
- **It refused two findings that looked BLOCKER-shaped**, correctly — an
  "unreviewed head about to merge" that was in fact the review in progress,
  and self-attestation in the four-point merge check that `AGENTS.md`
  discloses in the same breath. Not manufacturing severity is as valuable as
  finding it.
- **It declined to reproduce one claim on principle**: Brain's empirical
  branch-protection proof required changing repository settings, which
  `AGENTS.md` reserves for the owner, so it filed the claim as UNPROVEN rather
  than attempt it. That is the correct call and exposes a structural gap —
  Brain performed an owner-authorized action that Verifier is *structurally
  unable* to verify. Worth remembering whenever Brain acts under a one-off
  owner authorization.
- **Its highest-value findings were about this project's own guards**: two
  tests that check a proxy (a six-item substring list; a line-count cap)
  rather than the property they name, each missing a live violation in the
  very commit that introduced them. Brain reproduced both.
- Brain independently reproduced the three new `pre-push` escape hatches, the
  four `docs/state.md` staleness items, and the `/status` neutrality leak. All
  held. Brain's own first probe of the escape hatches was invalid — it checked
  out `master`, which does not contain `.githooks/` — and produced a false
  "control case failed" before being corrected.

**Brain-seat work, PR #14 commits `b2f62e2d`…`152014ac` (2026-08-31, Claude
Opus 5).** The push-guard redesign, branch protection, the authority change and
the contract/adapter split. Logged here because the round log had no entry for
the substantial Brain work in this PR — a gap Round 1's Verifier flagged.
Notable, and not flattering: Brain broke its own evidence rule twice in three
rounds — "tested against nine cases" presented as coverage, and asserting a
branch protection that did not exist. Both were caught by review, neither by
Brain. The framework's value is not that its author is careful; it is that
being careless is caught.

Cross-cutting note for both seats: Brain and Builder were Opus 5 and Sonnet 5
respectively, and Verifier was *also* Sonnet 5 — so `AGENTS.md`'s preference
for a different Verifier family went untested. What this round actually tested
was **context** diversity (fresh context, no access to the author's report),
and that alone was enough to surface defects the author had missed. Worth
watching whether family diversity adds anything beyond it.

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

## Round 3 — 2026-09-01 — first non-Anthropic seats

**Builder and Verifier both GPT 5.6 Luna at High effort; Brain Claude Opus 5.**
The first round where `AGENTS.md`'s preference for a model-diverse Verifier was
actually exercised — though not as intended, since Builder was the *same*
family as Verifier this time and Brain was the odd one out. Family diversity
between Builder and Verifier therefore still has not been tested.

**Builder seat (brief 007, `85a11055`).** A three-line deletion, so this says
little about capability at scale. What it does say something about is honesty
under a badly-drafted brief: the acceptance criterion demanded `policy/` pass
`ctest`, which was **impossible** on the target platform because of a
pre-existing defect. Builder ran it anyway, reported the failure unprompted
with the correct mechanism (`eofbit|failbit` versus `badbit`), correctly scoped
it out, and did not quietly narrow the criterion or claim it passed. Brain had
independently found the same failure before reading the report and expected to
catch a false green; there was none. That is the single most encouraging
observation of the round.

Weak points, both minor and both about durability rather than correctness: the
commit message was one line and recorded none of the brief's three required
investigations (compare `2f325e15` on the same branch), and the report's head
SHA was truncated to 39 characters.

**Verifier seat (two ranges, `ccbf7860..7639bf2d` and `3a2fea97..85a11055`).**
Genuinely productive. Four findings, three of which Brain reproduced exactly:
two stale PR-body figures, and a scope observation grounded in the brief's own
line 158. It kept its two passes properly separate, was explicit about what it
could not check, and refused to accept a Windows-only reproduction it could not
run — the right instinct.

Two things to watch:

- **It explained away the one live failure it had in hand.** The `policy/`
  `ctest` failure was called "environment-sensitive" and deferred to green
  Linux CI. It is a real cross-platform defect, and it is now brief 008. This
  is the precise failure mode Verifier exists to prevent — green CI, wrong
  elsewhere — and a fresh-context reviewer trusting CI over the failure in
  front of it is a pattern worth watching for in this seat.
- **It reported a real observation with the wrong culprit.** The out-of-scope
  coordination-file changes came from Brain's round-opening commits at the base
  of the branch, not from Builder. Diff-range attribution is easy to get wrong
  on a stacked PR; the finding was still worth having.

**Brain-seat note.** Brain issued brief 007 as a launch prompt only and never
wrote it to `active.md`, so Verifier reviewed that work with no acceptance
criteria to check against — and said so. A brief that exists only as a launch
prompt is not a brief. Third rule this framework defines that its own author
has broken.

## Round 010 — 2026-09-20 — framework adoption

Round 010 ran through three Builder/review passes before Brain accepted and
merged it at `d3b458bb` (PR #27). This is an observation log, not a model or
seat ranking.

- **Pass 1, head `55a8989f`:** the adoption delivered canonical contracts,
  report/checkout tooling and the Claude adapter, but review rejected the
  round. The neutrality scanner contradicted the project's established branch
  namespaces, and the round also exposed path, wording, test and state
  reconciliation defects.
- **Pass 2, head `3c212bda`:** C1-C5 removed the local neutrality stack,
  restored the project wording, repaired the two documentation guards and
  refreshed state. The reviewing seat returned findings that did not survive
  adjudication: it reported leftover marker blocks inside files required to
  remain byte-identical, although the canonical files were hash-identical to
  the framework, and called a citation incomplete although the cited file was
  character-for-character identical to `master`.
- **Pass 3, head `51a5a993`:** C6-C8 reconciled the active brief with the
  deliberate neutrality deferral, repaired the remaining live Authority
  cross-reference, and disclosed the intentionally removed non-empty parity
  assertion. The same two categories of reviewing findings again did not
  survive adjudication. Brain accepted the corrected round and merged it.

The durable lesson is to adjudicate review findings against the literal SHA
and source bytes before carrying them into project state: a finding can be
useful to investigate without being a defect after that check.

## Round 011 — 2026-09-20 to 2026-09-21 — the read-failure class

Five delivered heads before acceptance (PR #24, merged `823fa679`). The first
was reviewed on macOS. The other four were reviewed on Windows 11 / MSVC,
after the agent loop moved machines mid-round. This is an observation log,
not a ranking.

- **The Windows passes turned on one repeated failure: fixing the instance,
  not the class.** A preflight status check, then a two-prefix string guard,
  then a separate classification probe. Each fixed exactly the cases it had
  been shown and missed the next spelling or server behaviour. The round
  converged only when the correction named the underlying constraint (the
  object classified must be the object read) instead of the observed cases.
  That is `AGENTS.md`'s "fix the class" rule, and in this round it also
  applied to how Brain wrote corrections.
- **Verifier, pass `338fe1e8`:** correctly found the busy-pipe fall-through
  and the first-ever MSVC compile evidence. It over-read one PR sentence
  that described only the latest corrective step, and was overruled on that.
- **Verifier, pass `797a1d86`:** returned no BLOCKER, having tested only the
  two spellings the guard recognised. Brain found an indefinite hang on
  `\.\GLOBALROOT\Device\NamedPipe\…`. A review that tests the inputs the
  change names inherits the change's blind spot; after this, Verifier prompts
  asked for a sweep of the input class rather than a list.
- **Verifier, passes `2a3f3e43` and `45be9de6`:** given a class-sweep
  instruction, it found both real defects in the third pass (a pipe loaded as
  an empty deck, a `CON` hang) and extended its own probe beyond the
  requested spellings in the fourth. Brain reproduced every BLOCKER it
  raised. In the final pass it upheld two documentation-precision claims,
  and overruled one misreading of a starting-state line in the Builder
  report.
- **Builder:** its completion reports twice carried small figure errors
  (test counts one low). Figures in a report are claims like any other.

**Brain-seat note.** Brain's first cross-machine Verifier prompt worked only
because it supplied literal SHAs; the mechanical delivery check could not see
another clone's inbox. That has since been reported to and fixed by the
framework (its PR #10).

## Round 012 — 2026-09-21 — evidence freshness re-land (and the first Antigravity seats)

Four reviewed heads before acceptance (PR #28, merged `8bad984e`). This is an
observation log, not a ranking.

- **Claude Code passes (`80f8465f`, `e29ff3d2`):** Verifier BLOCKERs were
  real, but twice argued from the wrong cases. The first time it used
  disguised numerals, which are out of scope because the risk is honest
  drift. The second time it cited misses in bodies the checker already
  rejects. Brain's own sweep found the case that mattered: PR #15 passing
  with a pasted diff-stat. As in round 011, acceptance kept moving while the
  checker was judged one review's examples at a time. It converged once E6
  fixed a committed corpus with expected verdicts.
- **Antigravity pass (`27ec4157`), Builder and Verifier both on Gemini 3.8
  Flash (High):** both ran their checkout check in the right worktree and
  wrote their reports with `tools/report.py` under the correct role and
  Brief-ID (`source=cli`, no hook). Neither touched another checkout. Both
  reported no permission prompts and no command they could not run; that is
  their own claim. The Verifier's findings all survived adjudication, and it
  caught both of the Builder's unsupported claims: fixtures "normalized with
  LF" (two were double-spaced), and PR #1 figures the checker does not flag.
  Both seats reported the OS as `10.0.26100`; the machine is
  `10.0.26200.9457`. Treat self-reported tool, model and OS as claims.
- **What made Antigravity work:** the prompt's optional tool notes named the
  worktree for every command. Role tagging is derived from the directory, and
  that tool's terminal may start in the clone root.

**Brain-seat note.** Brain wrote a full-length base SHA into a Verifier
prompt from memory, and it did not exist. The prompt's fallback
(`git rev-parse 823fa679`) caught it. Every SHA in a prompt is now copied
from command output.

## Round 013 — 2026-09-21 — framework update to fed26f36 and neutrality guard

One pass (PR #29, merged `d035c66d`). Builder and Verifier both ran in
Antigravity (Gemini 3.8 Flash, High). This is an observation log, not a
ranking.

- **Mechanics were clean again.** Both seats ran the checkout check in the
  right worktree and wrote their reports under the right role and Brief-ID.
  They left no stray files. Asked to take the OS from `cmd /c ver`, both
  reported it correctly this time.
- **The installation itself was correct**, and Brain re-hashed every file.
- **The Verifier was less critical than in round 012.** It reported
  "Conflicts: None" while the Builder's report contained two false
  statements (a `.gitattributes` "stale" claim, and a SHA-table row). It did
  not notice that the Builder had deleted a settled-decisions record from
  `docs/state.md`. Brain found all three by reading the diff and re-hashing.
  With the same model in both seats, the Verifier's independence came only
  from its fresh context. That was enough in round 012, but not here.

## Round 014 — 2026-09-21 to 2026-09-22 — README standard

Two passes (PR #30, merged `113e2c7f`). This is an observation log, not a
ranking.

- **First pass, both seats in Claude Code (Sonnet 5).** The Builder's work
  was thorough: a generated status block with mutation tests, twelve
  corrected facts each with evidence, and a complete section mapping. The
  Verifier did as the prompt asked and walked the old README itself before
  reading the Builder's mapping, re-deriving six facts from primary sources.
  It did not run `cmd /c ver`, and reported the OS from its environment.
  Neither seat flagged that the landing page's first sentence described a
  usable client that does not exist; Brain did.
- **Corrective pass, both seats in Antigravity (Gemini 3.8 Flash, High).**
  It made exactly the three corrections, with correct mechanics. The
  Verifier's report file carries two `captured` header lines. It is harmless,
  but a sign the seat wrote its report twice.
- **Brain-seat note.** Brain wrote brief 014's base SHA from memory, for the
  second time in two days, and caught it before handing the brief out. The
  rule is now written down for future sessions: copy every SHA from
  `git rev-parse`, then verify it with `git cat-file -e`.

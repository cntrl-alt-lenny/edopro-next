# Brief 004 — M3 architecture citation audit

Status: queued

One brief lives here at a time. On delivery it moves to
[`delivered/`](delivered/), and only on adjudication to
[`archive/`](archive/) — see [`README.md`](README.md). Template and field
meanings: [`docs/roles/builder.md`](../roles/builder.md).

---

## MODE: UPSTREAM ARCHAEOLOGY

## Goal

Audit every claim this project's M3 architecture documents make about
**upstream** behaviour, against upstream source, and correct the ones the
source does not support.

The deliverable is corrections to the existing documents plus one countable
result: **how many upstream citations were checked, how many held, and how
many did not.** "I audited the docs" is not a result; "142 of 147 citations
verified, 3 line references stale, 2 claims broader than the source supports"
is.

## Why this is next

These documents are what a future Builder reads *instead of* re-reading
upstream. A wrong claim in `docs/architecture/` does not stay a documentation
defect; it becomes a correctness defect in the next implementation round that
trusts it. That is precisely the class of failure that has produced this
project's real bugs.

Two rounds have now each surfaced one of these **by accident, while looking
for something else**. One found by accident implies others found by nobody.

- Round 1 noticed that
  [`deck-builder-ui.md`](../architecture/deck-builder-ui.md):35 claims *"there
  is no upstream function that decides a card's section from its type at push
  time either"*, while `gframe/deck_con.cpp:1585-1588,1617-1621` does exactly
  that. Defensible read narrowly — the caller's cascade does the routing, not
  a dedicated classifier — but worded more broadly than the source supports.
- Round 1's adjudication of PR #15 then found **seven** citation defects in
  the brand-new `deck-builder-legality.md`, one of them a wrong behavioural
  claim (below). That document was written this month, by a careful round,
  with every claim cited. If a fresh document carries seven, the older ones
  have not been checked at all.

There is also a mundane reason: line numbers rot, and these documents cite
`file:line` heavily.

This brief was first written as brief 002, queued on 2026-08-31, and
superseded before delivery by the more urgent framework-hardening work
(brief 003). Its scope now additionally covers `deck-builder-legality.md`,
which has since merged and been adjudicated.

## Base SHA

Branch from `origin/master`. Verify with `git log -1` and record the SHA.

Note that PR #17 (brief 003, framework hardening) may still be open and
unmerged while you work. It touches `tests/`, `.claude/`, `.githooks/`,
`AGENTS.md` and `docs/roles/` — **not** `docs/architecture/` — so it does not
overlap this brief. Do not branch from it and do not read it.

## Relevant context

**In scope for the audit** — the M3 data/deck document family:

- `docs/architecture/card-database.md`
- `docs/architecture/deck-model.md`
- `docs/architecture/card-search.md`
- `docs/architecture/deck-legality.md`
- `docs/architecture/deck-builder-ui.md`
- `docs/architecture/ydk-interoperability.md`
- `docs/architecture/deck-builder-legality.md`

**The arbiter** is upstream source in `gframe/` and `ocgcore/`, read at the
cited file and line. Not our own prose, not an ADR's summary of it, and not
another document in this list.

**Do not read** the M1/M2 documents (`replay-regression.md`,
`semantic-model.md`, `query-stream.md`, `fixture-*.md`,
`live-semantic-observer.md`) or anything under `client/`. Different subsystem,
different round.

`ocgcore/` is a submodule and is not populated in a fresh worktree. If you
need engine source:
`git -C .worktrees/builder submodule update --init --recursive`. `gframe/` is
already present.

## Scope

For each document in the list:

1. Identify every claim about what **upstream** does, and every `file:line`
   citation supporting one.
2. Open the cited location and read it. Record whether the citation points at
   what the document says it does.
3. Classify each finding:
   - **stale line reference** — right code, wrong line numbers.
   - **overstated claim** — source supports something narrower than the words.
   - **wrong claim** — source contradicts it.
   - **unsupported claim** — an assertion about upstream with no citation and
     no obvious source.
4. Fix the wording of stale references and overstated claims **in place**,
   preserving the original meaning where it was defensible and saying what is
   actually true where it was not.
5. Record the count.

### Findings already established — close these, do not rediscover them

These were found and re-derived from source by Brain in earlier rounds. They
are listed so you spend your effort on what is *not* yet known. **Verify each
against source yourself before writing the correction** — a previous agent's
finding is evidence, not fact — but do not treat finding them again as the
result of this round.

1. `deck-builder-ui.md`:35 — the push-time classification claim above.
2. `deck-builder-legality.md` §2.4 — **a wrong behavioural claim, the most
   serious item here.** It says `check_limit` is guarded by the
   Shift-inclusive `forceInput` at `deck_con.cpp:641,719,756`. True at `:641`
   and `:756`; **false at `:719`**, which tests
   `gGameConfig->ignoreDeckContents` directly — Shift is read two lines later
   to choose the target section, not to bypass the check.
3. `deck-builder-legality.md` — three citation ranges that overshoot a
   function's closing brace.
4. `deck-builder-legality.md` — one two-line packet citation where only one of
   the two lines is the mechanism being described.
5. `deck-builder-legality.md` — two self-references to a "Non-goals" section
   that document does not have. They mean the *brief's* non-goals; a reader of
   the document cannot know that.
6. `deck-builder-legality.md` — one "open question" that ADR 0007 and
   `policy/include/edopro_next/policy/validation_policy.h` already answer.
7. `deck-builder-legality.md` — a closing paragraph that narrows a named
   architectural tension into a scheduling question.

Items 5-7 are not upstream-citation defects. Fix them in the same pass anyway;
they are in the same document and were found by the same review.

## Non-goals

- **No production code.** Documentation only.
- **Do not edit ADRs.** They record decisions that were accepted, and
  rewriting one retroactively destroys the record. If an ADR's *reasoning*
  rests on a claim this audit falsifies, that is a finding to report
  prominently — it may mean a decision needs revisiting, which is Brain's and
  the owner's call.
- **Do not restructure, renumber or reformat the documents.** Change the words
  that are wrong. A large diff of moved sections makes the real corrections
  unreviewable.
- **Do not add new claims**, new research, or new sections. If you discover
  something genuinely new about upstream, report it; do not fold it in.
- Do not touch `gframe/` or `ocgcore/`.
- Do not touch `docs/state.md`, `docs/ROADMAP.md` or `README.md`. If this
  audit falsifies something they claim, report it — Brain updates those.

## Protected invariants

- **Never weaken a document to make it true when the code is what is wrong.**
  If a document accurately describes what our code *should* do and the code
  does something else, that is a code defect. Report it; do not edit the
  sentence until it matches the bug.
- **A deliberate divergence from upstream is not an error.** These documents
  record intentional differences on purpose (`card-database.md`'s load
  atomicity and locale overlay, `deck-model.md`'s explicit sections and card
  code 0, `card-search.md`'s `CheckCardProperties` exclusions,
  `deck-legality.md`'s null-vs-`"N/A"` distinction and `CHECK_UNOFFICIAL`
  quirk). Do not "correct" one into a claim that we match upstream. If a
  divergence is described inaccurately, fix the description of the divergence.
- **Preserve narrow defensibility.** Where a claim is true read narrowly and
  false read broadly, the fix is to say the narrow thing precisely — not to
  delete it, and not to leave it ambiguous.
- **Never describe planned functionality as shipped.**
- **A citation you could not check is unchecked, not held.** The count is the
  deliverable; inflating it destroys the only thing this round produces.

## Required investigation

1. Is the citation a **line** reference, a **function** reference, or a
   **behavioural** claim? Stale lines are cheap to fix; a wrong behavioural
   claim is the valuable find. Do not let the volume of the former hide the
   latter.
2. For each *deliberate divergence*: does the document describe upstream's
   actual behaviour correctly, independently of whether our divergence from it
   is a good idea?
3. Where two documents in the list make the same claim about upstream, do they
   agree with each other? A disagreement between two of our own docs is a
   finding even if you cannot tell which is right.
4. Are there upstream claims with **no citation at all**? Those are the most
   likely to have drifted, because nothing anchors them.
5. **Is there a systematic cause?** Seven defects in one freshly written
   document suggests a habit, not seven accidents — for instance, citing a
   range by scrolling to a visual end rather than the closing brace. If a
   class is visible, name it; a class is worth more to the next round than
   seven instances.

## Acceptance criteria

- A countable coverage result: citations checked / held / did not hold, per
  document. If you could not check something, it is counted as unchecked, not
  as held.
- Every finding classified, and every in-place fix traceable to one.
- All seven established findings above closed, each with the source you
  re-read to confirm it.
- No ADR modified. No code modified.
- Anything you deliberately left unfixed is listed, with why.
- If the audit finds **nothing wrong** in a document, say so explicitly for
  that document. A clean result is a real result; do not manufacture findings
  to look thorough.

## Required evidence

Documentation only, so `AGENTS.md`'s per-layer table asks for very little —
and stating that plainly is the evidence:

- `git diff --stat <base>..<head>` showing only `docs/architecture/`.
- **Do not run the cmake/ctest cycles**, regardless of what is installed on
  the machine you are on. Nothing in this round can affect them, and reporting
  a green build that could not have failed is the false-coverage pattern this
  project's evidence rules exist to stop.
- `python -m unittest discover -s tests` is cheap and stdlib-only; run it to
  confirm you broke nothing, and paste the real output.

## Git expectations

Branch `m3/architecture-citation-audit`, in the Builder worktree
(`.worktrees/builder`). Commit in focused commits — ideally one per document,
so a reviewer can take them one at a time. Push, open a PR whose body carries
`DO NOT MERGE — under review`, and put the coverage table in the PR body.
**Do not merge.**

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../roles/builder.md), plus:

- **The coverage table, up front** — per document: citations checked, held,
  stale, overstated, wrong, unsupported.
- **The most serious finding**, stated in one paragraph, whatever its class.
  If the worst thing you found was a stale line number, say that plainly; it
  is a good outcome and Brain should not have to infer it from a table.
- **Any systematic cause you identified**, per required investigation 5, or an
  explicit statement that you looked and found none.
- **Any disagreement between two of our own documents**, quoted both ways.
- **Anything that looked wrong but turned out to be a deliberate divergence**
  correctly described — these are the near-misses, and knowing where the docs
  are subtle is useful to the next round.

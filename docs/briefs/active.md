# Active brief

Status: **queued, not started.**

One brief lives here at a time. When a round completes, Brain moves this file
to `docs/briefs/archive/<NNN>-<date>-<slug>.md` and replaces it with the next
brief, or with a "no brief queued" placeholder. The template and field
meanings are in [`.claude/agents/builder.md`](../../.claude/agents/builder.md).

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

Round 1 found one of these by accident. While researching something else,
Builder noticed that
[`docs/architecture/deck-builder-ui.md`](../architecture/deck-builder-ui.md):35
claims *"there is no upstream function that decides a card's section from its
type at push time either"*, while `gframe/deck_con.cpp:1585-1588,1617-1621`
does exactly that. The claim is defensible read narrowly — the caller's
cascade does the routing, not a dedicated classifier — but it is worded more
broadly than the source supports.

One found by accident implies others found by nobody. That matters more here
than it would in most projects, because these documents are what future
Builders read *instead of* re-reading upstream. A wrong claim in
`docs/architecture/` does not stay a documentation defect; it becomes a
correctness defect in the next implementation round that trusts it. This is
precisely the class of failure that has produced this project's real bugs.

There is also a mundane reason: line numbers rot. These documents cite
`file:line` heavily, and upstream merges move lines.

## Base SHA

Branch from `origin/meta/agentic-framework`. Verify with `git log -1` and
record the actual SHA. If PR #14 has merged by the time you start, branch from
`origin/master` instead and say which you used.

## Relevant context

**In scope for the audit** — the M3 data/deck document family:

- `docs/architecture/card-database.md`
- `docs/architecture/deck-model.md`
- `docs/architecture/card-search.md`
- `docs/architecture/deck-legality.md`
- `docs/architecture/deck-builder-ui.md`
- `docs/architecture/ydk-interoperability.md`

**The arbiter** is upstream source in `gframe/` and `ocgcore/`, read at the
cited file and line. Not our own prose, not an ADR's summary of it, and not
another document in this list.

`docs/architecture/deck-builder-legality.md` (PR #15, your own previous round)
is **not** in scope — it is unmerged and under review. Do not audit it, and do
not treat it as a source either.

**Do not read** the M1/M2 documents (`replay-regression.md`,
`semantic-model.md`, `query-stream.md`, `fixture-*.md`,
`live-semantic-observer.md`) or anything under `client/`. Different subsystem,
different round.

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

Fix `deck-builder-ui.md`:35 as part of this — Round 1 already established the
finding and quoted both sides; you are closing it, not rediscovering it.

## Non-goals

- **No production code.** Documentation only.
- **Do not edit ADRs.** They record decisions that were accepted, and rewriting
  one retroactively destroys the record. If an ADR's *reasoning* rests on a
  claim this audit falsifies, that is a finding to report prominently — it may
  mean a decision needs revisiting, which is Brain's and the owner's call.
- **Do not restructure, renumber or reformat the documents.** Change the words
  that are wrong. A large diff of moved sections makes the real corrections
  unreviewable.
- **Do not add new claims**, new research, or new sections. If you discover
  something genuinely new about upstream, report it; do not fold it in.
- Do not touch `gframe/` or `ocgcore/`.

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

## Acceptance criteria

- A countable coverage result: citations checked / held / did not hold, per
  document. If you could not check something, it is counted as unchecked, not
  as held.
- Every finding classified, and every in-place fix traceable to one.
- `deck-builder-ui.md`:35 closed.
- No ADR modified. No code modified.
- Anything you deliberately left unfixed is listed, with why.
- If the audit finds **nothing wrong** in a document, say so explicitly for
  that document. A clean result is a real result; do not manufacture findings
  to look thorough.

## Required evidence

Documentation only, so `AGENTS.md`'s per-layer table asks for very little —
and stating that plainly is the evidence:

- `git diff --stat <base>..<head>` showing only `docs/`.
- **Do not run the cmake/ctest cycles.** Note additionally that this machine
  has neither cmake nor Qt installed, so those builds cannot run here at all —
  if you find yourself wanting one, that is a signal the work belongs in a
  different round, not a reason to try.
- `python -m unittest discover -s tests` is cheap and stdlib-only; run it to
  confirm you broke nothing, and paste the real output.

## Git expectations

Branch `m3/architecture-citation-audit`, in the Builder worktree
(`.worktrees/builder`). Commit in focused commits — ideally one per document,
so a reviewer can take them one at a time. Push, open a PR whose body carries
`DO NOT MERGE — under review`, and put the coverage table in the PR body.
**Do not merge.**

## Completion-report schema

The standard report in `.claude/agents/builder.md`, plus:

- **The coverage table, up front** — per document: citations checked, held,
  stale, overstated, wrong, unsupported.
- **The most serious finding**, stated in one paragraph, whatever its class.
  If the worst thing you found was a stale line number, say that plainly; it
  is a good outcome and Brain should not have to infer it from a table.
- **Any disagreement between two of our own documents**, quoted both ways.
- **Anything that looked wrong but turned out to be a deliberate divergence**
  correctly described — these are the near-misses, and knowing where the docs
  are subtle is useful to the next round.

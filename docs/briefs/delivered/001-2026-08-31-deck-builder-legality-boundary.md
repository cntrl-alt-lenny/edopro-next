# Brief 001 — deck-builder legality boundary

Status: **delivered**

Builder produced PR #15 (`m3/deck-builder-legality-boundary`, head `83d976f0`)
on 2026-08-31, running Claude Sonnet 5 at High effort. Brain has independently
re-derived its load-bearing upstream claims and they hold. **Verifier has not
reviewed it, and Brain has not adjudicated it.**

This file lives in `delivered/`, not `archive/`, precisely because those are
different things — see [`README.md`](../README.md). It moves to `archive/`
with status `accepted` or `rejected`, and the outcome appended, when the round
is adjudicated. Its presence here is not acceptance.

The brief below is the text Builder actually received, unedited.

> **Note for a later reader:** this brief cites `.claude/agents/builder.md`.
> The role contracts have since moved to `docs/roles/` and `.claude/` holds
> only thin adapters. The path is left as Builder received it — a brief is
> evidence about how a round ran, so it is annotated, never rewritten.

---

## MODE: UPSTREAM ARCHAEOLOGY

## Goal

Establish, from upstream source, **how EDOPro actually applies deck legality
in its deck editor, and how that differs from the deck check performed when a
duel is entered** — and from that, determine what a `ValidationPolicy` for
this project's QML deck builder would have to be constructed from, and who
would own that state.

The deliverable is one new document,
`docs/architecture/deck-builder-legality.md`, ending in a **Recommended
boundary** section. No production code.

When this is done, Brain and the owner should be able to ratify a boundary
and brief the implementation round against it, instead of discovering the
question halfway through writing QML.

## Why this is next

`docs/ROADMAP.md`'s remaining M3 item is the deck builder UI, and the part of
it with a real dependency is legality: `policy/` is finished and correct, and
**nothing in `ui/` calls it** — `ui/CMakeLists.txt` does not even link it.

A previous planning round already deferred this slice, and recorded why in
[`docs/adr/0008-upstream-ydk-interop-harness.md`](../adr/0008-upstream-ydk-interop-harness.md)'s
Context section: `policy::ValidationPolicy` has no default constructor and no
session layer supplies one. That deferral was correct, and it is still the
blocker. This brief exists to remove it by answering the question rather than
guessing at it in the middle of an implementation.

## Base SHA

Branch from `origin/meta/agentic-framework` (PR #14) — the framework and this
brief are not on `master` yet. Verify with `git log -1` and record the actual
SHA in your report. If PR #14 has merged by the time you start, branch from
`origin/master` instead and say which you used.

## Relevant context

Read, in roughly this order:

**Ours**
- `policy/include/edopro_next/policy/validation_policy.h` — read the header
  comments, not just the fields. They state deliberately why there is no
  default and what each field mirrors upstream.
- `policy/include/edopro_next/policy/deck_validation.h` and
  `policy/include/edopro_next/policy/lf_list.h`.
- [`docs/architecture/deck-legality.md`](../architecture/deck-legality.md) and
  [ADR 0007](../adr/0007-deck-legality-policy-module.md) — what `policy/`
  reproduces, and the divergences it preserves on purpose.
- [`docs/architecture/deck-builder-ui.md`](../architecture/deck-builder-ui.md)
  and [ADR 0006](../adr/0006-deck-builder-qt-adapter-boundary.md) — the
  existing Qt adapter boundary any answer must fit inside.
- `ui/src/deckbuilder/` and `ui/CMakeLists.txt` — what exists today.

**Upstream** (the arbiter; quote file and line for every claim)
- `gframe/deck_con.cpp` — the deck editor.
- `gframe/deck_manager.h`, `gframe/deck_manager.cpp`.
- `gframe/network.h` — `HostInfo`, `DeckSizes`.
- `gframe/generic_duel.cpp` and `gframe/duelclient.cpp` — the duel-entry path.
- `gframe/game.cpp` and the game-config type — where per-user deck-editor
  state is persisted between sessions.

**Do not read for this task:** anything under `client/`, the replay or
semantic-trace material, `docs/architecture/ydk-interoperability.md`, or the
M1/M2 architecture docs. None of it bears on this question, and pulling it in
mostly imports unrelated conclusions.

## Scope

Produce `docs/architecture/deck-builder-legality.md`, covering:

1. **What upstream's deck editor actually does about legality**, precisely,
   with citations. When it acts, on what state, against what data, and what
   the user sees.
2. **What upstream does at duel entry**, and where those inputs come from.
3. **A direct comparison of the two**, naming every semantic difference
   between them rather than summarising them as "both check the deck".
4. **What each `ValidationPolicy` field would have to be sourced from** for
   this project's deck builder — field by field, all six — and for each,
   whether upstream's deck editor has a real counterpart, and where the value
   would live.
5. **Recommended boundary** — your proposal for which layer owns this state
   and what its lifetime is, with the alternatives you rejected and why.
   State your confidence honestly; this section is a recommendation for Brain
   and the owner to ratify, not a decision you are making.

## Non-goals

- **No production code.** No changes under `ui/`, `policy/`, `data/`,
  `client/` or `gframe/`, and no build-system changes. Not even linking
  `policy/` into `ui/CMakeLists.txt` — that is the implementation round's job,
  and doing it here would make this round unreviewable as archaeology.
- **Do not write an ADR.** ADRs record accepted decisions; nothing is accepted
  yet. The "Recommended boundary" section is the input to that.
- **Do not modify `policy/`'s API**, and specifically do not add a default
  constructor, a factory, or a "default ruleset" to `ValidationPolicy`. If
  your finding is that one is needed, that is a *finding to report*, not a
  change to make.
- Do not design the QML, the visual treatment, or the interaction.
- Do not touch `docs/ROADMAP.md` or `README.md` — nothing shipped this round.

## Protected invariants

- **The UI must not implement game rules** (`CLAUDE.md`). Any boundary you
  recommend must leave every legality decision inside `policy/` and the
  engine. A recommendation in which the UI computes, caches, or second-guesses
  a legality result is wrong regardless of how convenient it is.
- **`ValidationPolicy` has no default, deliberately.** Its own header explains
  why, including the specific undefined behaviour an earlier aggregate shape
  allowed. Treat that as a constraint to design around.
- **The null-`LFList*` versus concrete `"N/A"` list distinction is real and
  preserved on purpose** (`deck-legality.md`, ADR 0007). They are two
  different behaviours, not one collapsed into the other. Any recommendation
  that treats "no banlist selected" as equivalent to "the unlimited list" must
  say explicitly which upstream state it means and why.
- **`gframe/` is upstream's and authoritative here.** Read it; do not modify
  it. Where our docs paraphrase it, the source wins.
- **Never describe planned functionality as shipped.** Nothing ships this
  round.

## Required investigation

Answer these from source. Do not assume any of them, in either direction —
each is genuinely open until you have read the code.

1. **Does upstream's deck editor validate the whole deck at all?** Find every
   call site of `CheckDeckContent` and `CheckDeckSize` in `gframe/` and say
   which code paths reach them. If the deck editor does *not* reach them, then
   say what it does instead, exactly, and where.
2. **Where does the deck editor's active banlist come from**, how is it
   chosen, how is it persisted across sessions, and what is in that collection
   — including anything upstream synthesises rather than loads from a file?
3. **Where do the non-banlist `ValidationPolicy` inputs come from** — deck
   sizes, allowed-card pool, forbidden types, ritual placement, whether
   content checking is on? Are any of them meaningful in a deck editor with no
   duel and no host, or are they duel-entry concepts only?
4. **Can a user build and save a deck in upstream that would be rejected at
   duel entry?** If so, that difference is the heart of this document, and the
   recommended boundary has to take a position on whether we reproduce it.
5. **What does the user actually see** in upstream's editor when a card or a
   deck is restricted — and at what moment?

## Acceptance criteria

- Every claim about upstream behaviour carries a `file:line` citation, and the
  quoted code says what the claim says it says.
- All five investigation questions are answered explicitly, including any
  answered "no" or "upstream does not do this".
- All six `ValidationPolicy` fields are covered individually. None is skipped
  as obvious.
- The document distinguishes throughout between *what upstream does* and *what
  we should do*. A reader must never have to guess which they are reading.
- The "Recommended boundary" section states its own confidence, names the
  alternatives considered, and says what would change the recommendation.
- Anything you could not determine is listed as an open question, not rounded
  to a clean answer.

## Required evidence

This round touches documentation only, so the per-layer table in `AGENTS.md`
asks for very little — and **saying so plainly is itself the required
evidence.** Specifically:

- State that no code changed, and confirm it: `git diff --stat <base>..<head>`
  showing only `docs/`.
- Do not run the cmake/ctest cycles, and do not report them. Running a build
  that could not have been affected and reporting it green is exactly the
  false-coverage pattern this project's evidence rules exist to stop.
- If you find yourself wanting to compile something to check a claim, that
  means the claim belongs in the implementation round, not this document.

## Git expectations

Branch `m3/deck-builder-legality-boundary`, in the Builder worktree
(`.worktrees/builder`, relative to the repository root — see
[`docs/agents/worktree-mechanism.md`](../agents/worktree-mechanism.md)). You
will need `ocgcore` only if you decide you must read engine source; `gframe/`
is already present in every worktree.

Commit in focused commits, push the branch, open a PR whose body carries
`DO NOT MERGE — under review`. **Do not merge.**

## Completion-report schema

The standard report in `.claude/agents/builder.md`, plus:

- **A one-paragraph answer to investigation question 1**, up front. It is the
  finding most likely to change the shape of the implementation round, and
  Brain wants to read it before anything else.
- **Any place where our existing docs (`deck-legality.md`, ADR 0006, ADR 0007,
  `deck-builder-ui.md`) turn out to disagree with upstream source**, quoted
  both ways. Finding none is a fine answer; say so explicitly.
- **Your confidence in the recommended boundary**, and the single piece of
  evidence that would most change it.

# Brief 006 — two corrections holding the merge train

Status: queued

One brief lives here at a time. On delivery it moves to
[`delivered/`](delivered/), and only on adjudication to
[`archive/`](archive/) — see [`README.md`](README.md). Template and field
meanings: [`docs/roles/builder.md`](../roles/builder.md).

---

## MODE: CORRECTIVE

## Goal

Close two BLOCKERs. Three pull requests are stacked and none of them can merge
until both are fixed.

## Why this is next

`master` ← PR #19 ← PR #20 ← PR #21. PR #20 and PR #21 each carry PR #19's
commits, so a defect in #19 blocks all three. One BLOCKER sits in #19 and one
in #21.

## Base SHA

**This round does not create a branch.** Both corrections go onto branches
that already exist and already have open PRs. That is deliberate and it is the
one place this round departs from "one branch per task" — a corrective round
amends the work it corrects rather than opening a parallel copy of it.

This brief itself lives on `meta/round-3-corrections` (PR #22), stacked on
PR #21. Read it there; do not branch from it.

## Correction A — `docs/architecture/card-search.md`

**Branch:** `m3/architecture-citation-audit` (PR #19).

`card-search.md` §1.4 currently describes the ordering inside upstream's deck
sort comparators as a single shared shape: that each function compares the
stat it is named for first, with `get_monster_card_type` among the later
tiebreakers.

**The five comparators do not share one ordering.** Brain re-derived the
following directly from `gframe/data_manager.cpp`. It is given here rather
than posed as a question because this round's value is the corrected sentence,
not the rediscovery — but **read the source and confirm each line yourself
before you write anything.** A previous agent's finding is evidence, not fact,
and that principle does not stop applying because the previous agent was
Brain.

| Function | Actual comparison order |
|---|---|
| `deck_sort_lv` | `get_monster_card_type` **first**, then level, attack, defense, `check_codes` |
| `deck_sort_atk` | attack, defense, level, then `get_monster_card_type`, `check_codes` |
| `deck_sort_def` | defense, attack, level, then `get_monster_card_type`, `check_codes` |
| `deck_sort_name` | `GetUppercaseName()` compare, then `check_codes`. No `card_sorter`, no type check |
| `deck_sort_passcode_descending` | raw `code` compare only |

So `deck_sort_lv` is the odd one out, and the document's **previous** wording
("monster type, then the chosen stat") was correct for it and wrong for the
other two. The current wording is correct for those two and wrong for
`deck_sort_lv`. Neither sentence is true of all three.

Write something true of all five. If that means the sentence stops being one
sentence, let it.

While you are in that paragraph, check its cited line range against the actual
extent of the functions it describes, and note that `card_sorter` itself sits
outside the cited range — the current text already says so, and that part is
correct.

## Correction B — the QML mirror

**Branch:** `meta/windows-msvc-build` (PR #21).

`ui/tests/CMakeLists.txt` mirrors five QML files into
`ui/tests/.qml_mirror/` so that `qmlcachegen` never sees a `..` in a path.
`file(CREATE_LINK ... SYMBOLIC COPY_ON_ERROR)` makes each mirror entry a
symlink where the host allows it and a plain copy otherwise.

**On a host that falls back to copies, the mirror goes stale, and the test
passes anyway.** The staleness comparison runs at CMake *configure* time, and
editing a QML file does not trigger a reconfigure. Brain reproduced this on
the primary dev machine, where all five mirror entries are copies rather than
symlinks:

```
appended "// BRAIN-STALENESS-PROBE-12345" to ui/qml/screens/DeckBuilderScreen.qml
cmake --build ui/build --parallel        (no explicit reconfigure)

real   contains marker: True
mirror contains marker: False
```

`test_deckbuilder_screen` therefore compiled and tested the *previous* version
of the file and reported green. An explicit `cmake -S ui -B ui/build` does
refresh it, so the mechanism works — nothing makes it run when it needs to.

Two things make this blocking rather than a rough edge:

- **The next round edits exactly these five files** and cites exactly this
  test as its evidence. A green run against stale QML is precisely the
  "a test that reports green while the property it names is violated" failure
  this project treats as worse than no test at all.
- **CI cannot catch it.** `file(CREATE_LINK ... SYMBOLIC)` succeeds on Linux,
  so CI always gets a live symlink and never exercises the copy path. This is
  Windows-only and invisible to the pipeline — the same shape as the three
  defects PR #21 exists to fix.

Make the mirror correct under a plain `cmake --build`. How is yours.

## Non-goals

- **Do not redo either round.** Everything else in PR #19 and PR #21 has been
  independently verified and holds. Two targeted corrections, nothing else.
- **Do not force-push, rebase or amend existing commits** on either branch.
  Add new commits. Both branches have open PRs whose earlier heads were
  reviewed, and rewriting them destroys that record.
- **Do not remove the `DO NOT MERGE` line** from either PR body. You do not
  merge.
- Do not touch `gframe/`, `ocgcore/`, `.github/workflows/`, or any repository
  setting.
- Do not change production C++. PR #21 deliberately touched only test code and
  build files; keep it that way.

## Protected invariants

- **Never weaken a warning, or a test, to make something pass.** `/W4`,
  `/permissive-` and `EDOPRO_NEXT_WERROR` stay exactly as they are.
- **Do not trade a build-time failure for a runtime one.** PR #21's report
  records that moving `qt_add_qml_module` into `ui/`'s own directory broke
  `edopro_next_shell` at runtime (`Module "EdoproNext" contains no type named
  "Main"`). That approach was tried and rejected for good reason; if you
  revisit it, you must show the shell still loads.
- **Linux must keep working.** CI is the proof and it is not optional here.
  A fix that repairs the Windows copy path by breaking the symlink path is
  not a fix.
- **`card-search.md` describes upstream, not us.** Do not "correct" it toward
  what `CardSearchIndex` does; ADR 0005 Decision 1 records that our ranking is
  deliberately a different scheme.

## Required investigation

1. For B: **can the mirror be avoided entirely?** It exists only to keep `..`
   out of a path `qmlcachegen` computes. If there is a way to satisfy that
   without duplicating files at all, it is worth more than a better refresh.
   Say what you considered and why you chose what you chose.
2. If the mirror stays: what actually makes it refresh for a plain
   `cmake --build`? A configure-time dependency and a build-time custom
   command are different mechanisms with different failure modes. Name which
   you used and what it does when the source file is deleted or renamed.
3. Does your fix behave correctly on **both** paths — symlink and copy? The
   copy path is the one that broke; the symlink path is the one CI exercises.
   Neither may regress.
4. For A: are there other claims in `card-search.md` §1.4 that assume the
   three comparators share a shape? Fixing one sentence while an adjacent one
   makes the same wrong assumption is not a fix.

## Acceptance criteria

- **The staleness reproduction, run both ways.** Show the marker test failing
  before your change and passing after it: edit one of the five mirrored QML
  files, run **only** `cmake --build ui/build`, and demonstrate that what the
  test compiles reflects the edit. This is the acceptance bar for B and a
  narrative description of it is not sufficient.
- `card-search.md`'s ordering description is true for all five comparators,
  each confirmed against source.
- Both `ui/` suites still pass, including `test_deckbuilder_screen`.
- All four configure/build/`ctest` cycles still green on Windows/MSVC.
- CI green at **both** new head SHAs.
- Neither PR body's `DO NOT MERGE` line removed; no commit rewritten on either
  branch.

## Required evidence

- `git diff` for each correction, separately, against the branch's previous
  head.
- The staleness reproduction, real output, before and after.
- The four cycles' real output.
- `python -m unittest discover -s tests -v`.
- CI check-run status at both new head SHAs, queried rather than assumed.
- The source lines you read to confirm each of the five comparators.

## Git expectations

Two branches, both existing:

```
git -C .worktrees/builder fetch origin
git -C .worktrees/builder checkout m3/architecture-citation-audit   # correction A
git -C .worktrees/builder checkout meta/windows-msvc-build          # correction B
```

New commits on each. Push both. Do not open new PRs — #19 and #21 already
exist and will pick the commits up. Do not merge, and do not remove either
`DO NOT MERGE` line.

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../roles/builder.md), plus:

- **The staleness reproduction first**, both directions, as real output.
- **The five comparators as you read them**, with the line you read for each —
  including any place Brain's table above turned out to be wrong.
- **What you considered for B and rejected**, particularly whether the mirror
  could be removed entirely.
- The two new head SHAs, and CI status at each.

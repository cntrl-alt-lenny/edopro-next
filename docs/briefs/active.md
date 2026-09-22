Brief-ID: 016-2026-09-22-records-and-test-gaps

Status: active

## MODE: IMPLEMENTATION

## Goal

Correct the known-wrong statements that accepted rounds recorded but did not
fix, and close the small test gaps they left. One theme: the repository's
records and tests should say exactly what is true. No behaviour change to
any product code.

## Why this is next

Rounds 011, 012 and 015 were each accepted with a recorded, known defect in
a document or a test, and deferred to a later round. This is that round.
Leaving known-wrong records in place is the drift this project's evidence
discipline exists to stop.

## The items (each is a problem to solve, with its source)

1. **Two imprecise sentences in `docs/architecture/read-failure-class.md`.**
   They are named in
   [`archive/011-…`](archive/011-2026-09-20-read-failure-class.md)'s outcome,
   *Known imprecision merged*:
   - "no tested spelling reaches a second path open" is literally false.
     The `ERROR_ACCESS_DENIED` path makes a second zero-access
     classification open. The invariant that holds is that no second open
     is ever *read*.
   - The zero-access fallback is described as if only an inbound pipe
     triggers it. The code enters it for any `ERROR_ACCESS_DENIED`.

   Re-read `data/src/ydk.cpp` and `policy/src/lf_list.cpp` and make the
   document match the code.
2. **Two double-spaced PR-body fixtures.** `tests/fixtures/pr_bodies/pr_10.txt`
   and `pr_16.txt` were committed with every `\r\n` turned into `\n\n`, and
   `tests/test_pr_evidence.py` pins line numbers in the doubled files
   ([`archive/012-…`](archive/012-2026-09-21-evidence-freshness-reland.md)).
   The fixtures must be byte-faithful to the live bodies
   (`gh pr view <n> --json body`, line endings normalised to LF), and the
   pinned expectations must follow. Every fixture's verdict must stay what
   it is.
3. **A wrong citation in `docs/adr/0010-deck-builder-ruleset-and-legality-ui.md`,
   Decision 3.** "`ImportDeck`, `deck_manager.cpp:136-146`" points at
   `DeckManager::TypeCount`. The function is `DeckBuilder::ImportDeck` at
   `gframe/deck_con.cpp:136-146`
   ([`archive/015-…`](archive/015-2026-09-22-deck-builder-legality-ui.md)).
   Fix the citation and quote what is there. While in that file, re-read
   every other upstream citation in Decisions 2 and 3, which the last round
   did not re-derive.
4. **A missing regression test.** Under "No banlist", S3 of brief 015 named
   two examples that must show the "checks are not being made" disclosure:
   four copies of one card, and an Extra Deck monster in the Main Deck. Only
   the first is pinned by a test. Pin the second. It must fail if the
   disclosure is removed.
5. **The push-guard tests under Windows PowerShell.** From PowerShell, 8
   `tests/test_push_guard.py` cases fail with exit 127. The test finds `sh`
   or `bash` with `shutil.which`, and there that resolves to the WSL launcher
   (`C:\Users\…\AppData\Local\Microsoft\WindowsApps\bash.exe`). The launcher
   then receives a Windows path it cannot open. From Git Bash, all 13 pass.
   Required outcome: the suite gives a correct result from both shells on
   this machine. CI must still run these tests on Linux, not skip them, and
   no case may be weakened. If the only honest answer on a machine with no
   usable POSIX shell is a visible skip, say so and argue it.
6. **Seat worktrees and submodules.** `docs/agents/worktree-mechanism.md` is
   project-owned. It does not say that `git worktree add` leaves the
   `ocgcore` submodule uninitialised in a seat's worktree. That gap produced
   a false Verifier finding in round 015. Document the setup step and the
   reason.

## Scope and non-scope

- **In scope:**
  - the files named above;
  - `tests/`;
  - `docs/agents/worktree-mechanism.md`;
  - any link or regenerated artefact the changes require (for example the
    README status block, if `docs/ROADMAP.md` changes; it should not).
- **Out of scope:**
  - any production-code behaviour change (`client/`, `data/`, `policy/`,
    `ui/`: none of their code changes, only tests and documents);
  - `.github/workflows/`, settings, `gframe/`, `ocgcore/`;
  - canonical framework files under `docs/agents/` other than the
    project-owned three, and the installed framework tools.
- **Deferred:** the presentation tidy-ups (`HomeScreen.qml`, `hero.svg`,
  the social preview, and splitting ROADMAP M6), which belong to a later
  round.

## Protected invariants

- The push guard's behaviour and its CI enforcement are unchanged. Every
  historical bypass case in `tests/test_push_guard.py` still runs in CI.
- The neutrality guard still passes.
- Name every changed pre-existing test assertion, with its before and after
  property.

## Acceptance criteria

- **Each item** is closed with evidence, or reported as not closable, with
  the reason.
- **Item 2:** the fixture-vs-live comparison shows 28 of 28 identical, and
  the corpus test passes.
- **Item 4:** the new test fails with the disclosure removed and passes with
  it.
- **Item 5:** the relevant suite result is shown from PowerShell and from
  Git Bash, with CI's result at the final head.
- **Suite:** `python -m unittest discover -s tests -v` and the three
  generator `--check`s are green.
- **`ui/`:** builds and passes `ctest` under MSVC, since item 4 adds a `ui/`
  test.
- **The PR body:** begins `DO NOT MERGE — under review` and passes
  `python tools/check_pr_evidence.py`.

## Required evidence

- Checkout check, and the tool, exact model name and version, and OS (from
  `cmd /c ver`).
- For every citation touched, the verbatim line re-read.
- The evidence named in each acceptance criterion.
- Check-run conclusions at the final head.
- Every sentence removed from a durable document, listed explicitly.
- What you did not run.

## Completion-report schema

The standard report in
[`docs/agents/roles/worker.md`](../agents/roles/worker.md), with one section
per item.

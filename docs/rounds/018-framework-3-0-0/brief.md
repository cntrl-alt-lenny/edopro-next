# 018-framework-3-0-0: move to agentic-framework 3.0.0

Tier: 2
Mode: implementation
Supersedes: none

## Goal

When this round merges, edopro-next runs agentic-framework 3.0.0 and nothing
else: every step under "What an adopter must do" in the 3.0.0 section of the
framework's `CHANGELOG.md` is done, and the repository is in exactly the setup
it will keep. No `.framework` copy, retired framework file or 2.x-only
document is left behind, and nothing the project relied on is lost on the way.

## Context

Why now: 3.0.0 is a major release, and the framework says a major release is
taken before the next round starts. Round 016 (records and test gaps) is
finished: it merged as pull request 32 on 2026-09-22. `docs/briefs/active.md`
still calls it active. That file is out of date, and round 016 is closed
history.

Why 018 and not 017: the number 017 is already used. Branch
`meta/presentation-tidy` queues a 2.x brief "017-2026-09-22-presentation-tidy"
that was never started. That branch also holds round 016's accepted
adjudication record, which is not on `master`. Both are dealt with below.

Read these, in this order:

1. The framework at tag `v3.0.0`, cloned into a temporary folder outside this
   project and not next to it (called `<framework>` below):
   `<framework>/framework/FRAMEWORK.md`, `<framework>/framework/roles/worker.md`
   (or `verifier.md`), and the 3.0.0 section of `<framework>/CHANGELOG.md`,
   including "What an adopter must do". These documents decide how this round
   runs.
2. This project's current `AGENTS.md`, `CLAUDE.md` and `docs/state.md`. The
   project's invariants and evidence rules in them still apply to this round.
3. Only as the work needs them: the `.claude/` files, `docs/agents/`,
   `docs/roles/`, `docs/briefs/README.md`, `docs/contributing.md`, `.gitignore`,
   `tests/test_docs_consistency.py`, `tests/test_save_agent_reply.py`, and
   `origin/meta/presentation-tidy`.

Until this round lands, the project has no `tools/fw.py` of its own. Every
seat therefore runs the framework clone's copy, as
`python3 <framework>/tools/fw.py --cwd <project> <command>`, with `--cwd`
before the command word. Use `py -3` or `python` if `python3` is not found.

Do not read or write the owner's Dev Hub folder. Framework problems are
reported through the "Framework feedback" issue form on
`github.com/cntrl-alt-lenny/agentic-framework`.

Brain ran `adopt.py . --update --dry-run` from `v3.0.0` against `master` at
the time of writing. The plan is summarised here so you can compare, and it
is not a substitute for your own run:

- **Creates:** `docs/agents/FRAMEWORK.md`, `tools/fw.py`,
  `tests/test_framework.py`, `docs/rounds/README.md`,
  `.claude/agents/worker.md` and `docs/agents/framework.json`.
- **Replaces:** the three role cards.
- **Writes `.framework` copies beside:** `.claude/agents/brain.md`,
  `.claude/agents/verifier.md` and `.claude/commands/status.md`, because they
  were edited here.
- **Removes:** ten unedited 2.x files.
- **Keeps, because something still refers to them:**
  `.claude/settings.json`, `.claude/README.md`, both `.claude/hooks/` files,
  `docs/agents/roles/README.md`, `tools/checkout.py`, `tools/report.py`,
  `docs/agents/CONSTITUTION.md`, `topologies.md`, `adapters.md`, `evidence.md`,
  `reports.md` and `git-and-isolation.md`.
- **Does not mention:** `.claude/agents/builder.md`, `docs/roles/`,
  `docs/agents/model-notes.md`, `launching.md`, `worktree-mechanism.md`,
  `tests/test_save_agent_reply.py` or `tests/test_docs_consistency.py`. All of
  them are project-owned 2.x leftovers.

## Decisions Brain has made for this round

These decisions are Brain's, so do not reopen them. If one turns out to be
unworkable, stop and report it.

- **Merge rule: owner-approves.**
- **Builder is this project's name for the framework's Worker.** It uses the
  Worker contract (`docs/agents/roles/worker.md`). Its seat runs
  `fw.py start --role builder`, and its report is
  `docs/rounds/<id>/builder.md`. For Claude Code the project keeps only the
  framework's three seat files, so `.claude/agents/builder.md` goes; the
  second dry run below cannot come back clean while `worker.md` is missing.
- **Branch names follow 3.0.0.** Briefs go on `brain/<round id>`. Seats get
  `<role>/<round id>` from `fw.py start`, and Tier 0 housekeeping goes on
  `brain/<topic>`. The `m<N>/…` and `meta/…` convention, the rule "do not
  introduce role-prefixed branches", and its `guard:branch-namespaces` comment
  are retired.
- **Seats are not tied to folders.** Any clone, cloud workspace or linked
  checkout is fine, and none of it is required. After `fw.py start`, every
  seat runs `git submodule update --init` for `ocgcore`, because `fw.py start`
  does not warn about submodules (framework issue 19).
- **Rounds live in `docs/rounds/`.** `docs/briefs/` stays as history.
  `docs/briefs/active.md` is deleted. Round 016's accepted record goes into
  `docs/briefs/archive/`. Take it verbatim from
  `origin/meta/presentation-tidy` (commit `ab1b83fe`, file
  `docs/briefs/archive/016-2026-09-22-records-and-test-gaps.md`) rather than
  writing a new one. The queued 2.x brief 017 is not archived and not run. It
  becomes a later 3.0.0 round.

## Scope and non-goals

In scope:

- everything the 3.0.0 adopter steps touch;
- `AGENTS.md`, `CLAUDE.md`, `docs/state.md` and a new archive document for
  state history;
- `.claude/`, `docs/agents/` (including new `docs/agents/local/` guidance if
  it is needed), `docs/roles/`, `docs/briefs/`, `docs/rounds/`;
- `.gitignore` comments;
- project documents that describe the 2.x workflow (`docs/contributing.md`,
  for one). Search for the others; do not assume this list is complete;
- tests that exist only to enforce the 2.x layout, or that test retired files.

Out of scope:

- any product code (`client/`, `data/`, `policy/`, `ui/`, `integration/`);
- `gframe/`;
- `ocgcore/`, including its submodule pointer;
- `.github/workflows/`, branch protection, repository settings and remotes;
- `LICENSE`, `COPYING` and `notices/`;
- `README.md`'s generated status block and `docs/ROADMAP.md`;
- deleting or changing any remote branch.

Product clean-up you notice goes in the report as a suggestion, and does not
go in the diff.

## Invariants

- The authority rules in `CLAUDE.md` and `AGENTS.md` still hold after the
  move. The engine is not the UI, and the UI implements no rules. `ocgcore/`,
  CardScripts and BabelCDB are authoritative and unmodified. `gframe/` stays
  C++17 and sees only `integration/legacy/semantic_observer.h`. Licensing and
  honesty rules are unchanged. Wording may be condensed, but no rule may be
  weakened or dropped without saying so.
- A green replay harness is never evidence that duel behaviour is unchanged.
  Source: `docs/architecture/replay-regression.md` §0.
- Framework files change only as the 3.0.0 steps say (framework rule 14).
  If a step cannot be done as written, stop and report it. Do not work
  around it.
- The push guard behaves exactly as before. `.githooks/pre-push` and
  `tests/test_push_guard.py` are project-owned. CI's "push guard tests must
  not skip" step must keep working.
- The five required CI check names are unchanged
  (`tests/test_ci_required_checks.py`).
- Nothing personal goes into any tracked document, archived ones included:
  no home-folder or drive paths, and no email addresses.

## Acceptance criteria

1. Every 3.0.0 "What an adopter must do" step is done, one by one, and the
   report says how each was done.
2. **Nothing 2.x is left.** No `*.framework` file, no retired framework file,
   and no 2.x-only document remain. That includes `docs/agents/model-notes.md`,
   `launching.md`, `worktree-mechanism.md`, `docs/roles/`, the `.claude` hook
   files and their test, and `.claude/agents/builder.md`. Each of these goes
   only after its project-specific content has a home in `AGENTS.md` or
   `docs/agents/local/`.
3. **`AGENTS.md` is the entry point:**
   - It contains the line `Merge rule: owner-approves`.
   - It points at `docs/agents/FRAMEWORK.md`.
   - It holds every rule that used to live only in `CLAUDE.md`.
   - It ties no seat to a `.worktrees/` folder.
   - It does not mention the Dev Hub.
   - It is under 2,500 words, as `fw.py check` counts them.
   - It states which protections GitHub really enforces. Re-check them live
     and date the check.
4. `CLAUDE.md` only points at `AGENTS.md` and adds no rules. Every entry file
   and seat file under `.claude/` is a pointer.
5. **`docs/state.md`:**
   - It is within the 1,000-word budget.
   - It holds only decisions, parked items and pointers.
   - It stores no full commit id outside `## Historical anchors`.
   - Its history is moved to an archive document that the state file links to.
   - Nothing on its "not proven, must not be claimed" list disappears from
     every live document.
6. `docs/briefs/active.md` is gone. Round 016's accepted record is in
   `docs/briefs/archive/`. `docs/briefs/` reads as closed history.
7. No link in any tracked document points at a removed file.
8. `python3 tools/fw.py check` shows 0 errors and 0 warnings.
9. The project's full Python suite and its tooling checks pass.
10. A second `adopt.py <project> --update --dry-run` from `v3.0.0` finds
    nothing to do. In practice that means:
    - no `create`, `replace`, `beside`, `remove` or `record` lines;
    - no `keep` line that asks for a reference to be removed or a file to be
      deleted.

    Plain project-owned `keep` lines and `same` lines are fine.

## Required evidence

The report must include all of the following. A report missing any item is
sent back.

1. **Seat start:**
   - the real output of `fw.py start` (framework clone, with `--cwd`);
   - the output of `git submodule update --init`, run right after it;
   - the output of `git submodule status`;
   - the operating system, established by a command rather than stated.
2. **Adoption:** the full output of the first `adopt.py --update --dry-run`
   and of the real `adopt.py --update` run.
3. **The `.framework` merges:** for each `.framework` copy, what differed and
   where any project-specific content went.
4. **Rule accounting (mandatory; cannot be skipped).** A table with one row
   for every project rule in:
   - the old `AGENTS.md`;
   - the old `CLAUDE.md`;
   - every old `.claude/` seat, command, settings and README file;
   - every deleted document.

   The table has four columns:

   | Column | Content |
   |---|---|
   | Rule | The rule, in a few words. |
   | Source | The old file and line. |
   | Destination | The file and section it now lives in; or the numbered `FRAMEWORK.md` rule that covers it; or **retired**. |
   | Why | Required for every retired row. |

   For deleted framework copies that adopt reports as unedited, one row per
   file is enough, stating that it held no project rules and giving the
   adopt output that shows it was unedited.
5. **State changes (mandatory; cannot be skipped).**
   - Every sentence removed from `docs/state.md`, each with where it went: the
     archive, `AGENTS.md`, `docs/agents/local/`, another document, or deleted
     as stale, with the reason.
   - Every sentence added to `docs/state.md`.

   Sentence by sentence. A summary is not acceptable.
6. **Inventory of other 2.x leftovers.** Every 2.x leftover this round does
   not remove, with a recommendation and a reason for each. Look for:
   - remote branches;
   - PR-body test fixtures;
   - `docs/briefs/archive/`;
   - the `.gitignore` entry;
   - the `check_pr_evidence.py` convention;
   - anything else you find.
7. **Checks, each with its real output and exit status, at the reported
   commit:**
   - `python3 tools/fw.py check`;
   - `wc -w AGENTS.md docs/state.md`;
   - `python3 -m unittest discover -s tests -v`, with totals and every skip
     named;
   - `python3 tools/generate_messages.py --check`;
   - `python3 tools/generate_protocol_constants.py --check`;
   - golden reproduction, as `AGENTS.md`'s `tools/`/`tests/` row describes;
   - a `git grep` over tracked files outside `docs/briefs/archive/` and
     `tests/fixtures/` for `Dev Hub`, `.worktrees/`, `active.md`,
     `CONSTITUTION`, `report.py`, `checkout.py`, `model-notes`, `launching.md`
     and `worktree-mechanism`, with every remaining hit explained;
   - `git diff --stat origin/master -- client data policy ui integration gframe ocgcore .github`,
     which must be empty.

   C++ modules are out of scope and need not be built. Say that they were not.
8. **Final state:** the full output of the second
   `adopt.py --update --dry-run`, and of
   `python3 <framework>/tools/fw.py --cwd <project> status`.
9. **Possible later rounds:** product clean-up noticed along the way, listed
   separately and not done.

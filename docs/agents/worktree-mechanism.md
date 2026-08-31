# Worktree mechanism

Brain, Builder and Verifier use **separate git worktrees of the same clone** —
never the same checkout with branch-switching.

This is not hygiene theatre. It went wrong in a sibling project: a worker
round ran directly in the coordinating session's checkout, left it on the work
branch, and a later session did not notice before committing unrelated work on
top. Nothing was lost, but it should not have been possible.

## Layout

The worktrees live **inside the repository**, at fixed repo-relative paths:

```
<clone>/                      Brain's worktree, the primary checkout, stays on master
<clone>/.worktrees/builder    Builder's worktree, its own task branch
<clone>/.worktrees/verifier   Verifier's worktree, detached at the SHA under review
```

`.worktrees/` is gitignored.

**They are nested rather than sibling directories on purpose.** Sibling
directories (`../edopro-next-builder`) depend on where the clone happens to
sit, so the layout differs between a Windows machine, a Mac and a CI box, and
every doc has to name absolute paths that are wrong somewhere. Nested, the
paths are repo-relative and therefore **identical on every machine** —
`.worktrees/builder` means the same thing on Windows, macOS and Linux, and a
fresh clone on a new device reaches the same layout with the same two
commands. It also keeps the parent directory clean.

All three are worktrees of the same repository (`git worktree list` from any
of them shows all three). They share one object database and one remote, so a
`fetch`/`push` from any is visible to the others. They do **not** share a
working directory or index: Builder cannot check out a branch Brain is sitting
on, and Brain switching branches for its own work cannot disturb Builder's
uncommitted state.

Verifier gets its own because reviewing means **checking out an exact SHA** —
detached, at the head under review — and building and testing there. Doing
that in Builder's worktree would destroy the very state being reviewed.

## Setting them up on a new machine

Three commands, run once from the repository root. They are the same on
Windows, macOS and Linux:

```bash
git config core.hooksPath .githooks
git worktree add --detach .worktrees/builder master
git worktree add --detach .worktrees/verifier master
```

**The first line is easy to forget and fails silently.** `core.hooksPath` is
per-clone configuration; without it git finds no hooks and runs none, with no
warning. `/status` checks it at the start of every Brain session for exactly
that reason.

Two further wrinkles specific to worktrees:

- A **relative** `core.hooksPath` is resolved per worktree, and the config
  itself is shared across all of them. So the setting reaches every worktree,
  but the hook only *exists* in a worktree whose checked-out commit contains
  `.githooks/`. A worktree parked on an older commit has the config and no
  hook — again silently.
- Git does not invoke `pre-push` at all when a push turns out to be a no-op
  ("Everything up-to-date"), because there are no ref updates to hand it. This
  is worth knowing before testing the guard by hand: pushing an
  already-current branch proves nothing, and it is exactly how a first attempt
  at verifying this hook produced a false pass.

Neither is a reason to distrust the guard for ordinary use, but both are
reasons not to mistake it for the guarantee. That is GitHub branch protection
on `master` — see `AGENTS.md`, "Never push to `master`".

Both `--detach`, deliberately. Git refuses to check out a branch another
worktree already holds, so creating these *on* `master` only works while Brain
happens to be somewhere else — exactly the sort of setup step that works once
and then fails confusingly six months later. Detached, they can be created
from any state, and each round checks out what it actually needs.

Both are reusable across rounds. They do not need recreating, only re-syncing.

Nothing else is machine-specific. The worktrees carry the same tree as the
primary checkout, so the build commands in `AGENTS.md`'s evidence table work
inside them unchanged.

## Submodules

`ocgcore/` is a submodule, and **git does not populate submodules in a new
worktree**. A worktree that only touches `client/`, `data/`, `policy/`, `ui/`,
`tools/` or `tests/` does not need it — none of those build against
`ocgcore`, and CI proves that separation holds.

A worktree that needs the upstream baseline build, or that must read upstream
engine source to check a semantic claim, initialises it there:

```bash
git -C .worktrees/verifier submodule update --init --recursive
```

Reading upstream engine source is Verifier's normal job, so its worktree will
usually want this. Note that `gframe/` is *not* a submodule — it is in the
tree already, in every worktree.

## Using them

**Brain** operates from the primary checkout, on `master`. Rehydration,
independent review, adjudication, and the merge of an accepted round all
happen here — see `AGENTS.md`, "Authority", for what Brain may merge and what
still goes to the owner.

**Builder**, at the start of each round:

```bash
cd .worktrees/builder
git fetch origin
git checkout -b m3/some-scope origin/master
```

Builder commits there, pushes the branch, and opens a PR. It does not merge.

**Verifier**, at the start of each review:

```bash
cd .worktrees/verifier
git fetch origin
git checkout --detach <head-sha>
```

Detached and at the literal SHA, deliberately — reviewing "the branch
generally" is how a review ends up describing a different tree from the one
that will be merged.

## Consequences of nesting, and how they are handled

**Searching.** `.worktrees/` holds full copies of the source tree, so a naive
recursive search from the repository root would return every hit three times.
Because it is gitignored, `ripgrep` (and anything else that honours
`.gitignore`, including this project's own agent tooling) skips it
automatically. Plain `find`, `ls -R` and shell globs do **not** — exclude it
explicitly if you use them.

**Editing.** Never edit a file under `.worktrees/` from the primary checkout.
Those paths belong to another session's round. If a sweeping edit ever needs
to run across the tree, scope it to the tracked paths.

**`git status`.** `.worktrees/` being ignored is what keeps the primary
checkout's status meaningful. The framework's working discipline requires
re-checking `git status` at every task boundary, which is useless if status is
permanently dirty.

**`git clean`.** `git clean -fdx` is **safe**: git recognises each worktree as
a separate repository and reports `Would skip repository .worktrees/builder`.
Only the doubled force, `git clean -ffdx`, would delete them — and deleting a
worktree that way leaves stale metadata behind in `.git/worktrees/`. Use
`git worktree remove` instead.

**Build directories are per-worktree.** Each has its own `client/build/`,
`data/build/`, `policy/build/` and `ui/build/`. They are not shared and must
not be pointed at each other: a stale build tree from another worktree is
exactly the kind of invisible state that produces a confidently wrong "tests
pass".

## Deleting a merged branch that is still checked out

A worktree holds a lock on its checked-out branch, so `git branch -d` refuses
while Builder still has it. Either detach that worktree first
(`git -C .worktrees/builder checkout --detach master`), or leave the merged
branch until the next round replaces it. It is clutter, not a correctness
risk.

## Removing a worktree

```bash
git worktree remove .worktrees/builder
```

from the primary checkout. Do not `rm -rf` the directory — that leaves stale
metadata in `.git/worktrees/` which `git worktree prune` then has to clean up
anyway.

## The shared inbox

`.claude/hooks/save_agent_reply.py` writes each Claude Code session's final
reply to `<git-common-dir>/agent-inbox/<role>-latest.md`, where the role comes
from the worktree directory's name: `builder`, `verifier`, or `brain` for the
primary checkout. A `-builder` / `-verifier` suffix is also accepted, so a
two-clones-instead-of-worktrees setup keeps working. That path is inside
`.git/`, so it is never version-controlled and needs no `.gitignore` entry.

Two limits, both important:

- It only fires for **Claude Code** sessions. A Verifier round run in another
  vendor's tool — which this project explicitly prefers — never writes there.
- A missing or stale inbox file therefore means **unknown**, never *nothing
  happened*. Check the timestamp.

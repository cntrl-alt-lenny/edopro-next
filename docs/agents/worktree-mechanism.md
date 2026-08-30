# Worktree mechanism

Brain, Builder and Verifier use **separate sibling git worktrees of the same
clone** — never the same checkout with branch-switching.

This is not hygiene theatre. It went wrong in a sibling project: a worker
round ran directly in the coordinating session's checkout, left it on the work
branch, and a later session did not notice before committing unrelated work on
top. Nothing was lost, but it should not have been possible.

## Layout

```
C:\Users\leona\Dev\edopro-next            <- Brain's worktree, stays on master
C:\Users\leona\Dev\edopro-next-builder    <- Builder's worktree, its own task branch
C:\Users\leona\Dev\edopro-next-verifier   <- Verifier's worktree, detached at the SHA under review
```

All three are worktrees of the same repository (`git worktree list` from any
of them shows all three). They share one object database and one remote, so a
`fetch`/`push` from any is visible to the others. They do **not** share a
working directory or index: Builder cannot check out a branch Brain is sitting
on, and Brain switching branches for its own work cannot disturb Builder's
uncommitted state.

Verifier gets its own because reviewing means **checking out an exact SHA** —
detached, at the head under review — and building and testing there. Doing
that in Builder's worktree would destroy the very state being reviewed.

## Creating them

Once per machine, from the primary checkout:

```bash
git worktree add --detach ../edopro-next-builder master
git worktree add --detach ../edopro-next-verifier master
```

Both `--detach`, deliberately. Git refuses to check out a branch that another
worktree already holds, so creating these *on* `master` only works while Brain
happens to be somewhere else — which is exactly the sort of setup step that
works once and then fails confusingly six months later. Detached, they are
created from any state, and each round checks out what it actually needs.

Both are reusable across rounds. They do not need recreating, only re-syncing.

## Submodules

`ocgcore/` is a submodule, and **git does not populate submodules in a new
worktree**. A worktree that only touches `client/`, `data/`, `policy/`, `ui/`,
`tools/` or `tests/` does not need it — none of those build against
`ocgcore`, and CI proves that separation holds.

A worktree that needs the upstream baseline build, or that must read upstream
engine source to check a semantic claim, initialises it there:

```bash
git -C ../edopro-next-verifier submodule update --init --recursive
```

Reading upstream engine source is Verifier's normal job, so its worktree will
usually want this.

## Using them

**Brain** operates from the primary checkout, on `master`. Rehydration,
independent review, and the merge recommendation happen here. Brain does not
merge — the owner does.

**Builder**, at the start of each round:

```bash
cd ../edopro-next-builder
git fetch origin
git checkout -b <m3/some-scope> origin/master
```

Builder commits there, pushes the branch, and opens a PR. It does not merge.

**Verifier**, at the start of each review:

```bash
cd ../edopro-next-verifier
git fetch origin
git checkout --detach <head-sha>
```

Detached and at the literal SHA, deliberately — reviewing "the branch
generally" is how a review ends up describing a different tree from the one
that will be merged.

## Build directories are per-worktree

Each worktree has its own `client/build/`, `data/build/`, `policy/build/` and
`ui/build/`. They are not shared and must not be pointed at each other: a
stale build tree from another worktree is exactly the kind of invisible state
that produces a confidently wrong "tests pass".

## Deleting a merged branch that is still checked out

A worktree holds a lock on its checked-out branch, so `git branch -d` refuses
while Builder still has it. Either detach that worktree first
(`git -C ../edopro-next-builder checkout --detach master`), or leave the
merged branch until the next round replaces it. It is clutter, not a
correctness risk.

## Removing a worktree

```bash
git worktree remove ../edopro-next-builder
```

from the primary checkout. Do not `rm -rf` the directory — that leaves stale
metadata in `.git/worktrees/` which `git worktree prune` then has to clean up
anyway.

## The shared inbox

`.claude/hooks/save_agent_reply.py` writes each Claude Code session's final
reply to `<git-common-dir>/agent-inbox/<role>-latest.md`, where the role is
inferred from the worktree directory name. That path is inside `.git/`, so it
is never version-controlled and needs no `.gitignore` entry.

Two limits, both important:

- It only fires for **Claude Code** sessions. A Verifier round run in another
  vendor's tool — which this project explicitly prefers — never writes there.
- A missing or stale inbox file therefore means **unknown**, never *nothing
  happened*. Check the timestamp.

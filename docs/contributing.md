# Working on this repository

[`AGENTS.md`](../AGENTS.md) is the project's instructions — what it is, the coordination
model (Brain, Builder, Verifier), and the evidence each kind of change must produce;
[`CLAUDE.md`](../CLAUDE.md) only points at it. [`state.md`](state.md) is the short
rehydration doc for a fresh session; [`agents/FRAMEWORK.md`](agents/FRAMEWORK.md) is the
framework this project runs on.

A Builder batch runs on its own `worker/<batch>` branch (see
[`agents/FRAMEWORK.md`](agents/FRAMEWORK.md)) — any clone, cloud workspace or linked
checkout works. After checking it out, run `git submodule update --init` for `ocgcore`.
Run `git config core.hooksPath .githooks` once per clone to enable the local push guard.

[`.githooks/pre-push`](../.githooks/pre-push) rejects a push to `master` or one that would
ship drift in the derived protocol tables. It is a local convenience, not a control — it
needs that config in every clone, it is bypassed by `git push --no-verify`, and a fresh
clone has no guard until it is set.

The real guarantee is GitHub branch protection on `master`: changes only via PR, required
checks green, applied to administrators too, no force-pushes or deletions. See
[`AGENTS.md`](../AGENTS.md), "What is actually enforced", which says how to re-check this
live rather than trusting a date here.

A linked worktree (`git worktree add --detach .worktrees/<role> master`) is an optional
convenience for running a seat alongside the primary checkout; `.worktrees/` is gitignored.
It is never required — any clone or workspace works equally well.

## Derived files

Some files are generated, and a check fails if they drift, so do not edit them by hand:

- the message table and protocol constants
  (`tools/generate_messages.py`, `tools/generate_protocol_constants.py`);
- the "What works" block of the landing page, derived from [`ROADMAP.md`](ROADMAP.md) by
  `tools/generate_readme_status.py`. Change the roadmap, then run that tool.

[Building](building.md) has the commands.

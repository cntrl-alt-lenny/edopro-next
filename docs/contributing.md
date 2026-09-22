# Working on this repository

[`CLAUDE.md`](../CLAUDE.md) is the working agreement — what the project is, and what may not
be broken. [`AGENTS.md`](../AGENTS.md) is the coordination model: three roles (Brain,
Builder, Verifier), the brief and review loop, and the evidence each kind of change must
produce. [`state.md`](state.md) is the short rehydration doc for a fresh session.

Builder and Verifier each work in a git worktree of this same clone, at fixed
repo-relative paths so the layout is identical on every machine. From the repository root,
on any OS:

```bash
git config core.hooksPath .githooks
git worktree add --detach .worktrees/builder master
git worktree add --detach .worktrees/verifier master
```

The first line installs [`.githooks/pre-push`](../.githooks/pre-push), which rejects a push
to `master` or one that would ship drift in the derived protocol tables. It is a local
convenience, not a control — it needs that config in every clone, it is bypassed by
`git push --no-verify`, and a fresh clone has no guard until it is set.

The real guarantee is GitHub branch protection on `master`, **enabled** as of 2026-08-31:
changes only via PR, required checks green, applied to administrators too, no force-pushes
or deletions. See [`AGENTS.md`](../AGENTS.md), "Never push to `master`". On 2026-09-21
`gh api repos/cntrl-alt-lenny/edopro-next/branches/master --jq .protected` returned `true`.

`.worktrees/` is gitignored. Rationale, per-role usage and the caveats that come with
nesting are in [`agents/worktree-mechanism.md`](agents/worktree-mechanism.md).

## Derived files

Some files are generated, and a check fails if they drift, so do not edit them by hand:

- the message table and protocol constants
  (`tools/generate_messages.py`, `tools/generate_protocol_constants.py`);
- the "What works" block of the landing page, derived from [`ROADMAP.md`](ROADMAP.md) by
  `tools/generate_readme_status.py`. Change the roadmap, then run that tool.

[Building](building.md) has the commands.

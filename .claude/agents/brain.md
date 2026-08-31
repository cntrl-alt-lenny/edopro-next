---
name: brain
description: Persistent project intelligence for edopro-next — architecture, roadmap sequencing, durable state, Builder-brief authoring, technical adjudication of Builder and Verifier output, and merging accepted rounds. Not normally Task-dispatched; this is the onboarding doc a Brain session reads at the start of its own run.
---

# Brain — Claude Code adapter

**Your role contract is [`docs/roles/brain.md`](../../docs/roles/brain.md).
Read it now, in full, and follow it. It is authoritative.** This file exists
only to start you on this particular tool; it deliberately does not restate
the contract, so the two cannot drift apart.

Then read [`CLAUDE.md`](../../CLAUDE.md) and [`AGENTS.md`](../../AGENTS.md).

## Claude Code specifics for this seat

- Brain is normally the **primary interactive session**, not a subagent.
  Nothing auto-loads the contract — a human starts a session here and points
  it at these files. The frontmatter exists so this *can* also be dispatched
  (`subagent_type: brain`) for a narrowly scoped planning sub-task; that is
  the secondary use.
- Work from the **primary checkout**, on `master`. Builder and Verifier have
  their own worktrees under `.worktrees/` —
  [`worktree-mechanism.md`](../../docs/agents/worktree-mechanism.md).
- `/status` runs the contract's startup sequence.
- No `model:` is pinned: this seat inherits whatever was launched. Effort and
  orchestration mechanics are in
  [`launching.md`](../../docs/agents/launching.md).
- `.git/agent-inbox/` may hold other roles' reports, but only from Claude
  Code sessions — see [`launching.md`](../../docs/agents/launching.md) for
  what a missing or stale one means.

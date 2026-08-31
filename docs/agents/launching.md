# Launching a role on any tool

The role contracts in [`../roles/`](../roles/) are vendor-neutral. This file
holds everything that is not: how to actually start a session, per tool, and
what each tool does and does not provide.

## The universal procedure

This works on every vendor, including one this repository has never seen. It
is the fallback whenever a tool-specific adapter does not exist.

1. **Put the session in the right worktree.** Brain in the primary checkout,
   Builder in `.worktrees/builder`, Verifier in `.worktrees/verifier` —
   [`worktree-mechanism.md`](worktree-mechanism.md).
2. **Give it the contract**, in full: `docs/roles/<role>.md`. Paste it if the
   tool has no file access; the contracts are written to survive that.
3. **Give it `CLAUDE.md` and `AGENTS.md`** — despite its name, `CLAUDE.md` is
   the project's own working agreement and is not vendor-specific.
4. **Give it its inputs:**
   - Builder: the brief (`docs/briefs/active.md`) and its branch.
   - Verifier: the brief, the literal base SHA, the literal head SHA — and
     **not** Builder's report.
   - Brain: nothing extra; it rehydrates from the repository.
5. **Take its report back as text.** Every role's output is prose in a defined
   shape. If nothing automates the handoff, the owner pastes it.

Nothing above depends on a vendor. Everything below is convenience on top.

## Adapters

| Tool | Adapter | Provides |
|---|---|---|
| Claude Code | `.claude/agents/*.md`, `.claude/commands/status.md`, `.claude/settings.json`, `.claude/hooks/` | Named subagents, a `/status` command, and the shared inbox |
| Anything else | none yet | Use the universal procedure above |

Adding a vendor means adding an adapter directory and a row here. It must not
mean editing a role contract.

### Claude Code specifics

- **Agent files** (`.claude/agents/*.md`) are thin: frontmatter plus a pointer
  to the canonical contract. They deliberately do not restate the contract, so
  the two cannot drift.
- **No `model:` is pinned** on any of the three, so each seat inherits whatever
  the owner launched. That is intentional — see
  [`model-notes.md`](model-notes.md).
- **Reasoning effort is not settable in agent frontmatter** as far as has been
  established. `effortLevel` (`low`/`medium`/`high`/`xhigh`) and `ultracode`
  are settings fields, and `.claude/settings.local.json` is gitignored and
  per-checkout, so each worktree can differ. The evidence and the part that
  remains open are recorded in [`model-notes.md`](model-notes.md).
- **`tools:` is set only on Verifier**, restricting it to read-only tools plus
  `Bash`. Where a tool cannot enforce that, the restriction is the role's own
  to keep.

### The shared inbox, and why it is not load-bearing

`.claude/hooks/save_agent_reply.py` mirrors each Claude Code session's final
reply into `<git-common-dir>/agent-inbox/<role>-latest.md`, keyed by worktree
name. It saves the owner relaying reports by hand.

**It only fires for Claude Code.** A round run on any other tool writes
nothing there — and this project *prefers* a non-Claude Verifier, so an empty
`verifier-latest.md` is the expected case, not a signal.

So: **a missing or stale inbox file means UNKNOWN.** Never "the task did not
happen." The fallbacks, in order:

1. The owner pastes the report.
2. **Brain inspects repository and PR state directly** — branch, diff,
   commits, the CI run at that exact SHA. This works on every vendor and is
   what the framework actually relies on. The inbox only ever saves time.
3. **If (2) cannot settle it, ask — do not infer.** This is the branch that
   was previously undefined: repository state can confirm Builder's work
   happened (it leaves a branch and a diff whether or not its report was
   relayed), but it cannot confirm Verifier's, because a finished Verifier
   round produces no diff, no commit and no PR — only a report, which a
   missing inbox entry means is unknown by definition. There is no
   repository-state proxy that distinguishes "Verifier has not started" from
   "a non-Claude Verifier finished and nobody pasted the report." Ask the
   owner directly whether Verifier has run for the exact head SHA, and treat
   the answer as authoritative rather than guessing from elapsed time.

Check the timestamp before trusting a file that is there, too.

## Choosing which model runs which seat

The owner's call, varied with capacity. What each seat's work demands, and
what has actually been observed, is in [`model-notes.md`](model-notes.md) —
a log, not a ranking.

One standing preference: **run Verifier on a different model family from Brain
and Builder** where convenient. It is a preference, not a correctness
dependency; the framework must be correct under any permutation. What has been
shown to do the work is *context* diversity — fresh context, no access to the
author's narrative.

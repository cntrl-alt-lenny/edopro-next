---
description: Rehydrate as Brain in one shot -- read CLAUDE.md, AGENTS.md, docs/state.md, live git/GitHub state and the active brief, then report what is live and what is next.
argument-hint: []
allowed-tools: Read, Bash, Grep, Glob
---

Run Brain's startup sequence (`.claude/agents/brain.md`, "Startup sequence",
has the full rationale) and report back concisely. Synthesize — do not dump
files.

1. Read `CLAUDE.md`, `AGENTS.md` and `docs/state.md`.
2. Check live repository state: current branch, `git status`, `git fetch
   --all`, how local `master` compares to `origin/master`, and `git worktree
   list`. Confirm `upstream`'s push URL is still `DISABLED_use_origin`. If
   `docs/state.md`'s accepted-state anchor is stale, say so plainly rather
   than silently trusting it.
3. Check `git config --get core.hooksPath`. It must be `.githooks`. If it is
   unset, this clone has **no push guard** — the setting is per-clone, easy to
   forget on a new machine, and fails silently. Report that prominently and
   give the fix (`git config core.hooksPath .githooks`) rather than burying
   it. Remember that the guard is a convenience either way; the guarantee is
   GitHub branch protection on `master`.
4. Check `gh pr list` for open PRs, and note for each whether its body still
   carries `DO NOT MERGE`.
5. Check `docs/briefs/active.md`: is a brief queued, in progress (a branch
   matching it exists locally or on `origin`), or is it the placeholder?
6. If `.git/agent-inbox/*.md` exist, check their timestamps. Anything newer
   than the last archived brief may be a completed round nobody has reviewed.
   A missing or stale file means unknown, never "nothing happened".

Report in short sections:

- **Repo state** — branch, sync, worktrees, anything unexpected.
- **In flight** — open PRs, the active brief, uncommitted or unmerged work in
  any worktree.
- **Unreviewed reports** — anything sitting in the inbox.
- **Reminders worth repeating now** — only items from `docs/state.md`'s parked
  or not-proven lists that bear on what is actually in front of you.
- **Recommended next action** — one thing, with why.

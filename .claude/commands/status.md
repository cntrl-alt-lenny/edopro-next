---
description: Rehydrate as Brain in one shot -- read CLAUDE.md, AGENTS.md, docs/state.md, live git/GitHub state and the active brief, then report what is live and what is next.
argument-hint: []
allowed-tools: Read, Bash, Grep, Glob
---

Run Brain's startup sequence (`docs/roles/brain.md`, "Startup sequence",
has the full rationale) and report back concisely. Synthesize — do not dump
files.

1. Read `CLAUDE.md`, `AGENTS.md` and `docs/state.md`.
2. Check live repository state: current branch, `git status`, `git fetch
   --all`, how local `master` compares to `origin/master`, and `git worktree
   list`. Confirm `upstream`'s push URL is still `DISABLED_use_origin`. If
   `docs/state.md`'s accepted-state anchor is stale, say so plainly rather
   than silently trusting it.
3. **Verify both push-guard layers against live state. Do not restate what the
   docs claim — query it.**

   ```bash
   git config --get core.hooksPath
   gh api repos/cntrl-alt-lenny/edopro-next/branches/master --jq .protected
   gh api repos/cntrl-alt-lenny/edopro-next/branches/master/protection --jq      '{admins: .enforce_admins.enabled, strict: .required_status_checks.strict, checks: .required_status_checks.contexts}'
   gh api repos/cntrl-alt-lenny/edopro-next/rulesets --jq length
   ```

   - `core.hooksPath` must be `.githooks`. Unset means this clone has **no
     local push guard** — per-clone config, easy to forget on a new machine,
     fails silently. Give the fix: `git config core.hooksPath .githooks`.
   - **`branches/master --jq .protected` is the reliable yes/no.** Do **not**
     use `rules/branches/master` for this: that endpoint reports *rulesets*
     only and returns `[]` even when classic branch protection is fully
     active. An earlier version of this command made exactly that mistake and
     would have reported `master` unprotected while it was protected.
   - Expected as of 2026-08-31: `protected: true`, `enforce_admins: true`,
     `strict: true`, five required checks. Report any drift from that in
     either direction — protection weakening is the dangerous direction, but
     protection strengthening means the docs are understating the guarantee.
   - The `rulesets` call is only to notice if someone later adds one alongside
     classic protection.

   Report the live answer as a fact obtained this session. This check exists
   because the framework once asserted a protection that did not exist.
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

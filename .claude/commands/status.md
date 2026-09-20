---
description: Rehydrate the coordinating session from the repository and live git state.
argument-hint: []
allowed-tools: Read, Bash, Grep, Glob
---

Run the rehydration sequence from `docs/agents/roles/brain.md`, in order, and
report what you actually observed this session — never what a document says is
true.

1. Read `AGENTS.md` and the project specification it names, then read
   `docs/state.md`. Treat every fact in it as a claim to spot-check.
2. Check live repository state: current branch, `git status`, `git fetch
   --all`, how local `master` compares to `origin/master`, and
   `git worktree list`. Confirm `upstream`'s push URL is still
   `DISABLED_use_origin`. If `docs/state.md`'s accepted-state anchor is stale,
   say so plainly rather than silently trusting it.
3. Verify both push-guard layers against live state; do not restate what the
   docs claim — query it.

   ```bash
   git config --get core.hooksPath
   gh api repos/cntrl-alt-lenny/edopro-next/branches/master --jq .protected
   gh api repos/cntrl-alt-lenny/edopro-next/branches/master/protection --jq      '{admins: .enforce_admins.enabled, strict: .required_status_checks.strict, checks: .required_status_checks.contexts}'
   gh api repos/cntrl-alt-lenny/edopro-next/rulesets --jq length
   ```

   - `core.hooksPath` must be `.githooks`. Unset means this clone has no local
     push guard; give the fix: `git config core.hooksPath .githooks`.
   - **`branches/master --jq .protected` is the reliable yes/no.** Do **not**
     use `rules/branches/master` for this: that endpoint reports rulesets only
     and returns `[]` even when classic branch protection is fully active.
   - Expected as of 2026-08-31: `protected: true`, `enforce_admins: true`,
     `strict: true`, five required checks. Report drift in either direction.
   - The `rulesets` call is only to notice if someone later adds one alongside
     classic protection.

   Report the live answer as a fact obtained this session. This check exists
   because the framework once asserted a protection that did not exist.
4. Check `gh pr list` for open PRs, and note for each whether its body still
   carries `DO NOT MERGE`.
5. Check `docs/briefs/active.md` and `docs/briefs/delivered/`: state whether
   each brief is queued, active, delivered-and-unadjudicated, or archived.
   Check `.git/agent-inbox/` too; a missing or stale report means unknown.

Report in short sections:

- **Repo state** — branch, sync, worktrees, anything unexpected.
- **In flight** — open PRs, active and delivered briefs, uncommitted or
  unmerged work in any worktree.
- **Unreviewed reports** — anything sitting in the inbox.
- **Reminders worth repeating now** — only items from `docs/state.md`'s parked
  or not-proven lists that bear on what is actually in front of you.
- **Recommended next action** — one thing, with why.

$ARGUMENTS

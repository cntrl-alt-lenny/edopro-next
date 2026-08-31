# Role contracts

The three permanent roles — [Brain](brain.md), [Builder](builder.md),
[Verifier](verifier.md) — are defined here, once, in vendor-neutral terms.

## Contract versus adapter

| | Contract | Adapter |
|---|---|---|
| Lives in | `docs/roles/` | `.claude/`, and whatever is added for other vendors |
| Says | what the role reads, what it may touch, what it must report, what it may never do | how to start it on one particular tool |
| Changes when | the role's responsibilities change | a tool changes |
| Audience | any model, any vendor | one tool |

The owner runs Anthropic, OpenAI and Google models, and any of the three seats
may be any of them on any given round. So a contract that only works on one
vendor's tooling is a defect, not a convenience.

**Test for whether something belongs here:** could a fresh model, on a tool
this repository has never seen, be handed this file plus a brief and do the
job? If a sentence would leave it stuck, that sentence is an adapter detail.

Things that are therefore *not* in these files: model names, reasoning-effort
settings, frontmatter schemas, slash commands, hook mechanics, subagent
dispatch syntax, and anything named after a specific product version.
[`../agents/launching.md`](../agents/launching.md) holds those.

## What must survive any vendor permutation

These are properties of the repository and the process, not of any tool, and
they are what makes the framework portable:

- **Git and GitHub** carry the state: branches, exact SHAs, PRs, CI results.
  Every role's inputs and outputs are addressable there.
- **Exact-SHA review.** Verifier is given a literal base and head, and confirms
  ancestry itself.
- **Evidence is reproduced, not relayed.** Any role can re-run the commands in
  `AGENTS.md`'s per-layer table.
- **The blind first pass.** Verifier forms a verdict before reading Builder's
  narrative. This is a discipline, not a feature of any tool.
- **Reports are text.** A completion report or a verifier report is plain
  prose in a defined shape. It can be pasted by a human if no automation
  carries it.

## Vendor-specific conveniences must fail loudly

Some adapters add convenience — Claude Code's `Stop` hook mirrors a session's
final reply into `.git/agent-inbox/`, so Brain can read what Builder said
without the owner relaying it.

**Every such convenience needs a stated fallback, and its absence must mean
UNKNOWN — never "the task did not happen."** A round run on a tool with no
inbox hook writes nothing there, and that is normal, not evidence. The
fallbacks, in order:

1. The human pastes the completion report.
2. Brain inspects repository and PR state directly — the branch, the diff, the
   commits, the CI run at that SHA. This always works, on every vendor, and is
   the reason the framework does not depend on the convenience.
3. **If (2) cannot settle it, ask — do not infer.** Repository state answers
   "has Builder's work happened?", because Builder leaves a branch and a diff
   behind whether or not anyone relayed its report. It cannot answer the same
   question for Verifier: a Verifier round that finished produces no diff, no
   commit and no PR of its own — only a report, which is exactly the thing a
   missing inbox entry means is unknown. So repository state cannot
   distinguish "Verifier has not started" from "a non-Claude Verifier
   finished and nobody pasted the report" — there is no proxy for that
   distinction to fall back on. Ask the owner directly whether Verifier has
   run for the exact head SHA in question, and treat the answer as
   authoritative rather than guessing from elapsed time or session activity.

A missing or stale artifact is a prompt to go and look, not a conclusion —
and where looking cannot resolve it, a prompt to ask.

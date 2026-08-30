# Model notes

A dated log of what has actually been observed running each seat on each
model. Kept out of `AGENTS.md` so that file stays lean and does not accumulate
model trivia that goes stale.

All three roles are explicitly model-agnostic (see `AGENTS.md`). This file is
supporting evidence for that design, **not a ranking and not a requirement**.
Brain's review standard — independently re-check everything — does not change
based on which model ran a round.

**Only record what was actually observed in a real round.** Do not
pre-populate with generic advice this project has not earned.

## Seat characteristics, for choosing on merit

Not model recommendations — a description of what each seat's work actually
demands, so the choice can be made deliberately.

- **Brain** — holds upstream source, our source, an ADR, a Verifier finding
  and CI state simultaneously, and compares them. Wants context headroom and
  a willingness to reject work it commissioned itself.
- **Builder** — one coherent problem, its own investigation, real code and
  real tests. Wants sustained engineering over a single task rather than
  breadth.
- **Verifier** — fresh context, adversarial, re-derives upstream semantics.
  **Prefer a different model family from Brain and Builder**; the seat's whole
  value is not sharing their blind spots.

## Mechanics observed in this environment

Verified against **Claude Code 2.1.181** on 2026-08-30. Recheck if the version
changes rather than inheriting these as permanent facts.

- Reasoning effort is **not** an agent-file frontmatter field. `effortLevel`
  (`low` / `medium` / `high` / `xhigh`) and `ultracode` exist in the settings
  schema instead. Because `.claude/settings.local.json` is gitignored and
  per-checkout, each worktree can pin a different effort — Brain's, Builder's
  and Verifier's need not match.
- Agent frontmatter does carry `name`, `description`, `tools` and `model`.
  This project's role files deliberately omit `model:` so each seat inherits
  whatever the owner launched; `verifier.md` does set `tools:` to keep that
  seat read-only apart from `Bash`.
- Multi-agent orchestration can pin model and effort per dispatched agent,
  which is the one mechanism that sets both declaratively.

## Round log

No rounds have run under this framework yet.

Record each round as: date, seat, brief, model and effort actually used,
outcome (accepted / corrected / rejected), and what was specifically observed
— including operational mishaps, which are usually more useful than
impressions of quality.

**Framework install (2026-08-30) — Brain seat, Claude Opus 5.** Reviewed both
sibling frameworks (`gx-spirit-caller`, `edopro-retro-formats`), this
repository's state, CI, and merged PR history, and authored the framework on
`meta/agentic-framework`. Not a Builder round and not evidence about the
Builder or Verifier seats. Owner's stated intent is to vary Brain's model with
available capacity rather than pin it.

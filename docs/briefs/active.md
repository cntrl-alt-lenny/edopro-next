# Active brief

Status: **no brief queued.**

One brief lives here at a time. When a round completes — accepted or rejected
— Brain moves this file to
`docs/briefs/archive/<NNN>-<date>-<slug>.md` (zero-padded sequence number,
then date, then slug) and replaces it with the next brief, or with this
placeholder.

The template and the meaning of each field are in
[`.claude/agents/builder.md`](../../.claude/agents/builder.md) under
"Brief template". The rule that matters most when writing one:

> **Describe the problem, not the solution.** Goal, why now, scope, non-scope,
> protected invariants, required investigation, acceptance criteria, required
> evidence. Not "edit function X at line Y". If you have found a correctness
> constraint, state it as an invariant with its source and let Builder design
> around it.

The next slice Brain currently recommends is in
[`docs/state.md`](../state.md) — confirm scope with the owner before briefing
it.

# Brief lifecycle

A brief moves through five states. The state is written on the brief itself,
as a `Status:` line, and it is also encoded in which directory the file sits
in — so the answer to "what is in flight?" is `ls`, not a careful read.

| Status | Lives in | Means |
|---|---|---|
| `queued` | `active.md` | Written, not started. |
| `active` | `active.md` | Builder is executing it now. |
| `delivered` | `delivered/` | Builder finished and opened a PR. **Not reviewed, not accepted.** |
| `accepted` | `archive/` | Verifier reviewed, Brain adjudicated, Brain merged (see `AGENTS.md`, "Authority", for what still goes to the owner). |
| `rejected` | `archive/` | Adjudicated and not merged. The record is kept; rejected rounds are the more informative half of this directory. |

`tests/test_docs_consistency.py` enforces the parts of this a machine can
check: that every brief carries a valid status, that `archive/` holds only
adjudicated ones, and that `delivered/` holds only delivered ones.

## Why `delivered` exists as its own state

It was originally two states, and that was wrong. Brief 001 was delivered but
not adjudicated, and `active.md` was needed for brief 002 — so 001 was moved
to `archive/` with a header explaining it was not really archived. That made
`archive/` mean two incompatible things at once: *completed history* and
*temporarily moved aside while still under review*. A cold session reading
`ls archive/` would have counted an unreviewed round as finished.

The underlying cause is that **the pipeline is genuinely concurrent and the
original lifecycle was written as if it were linear.** Builder can start round
N+1 while round N is still waiting on Verifier, and that is good for
throughput. The states have to model that rather than pretend it away.

## The constraint that concurrency introduces

**Brain must not queue a brief whose correctness depends on the outcome of an
unadjudicated round.** If round N's finding might be overturned by Verifier,
round N+1 must not build on it.

This is not hypothetical: brief 002 deliberately excludes
`docs/architecture/deck-builder-legality.md` from its audit scope, because
that document *is* round 001's unreviewed deliverable. Auditing it would have
meant treating an unadjudicated result as established.

When a brief must depend on an unadjudicated round, do not queue it — wait for
adjudication, or rescope it so it does not.

## Naming

```
<NNN>-<YYYY-MM-DD>-<slug>.md
```

Zero-padded sequence number first, then the date, then a short slug. The
number is the primary sort key — dates alone stop disambiguating once two
rounds land on the same day. The number is assigned when the brief is written
and never changes as it moves between directories.

## Archiving honestly

Archive the brief **as Builder actually received it**. If Brain amended it
mid-round, append the amendment at the bottom rather than editing the original
text; a brief silently improved after the fact is useless as evidence about
how the round really went.

The outcome — what was accepted, what was corrected, what was rejected and why
— is appended when it moves to `archive/`. What was observed about the *model*
that ran it goes in [`../agents/model-notes.md`](../agents/model-notes.md)
instead.

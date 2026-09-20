# Git conventions and isolation

## The invariant

**Concurrently-active agents must never share a working directory.**

That is the whole requirement. The mechanism is a project and platform choice;
the isolation is not.

This exists because it has already gone wrong: an executor round ran directly in
the coordinating session's checkout, left it on the work branch, and a later
session did not notice before committing unrelated work on top. Nothing was
lost, but it should not have been possible.

One more thing depends on this invariant besides safety: [`reports.md`](reports.md)'s
completion-report mechanism derives a role's identity from *which checkout it is
in*, and relies on two concurrently-active roles having two different checkouts
to guarantee neither can silently overwrite the other's report.

## Layout

Prefer **one top-level project directory**, with agent checkouts nested beneath
it:

```
<project>/                     the primary checkout — Brain works here
<project>/.worktrees/<role>/   one isolated checkout per concurrently-active role
```

Nested rather than sibling directories, deliberately. Sibling paths depend on
where the clone happens to sit, so the layout differs between machines and every
document has to name absolute paths that are wrong somewhere. Nested, the paths
are repo-relative and **identical on every operating system**, and a fresh clone
on a new machine reaches the same layout with the same commands.

`.worktrees/` is ignored by git.

Role names used for linked-worktree directories are portable checkout names:
lowercase ASCII, starting with a letter, followed by lowercase letters,
digits, `-` or `_`, and not a Windows device name such as `con` or `prn`.
`tools/checkout.py` applies this rule as the first-action check, and the
completion-report writer applies the same rule before using a role in an inbox
path. An invalid name fails early with its current location and the naming
rule; it is not silently rewritten into another role.

Every role prompt starts with the shipped mechanical check:

```bash
python3 tools/checkout.py --seat <seat named in the prompt>
```

It passes for the primary coordinating checkout and for a linked worktree at
`.worktrees/<role>`, and fails with the current and expected locations when a
seat is claimed from the wrong checkout.

A separate clone has the same shape as the primary checkout, so it can only
ever be **the coordinating seat** — leave its `framework.checkout-seat` unset,
or explicitly set it to the coordinator's own name. It cannot host any other
role. [`reports.md`](reports.md)'s shared completion-report inbox is
`<git-common-dir>/agent-inbox/`, which is the *same directory only for
worktrees of one clone* — a genuinely separate `git clone` has its own private
git-common-dir, so a report written there is invisible to a delivery check run
from anywhere else. Assigning a non-coordinator seat to a separate clone would
therefore look accepted by the checkout check and then silently fail every
delivery check run from a different checkout — `tools/checkout.py` refuses
this outright: setting `framework.checkout-seat` to anything other than the
coordinator on a clone-shaped checkout is a hard error, not a mis-tag. Use a
linked worktree for every role except the coordinator.

Git worktrees share one object database and one remote, so a fetch or push from
any is visible to the others. They do **not** share a working directory or index:
one role cannot check out a branch another is sitting on, and one role switching
branches cannot disturb another's uncommitted state.

A Verifier gets its own checkout because reviewing means checking out an **exact
SHA**, detached, and building and testing there. Doing that in the executor's
checkout would destroy the very state being reviewed.

```bash
git worktree add --detach .worktrees/<role> <default-branch>
```

`--detach` deliberately: git refuses to check out a branch another worktree
already holds, so creating these *on* a branch only works while nobody else
happens to be there — exactly the sort of setup step that works once and then
fails confusingly six months later.

**Any mechanism providing equivalent isolation is acceptable** — a tool's own
per-session sandbox, containers, an unconventionally-named linked worktree.
What is not acceptable is two concurrent agents in one checkout, or a
provider-specific layout being treated as the rule rather than as one
implementation of it. A genuinely separate clone satisfies the *isolation*
invariant (no shared working directory) but not the *reporting* one — see
above — so it is acceptable only for the coordinating seat.

## Branch naming

```
<role>/<kebab-scope>
```

The prefix left of the slash identifies **which role owns pushes to that
branch** — never which provider ran it. A project may use a different scheme
(milestone prefixes, for example) as long as the namespace derives from roles or
project structure and never from a provider.

<!-- guard:counterexample -->
<!-- guard:violation branch-namespace roles=builder text="gemini/<task>" -->
Never: `claude/<task>`, `codex/<task>`, `gemini/<task>`, or any branch namespace
named after the tool that happened to run the round.
<!-- /guard:counterexample -->

Existing branches carrying an old provider-shaped name are history. Migrating a
framework does not require renaming them; preserve active work safely and apply
the convention to new branches.

One branch, one task, one concern.

## Working discipline

- **Re-check branch and status at the start of every discrete task**, not only at
  session start. The failure above happened mid-session.
- **Never push to the default branch.** Every change arrives through a pull
  request where the hosting supports it, or through Brain's merge where it does
  not.
- **Protect unrelated work.** Before anything destructive — resetting hard,
  force-pushing, discarding uncommitted changes — check whether another session
  has work in flight. Stash or branch; do not clobber.
- **Focused commits.** Not one giant commit at the end.

## Push gates, and what they are actually worth

Be precise about which layer is providing which guarantee.

| Layer | Strength | Reality |
|---|---|---|
| Server-side branch protection | **The guarantee.** Server-side, no local setup, survives any tool or machine. | Requires the hosting to support it, and requires administrator enforcement to be enabled — otherwise it stops nobody who has admin rights. |
| Git `pre-push` hook | Local convenience, early feedback. | Opt-in per clone. Bypassable. May simply not exist wherever a push comes from. |
| A tool's own pre-command hook | Weakest. | Fires only for that tool, and only if it correctly recognises the intent. |

**Put a client-side gate at git's `pre-push` layer, not at a tool's
command-inspection layer.** This is not a preference; both alternatives were
tried and failed in two independent projects for the same reasons:

1. **It missed real pushes.** A command-text matcher was defeated by
   `git -C <path> push`, by wrapping the push in a subshell or a
   command-substitution, by invoking a differently-named executable, and by
   several refspec spellings. Shell has unbounded ways to spell the same push.
2. **It blocked things that were not pushes** — including a commit whose message
   merely mentioned pushing.
3. **It only fires for one tool.** A round run through any other tool never
   triggered it at all, which makes it useless as a control in a
   provider-neutral framework.

At the `pre-push` layer there is no command text to interpret: git has already
decided a push is happening and hands the hook exactly what will be written. It
also fires for every client — any agentic tool, a plain terminal, an IDE.

Two wrinkles worth knowing before you trust a `pre-push` hook:

- `core.hooksPath` is **per-clone configuration and fails silently.** A fresh
  clone has no hook until it is set. There is no way for a repository to
  configure its own hooks on clone; that is a deliberate git security property,
  not an oversight to work around. Check it during rehydration.
- Git does not invoke `pre-push` at all when a push turns out to be a no-op,
  because there are no ref updates to hand it. Pushing an already-current branch
  therefore proves nothing — and that is exactly how a first attempt at
  verifying such a hook produced a false pass.

**Treat a green local hook as "I probably did not just waste a CI round", never
as "this is enforced."**

## The identity limit

When every agent authenticates with the same repository credentials, the server
**cannot distinguish one role from another**. It knows only that a change arrived
through a pull request with green checks.

So: the server enforces the **path**. That an executor never merges is enforced
by the role contract and by nothing else. Do not describe it as server-enforced,
and do not design as though it were.

Requiring a human approval count on pull requests would defeat the point of this
framework — it would reinstate the owner as the merge button. Prefer required
status checks plus administrator enforcement, and accept that role separation is
a contract property.

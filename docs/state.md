# Project state

Fast rehydration for a fresh Brain session. Keep this short — point at the
detailed doc rather than duplicating it. **Every fact here is a claim to
spot-check against live repository state, not a fact to relay forward.**

**Last updated:** 2026-08-31.

**Derive these before trusting anything below them.** This file drifted within
two rounds of being written — it claimed no Builder round had run while
describing one further down. Anything a command can answer, answer with the
command:

```bash
git rev-parse origin/master              # the real tip, not the anchor below
gh pr list --state open                  # what is actually in flight
ls docs/briefs docs/briefs/delivered docs/briefs/archive
git config --get core.hooksPath          # empty means this clone has no push guard
gh api repos/cntrl-alt-lenny/edopro-next/rules/branches/master   # [] means master is unprotected
```

`/status` runs all of these. `tests/test_docs_consistency.py` enforces the
structural half of it. Neither can tell you whether the prose below is still
true — that stays a judgement call at every session start.

## Repository

`cntrl-alt-lenny/edopro-next`, default branch `master`. Standalone repository
carrying full upstream history, forked from `edo9300/edopro` at
`54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` (2026-08-20) — see
[`UPSTREAM.md`](UPSTREAM.md).

```
origin    https://github.com/cntrl-alt-lenny/edopro-next.git
upstream  https://github.com/edo9300/edopro.git   (fetch)
upstream  DISABLED_use_origin                     (push — do not undo)
```

**Accepted-state anchor:** `origin/master` was
`b6315df14a2307e0c3cda17cd8782e7f2d5c517c` when this was written, after PR #13
merged. This is an anchor, not a current value — **always derive live HEAD
from git** (`git rev-parse origin/master`) rather than trusting this string.

Thirteen PRs merged, none open, at the time of writing. Every milestone so far
landed through a reviewed PR; several used explicit `DO NOT MERGE` review
gates. That practice is now the framework's rule — see `AGENTS.md`.

## Milestones (detail and honest status: [`ROADMAP.md`](ROADMAP.md))

| | Status | The part that matters |
|---|---|---|
| **M0** Foundation | done | Baseline build verified, ADR 0001, Qt/QML shell that compiles and runs |
| **M1** Make change provable | **in progress** | Level 1 (recorded-protocol regression) done. **Level 2 — re-simulation through `ocgcore` — not started**, and it is what the strong claim actually needs |
| **M2** Semantic client model | done | 34 of upstream's ~90 messages decoded; live observer + reviewed fixture-equivalence verifier |
| **M3** Deck and card data | **in progress** | `data/`, deck/`.ydk`, search, `policy/` and the real upstream `.ydk` interop proof are done; the deck-builder UI item is not |
| M4 / M5 / M6 | not started | M5 (duel field) is deliberately last |

## Architecture boundaries currently accepted

```
ocgcore/ + CardScripts + BabelCDB   authoritative, never modified by us
gframe/                             upstream's legacy client, C++17, touched minimally
integration/legacy/                 the C++17-compatible observer seam gframe may see
client/                             semantic duel model, C++20, no UI types
data/                               card database facade, .ydk codec, card search
policy/                             deck legality / LFList, presentation-independent
ui/                                 Qt 6 / QML presentation + a thin Qt adapter layer
```

The rule that resolves most arguments: **the rules engine must not become the
UI, and the UI must not implement game rules.** Decisions and their reasoning
are in [`adr/`](adr/); per-subsystem source research and every deliberate
upstream divergence is in [`architecture/`](architecture/).

## What is proven, versus what is merely intended

This distinction is the single most useful thing in this file.

**Proven, with a mechanism that can fail:**

- Our *reading* of the recorded duel protocol is pinned by golden traces, and
  the goldens must reproduce byte-for-byte from a clean tree.
- Message and protocol-constant tables are derived, not hand-edited
  (`--check` in CI).
- The semantic model's own invariants, including transactional decoding —
  a refused packet leaves `DuelState` byte-for-byte unchanged.
- Scoped structural equivalence between the semantic projection and real
  legacy state across every packet of both committed fixtures (990 and 1133,
  pinned in CI outside the verifier's own logic), with a deterministic
  fault-injection path proving the failure mode is live.
- `client/`, `data/` and `policy/` build with no Qt, no Irrlicht, no vcpkg and
  no `ocgcore` — CI would break if that separation broke.
- The push guard's own behaviour. `tests/test_push_guard.py` drives
  `.githooks/pre-push` through git's real stdin protocol, including every
  historical bypass by name, and CI fails if those tests skip. Mutation-tested:
  emptying the protected list fails 7 of 12, and replacing exact matching with
  substring matching — the bug class that broke the previous guard — fails 4.
  Note what this does *not* prove: that git invokes the hook at all. That needs
  `core.hooksPath` set per clone. And **GitHub branch protection on `master` is
  not enabled** (verified 2026-08-31 — `rules/branches/master` returns `[]`),
  so there is currently no server-side guarantee behind it at all.
- A `.ydk` written by our own `save_ydk()` loads correctly through the real,
  preserved `DeckManager::LoadDeckFromFile()`, for both of upstream's
  `separated` load modes, against a synthetic committed-safe fixture — with a
  fault-injection path proving the comparator is live
  ([`architecture/ydk-interoperability.md`](architecture/ydk-interoperability.md),
  ADR 0008).

**Not proven, and must not be claimed:**

- **That duel behaviour is unchanged.** No automated check in this repository
  can establish that. The replay harness never loads `ocgcore`, so **no C++
  change in this tree can fail it** — [`architecture/replay-regression.md`](architecture/replay-regression.md)
  §0. M1 Level 2 is what would close this, and it does not exist.
- Complete legacy-client or duel-engine equivalence. The fixture comparator
  covers life points, turn, structural card occupancy/location/sequence and
  material topology — not card code, not position.
- That a deck built in the new client opens in upstream EDOPro **end to end**.
  The format/loader level is now genuinely proven (above); upstream's own
  GUI and file-picker path is outside that harness by design, and the reverse
  direction — upstream's `SaveDeck` output read back by our parser — is not
  covered either.
- Semantic coverage beyond the 34 decoded message types.

## Intentional upstream deltas

Recorded, not silent. Each is argued in the linked doc — reopen only with a
concrete defect.

- **Card database** — load atomicity, and locale overlay semantics.
  [`architecture/card-database.md`](architecture/card-database.md), ADR 0003.
- **Deck model** — explicit sections rather than type-based auto-classification;
  card code 0 excluded rather than stored.
  [`architecture/deck-model.md`](architecture/deck-model.md), ADR 0004.
- **Card search** — deliberate exclusions from upstream's
  `CheckCardProperties`. [`architecture/card-search.md`](architecture/card-search.md), ADR 0005.
- **Deck legality** — the null-`LFList*` versus concrete `"N/A"` list
  distinction, the `CHECK_UNOFFICIAL` magnitude quirk, the `$whitelist` prefix
  match, and duplicate-code content/hash divergence; failing closed for the one
  count domain where upstream's own hash expression is undefined behaviour.
  [`architecture/deck-legality.md`](architecture/deck-legality.md), ADR 0007.

## Parked — do not reopen without new evidence

- **ADR 0001** — Qt 6 / QML, and no Rust between the UI and the engine.
- **M1 Level 2 scoping** — deliberately its own milestone rather than folded
  into M1. It needs a compiled `ocgcore`, a pinned card database and pinned
  CardScripts, none of which may be committed here.
- **Ordering: do not start with the duel field.** Highest-risk screen;
  [`architecture/current-edopro.md`](architecture/current-edopro.md) has the
  reasoning.
- **Do not delete Irrlicht code** before its replacement demonstrably reaches
  parity.
- The M2 semantic-model design (ADR 0002), including the central fix for
  transactional decoding and the test-only legacy-perspective reference
  implementation.

## In flight

**The agent framework itself** — PR #14, branch `meta/agentic-framework`:
`AGENTS.md`, `.claude/agents/`, this file, `docs/briefs/`, `docs/agents/`,
`.claude/hooks/`. Open, under Verifier review, not merged.

**PR #15**, branch `m3/deck-builder-legality-boundary` — the first Builder
round, executed against the brief still in [`briefs/active.md`](briefs/active.md):
`UPSTREAM ARCHAEOLOGY` on the deck-builder legality boundary. Delivered
`docs/architecture/deck-builder-legality.md`. Open, **not yet Verifier-reviewed**,
not merged. It branches from `meta/agentic-framework` because the framework is
not on `master` yet, so #14 merges first.

Round 1's headline finding, which reshapes the implementation round: upstream's
deck editor **never calls `CheckDeckContent`/`CheckDeckSize`** — those have a
single call site each, both in `GenericDuel::PlayerReady`. The editor runs three
weaker, independently-bypassable mechanisms of its own, and `SaveDeck` checks
nothing at all. So "surface legality in the deck builder" is not one thing:
reproducing upstream's editor and running `policy::validate_deck()` are
different products.

**Brain has independently re-derived the load-bearing claims** — the two call
sites in `GenericDuel::PlayerReady` (`generic_duel.cpp:373,380`) and their
absence from `deck_con.cpp`; `SaveDeck`'s lack of any check; `push_main`'s
unconditional type gate versus its `forced`-bypassable count caps;
`forceInput = ignoreDeckContents || Shift` (`deck_con.cpp:624`); and
`GetLFList` returning `nullptr` on a hash miss. All hold. Still pending:
Verifier's independent review of PR #15.

**Follow-up found by that round, not yet fixed:**
[`architecture/deck-builder-ui.md`](architecture/deck-builder-ui.md):35 says
there is "no upstream function that decides a card's section from its type at
push time either". `push_main`/`push_extra` do type-gate at push time
(`deck_con.cpp:1585-1588,1617-1621`). The claim is defensible read narrowly —
the caller's cascade does the routing, not a dedicated classifier — but it is
worded more broadly than the source supports. Builder correctly flagged it
rather than editing an out-of-scope file; it needs its own small brief.

## Local toolchain constraint — read before writing any brief

**The primary dev machine (Windows) has neither `cmake` nor Qt installed.**
Verified 2026-08-31. `ninja` and Python 3.12.10 are present.

That means `client/`, `data/`, `policy/` and `ui/` **cannot be built or tested
locally at all**, and no brief touching them can produce the evidence
`AGENTS.md`'s per-layer table requires. Only Python tooling
(`tools/`, `tests/`) and documentation work are locally verifiable today.

This is a real constraint on the framework, not a detail: `AGENTS.md` says CI
is the backstop rather than the primary evidence, and for four of the six
layers there is currently no primary evidence available. Until cmake and Qt
are installed, either scope briefs to what is locally verifiable, or accept
CI-at-an-exact-SHA as the evidence and **say so explicitly** rather than
letting the gap go unmentioned.

## Known open items

- **The remaining M3 item**: the deck-builder UI. A functional core exists
  (M3D1). Missing: **legality is not surfaced anywhere in the UI** — `policy/`
  exists and nothing in `ui/` calls it — plus automatic Main/Extra
  classification, artwork, the legacy sigil search grammar, structured filters
  beyond plain text, and full keyboard/controller parity.
- **M1 Level 2** — not started, and required before any "duel behaviour is
  unchanged" claim.

## Recommended next slice

**Surface deck legality in the deck-builder UI** — but read the blocker first.

It is the natural next M3 step and the one place where `policy/`'s existing
foundation becomes something a user can see. The risk sits in the Qt adapter
boundary (ADR 0006), nowhere near duel behaviour, so a bad round is cheap —
which makes it a good first exercise of this framework. It also carries a
sharp invariant for Verifier to attack: **the UI must render legality decided
by `policy/`, and never compute or second-guess it.**

**The known blocker, recorded in ADR 0008's context section:** a prior
planning round deferred this slice because `policy/`'s `ValidationPolicy` has
no default, and no session layer supplies one. So the real first question is
not a QML question at all — it is *where does the active LFList and validation
policy come from, and who owns that lifetime?* A brief that starts by wiring
QML will hit this immediately. Answering it may itself be the right slice.

Confirm scope with the owner before briefing it. Brain does not start a new
direction on its own initiative.

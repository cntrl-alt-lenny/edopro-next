# Project state

Fast rehydration for a fresh Brain session. Keep this short — point at the
detailed doc rather than duplicating it. **Every fact here is a claim to
spot-check against live repository state, not a fact to relay forward.**

**Last updated:** 2026-09-01.

**Derive these before trusting anything below them.** This file drifted within
two rounds of being written — it claimed no Builder round had run while
describing one further down. Anything a command can answer, answer with the
command:

```bash
git rev-parse origin/master              # the real tip, not the anchor below
gh pr list --state open                  # what is actually in flight
ls docs/briefs docs/briefs/delivered docs/briefs/archive
git config --get core.hooksPath          # empty means this clone has no push guard
gh api repos/cntrl-alt-lenny/edopro-next/branches/master --jq .protected   # NOT rules/branches
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
`f1eb9d66583d3204639fd1ab938605f00ce215df` when this was written, after PRs #19, #20 and #21 merged in that
order on 2026-09-01. This is an anchor, not a current value — **always derive live HEAD
from git** (`git rev-parse origin/master`) rather than trusting this string.

Every milestone so far landed through a reviewed PR; several used explicit
`DO NOT MERGE` review gates. That practice is now the framework's rule, and
since 2026-08-31 it is enforced server-side — see `AGENTS.md`. For what is
merged and open right now, run `gh pr list` (above); do not read a count from
this file.

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
  `core.hooksPath` set per clone.
- **Server-side protection on `master`** (enabled 2026-08-31): changes only via
  PR, five required checks, `enforce_admins: true`, `strict: true`, no
  force-push or deletion. Proven by an admin `--no-verify` push being rejected
  with `GH006` on a branch carrying the same shape. It enforces the *path*,
  not the *role* — every agent authenticates as the owner, so "Builder never
  merges" remains a contract, not something the server can know.
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
- **That our layers behave the same on every platform we support.** Six
  divergences have been found so far that Linux CI could not see, and the
  sixth is a *runtime* one rather than a compiler or build-system one:
  `load_lflist()` and `load_ydk()` both test `file.bad()` to detect a failed
  read, and on macOS a directory opens, reads as a clean EOF, and is reported
  as **success**. Confirmed directly: `load_ydk(directory)` returns
  `ok=1, error=""`, which contradicts `data/include/edopro_next/data/ydk.h:56`
  in as many words. `policy/` has a test for it and is red on macOS today;
  `data/` has no such test and is silently wrong. Brief 008.

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

Derive this from `gh pr list` and `ls docs/briefs*` before trusting it.

**PR #14 — merged 2026-08-31** (`c3592a84`). The framework itself: `AGENTS.md`,
vendor-neutral role contracts in [`roles/`](roles/), thin `.claude/` adapters,
this file, [`briefs/`](briefs/), [`agents/`](agents/), `.githooks/pre-push`,
and `tests/test_push_guard.py` + `tests/test_docs_consistency.py`. Verifier
reviewed the exact head, returned zero BLOCKERs, **seven SHOULD FIX and five
NOTE** (an earlier version of this line said "nine", which silently dropped
three NOTEs); Brain reproduced the load-bearing ones and merged under the
delegated authority described in `AGENTS.md`.

**PR #15 — merged 2026-08-31** (`5c5f371f`). Brief 001's deliverable,
`architecture/deck-builder-legality.md`. Verifier returned zero BLOCKERs, seven
SHOULD FIX, two NOTE; Brain re-derived the headline finding and the one
substantive defect below. Brief archived as `accepted`.

**Known-wrong, merged, not yet fixed:**
[`architecture/deck-builder-legality.md`](architecture/deck-builder-legality.md)
§2.4 claims `check_limit` is guarded by the Shift-inclusive `forceInput` at
`deck_con.cpp:641,719,756`. True at `:641` and `:756`; **false at `:719`**,
which tests `gGameConfig->ignoreDeckContents` directly — Shift is read two
lines later to choose the target section, not to bypass the check. Do not rely
on that sentence. Six further citation-precision defects are listed in the
archived brief. All fold into the re-queued citation audit.

**PR #17 — merged 2026-08-31** (`ccbf7860`). Brief 003, framework hardening.
Verifier returned **zero BLOCKERs, zero SHOULD FIX, two informational NOTEs** —
the first round here to come back with nothing actionable. Brain independently
reproduced the bare-repo push bypass in both directions, confirmed the new CI
check is not tautological, and ran two mutations Verifier had not. Brief
archived as `accepted`; what the round deliberately did **not** close is
recorded there.

**Round 3 merged 2026-09-01 — PRs #19, #20 and #21, in that order.**
`master` is `f1eb9d66`. All three had been open simultaneously as a stack, and
one defect in #19 was holding all of them.

- **PR #19** (`436f4265`) — brief 004's citation audit across seven M3
  architecture documents, plus brief 006's Correction A. Verifier reviewed
  head `7639bf2d`: zero BLOCKERs, one SHOULD FIX, two UNPROVEN CLAIMs. Both
  UNPROVEN CLAIMs were correct and are closed; the SHOULD FIX was a correct
  observation misattributed to Builder. See
  [`briefs/archive/004-…`](briefs/archive/004-2026-08-31-architecture-citation-audit.md).
- **PR #20** (`6d9cf640`) — Brain's own round-2 close-out. Its single commit
  `3a2fea97` fell **between** Verifier's two reviewed ranges and was never
  independently reviewed; it touches `docs/briefs/` and `docs/state.md` only,
  and the owner explicitly authorised merging the stack. Recorded because
  "Verifier reviewed this" must not be assumed of it later.
- **PR #21** (`f1eb9d66`) — briefs 005, 006's Correction B, and 007.
  Verifier's **first** review of that branch, at `3a2fea97..85a11055`: zero
  BLOCKERs, two UNPROVEN CLAIMs, both re-derived by Brain and resolved in
  [`briefs/archive/005-…`](briefs/archive/005-2026-08-31-windows-msvc-build.md).

**PR #22 was closed, not merged** — it queued brief 006, which had already
been delivered on the two branches it was meant to correct. Its text is
preserved as
[`briefs/archive/006-…`](briefs/archive/006-2026-08-31-merge-train-corrections.md).

**Two things this round established that outlive it:**

- **`strict: true` rots PR-body evidence.** Every branch update produces a new
  head, and every figure quoted in the body silently stops describing the
  range that will merge. `AGENTS.md` says a Verifier review does not survive a
  new head; it says nothing about the body's own numbers, and both of PR #19's
  UNPROVEN CLAIMs were exactly this. Worth a framework fix.
- **A brief issued only as a launch prompt is not a brief.** Brief 007 was
  never written to `active.md`, so Verifier reviewed it with no acceptance
  criteria to check against and said so. See
  [`briefs/archive/007-…`](briefs/archive/007-2026-09-01-apple-clang-build.md).

**Brief 008 is queued** in [`briefs/active.md`](briefs/active.md): the
read-failure predicate, and what mechanism should catch platform divergence.

## Local toolchain — state, and what still does not build

**There are two dev machines, and they do not have the same
capabilities.** This section has been rewritten twice for that reason. Say
which machine you are on in every completion report; `AGENTS.md`'s evidence
table asks for the platform and this is why.

**Updated 2026-09-01: the primary machine is now a Mac.** Neither entry below
was written on the strength of an install — each was written after the cycles
were actually run.

### Windows — installed 2026-08-31, with the owner's authorization

| | |
|---|---|
| MSVC | 19.44.35228 (VS 2022 Build Tools 17.14.39) |
| CMake | 3.31.6, portable, under `%LOCALAPPDATA%\Programs\` |
| Qt | 6.8.3 `msvc2022_64`, under `C:\Qt\` — the version CI pins |
| SQLite3 | vcpkg `x64-windows`; vcpkg lives outside the repository |
| Ninja, Python | 3.12.10, already present |

**What was actually established, by running it:** all four modules configure,
and **13 of 13 CTest suites pass** — `client/` 7, `data/` 3, `policy/` 2,
`ui/` 1 — on a compiler this code had never been compiled with. That is real
evidence about the code, independent of CI.

**What still does not build, and it is three specific things**, none of which
CI can see because CI is Linux/GCC only:

1. `client/tests/test_protocol_decoder.cpp` — `C4244` narrowing under MSVC
   `/W4 /WX`; `uint32_t` protocol constants passed into `uint8_t` tuple slots.
   GCC's `-Wall -Wextra` is silent on it.
2. `data/CMakeLists.txt` — `bench_card_search` forces `/O2`, which MSVC
   refuses to combine with a Debug build's `/RTC1` (`D8016`). A hard error,
   not a warning.
3. `ui/tests/CMakeLists.txt` — the generated QML cache path contains a literal
   `..` segment that Windows cannot `mkdir`, killing
   `test_deckbuilder_screen`. `edopro_next_shell` itself builds and links.

**All three are now closed.** Brief 005 fixed them and merged
2026-09-01 in `f1eb9d66` (PR #21); brief 006's Correction B replaced the third
fix's mirror with `NO_CACHEGEN`, removing the mechanism rather than repairing
it. The list is kept because it is the clearest record of what "green on Linux
CI" does not cover.

### macOS — the primary machine as of 2026-09-01

Apple clang 21 (`Apple clang 21.0.0`), CMake 4.4.3, Ninja 1.13.2, Python
3.13 at `/opt/homebrew/bin/python3.13`. **Qt is not installed**, and the
system Python is 3.9.

| Layer | On this machine |
|---|---|
| `client/` | configures, builds, **7/7 `ctest`** |
| `data/` | configures, builds, **3/3 `ctest`** |
| `policy/` | configures and builds, **`ctest` FAILS** — see below |
| `ui/` | **cannot be configured at all**; no Qt |
| `tools/`, `tests/` | 69/69 pass; both generator `--check`s clean |

Two traps specific to this machine, both of which cost real time:

1. **`policy/` `ctest` fails on `master` right now**, and it is the code that
   is wrong, not the environment — `loadLflistDirectoryPathFailsCleanly`.
   Brief 008 is this. Do not "fix" it by re-running.
2. **A stale `client/build/edopro_next_semantic_trace` is silently preferred**
   by `tests/test_semantic_trace.py`'s `find_binary()`, which searches fixed
   paths with no freshness check. A binary four days old made four Python
   tests fail against a clean tree. Delete the directory or set
   `EDOPRO_NEXT_SEMANTIC_TRACE`. The dangerous direction is not the failure —
   it is a C++ edit that never gets compiled and reports green.

Two operational facts, true and previously written down nowhere: on Windows
`cmake -G Ninja` finds no compiler outside the MSVC environment
(`vcvars64.bat`), and Qt-linked tests do not launch unless Qt's `bin` is on
`PATH` (CTest reports `BAD_COMMAND`, which reads as a build failure and is
not one). `AGENTS.md`'s evidence-table commands do not mention either.

*(The predecessor of this section was once deleted by accident during an
unrelated edit and restored after review caught it. It is durable state — do
not fold it into a narrative section.)*

## Known open items

- **The remaining M3 item**: the deck-builder UI. A functional core exists
  (M3D1). Missing: **legality is not surfaced anywhere in the UI** — `policy/`
  exists and nothing in `ui/` calls it — plus automatic Main/Extra
  classification, artwork, the legacy sigil search grammar, structured filters
  beyond plain text, and full keyboard/controller parity.
- **M1 Level 2** — not started, and required before any "duel behaviour is
  unchanged" claim.

## Recommended next slice

**Brief 008 — the read-failure predicate**, queued in
[`briefs/active.md`](briefs/active.md). It is first for a blunt reason:
`master` is red on macOS today, and the same defect is silently live in
`data/` where no test looks for it. It is also entirely inside `policy/` and
`data/`, both of which build and test on the current machine, so it can
produce its own required evidence here.

It carries a second question deliberately left open rather than pre-answered:
**what mechanism should catch platform divergence**, given six instances and
at least three different classes among them. A macOS CI leg is a candidate to
be argued, not the assumed answer — the classes have different costs and only
one of them needs tests to be *run* rather than merely compiled.

**After that, the deck-builder legality UI** — the remaining M3 item, and the
one the roadmap actually cares about. Two things gate it, and both are now
tractable:

- It touches `ui/`, which **cannot be built on the current machine**; Qt is
  not installed. Either install Qt here or run that round on the Windows
  machine, and say which in the brief. Do not queue it without resolving this
  — an earlier version of this file recommended a `ui/` round while recording
  two sections above that `ui/` could not be built, and the contradiction
  survived several readings.
- Its design blocker is **resolved**: brief 001's research was adjudicated
  `accepted` on 2026-08-31, including the headline finding that upstream's
  deck editor never calls `CheckDeckContent`/`CheckDeckSize`. That reshapes
  the round — "surface legality in the deck builder" is not one thing — and
  the corrections outstanding against it are listed in
  [`briefs/archive/001-…`](briefs/archive/001-2026-08-31-deck-builder-legality-boundary.md).

**Still not the duel field.** Unchanged and not near.

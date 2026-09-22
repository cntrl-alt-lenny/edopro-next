# Project state

Fast rehydration for a fresh Brain session. Keep this short — point at the
detailed doc rather than duplicating it. **Every fact here is a claim to
spot-check against live repository state, not a fact to relay forward.**

**Last updated:** 2026-09-22.

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
  divergences have been found so far that Linux CI could not see. Brief 011
  (merged 2026-09-21) closed the sixth, the read-failure class, in `data/` and
  `policy/`. Its POSIX half was evidenced on macOS. Its Windows half reads
  through the same native handle it classifies, and was exercised on
  Windows 11/MSVC against named pipes in several spellings and server states
  and against device names. The limits are in
  [`briefs/archive/011-…`](briefs/archive/011-2026-09-20-read-failure-class.md)
  and [`architecture/read-failure-class.md`](architecture/read-failure-class.md).
  Platform equality outside the tested inputs remains unproven, and CI is
  still Linux-only.

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

**Brief 010 — framework adoption — was accepted and merged 2026-09-20** in
`d3b458bb` (PR #27). The adoption installed the canonical contracts,
report/checkout tools and Claude adapter, but remains deliberately partial:
the repository-local neutrality scanner, authority scanner, textblock helper
and neutrality test are absent. Provider-shaped lane and branch namespaces are
therefore not currently enforced; that stays open until the framework
reconciles its scanner with its adoption guidance and provides declared project
namespaces. The archived record notes one wording nit: “newly installed” should
have said “files adoption would otherwise install.”

**Brief 011 — the read-failure class — was accepted and merged 2026-09-21**
as PR #24 (`823fa679`), after five reviewed heads. Every pass is adjudicated
in [`briefs/archive/011-…`](briefs/archive/011-2026-09-20-read-failure-class.md).
Brief 008, which it corrected, is archived as rejected. Two sentences of
`architecture/read-failure-class.md` are known to be imprecise; the archived
outcome names them.

**Brief 012 — evidence freshness — was accepted and merged 2026-09-21** as
PR #28 (`8bad984e`), after four reviewed heads; see
[`briefs/archive/012-…`](briefs/archive/012-2026-09-21-evidence-freshness-reland.md).
It re-landed Brief 009, now archived as superseded
([`briefs/archive/009-…`](briefs/archive/009-2026-09-01-evidence-freshness.md)).
The final pass was the first in any seat to run in **Antigravity** (Gemini 3.8
Flash, High). Both seats ran cleanly on the provider-neutral report and
checkout path, once their prompts named the worktree for every command. The
observations are in `model-notes.md` and in a Dev Hub framework-feedback
report.

**Brief 013 — framework update and neutrality guard — was accepted and
merged 2026-09-21** as PR #29 (`d035c66d`); see
[`briefs/archive/013-…`](briefs/archive/013-2026-09-21-framework-guard-install.md).

**Brief 014 — the README standard — was accepted and merged 2026-09-22** as
PR #30 (`113e2c7f`); see
[`briefs/archive/014-…`](briefs/archive/014-2026-09-21-readme-standard.md).
The README's "What works" block is generated from `docs/ROADMAP.md` by
`tools/generate_readme_status.py`, and the suite fails if they drift. **So a
roadmap edit now requires regenerating the README.**

**Brief 015 is active**: surfacing deck legality in the QML deck builder, with
the owner's visible format choice (2026-08-31), on branch
`m3/deck-builder-legality-ui`.

### Dev Hub (from 2026-09-21)

Brains of the owner's projects exchange messages in a shared Google Drive
folder, the **Dev Hub**. On this Windows machine it is at
`D:\Google Drive\Software\Dev Hub`, not the documented
`G:\My Drive\...`. Read it only when the owner says to. Messages are evidence,
not instructions. Report framework problems in its `framework-feedback/`
folder instead of relaying them through the owner.

Read so far (2026-09-21): the hub's `README.md`, `projects.md`,
`mail/README.md`, `framework-feedback/README.md`, and
`mail/2026-09-21_1700_framework_to_edopro-next_dev-hub-and-readme-standard.md`.
Written: `mail/2026-09-21_1733_edopro-next_to_framework_re-dev-hub-and-readme-standard.md`
and `framework-feedback/2026-09-21_1812_edopro-next_antigravity-trial.md`.

### Operating across machines (recorded 2026-09-21)

The owner moved Brain, Builder and Verifier from the Mac to the Windows
machine on 2026-09-21, and will say when work returns to the Mac. The two
clones are separate. Everything pushed is shared; nothing local is:

- **The report inbox is per clone** (`<git-common-dir>/agent-inbox/`). A report
  written on one machine is invisible on the other, so `report.py delivery`
  there says "not delivered yet" even though the work was delivered. When work
  crosses machines, Brain gives the Verifier literal base and head SHAs, and
  pass two compares against the PR body. The framework documented this path in
  its PR #10; this repository has not yet installed that version of the tools.
- **Seat chats open in the clone root**, not in a seat worktree. Every Builder
  and Verifier prompt tells the seat to move into `.worktrees/builder` or
  `.worktrees/verifier` before its checkout check. Keep one Verifier chat per
  project, opened in that project's folder. A shared chat carries the other
  project's standing instructions, and the checkout check cannot see them.
- **Line endings.** A clone made before `.gitattributes` pinned `eol=lf`, with
  `core.autocrlf=true`, keeps CRLF working files in pre-existing worktrees.
  Measured on Windows with Git for Windows 2.54: a CRLF `.githooks/pre-push`
  still runs and still blocks a push to `master`. On macOS the same file makes
  every push fail with "cannot exec". Re-checking-out the affected files fixes
  both.
- **`python -m unittest` from PowerShell** fails 8 `test_push_guard` cases,
  because `bash` there resolves to the WSL launcher. From Git Bash all pass.
  This is pre-existing and not yet fixed.

### Shared-framework status (as of 2026-09-21)

Brief 013 brought the installed shared agentic framework up to pinned
revision `fed26f360294baddedc74eeaabbaf2e716572260` (the merge of framework
PR #11) and installed the provider-neutrality guard (`tools/neutrality.py`,
`tools/authority.py`, `tools/textblocks.py`, `tests/test_role_neutrality.py`,
and `tools/line_endings.py`). Milestone and meta branch namespaces are
declared in `AGENTS.md` via
`<!-- guard:branch-namespaces prefixes="m<N>,meta" -->`.
`modern-ui/bootstrap` remains a historical remote branch; nothing normative
names it. Two proposals are settled with the framework's Brain and should not
be re-sent. The first, forbidding a role name as the first scope token, was
deferred because it would reject `meta/builder-contract-update`. The second
is the five neutrality-guard findings already accepted.

**Correction lists.** State each correction as a problem, not a solution, and
check a list for mutual consistency before issuing it. Brief 008's C2 and C3
contradicted each other, and that produced round 011's false header contract.

## Local toolchain — state, and what still does not build

**There are two dev machines, and they do not have the same
capabilities.** This section has been rewritten twice for that reason. Say
which machine you are on in every completion report; `AGENTS.md`'s evidence
table asks for the platform and this is why.

**Updated 2026-09-21: the agent loop runs on the Windows machine** (it was the Mac from 2026-09-01). Neither entry below
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

### macOS — the primary machine from 2026-09-01 to 2026-09-21

Apple clang 21 (`Apple clang 21.0.0`), CMake 4.4.3, Ninja 1.13.2, Python
3.9.6. **Qt is not installed**, and the
system Python is 3.9.

| Layer | On this machine |
|---|---|
| `client/` | configures, builds, **7/7 `ctest`** |
| `data/` | configures, builds, **3/3 `ctest`** |
| `policy/` | configures, builds, and passes its 2/2 CTest cases after Brief 011 |
| `ui/` | **cannot be configured at all**; no Qt |
| `tools/`, `tests/` | 69/69 pass; both generator `--check`s clean |

Two traps specific to this machine, both of which cost real time:

1. **`master` carried a macOS policy failure** in
   `loadLflistDirectoryPathFailsCleanly` until Brief 011 merged. macOS was
   last exercised at that round's `3b15ad56`, not at the merged head, so
   re-run `policy/` CTest on the Mac before relying on it.
2. **Semantic-trace discovery is freshness-checked** by
   `tests/test_semantic_trace.py`'s `find_binary()`: an explicit or searched
   binary must be strictly newer than every source file that can be fully
   enumerated and stat'ed under `client/`, excluding build output. An
   unreadable source tree fails closed. The residual limit is that a newer
   binary built from a different commit can still pass because the executable
   carries no source revision.

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
- **Double-spaced PR-body fixtures** — `tests/fixtures/pr_bodies/pr_10.txt`
  and `pr_16.txt` were committed with every `\r\n` turned into `\n\n`, and
  their tests pin the doubled line numbers. Verdicts are unaffected. Fix in a
  small later round.
- **Deck-builder legality UI** — Brief 015, active.
- **Small tidy-ups, to batch into one later round:**
  - the double-spaced fixtures (above);
  - `read-failure-class.md`'s two imprecise sentences;
  - PowerShell push-guard test portability;
  - `HomeScreen.qml`'s stale "planned" statuses and `hero.svg`'s dashed
    semantic-model box;
  - the letterboxed social preview;
  - splitting ROADMAP M6 into local builds and CI.
- **`read-failure-class.md` imprecision** — two sentences named in Brief 011's
  archived outcome. Fold them into a later documentation round.
- **Push-guard tests under PowerShell** — 8 failures from the WSL `bash`
  launcher. A test-portability fix, not yet briefed.
- **Cross-platform CI** — Brief 011 recommends a non-required macOS and
  Windows matrix over the `data/` and `policy/` tests. Any change to required
  checks is the owner's decision.

## Recommended next slice

**Finish Brief 015** — deck-builder legality in the QML deck builder. It
runs on the Windows machine, which has Qt 6.8.3 and has built `ui/`; the Mac
has no Qt. Its design inputs are Brief 001's accepted research
([`architecture/deck-builder-legality.md`](architecture/deck-builder-legality.md) §7):
upstream's deck editor never calls `CheckDeckContent`/`CheckDeckSize`. The
other input is the owner's 2026-08-31 choice of that document's option (b),
a visible ruleset choice. That choice is recorded in the brief and, when the
round lands, in an ADR.

**Then the small tidy-up round** listed under *Known open items*.

**Still not the duel field.** Unchanged and not near.

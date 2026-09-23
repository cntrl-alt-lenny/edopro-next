# State history (pre-3.0.0)

Frozen history moved out of `docs/state.md` when this project adopted
agentic-framework 3.0.0 (round 018-framework-3-0-0), which retired the
`docs/briefs/active.md` / worktree / shared-inbox workflow described below
in favour of `tools/fw.py` and `docs/rounds/`. Nothing here is live state:
every fact is a claim as of the date it was written, not a fact to relay
forward. Round-by-round detail for each brief named below is in
[`briefs/archive/`](briefs/archive/). Current and future rounds are in
[`rounds/`](rounds/).

## Proven, with a mechanism that can fail (as of 2026-09-22)

- Our *reading* of the recorded duel protocol is pinned by golden traces,
  reproduced byte-for-byte from a clean tree.
- Message and protocol-constant tables are derived, not hand-edited
  (`--check` in CI).
- The semantic model's own invariants, including transactional decoding — a
  refused packet leaves `DuelState` byte-for-byte unchanged.
- Scoped structural equivalence between the semantic projection and real
  legacy state across every packet of both committed fixtures (990 and
  1133, pinned in CI outside the verifier's own logic), with a deterministic
  fault-injection path proving the failure mode is live.
- `client/`, `data/` and `policy/` build with no Qt, no Irrlicht, no vcpkg
  and no `ocgcore` — CI would break if that separation broke.
- The push guard's own behaviour. `tests/test_push_guard.py` drives
  `.githooks/pre-push` through git's real stdin protocol, including every
  historical bypass by name, and CI fails if those tests skip.
  Mutation-tested: emptying the protected list fails 7 of 12, and replacing
  exact matching with substring matching fails 4. This does not prove git
  invokes the hook at all — that needs `core.hooksPath` set per clone.
- Server-side protection on `master` (enabled 2026-08-31): proven by an
  admin `--no-verify` push being rejected with `GH006` on a branch carrying
  the same shape. It enforces the path, not the role.
- A `.ydk` written by our own `save_ydk()` loads correctly through the real,
  preserved `DeckManager::LoadDeckFromFile()`, for both of upstream's
  `separated` load modes, against a synthetic committed-safe fixture, with a
  fault-injection path proving the comparator is live
  ([`architecture/ydk-interoperability.md`](architecture/ydk-interoperability.md),
  ADR 0008).

## Brief-by-brief round history

**PR #14 — merged 2026-08-31** (`c3592a84`). The framework itself:
`AGENTS.md`, vendor-neutral role contracts, thin `.claude/` adapters,
`docs/state.md`, `docs/briefs/`, `docs/agents/`, `.githooks/pre-push`,
`tests/test_push_guard.py` and `tests/test_docs_consistency.py`. Verifier
reviewed the exact head, zero BLOCKERs, seven SHOULD FIX, five NOTE; Brain
merged.

**PR #15 — merged 2026-08-31** (`5c5f371f`). Brief 001's deliverable,
`architecture/deck-builder-legality.md`. Zero BLOCKERs, seven SHOULD FIX, two
NOTE.

**Known-wrong, merged, fixed by brief 016:**
`architecture/deck-builder-legality.md` §2.4 claimed `check_limit` is
guarded by Shift-inclusive `forceInput` at `deck_con.cpp:641,719,756` — true
at `:641` and `:756`, false at `:719` (tests `gGameConfig->ignoreDeckContents`
directly).

**PR #17 — merged 2026-08-31** (`ccbf7860`). Brief 003, framework hardening.
Zero BLOCKERs, zero SHOULD FIX, two informational NOTEs.

**Round 3, merged 2026-09-01 — PRs #19, #20, #21**, `master` at `f1eb9d66`.
PR #19 (`436f4265`): brief 004's citation audit plus brief 006 Correction A;
one SHOULD FIX, two UNPROVEN CLAIMs, both correct and closed. PR #20
(`6d9cf640`): Brain's round-2 close-out, commit `3a2fea97` fell between
Verifier's two reviewed ranges and was never independently reviewed
(state-only diff; owner explicitly authorised). PR #21 (`f1eb9d66`): briefs
005, 006 Correction B, 007; zero BLOCKERs, two UNPROVEN CLAIMs, both
resolved. PR #22 was closed, not merged (brief 006 had already landed on the
branches it corrected).

Two lessons from that round: `strict: true` rots PR-body evidence, since
every branch update silently stops the quoted figures describing the merging
range; and a brief issued only as a launch prompt, never written to
`active.md`, is not a brief (brief 007).

**Brief 010 — framework adoption — merged 2026-09-20** (`d3b458bb`, PR #27),
after three review passes. Deliberately partial: the repository-local
neutrality/authority scanners were absent until brief 013.

**Brief 011 — the read-failure class — merged 2026-09-21** (PR #24,
`823fa679`), after five reviewed heads across a mid-round move from macOS to
Windows. The Windows passes converged only once the correction named the
underlying constraint (the object classified must be the object read)
instead of the observed cases — the "fix the class" lesson in `AGENTS.md`
applied to how corrections themselves were written. Brief 008, which it
corrected, is archived as rejected.

**Brief 012 — evidence freshness re-land — merged 2026-09-21** (PR #28,
`8bad984e`), after four reviewed heads, re-landing brief 009 (archived as
superseded). First round with a seat in Antigravity (Gemini 3.8 Flash,
High); both seats ran cleanly on the provider-neutral report/checkout path
once their prompts named the worktree for every command.

**Brief 013 — framework update to `fed26f36` and the neutrality guard —
merged 2026-09-21** (PR #29, `d035c66d`). One pass, both seats in
Antigravity.

**Brief 014 — the README standard — merged 2026-09-22** (PR #30,
`113e2c7f`). Two passes. The README's "What works" block is generated from
`docs/ROADMAP.md` by `tools/generate_readme_status.py`; a roadmap edit
requires regenerating it.

**Brief 015 — deck-builder legality UI — merged 2026-09-22** (PR #31,
`45bb2f33`), ADR 0010. Two passes: the first (both seats in Antigravity) let
a fabricated upstream citation through review; the second (both seats in
Claude Code) re-derived every citation with verbatim quotes and found a real
extra fact (upstream's default banlist is a concrete "N/A", not null).

**Brief 016 — records and test gaps — merged 2026-09-22** (PR #32,
`b130d5a7`, head `81de44a2`), one pass, both seats in Antigravity. All six
items closed with no production-code behaviour change (empty diff over
`client/src data/src policy/src ui/src ui/qml`): corrected two imprecise
sentences in `read-failure-class.md`; made all 28 PR-body fixtures
byte-faithful to their live bodies; fixed the ADR 0010 `ImportDeck` citation
and re-derived Decisions 2-3's upstream quotes; pinned the Extra-Deck-in-Main
disclosure regression test; confirmed the push-guard tests pass from both
Git Bash and Windows PowerShell (13/13); documented `ocgcore` submodule
initialisation for seat worktrees. Full outcome, including a Builder report
defect (narrated a test and mutation against code that did not exist) that
Verifier and Brain caught, is in
[`briefs/archive/016-2026-09-22-records-and-test-gaps.md`](briefs/archive/016-2026-09-22-records-and-test-gaps.md).

**Durable lesson: correction lists.** State each correction as a problem,
not a solution, and check a list for mutual consistency before issuing it —
brief 008's C2 and C3 once contradicted each other and produced a false
header contract.

## Local toolchain, as last exercised (pre-3.0.0)

Two machines were used across this project's life, and they do not have the
same capabilities; say which one was used in any report relying on this.

**Windows** (installed 2026-08-31): MSVC 19.44.35228 (VS 2022 Build Tools
17.14.39), CMake 3.31.6, Qt 6.8.3 `msvc2022_64`, SQLite3 via vcpkg
`x64-windows`, Ninja, Python 3.12.10. All four modules configured and
**13/13 CTest suites passed** (`client/` 7, `data/` 3, `policy/` 2, `ui/`
1) — the first time this code had compiled under MSVC. Three MSVC-only
defects (narrowing in `test_protocol_decoder.cpp`, an `/O2`+`/RTC1` conflict
in `bench_card_search`, a literal `..` QML cache path) were fixed by brief
005 and brief 006 Correction B (`f1eb9d66`, 2026-09-01); CI is Linux-only and
cannot see any of the three. See
[`agents/local/windows-notes.md`](agents/local/windows-notes.md) for the
environment-setup detail that remains current.

**macOS** (primary machine 2026-09-01 to 2026-09-21): Apple clang 21, CMake
4.4.3, Ninja 1.13.2, Python 3.9.6, no Qt installed. `client/` 7/7 `ctest`,
`data/` 3/3, `policy/` 2/2 (after brief 011), `ui/` cannot configure (no
Qt), `tools/`/`tests/` 69/69 plus clean generator `--check`s.
`tests/test_semantic_trace.py`'s `find_binary()` fails closed: an explicit
or discovered semantic-trace binary is accepted only when strictly newer
than every readable file under `client/` (excluding build output); mtime
cannot prove the binary came from the exact reviewed commit, so that residual
limitation stays explicit.

## Operating across machines (recorded 2026-09-21, now superseded)

The owner moved the agent loop from the Mac to a Windows machine on
2026-09-21. Under the pre-3.0.0 workflow, the completion-report inbox was
per-clone (`<git-common-dir>/agent-inbox/`), so a report written on one
machine was invisible on another and Brain had to hand the Verifier literal
SHAs directly; framework 3.0.0's `tools/fw.py report` commits reports to git
instead, which removes this whole class of problem. A CRLF working-tree file
in a clone made before `.gitattributes` pinned `eol=lf` was observed to make
`.githooks/pre-push` fail with "cannot exec" on macOS while still running
correctly (and still blocking a protected push) on Windows 11/Git 2.54 —
re-checking out the affected files fixed both. Running
`tests/test_push_guard.py` from native Windows PowerShell once failed 8
cases because `bash` resolved to the WSL launcher, which cannot open a
Windows path; brief 016 fixed this by finding Git for Windows' bundled `sh`
first, confirmed 13/13 from PowerShell.

## Dev Hub (historical, 2026-09-21 to 2026-09-22)

Under the pre-3.0.0 workflow, this project's Brain exchanged messages with
the framework's own Brain in a shared Google Drive folder ("Dev Hub"),
reading its `README.md`, `projects.md`, `mail/README.md` and
`framework-feedback/README.md`, and writing two messages there. As of round
018 this project reports framework problems through the framework
repository's "Framework feedback" GitHub issue form instead (see
`docs/agents/FRAMEWORK.md`). The Dev Hub is outside this project's own
tracked history; nothing here describes its current state.

## Shared-framework status, pre-3.0.0 (superseded by round 018)

Brief 013 brought the installed framework to pinned revision
`fed26f360294baddedc74eeaabbaf2e716572260` and installed
`tools/neutrality.py`, `tools/authority.py`, `tools/textblocks.py`,
`tests/test_role_neutrality.py` and `tools/line_endings.py`, with milestone
and meta branch namespaces declared via a `guard:branch-namespaces` comment
in `AGENTS.md`. Round 018 (this framework-3.0.0 adoption) retired all of
this in favour of `tools/fw.py` and the `<role>/<round-id>` branch scheme
`fw.py start` creates automatically.

# State

The owner's standing decisions, what is deliberately parked, and pointers.
Read it in a minute. It holds **no live state**: what is in flight, merged
or failing comes from `python3 tools/fw.py status` and git, every session.
Round-by-round history through round 016 (the last round before this
project ran on the 3.0.0 framework) is in
[`docs/state-history.md`](state-history.md); every fact there, like every
fact here, is a claim to spot-check against live state, not to relay
forward.

## Where we are going

edopro-next replaces EDOPro's Irrlicht client with a Qt 6/QML one over the
unmodified duel engine — see [`ROADMAP.md`](ROADMAP.md) for the full
milestone table and honest status. Current state: **M0** (foundation) and
**M2** (semantic client model) done; **M1** (make change provable) has its
Level 1 (recorded-protocol regression) done and Level 2 (re-simulation
through `ocgcore`) not started — that is what "duel behaviour is unchanged"
actually needs; **M3** (deck/card data) has `data/`, `.ydk`, search,
`policy/` and advisory deck-builder legality done, with automatic
Main/Extra classification, the legacy sigil search grammar, structured
filters and full keyboard/controller parity still open; **M4** and **M5**
not started, M5 (duel field) deliberately last; **M6** (platform and input)
has local Windows and macOS builds evidenced, with Windows and macOS CI,
controller navigation, Steam Deck and accessibility still open.

## Owner decisions

- **Qt 6/QML, no Rust between the UI and the engine** (ADR 0001).
- **Do not start the migration with the duel field.** Highest-risk screen —
  see `docs/architecture/current-edopro.md`.
- **Do not delete Irrlicht code** before its replacement demonstrably
  reaches parity.
- **This project runs agentic-framework 3.0.0** as of round
  018-framework-3-0-0. Merge rule: owner-approves (declared in
  `AGENTS.md`).

## Not proven, and must not be claimed

- **That duel behaviour is unchanged.** No automated check here can
  establish that — the replay harness never loads `ocgcore`, so no C++
  change in this tree can fail it
  ([`architecture/replay-regression.md`](architecture/replay-regression.md)
  §0). M1 Level 2 would close this and does not exist yet.
- **Complete legacy-client or duel-engine equivalence.** The fixture
  comparator covers life points, turn, structural card
  occupancy/location/sequence and material topology — not card code, not
  position.
- **That a deck built in the new client opens in upstream EDOPro end to
  end.** The format/loader level is proven; upstream's own GUI/file-picker
  path and the reverse direction (upstream `SaveDeck` read back by our
  parser) are not covered.
- **Semantic coverage beyond the 34 decoded message types.**
- **That our layers behave the same on every supported platform.** Six
  platform-only divergences have been found so far; CI is Linux-only. See
  [`architecture/read-failure-class.md`](architecture/read-failure-class.md).

## Intentional upstream deltas

Recorded, not silent, each argued in its doc — reopen only with a concrete
defect: card database load/locale semantics
([`architecture/card-database.md`](architecture/card-database.md), ADR
0003); deck model's explicit sections and card-code-0 exclusion
([`architecture/deck-model.md`](architecture/deck-model.md), ADR 0004);
card search exclusions
([`architecture/card-search.md`](architecture/card-search.md), ADR 0005);
deck legality's null-vs-"N/A" `LFList`, `CHECK_UNOFFICIAL`, `$whitelist` and
duplicate-code divergences
([`architecture/deck-legality.md`](architecture/deck-legality.md), ADR
0007).

## Parked — do not reopen without new evidence

- **M1 Level 2 scoping** — its own milestone, needing a compiled `ocgcore`,
  a pinned card database and pinned CardScripts, none of which may be
  committed here.
- **Cross-platform CI** — a non-required macOS/Windows matrix over
  `data/`/`policy/` tests has been recommended but not decided; any change
  to required checks is the owner's decision.

## Pointers

- Fork point and other fixed facts: **Historical anchors**, below.
- Round-by-round history (pre-3.0.0): [`state-history.md`](state-history.md).
  Current and future rounds: [`docs/rounds/`](rounds/).
- Windows/MSVC build detail: [`agents/local/windows-notes.md`](agents/local/windows-notes.md).
- **Recommended next slice**: M3's remaining deck-builder parts (automatic
  Main/Extra classification, structured filters, keyboard parity). Still
  not the duel field.

## Historical anchors

- This repository is a standalone fork of `edo9300/edopro` at
  `54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` (2026-08-20) — see
  [`UPSTREAM.md`](UPSTREAM.md).
- GitHub branch protection on `master` (PR required, `enforce_admins`,
  `strict`, five required checks) was enabled 2026-08-31 — re-verify live
  per `AGENTS.md`, do not trust this date going forward.

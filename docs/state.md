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
`policy/`, advisory deck-builder legality and Main/Extra placement by
upstream's rule and structured filter controls done, with artwork, the legacy
sigil search grammar, descriptive effect labels and full keyboard/controller
parity still open; **M4** and **M5**
not started, M5 (duel field) deliberately last; **M6** (platform and input)
has local Windows and macOS builds evidenced, with Windows and macOS CI,
controller navigation, Steam Deck and accessibility still open.

## Owner decisions

- **Qt 6/QML, no Rust between the UI and the engine** (ADR 0001).
- **Do not start the migration with the duel field.** Highest-risk screen —
  see `docs/architecture/current-edopro.md`.
- **Do not delete Irrlicht code** before its replacement demonstrably
  reaches parity.
- **Deck-builder Tab navigation should reach every enabled control inside
  the application.** Use supported public Qt APIs compatible with the
  supported Qt floor; leave macOS keyboard settings untouched. The owner
  selected this approach on 2026-10-08.
- **Use the framework's batch workflow.** The current release pin is in
  `agents/framework.json`. Merge rule: owner-approves (declared in `AGENTS.md`).

## Scorecard

Every two weeks, Brain adds one line: dates, product progress, prompts the
owner relayed, and batches that only fixed an earlier batch. First line due
2026-10-20.

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
0007); the deck builder placing an added card by `LoadDeck`'s rule rather
than upstream's push cascade, and never reclassifying an opened `.ydk`
([`architecture/deck-placement.md`](architecture/deck-placement.md), ADR
0011).

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
  3.x rounds: [`docs/rounds/`](rounds/); batches since 4.0: [`docs/batches/`](batches/).
- Windows/MSVC build detail: [`agents/local/windows-notes.md`](agents/local/windows-notes.md).
- **Recommended next slice**: M3's native deck-builder keyboard navigation,
  followed by the legacy sigil search grammar, descriptive effect labels and artwork.
  Still not the duel field.

## Historical anchors

- This repository is a standalone fork of `edo9300/edopro` at
  `54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` (2026-08-20) — see
  [`UPSTREAM.md`](UPSTREAM.md).
- GitHub branch protection on `master` (PR required, `enforce_admins`,
  `strict`, five required checks) was enabled 2026-08-31 — re-verify live
  per `AGENTS.md`, do not trust this date going forward.
- Refresh on 2026-10-08: merged structured filters and focus-visibility work
  were present on the default branch. Round 026's native investigation and
  both reports were pushed but unmerged; it shipped no navigation fix.
  Its required macOS empty-stderr assertion failed, and intermittent popup
  failures remained unresolved. Preserve that evidence when continuing;
  a successful diagnostic policy override is not a product result.

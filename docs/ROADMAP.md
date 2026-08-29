# Roadmap

Sequenced so that **the repository is always in a usable state**. No milestone requires
the previous one to be half-finished, and no milestone deletes working behaviour before
its replacement is demonstrably equivalent.

Status vocabulary is deliberately narrow: **done**, **in progress**, **not started**.
Nothing is described as shipped until it is.

---

## M0 — Foundation ✅ done

- [x] Standalone repository with full upstream history, `origin`/`upstream` configured
- [x] Architecture survey of upstream, read from source (`docs/architecture/`)
- [x] Baseline build of untouched upstream, verified running (`docs/BASELINE.md`)
- [x] UI stack decision recorded (ADR 0001)
- [x] Agent working agreement (`CLAUDE.md`)
- [x] Qt 6 / QML shell that compiles and runs, with a design token system

## M1 — Make change provable  🔶 in progress

The most important milestone in the project, because everything after it depends on being
able to assert *"this changed presentation, not duel behaviour."*

That assertion has two layers. **M1 delivers the first: a deterministic recorded-protocol
regression baseline.** The second — re-simulating through `ocgcore` to prove *live* engine
equivalence — is scoped below and deliberately not claimed here.

- [x] Determine whether `.yrp`/`.yrpX` replays can support deterministic regression.
      **Yes** — and better than expected: a `.yrpX` stores the duel message stream itself,
      so the common case needs no engine at all. Full trace in
      [replay-regression.md](architecture/replay-regression.md).
- [x] Capture a small fixture corpus and record golden traces.
      Three sanitised fixtures — two recorded duels covering turn progression, summons,
      chains, targeting, movement, battle, draws, reveals and a terminal win, plus one in
      the input format so both `.yrpX` and `.yrp` parsing are exercised by real data.
- [x] Deterministic harness with human-readable diffs, running headlessly.
      Python standard library only; no `ocgcore`, no card data, under a second.
- [x] CI on Linux covering the harness, the Qt shell and the upstream baseline.
- [ ] **Level 2 — not started, and required for the strong claim.** Re-simulate the
      embedded YRP1 through a headless `ocgcore` and compare the produced stream against
      the recorded one. Everything needed is already in the fixtures — seed, decks, duel
      parameters, ordered responses — but it also needs a compiled `ocgcore`, an
      `OCG_DataReader` backed by a pinned card database and an `OCG_ScriptReader` backed by
      pinned CardScripts, neither of which may be committed here. Scoped as its own
      milestone rather than folded into M1.

**Exit criterion:** a deterministic regression suite exists that fails if this project's
reading of the recorded duel protocol changes.
**Met** — verified by perturbing a golden and observing a failing test with a precise diff.

**What this exit criterion deliberately does not claim.** The fixtures are frozen
recordings and the harness never loads `ocgcore`, so **no change to C++ in this tree can
fail this suite.** It secures our decoding, not the engine's behaviour. Level 2 above is
what would close that gap; until it exists, "duel behaviour is unchanged" is not a claim
this repository can make automatically.

**Known limit:** the trace is structural (message ids and payload digests), not semantic.
A change altering payload *contents* is caught; one altering *interpretation* of unchanged
bytes is not. M2 adds a second, semantic trace over the same fixtures which does catch the
latter — for the 34 message types it decodes, and no further.

## M2 — Semantic client model  ✅ done

- [x] Presentation-free duel state types — no Irrlicht, no Qt, no rendering concepts.
      `client/` builds against a C++20 compiler and nothing else. Design and the source
      research behind it: [semantic-model.md](architecture/semantic-model.md),
      [ADR 0002](adr/0002-semantic-event-model.md).
- [x] Decode the duel message stream into semantic events and state patches. **34 of
      upstream's ~90 messages**, chosen for clear semantics and real coverage in the
      fixtures rather than for headline count. `MSG_UPDATE_DATA` and
      `MSG_UPDATE_CARD` use the bounded modern/compat query parsers documented in
      [query-stream.md](architecture/query-stream.md).
- [x] Semantic golden traces over the same fixtures, alongside the structural M1 traces,
      plus direct assertions that no packet is malformed, unknown or inconsistent and that
      the model's integrity invariants hold.
- [x] Unit tests for the model, independent of any UI. Seven CTest suites covering zone
      bookkeeping, decoder layouts and refusals, hidden information, the trace, the
      all-or-nothing decode guarantee, and the legacy player-perspective formula.
- [x] Decoding is transactional: a refused packet leaves `DuelState` byte-for-byte
      unchanged, verified by whole-state equality, not a field-by-field spot check.
      Pre-merge review found this false for four handlers; fixed centrally rather than
      handler by handler — [ADR 0002, Decision 6](adr/0002-semantic-event-model.md).
- [x] Legacy player-perspective normalization documented and pinned by a small, tested,
      deliberately test-only reference implementation, so the equivalence work below has a
      verified formula to build on without pulling perspective into the model itself —
      [ADR 0002, Decision 7](adr/0002-semantic-event-model.md).
- [x] **Run the model alongside the legacy handler.** The opt-in observer receives the
      normalized packet at one RAII seam in `DuelClient::ClientAnalyze`, decodes privately,
      and projects the real legacy state after the handler returns. It is observational and
      reports, rather than propagates, semantic failures.
- [x] **Complete external review of the live verifier.** The verifier drives every
      committed YRPX packet through the direct `Replay::packets_stream` to
      `DuelClient::ClientAnalyze` path with `isCatchingUp = false`, while an explicit
      verification seam suppresses presentation-only work. Its comparison is
      deliberately scoped to life points, turn, structural card occupancy/location/
      sequence, and material topology — [fixture-equivalence.md](architecture/fixture-equivalence.md).
      External review found and required the restoration of four ordinary `isCatchingUp`
      behavior changes accidentally introduced by the verifier work, then approved the
      result with no remaining technical blocker.
- [x] Add fail-closed packet/comparison accounting and a deterministic fault-injection
      CLI path so CI proves that a synthetic legacy mismatch returns non-zero.

**Exit criterion:** game state can be inspected without instantiating a renderer, and a
reviewed verifier can compare the declared semantic projection to real legacy state across
all recorded fixture packets.
**Met** — external review is complete and approved. CI independently pins both committed
fixtures to their known packet totals (990 and 1133) and requires every packet decoded,
processed and compared with zero decode/observer/comparison failures and zero mismatches;
a deterministic fault-injection path proves the failure mode is live.

The result is a scoped structural equivalence signal, not a claim of complete internal
legacy-client or duel-engine equivalence. The comparator does not include card code or
position, the verifier does not run `ocgcore`, and this is not M1 Level 2 — M1's own
re-simulation-based proof of live engine equivalence, scoped separately above and still
not started.

## M3 — Deck and card data  🔶 in progress

Chosen first among screens because upstream's `deck_manager` and card database are
already presentation-independent, so it can be built without touching the duel path.

- [x] Card database facade (SQLite, Project Ignis `.cdb` schema). `data/` reads the same
      `datas`/`texts` tables `DataManager::ParseDB` does, into a presentation-independent
      `CardRecord` - no Irrlicht, no Qt, no `ocgcore`, no legacy `DataManager`, and no
      legality or search logic of its own. Source research, exact schema semantics, and
      the deliberate divergences (load atomicity, locale overlay) are in
      [card-database.md](architecture/card-database.md) and
      [ADR 0003](adr/0003-card-database-facade.md).
- [x] Deck model reading and writing `.ydk`. `data/` reads and writes the same file-
      structural grammar `DeckManager::LoadCardList`/`SaveDeck` do, into a presentation-
      independent `Deck` of `data::CardCode` values - no `CardDataC` pointers, no
      `CardDatabase` dependency, no card-type classification or legality of its own. Source
      research, the two upstream load modes, and the deliberate divergences (explicit
      sections over type-based auto-classification, card code 0 excluded rather than
      stored) are in [deck-model.md](architecture/deck-model.md) and
      [ADR 0004](adr/0004-deck-model-ydk-codec.md).
- [x] Fast search over the full card pool. `data/`'s `CardSearchIndex` builds an explicit,
      presentation-independent snapshot from a `CardDatabase` and answers structured
      `SearchQuery`s - name/text matching, static metadata filters, deterministic ranking -
      with no `CardDatabase` mutation, no legality, and no UI syntax of its own. A simple
      linear scan over precomputed normalized strings measured comfortably fast
      (single-digit milliseconds) across a synthetic 22,000-card catalogue, so no inverted
      index or third-party search dependency was added. Source research, the deliberate
      exclusions from upstream's `CheckCardProperties`, and the performance measurements are
      in [card-search.md](architecture/card-search.md) and
      [ADR 0005](adr/0005-card-search-structured-query.md).
- [ ] Deck builder UI in QML: filters, legality, preview, keyboard parity
      **A functional core exists (M3D1), not the complete item.** A real `DeckBuilderScreen`
      wires `CardCatalog`/`CardSearchIndex` text search, an explicit-choice Main/Extra/Side
      editor over one canonical `Deck`, and `.ydk` open/save/new with a tested dirty-state
      contract, through a small Qt adapter layer (`ui/src/deckbuilder/`) that keeps `data/`
      Qt-free. Still missing: legality (deck size, three-copy, banlists), automatic
      Main/Extra classification, artwork, the legacy sigil search grammar and structured
      filters beyond plain text, and full keyboard/controller parity. Design and the
      deliberate exclusions: [deck-builder-ui.md](architecture/deck-builder-ui.md) and
      [ADR 0006](adr/0006-deck-builder-qt-adapter-boundary.md).

**Exit criterion:** a deck can be built and saved in the new client, and opened by
upstream EDOPro unchanged.
**Pending** - the card database facade, the deck model/`.ydk` codec, and fast search above
are done, and a functional deck-builder core now exists (M3D1); legality, automatic
classification and full keyboard/controller parity remain, so the milestone is not complete.
The format-level half of "opened by upstream EDOPro unchanged" is supported by construction
(the writer matches `SaveDeck`'s own section syntax and `LoadCardList` accepts it) but not
proven by an end-to-end GUI test, which does not exist yet.

## M4 — Low-risk screens

- [ ] Settings
- [ ] Replay browser
- [ ] Lobby and network screens

**Exit criterion:** the new shell is genuinely useful for something before the duel field
is attempted.

## M5 — Duel field

Last, deliberately. Highest risk, and dependent on everything above.

- [ ] Field rendering against the semantic model
- [ ] Chain visualisation and targeting
- [ ] Prompt system
- [ ] Motion that communicates rules state
- [ ] Compatibility path retained until parity is demonstrated

**Exit criterion:** a full duel is playable in the new client. Note that demonstrating
*duel behaviour* is unchanged requires the M1 Level 2 re-simulation harness, which does not
exist yet — the recorded-protocol baseline alone is not sufficient evidence for this
milestone.

## M6 — Platform and input

- [ ] Windows and macOS builds and CI
- [ ] Controller navigation over the existing focus model
- [ ] Steam Deck as a first-class target
- [ ] Accessibility pass

## Only after all of the above

- [ ] Consider removing legacy Irrlicht presentation code
- [ ] Optional semantic effect metadata layer (research first; must never determine
      legality — `ocgcore` and CardScripts stay authoritative)
- [ ] Generic AI reasoning research (long-term R&D; must not derail client work)

---

## Explicitly out of scope

Stated so they do not creep in:

- Reimplementing Yu-Gi-Oh's rules
- Replacing Project Ignis's Lua CardScripts
- Rewriting WindBot
- Porting MTG Forge
- Cloning Master Duel's visual design

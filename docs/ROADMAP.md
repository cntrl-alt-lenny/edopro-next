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
bytes is not. That is the natural extension in M2, once a semantic decoder exists.

## M2 — Semantic client model

- [ ] Presentation-free duel state types — no Irrlicht, no Qt, no rendering concepts
- [ ] Decode the duel message stream into semantic events, *in parallel with* the existing
      handler rather than replacing it
- [ ] Prove equivalence: legacy state and model state agree across the M1 fixtures
- [ ] Unit tests for the model, independent of any UI

**Exit criterion:** game state can be inspected without instantiating a renderer.

## M3 — Deck and card data

Chosen first among screens because upstream's `deck_manager` and card database are
already presentation-independent, so it can be built without touching the duel path.

- [ ] Card database facade (sqlite, Project Ignis `.cdb` schema)
- [ ] Deck model reading and writing `.ydk`
- [ ] Fast search over the full card pool
- [ ] Deck builder UI in QML: filters, legality, preview, keyboard parity

**Exit criterion:** a deck can be built and saved in the new client, and opened by
upstream EDOPro unchanged.

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

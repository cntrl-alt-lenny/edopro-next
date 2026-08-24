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

## M1 — Make change provable  🔜 next

The most important milestone in the project, because everything after it depends on being
able to assert *"this changed presentation, not duel behaviour."*

- [ ] Determine whether `.yrp`/`.yrpX` replays can be driven headlessly through `ocgcore`
      to produce a deterministic message stream. This is tracked as an open question in
      the architecture survey and is the key enabler for everything below.
- [ ] If yes: capture a small corpus of replay fixtures and record their message streams
      as golden files.
- [ ] If no: design an alternative deterministic fixture (scripted duel, seeded RNG) and
      document why the replay route failed.
- [ ] CI on Linux building both the upstream client and the Qt shell.

**Exit criterion:** a regression suite exists that fails if duel message semantics change.

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

**Exit criterion:** a full duel is playable in the new client, with M1 proving duel
behaviour is unchanged.

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

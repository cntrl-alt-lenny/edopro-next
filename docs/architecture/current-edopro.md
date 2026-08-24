# EDOPro as it exists today

A map of the upstream client, written by reading the source rather than the README.
Everything here cites real paths and was verified against upstream commit
`54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` (2026-08-20).

Its purpose is to identify **seams**: places where presentation can be separated from
state without disturbing duel behaviour. It is descriptive, not a criticism — this
codebase has years of correct behaviour encoded in it.

## 1. The three layers that already exist

EDOPro is not monolithic. There is a real boundary already; it is just not the boundary
we want.

| Layer | Where | Language | Ownership |
|---|---|---|---|
| Rules engine | `ocgcore/` (submodule to `edo9300/ygopro-core`) | C++ | Upstream, authoritative |
| Card behaviour | Project Ignis CardScripts (runtime data, not vendored) | Lua | Upstream, authoritative |
| Client | `gframe/` | C++ + Irrlicht | **What this project replaces** |

`gframe/` is roughly 29,000 lines of `.cpp` across 107 files. That is the entire surface
area this project is concerned with.

## 2. The engine interface is already clean

This is the single most important finding, and it is good news.

The client does **not** link `ocgcore` statically. `travis/build.sh` passes
`--no-core=true`, and the engine is loaded as a shared library through a C ABI:

- `gframe/ocgapi_types.h` — `OCG_Duel`, `OCG_Player`, `OCG_CardData`, `OCG_DuelOptions`
- `gframe/ocgapi_constants.h` — duel mode flags, locations, positions
- `gframe/ocgcore_functions.inl` — the dynamically resolved function table
- `gframe/core_utils.cpp` — helpers over the raw byte protocol

The API surface actually referenced by the client is small: duel creation and teardown,
card registration, `OCG_DuelProcess`, message retrieval, response submission, and the
query family, plus the `OCG_DataReader` / `OCG_ScriptReader` callbacks the host supplies
so the engine can pull card data and Lua scripts.

**Consequence:** the engine boundary is a byte-oriented message stream, not a set of UI
callbacks. A new client does not need to modify `ocgcore` at all. This is the fact that
makes the whole project viable.

## 3. Where the boundary goes wrong: `duelclient.cpp`

`gframe/duelclient.cpp` is 4,658 lines and contains **97 `case MSG_*:` branches** in its
duel message dispatch. That single dispatch is simultaneously:

- the protocol decoder (reads the engine's byte stream)
- the client state mutator (updates card, zone and phase state)
- the animation scheduler (starts card movement tweens)
- the widget driver (shows and hides Irrlicht dialogs, sets button text)
- the input gate (decides what the player may click)

For example, `MSG_SELECT_CARD` (around line 1976) decodes the candidate list, marks
`ClientCard::is_selectable`, populates a selection dialog and sets the prompt string, all
inline in one case body.

**This is the primary seam.** Splitting decode into semantic event into presentation is
the core of the migration, and it can be done incrementally, one message at a time.

## 4. The client model is the render model

`gframe/client_card.h` defines `ClientCard`, and it interleaves two unrelated concerns in
a single struct:

```cpp
irr::core::matrix4   mTransform;                 // presentation
irr::core::vector3df curPos, curRot, dPos, dRot; // presentation
irr::f32             curAlpha;                   // presentation
int32_t              aniFrame;                   // presentation
bool                 is_moving, is_fading, is_hovered;

uint32_t code, type, level, rank, link, attribute;   // semantics
int32_t  attack, defense, base_attack, base_defense; // semantics
uint32_t lscale, rscale, link_marker, reason;        // semantics
```

A card's identity and game state cannot currently be reasoned about without dragging in
Irrlicht math types. Any tooling, AI, test harness or alternative UI inherits the
renderer as a dependency.

`gframe/client_field.cpp` (1,284 lines) holds the zone vectors and has the same problem.

**Consequence:** the first substantial engineering task is a presentation-free duel state
model that `ClientCard` can be *projected onto* rather than immediately replaced by, so
the legacy renderer keeps working during transition.

## 5. Irrlicht ownership

`gframe/game.h` forward-declares around twenty Irrlicht GUI types, and the `Game` class
owns the `IrrlichtDevice`, `IVideoDriver` and `IGUIEnvironment` along with direct pointers
to essentially every widget. Irrlicht is not isolated behind an interface; it is ambient.

Supporting files:

- `gframe/drawing.cpp` (1,449) — the 3D field, card meshes, animation stepping
- `gframe/event_handler.cpp` (3,068) — a single Irrlicht event receiver for all input
- `gframe/utils_gui.cpp` — window and DPI helpers

A **custom Irrlicht fork** is used (`edo9300/irrlicht1-8-4`, branch `1.9-custom`), fetched
at build time on Windows; the repository also carries an `irrlicht/` tree locally.

This matters: the migration is away from a *patched* Irrlicht, not stock Irrlicht.
Behavioural differences live in that fork and are not documented upstream.

## 6. Subsystems and their coupling

| Concern | File | Lines | Coupled to Irrlicht? |
|---|---|---|---|
| Duel message handling | `duelclient.cpp` | 4,658 | Heavily |
| Application/window lifecycle | `game.cpp` | 4,022 | Totally |
| Input and GUI events | `event_handler.cpp` | 3,068 | Totally |
| Deck editor | `deck_con.cpp` | 1,703 | Heavily |
| Local duel host | `generic_duel.cpp` | 1,461 | No (server side) |
| Field rendering | `drawing.cpp` | 1,449 | Totally |
| Client duel state | `client_field.cpp` | 1,284 | Partly, via `ClientCard` |
| Menu and lobby flow | `menu_handler.cpp` | 1,180 | Totally |
| Card images | `image_manager.cpp` | 1,030 | Textures |
| Card database | `data_manager.cpp` | 767 | Via a custom sqlite VFS |
| Deck files | `deck_manager.cpp` | 604 | **No** |
| Networking | `netserver.cpp` | 444 | **No** |
| Replays | `replay.cpp` | 413 | **No** |
| Updater | `repo_manager.cpp` | 383 | Progress UI only |
| Sound | `sound_manager.cpp` | 281 | No |

The rows marked **No** — deck files, networking, replay, sound — are already
presentation-independent and are the cheapest things to reuse unchanged.

## 7. Card data and decks

`gframe/data_manager.cpp` uses **sqlite3** directly against Project Ignis `.cdb`
databases, through a custom Irrlicht read-file VFS shim (`gframe/ireadfile_sqlite.h`) so
databases can be read out of archives. Card text and card data are separate concerns in
the schema.

Decks are plain-text `.ydk` files handled by `gframe/deck_manager.cpp`
(`LoadDeckFromFile`, `LoadDeckFromBuffer`) with no UI dependency. A deck is a list of
passcodes plus main / extra / side partitioning.

**Consequence:** a modern deck builder can be built against `deck_manager` plus sqlite
without touching the duel path at all. This is the main reason the deck and library
screens are the right first migration target.

## 8. What is *not* in this repository

Worth stating explicitly, because it constrains what may be redistributed:

- **Card scripts** (Lua) — Project Ignis CardScripts, fetched at runtime
- **Card databases** (`.cdb`) — Project Ignis BabelCDB, fetched at runtime
- **Card images** — downloaded by the client; licensing does not permit vendoring
- **WindBot** — a separate .NET project invoked as an external process

`gframe/repo_manager.cpp` is what fetches several of these at runtime via git.

## 9. Platform surface

Windows, macOS and Linux desktop, plus Android and iOS. Mobile-specific code is scattered
(`ios-assets/`, Android scoped-storage handling on a dedicated upstream branch).

The build is `premake5` (5.0.0-beta2) generating gmake2 or Visual Studio projects, with
dependencies supplied by a **prebuilt vcpkg cache published by upstream** rather than
built from source. See `docs/BASELINE.md` for the reproduction record.

## 10. Identified seams, in migration order

Ranked by value divided by risk:

1. **Deck and card data** — already UI-free. Build the semantic model here first.
2. **Settings and config** — `game_config.cpp` is plain data.
3. **Replay browser** — `replay.cpp` is UI-free; only the list UI is coupled.
4. **Lobby and network** — `netserver.cpp` is UI-free; only the screens are coupled.
5. **Duel state projection** — split `ClientCard` into semantic and presentation halves.
6. **Duel message decode** — extract the 97 `MSG_*` cases into a pure decoder emitting
   semantic events, with the legacy handler initially consuming those events so that
   behaviour is provably unchanged.
7. **Field rendering** — last, and only once items 1 through 6 are demonstrated.

## 11. Open questions

Honest unknowns, to be resolved before they gate a decision:

- Does the custom Irrlicht fork contain behaviour (font metrics, texture handling) that
  the current UI depends on in ways a Qt port would need to replicate?
- Can `.yrp` / `.yrpX` replays be replayed headlessly through `ocgcore` to produce a
  deterministic message stream usable as a regression fixture? `replay_mode.cpp` suggests
  yes; this is unproven and is tracked as the key enabler for behavioural testing.
- How much of `event_handler.cpp` encodes rules-adjacent input gating rather than pure
  input handling?
- Are there duel messages whose handling depends on Irrlicht timing or animation state?

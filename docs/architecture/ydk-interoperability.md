# Real upstream `.ydk` interoperability proof (`integration/legacy/ydk_interop.{h,cpp}`)

A narrowly-scoped proof that a `.ydk` file produced by this project's own, reviewed
`edopro_next::data::save_ydk()`/`serialize_ydk()` (M3B, [deck-model.md](deck-model.md)) is
accepted by the real, unmodified `DeckManager::LoadDeckFromFile()` - the same function
upstream's own deck builder calls - and produces the `ygo::Deck` structure that source
reading says it should. It exists to convert M3's most load-bearing unproven claim -
`docs/ROADMAP.md`'s "opened by upstream EDOPro unchanged" - from "supported by construction"
into something CI checks against the real preserved loader, every run.

This document is the source-verified account this module is reviewed against, in the same
spirit as [deck-model.md](deck-model.md) and [deck-legality.md](deck-legality.md). Read it
before changing `integration/legacy/ydk_interop.cpp`.

---

## 0. What this is not

- **Not a GUI/file-dialog proof.** No Irrlicht device, no `mainGame`, no file picker. See
  §5.
- **Not an upstream `SaveDeck` proof.** Only the new-client-writes -> upstream-opens
  direction is exercised. See §6 for why the reverse direction is out of scope, not
  "impossible."
- **Not a claim that this project's explicit `Deck` sections always survive upstream's
  loader unchanged.** The fixture's own item B (§4) exists specifically to demonstrate a
  case where they do not.
- **Not a claim about real BabelCDB card data.** The database is a small, synthetic,
  committed-safe fixture (§3) - CLAUDE.md forbids committing or fabricating real card data,
  and this harness fetches none at runtime either.
- **Not a legality test.** Nothing here calls `CheckDeckSize`/`CheckDeckContent`, or this
  project's own `policy::validate_deck()`. Both are untouched.
- **Not an `ocgcore` test.** No duel is simulated; no core function is called.
- **Not an independent claim about `LoadCardList`'s raw text grammar.** See §2.

---

## 1. Upstream source this harness drives directly

| Concept | Upstream source |
|---|---|
| File-open entry point (what this harness calls) | `DeckManager::LoadDeckFromFile` (`gframe/deck_manager.cpp:319-328`) |
| The private `.ydk` grammar reader | `LoadCardList` (`gframe/deck_manager.cpp:272-317`, file-local `static`) |
| Classification into Main/Extra | `DeckManager::LoadDeck` (`gframe/deck_manager.cpp:330-392`) |
| Dummy/unknown-card synthesis | `DeckManager::GetDummyOrMappedCardData` (`gframe/deck_manager.cpp:17-27`) |
| Card database load | `DataManager::LoadDB`/`ParseDB` (`gframe/data_manager.h:140-142`, `gframe/data_manager.cpp:98-179`) |

`LoadDeckFromFile` is the same entry point upstream's own deck builder calls
(`gframe/deck_con.cpp`'s `DeckBuilder::SetCurrentDeckFromFile`, a five-line wrapper over it -
see §5). This harness reaches it exactly the way that real caller does: one `epro::
path_stringview` and a `separated` bool, nothing lower-level.

---

## 2. Why `LoadCardList` is not an independently-proven level

`LoadCardList` is declared `static` at file scope (`gframe/deck_manager.cpp:272`) - it has
no declaration in `deck_manager.h` and no external linkage. This harness does not call it,
and this project does not modify its visibility to make that possible: no upstream
declaration was changed to manufacture a cleaner claim (see
[ADR 0008](../adr/0008-upstream-ydk-interop-harness.md) for the explicit decision record).

Concretely, this means: the raw textual `.ydk` grammar `LoadCardList` implements is
exercised here **only transitively**, through `LoadDeckFromFile`. Every observation this
harness makes is of `LoadDeck`'s output (the classified `ygo::Deck`), not of `LoadCardList`'s
own intermediate `mainlist`/`extralist`/`sidelist` vectors. A grammar-level regression in
`LoadCardList` that `LoadDeck` happens to mask would not necessarily be caught here. Do not
describe this harness as proving "`LoadCardList` in isolation" - it proves the composed
behaviour of the real public entry point, nothing narrower and nothing broader.

---

## 3. The synthetic database

`integration/legacy/ydk_interop.cpp` builds its own tiny `datas`/`texts` SQLite file
directly against the SQLite C API - a fresh, minimal implementation, not a reuse of
`data/tests/synthetic_cdb.h` (which builds the same *shape* of schema for `data/`'s own
CMake test suite). Two reasons, both recorded as comments at the point they govern in
`ydk_interop.cpp`:

1. `data/tests/` is scoped to `data/`'s own CMake test targets. `integration/legacy/` is a
   Premake-built, opt-in production harness leg (the observer/interop build), not a `data/`
   test - reaching into a sibling module's `tests/` directory from here would blur exactly
   the ownership boundary CLAUDE.md's "Where code belongs" table draws.
2. This harness only ever needs the columns `DeckManager::LoadDeck` actually reads for the
   load path - `id`, `alias` (always 0, since no fixture card is aliased), `type`. Every
   other column (`ot`, `setcode`, `atk`, `def`, `level`, `race`, `attribute`, `category`) is
   irrelevant here and left at a fixed `0`.

The schema itself mirrors `DataManager::ParseDB`'s own query exactly
(`gframe/data_manager.cpp`'s `SELECT_STMT`: `datas`/`texts` joined on `id`) - see
[card-database.md](card-database.md) for the full, separately-verified account of that
schema. Every card name inserted is a synthetic, runtime-generated placeholder
(`"synthetic card <code>"`) - never real card text, matching CLAUDE.md's licensing
constraints. Nothing here is committed to the repository; the file is written to a temporary
path and removed when the harness process's `TempPath` RAII wrapper goes out of scope.

---

## 4. The fixed fixture deck {#fixture}

Five card codes, four of them present in the synthetic database:

| Code | Constant | In DB? | Type | Written under | Role |
|---|---|---|---|---|---|
| `10000001` | `kCardMain` | yes | `TYPE_MONSTER` | `#main` | an ordinary Main-deck card |
| `10000002` | `kCardFusion` | yes | `TYPE_MONSTER \| TYPE_FUSION` | `#main` | a real Extra-deck-typed card, written under `#main` - proves `LoadDeck` reclassifies it regardless of which section our own file put it in |
| `10000003` | `kCardExtra` | yes | `TYPE_SPELL` | `#extra` | an ordinary, non-Extra-typed card, written explicitly under `#extra` - see §5 for why its fate differs sharply between the two `separated` modes |
| `10000004` | `kCardSide` | yes | `TYPE_TRAP` | `!side` | an ordinary Side-deck card |
| `10000099` | `kCardUnknown` | **no** | - | `#main` | a non-zero code never inserted into the database - the unknown-card asymmetry, §5 |

The fixture `edopro_next::data::Deck` is `{main: [kCardMain, kCardFusion, kCardUnknown],
extra: [kCardExtra], side: [kCardSide]}`, serialized by the real, unmodified
`edopro_next::data::save_ydk()` (default `YdkWriteOptions{}`, so no `"#created by"` line) -
never hand-authored `.ydk` text. Per `serialize_ydk`'s own documented, unconditional-marker
behaviour ([deck-model.md](deck-model.md)§4), the resulting file is exactly:

```
#main
10000001
10000002
10000099
#extra
10000003
!side
10000004
```

No Ritual monster is included, so `RITUAL_LOCATION::DEFAULT` (the value this harness passes
throughout) is inert for this fixture - `is_extra_deck_card`'s Ritual branch
(`gframe/deck_manager.cpp:339-344`) is never reached. Exhaustive `MAIN`/`EXTRA`/`DEFAULT`
Ritual-placement coverage is out of scope for this slice; see §7.

---

## 5. `separated=false` vs. `separated=true`: two different claims, kept separate {#separated-modes}

`LoadDeckFromFile` passes `separated ? &extralist : nullptr` as `LoadCardList`'s `extralist`
argument (`gframe/deck_manager.cpp:322-325`), and that single pointer changes two things at
once: whether `"#extra"` is a functional marker at all, and whether an unresolvable code is
dropped or kept. Both expected results below were derived **by hand, from reading
`gframe/deck_manager.cpp:272-392` directly** - never by running this harness, this project's
own parser, or upstream and capturing the output; the harness's expected constants
(`ydk_interop.cpp`) simply format the already-fixed `kCard*` numeric constants as text.

### `separated=false` (`extralist == nullptr`)

In `LoadCardList`: every `"#..."` line is skipped unconditionally
(`gframe/deck_manager.cpp:288-291`: `if(!extralist || str != "#extra") continue;` - `
!extralist` is always true here), so `"#extra"` never sets `is_extra`, and every numeric
line before `"!side"` - including `10000003`, physically written after the `"#extra"`
marker - lands in the single combined `mainlist`: `[10000001, 10000002, 10000099,
10000003]`. `sidelist = [10000004]`.

In `LoadDeck` (`loadalways = !!extralist = false`):
- `10000001` -> not extra-typed -> **Main**.
- `10000002` -> Fusion-typed, `is_extra_deck_card` true -> **Extra**.
- `10000099` -> not in the database -> `GetDummyOrMappedCardData` returns a dummy
  (`code=0, alias=10000099`) -> `(!cd || cd->code==0) && !loadalways` is true ->
  `errorcode = 10000099; continue;` - **dropped from the deck entirely**.
- `10000003` -> not extra-typed -> **Main**.
- `10000004` (side list) -> **Side**.

**Expected: `main=[10000001, 10000003]`, `extra=[10000002]`, `side=[10000004]`.**

### `separated=true` (`extralist == &extralist`)

In `LoadCardList`: `"#extra"` now sets `is_extra = true` (the condition above becomes
false), so `mainlist = [10000001, 10000002, 10000099]`, `extralist = [10000003]`,
`sidelist = [10000004]` - the file's own sections round-trip exactly.

In `LoadDeck` (`loadalways = !!extralist = true`):
- `10000001`, `10000002` -> same classification as above -> **Main**, **Extra**.
- `10000099` -> dummy (`code=0, alias=10000099`); the drop condition's `!loadalways` is now
  false, so it is **not** dropped. The reclassification gate itself
  (`(!extralist || cd->code != 0)`, `gframe/deck_manager.cpp:346`) is also false here
  (`extralist` is non-null **and** `cd->code == 0`), so classification is skipped and it
  falls to the `else` branch: **kept in Main, as a dummy** (`code=0, alias=10000099`).
- `10000003` (extra list) -> the `extralist` loop (`gframe/deck_manager.cpp:365-374`)
  applies **no type check at all** - every non-token code reaching it is pushed to
  `deck.extra` unconditionally -> **Extra**, appended after `10000002`.
- `10000004` (side list) -> **Side**.

**Expected: `main=[10000001, 10000099-as-dummy]`, `extra=[10000002, 10000003]`,
`side=[10000004]`.**

### What this pair actually shows

`10000002`'s reclassification from `#main` into Extra happens **identically in both
modes** - a real Extra-typed card's file placement never matters. `10000003`'s fate is the
opposite: it moves between Main and Extra depending purely on `separated`, with **no
relationship to its own card type** in the `separated=true` case. Neither mode alone is "the
`.ydk` format" - they are two different, real upstream call configurations, and this harness
proves both, separately, rather than picking one and generalizing.

### Unknown-card semantics, made concrete against the real loader

[deck-model.md](deck-model.md)§8 documents this asymmetry from reading source; this harness
is what turns that reading into a running check against the real, unmodified loader. Per
the correction accepted for this slice: `LoadDeckFromFile` calls `LoadDeck` and **discards**
its `uint32_t errorcode` return value (`gframe/deck_manager.cpp:327`: `LoadDeck(...); return
true;`). This harness does not, and must not, claim to observe that discarded value - it
was not changed to expose it, and doing so would misdescribe what `LoadDeckFromFile` (the
function this harness actually calls) does. What **is** observable, and is exactly what this
harness checks, is the unknown code's effect on the resulting `ygo::Deck`: absent entirely
in `separated=false`, present as an explicit dummy entry (`code=0`, `getRealCode()` returning
the original code) in `separated=true`.

---

## 6. Upstream `SaveDeck`: real scope, not "impossible"

The reverse direction - real upstream `SaveDeck` output, read back by this project's own
`edopro_next::data::parse_ydk()`/`load_ydk()` - is **not attempted in this slice**, for a
concrete, source-verified reason, not a hand-wave: `DeckManager::SaveDeck`
(`gframe/deck_manager.cpp:436-451`) reads live presentation/global state directly -
`mainGame->ebNickName->getText()` for the `"#created by "` line
(`gframe/deck_manager.cpp:441`) - and `MakeYdkEntryString`
(`gframe/deck_manager.cpp:469-472`) additionally depends on `gGameConfig-
>addCardNamesToDeckList` and, when that option is set, `gDataManager->GetName(code)`.
Proving this direction headlessly would require substantially more legacy presentation
scaffolding than the load direction does (at minimum, a real `mainGame`/`Game` instance with
a populated nickname field) - genuinely more work, not a fundamental barrier. It is simply
unnecessary for M3's actual claim (new-client-writes -> upstream-opens), so it is deliberately
out of scope here, and may be picked up as its own slice if a concrete need for it arises.

---

## 7. Ritual placement

This fixture contains no Ritual monster, so `RITUAL_LOCATION::DEFAULT` is passed throughout
and never actually exercises `is_extra_deck_card`'s Ritual branch
(`gframe/deck_manager.cpp:339-344`) or the `MAIN`/`EXTRA` values at all. Exhaustive coverage
of all three `RITUAL_LOCATION` values is deliberately not built here - it would need a
Ritual-typed synthetic card and roughly doubles the mode matrix for a case this slice's
scope does not require. A future slice may add it if a real interoperability requirement
demands it.

---

## 8. Minimal real legacy state {#minimal-legacy-state}

Audited directly from source before writing any harness code, not assumed:

- `DeckManager::LoadDeckFromFile`/`LoadDeck` read `gDataManager` (via `GetCardData`) and
  write/read `gdeckManager` (via `GetDummyOrMappedCardData`) - both must be valid pointers.
  Nothing else in `gframe/`'s global state is touched by this call path.
- `DataManager::LoadDB`/`OpenDb(epro::path_stringview)` (`gframe/data_manager.cpp:56-64`)
  calls `sqlite3_open_v2` **directly** on the given path - it does not go through the
  Irrlicht-backed `irrsqlite` virtual filesystem `DataManager`'s constructor also sets up
  (that VFS is only used by the separate `OpenDb(irr::io::IReadFile*)` overload, for loading
  from inside an archive - not exercised here). No Irrlicht device is required to load a
  plain-filesystem `.cdb`.
- `DeckManager::GetDeckPath` / `Utils::NormalizePath` perform pure string manipulation with
  no filesystem access and no working-directory dependency. `LoadDeckFromFile`'s first
  attempt (`GetDeckPath(file)`, treating `file` as a bare deck *name* under `./deck/`)
  therefore always fails harmlessly for this harness's absolute temp paths, and its fallback
  attempt (the literal `file` argument) succeeds - no `./deck/` directory needs to exist.
  `FileStream` is a plain `std::fstream` on the Linux target this harness builds for
  (`gframe/file_stream.h`'s non-MinGW, non-Android branch).

Consequently, this harness constructs exactly one `ygo::DataManager` and one
`ygo::DeckManager`, installs them as `gDataManager`/`gdeckManager` for the duration of one
scoped RAII object (`ScopedLegacyState`, mirroring `replay_verifier.cpp`'s own
`ActiveVerification` pattern), and restores both to `nullptr` on every exit path, including
an exception. It deliberately does **not** construct `ygo::GameConfig`, `ygo::SoundManager`,
`ygo::Game`, `mainGame`, or any Irrlicht device - the load direction this harness exercises
never touches any of them, unlike `replay_verifier.cpp`'s duel-replay path, which does.

---

## 9. The dual `edopro_next_deck` build seam

`data/`'s own standalone developer/test build (`data/CMakeLists.txt`'s `edopro_next_deck`
target) is CMake-based, exactly as before. The real `ygoprodll` integration build - the only
place this harness can actually link against the real, preserved `DeckManager` - is
Premake-based, and had no Premake project for `edopro_next_deck` before this slice.
`data/premake5.lua` adds one: a small, logically-equivalent `StaticLib` project compiling
only `src/ydk.cpp` (mirroring `data/CMakeLists.txt`'s own split - no SQLite, no
`card_database.cpp`, no search sources, no Qt, no `gframe`, no `ocgcore`), included from the
top-level `premake5.lua` only inside the existing `_OPTIONS["semantic-observer"]` guard,
alongside `client` and before `integration/legacy` (which now links it).

This is a second, parallel *build description* of the same source file
(`data/src/ydk.cpp`), not a second *copy* of it - deliberately, so the exact bytes
`DeckManager::LoadDeckFromFile` receives in this harness are produced by the identical,
already-reviewed `save_ydk()`/`serialize_ydk()` implementation the CMake-built
`data/tests/test_deck_ydk.cpp` suite exercises, with no drift possible between "the writer
this harness proves against upstream" and "the writer `data/`'s own tests already verify."
See [ADR 0008](../adr/0008-upstream-ydk-interop-harness.md) for the alternatives this was
weighed against.

---

## 10. Proof boundaries

### Proven, by CI, against the real preserved loader

- Bytes produced by the current `edopro_next::data::save_ydk()`/`serialize_ydk()` are
  accepted by real `DeckManager::LoadDeckFromFile()`.
- `LoadCardList`'s grammar is exercised **transitively** through that real, public entry
  point (§2) - not called or asserted against in isolation.
- Real `LoadDeck` classification behaviour, for this fixture's synthetic card types,
  including the Extra-Deck reclassification of a card written under `#main` (§5).
- `separated=false` behaviour for this fixture (§5).
- `separated=true` behaviour for this fixture (§5) - a materially different result, kept
  explicitly distinct from the above.
- The unknown-card/dummy asymmetry, as actually observable through the resulting `ygo::
  Deck` (§5) - never via `LoadDeck`'s discarded `errorcode`.
- The comparator itself is live: `--verify-ydk-interop-fault` deterministically corrupts one
  already-observed value after the real load and produces a reproducible mismatch (two runs,
  byte-identical output, non-zero exit both times) - mirroring M2's own fault-injection
  proof that its comparator is not vacuously green.

### Not proven here

- `LoadCardList` called or asserted independently of `LoadDeckFromFile`.
- Real-world BabelCDB classification behaviour - only synthetic types are exercised.
- Upstream's GUI/file-picker interaction (`DeckBuilder::SetCurrentDeckFromFile`'s own
  Irrlicht-facing caller).
- Upstream `SaveDeck` -> this project's own parser (§6) - a real, scoped, deliberate
  omission, not a claim of impossibility.
- Arbitrary, hand-authored, or malformed `.ydk` files - only this project's own writer's
  output is exercised.
- `ocgcore` behaviour of any kind.
- Deck legality (`policy::validate_deck()`, or upstream's `CheckDeckSize`/`CheckDeckContent`).
- Exhaustive `RITUAL_LOCATION` coverage (§7).

---

## 11. CLI

Two entry points, both requiring no arguments (the fixture and the synthetic database are
built internally, deterministically, every run):

```
ygoprodll --verify-ydk-interop         # exit 0 iff both modes match their expected result
ygoprodll --verify-ydk-interop-fault   # exit non-zero, deterministically, every run
```

Both are only meaningfully wired up in the same opt-in integration build leg the semantic
replay verifier already uses (`EDOPRO_NEXT_SEMANTIC_OBSERVER`, `--semantic-observer` at
Premake configure time) - reused here purely as the existing CI vehicle
(`.github/workflows/edopro-next.yml`'s `observer=true` matrix leg already builds and runs
this configuration; no second, expensive matrix dimension was added). This is **not**
conceptually a semantic-observer feature - it shares the build leg, not the concept - see the
comments at each of the three small `gframe/` call sites
(`cli_args.h`/`edopro_main.cpp`/`gframe.cpp`). On an ordinary, non-integration build, the two
CLI flags are still recognized (matching the existing `--semantic-verify-replay` flags'
own behaviour) but do nothing: the code that would act on them does not exist in that binary
at all, since `integration/legacy/` itself is only compiled into the observer-enabled
configuration.

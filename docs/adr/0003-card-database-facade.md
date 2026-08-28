# ADR 0003 — A standalone card-database facade

- **Status:** Accepted
- **Date:** 2026-08-28
- **Context commit:** upstream `54ea755a` (unchanged since ADR 0002; see `docs/UPSTREAM.md`)
- **Supersedes:** nothing. Sits alongside [ADR 0002](0002-semantic-event-model.md), which
  made the same kind of case for `client/`.

## Context

M3's first slice needs a way to read Project Ignis `.cdb` card databases into something
the eventual deck model, card search, and QML deck builder can use without any of them
knowing SQLite or touching a `.cdb` file directly. Upstream already has this:
`gframe/data_manager.h`/`.cpp`'s `DataManager`. The question this ADR answers is not
"what fields does a card have" - that is source-verified fact, written down once in
[card-database.md](../architecture/card-database.md) so it does not need re-deriving - but
where this capability should live and what it should depend on.

Five decisions turned out to be load-bearing:

1. a new top-level module, not `client/` and not `gframe/`;
2. SQLite via CMake's own `FindSQLite3`, not the project's vcpkg bundle;
3. a stronger per-file load guarantee than `DataManager::ParseDB` actually gives;
4. locale is a separate, clearable overlay layer, matching upstream's actual fallback rules
   (which are *not* uniform across fields) rather than the uniform per-field rule an earlier
   draft of this module invented;
5. a `.cdb` row with card code 0 is a load failure, not data.

---

## Decision 1 — `data/`, a new top-level module

**Card data gets its own directory and its own CMake project, not a corner of `client/` or
a new file in `gframe/`.**

### Why not `gframe/`

CLAUDE.md is explicit: `gframe/` is upstream's, touched minimally, and new systems do not
scatter through it. `DataManager` already lives there and works; this module exists
*because* callers other than the legacy Irrlicht client will eventually need card data
(the QML deck builder, first), not to replace it.

### Why not `client/`

`client/` earned a hard rule the moment M2 shipped it: "no Qt, no Irrlicht, no ocgcore, no
card database" (`client/CMakeLists.txt`'s own header comment). It exists to answer "what is
true about this duel", built from nothing but the protocol stream. Card data answers a
different question - "what does this card code mean" - true independent of any duel, and
useful to code that never touches `DuelState` at all (a deck list validator, for instance).
Folding the two together would make `client/` depend on SQLite for every consumer, whether
or not that consumer ever names a card, and would make this module depend on protocol
decoding for no reason of its own.

### The cost, and why it is acceptable

Two modules that could share a `CardCode` type instead each define their own
(`edopro_next::client::CardCode`, `edopro_next::data::CardCode`) - see
`data/include/edopro_next/data/card_code.h`. A future caller using both pays one
`static_cast<std::uint32_t>` round-trip to convert between them. That is a smaller and more
honest cost than the alternative: either module silently depending on the other's whole
include tree so they can share four lines of enum declaration, which is exactly the kind of
accidental coupling this project's module boundaries exist to prevent (see ADR 0002's own
`client/` vs. `gframe/` separation, and CLAUDE.md's "Where code belongs" table, to which
this ADR adds a `data/` row).

---

## Decision 2 — SQLite via `find_package(SQLite3)`, not the vcpkg bundle

**`data/CMakeLists.txt` calls CMake's own `find_package(SQLite3 REQUIRED)`. It does not
link against the vcpkg-provided `sqlite3` upstream's premake build uses.**

### What upstream actually does

`gframe/premake5.lua` links `sqlite3` for the full client build, resolved through the
project's existing vcpkg bundle (`VCPKG_ROOT`, fetched in CI from
`edo9300/edopro-vcpkg-cache`; see `.github/workflows/edopro-next.yml`'s `upstream-baseline`
job). That bundle is real, but it is fetched by - and sized for - the full, expensive,
premake/gmake2-built `ygoprodll` target.

### Options considered

**A. Reuse the same vcpkg bundle for `data/`'s CMake build.** Rejected. `client/`'s own CI
job is deliberately in the cheap, every-push tier (`semantic-client`: Ninja and a plain
`cmake -S client -B client/build`, no vcpkg, no premake) precisely so M2's tests do not pay
for the full baseline's dependency fetch. Wiring vcpkg into a CMake-only job would either
duplicate that expensive fetch in a job that does not otherwise need it, or couple `data/`'s
build to premake's own toolchain setup - the opposite of "as little as practical".

**B. Vendor a copy of SQLite's amalgamation source into this repository.** Rejected without
much debate. It is the same library upstream already depends on; vendoring a second copy
of it would violate "no new dependencies without justification" in the other direction -
justifying a *duplicate* dependency is a higher bar than justifying reuse of an existing
one, and this repository has no reason to carry two SQLite copies that must be kept in sync.

**C. `find_package(SQLite3)` against the system package.** *(chosen)* SQLite is the same
open-source C library either way; only its provisioning differs. CMake has shipped a
`FindSQLite3` module since 3.14 (this project already requires 3.21), and Ubuntu's
`libsqlite3-dev` is a normal, small, `apt-get`-installable package - see the `card-database`
CI job, which installs it alongside `ninja-build` exactly like `semantic-client` installs
just `ninja-build`. This keeps `data/`'s tests in the same cheap tier as `client/`'s, with
no vcpkg fetch and no premake dependency, while still being "the repository's existing
dependency strategy" in the sense CLAUDE.md's style rule actually cares about: the same
database library, not a second SQL engine or ORM.

### Consequence

`data/CMakeLists.txt` cannot be built in the exact same invocation as `gframe/`'s premake
build, and does not try to be - it is its own standalone CMake project, exactly like
`client/` and `ui/` already are (there is no root `CMakeLists.txt` in this repository; each
owns its own build). A future full-client build that wants to link `edopro_next_data` into
`ygoprodll` would need its own bridging work, out of scope for this slice, and not
implied by anything here.

---

## Decision 3 — A load is atomic against the catalogue, which is stronger than upstream

**`CardDatabase::load_database()` parses an entire file into a private staging map first,
and only merges it into the live catalogue if the whole file read without error. A file
that fails partway through leaves the catalogue exactly as it was before the call.**

### What upstream actually does

`DataManager::ParseDB` (`gframe/data_manager.cpp:98`) writes each row directly into
`cards[code]` as it steps through the query. If row *N* fails - a corrupt page, a value
`sqlite3_step` cannot produce mid-scan - `Error()` closes the statement and the database
handle and returns `false`, but rows `0..N-1` are already live in `cards`. The catalogue is
left part-old-file, part-new-file, part-neither, with no record of which.

### Why this module does not copy that

Task guidance for this slice was explicit: prefer a strong load guarantee where practical,
because leaving a card catalogue half-mutated after a failed load is a worse failure mode
than the load simply not happening. Unlike M2's transactional-decoding fix
(ADR 0002, Decision 6), this is not a bug being corrected in code that claimed a guarantee
it did not have - `DataManager` never claimed one - so there is no "options considered"
table of alternatives that were tried and found wanting; the design is new, not a repair.
The mechanism is the same shape as Decision 6's, because it is the same problem: parse into
a private copy, commit only once the whole operation has succeeded. The cost is the same
kind too - a failed load parses the file twice as much SQLite work as upstream's early-exit
would (staging map, then merge) - which is the right trade for a load path measured in
tens of thousands of rows once per file, not a per-packet budget.

---

## Decision 4 — Locale is a separate, clearable overlay layer with upstream's actual fallback rules

**`CardDatabase` keeps the base layer (`load_database()`) and the active locale layer
(`load_locale()`) as independent state, with an explicit `clear_locale()` to discard the
locale layer and restore base text - and the layer's fallback rule is upstream's actual
one, not a "cleaner" invented uniform rule.**

### The bug this replaces

An earlier version of this decision recorded a different design: `load_locale()` wrote
translated values directly into the only stored `CardRecord`, field by field, with no way
to discard them. External review of the resulting PR found the consequence directly:
switching locale - or returning to the base language - could not un-apply a previously
loaded locale, because the base string it would need to restore had already been
overwritten and discarded. A `.cdb` schema fact search would have caught this immediately
by asking how `Game::ApplyLocale` actually works; that research had not been done.

### What upstream actually does, traced end to end

`Game::ApplyLocale` (`gframe/game.cpp:3987-4020`) is the real state machine:

```cpp
gDataManager->ClearLocaleStrings();
gDataManager->ClearLocaleTexts();
if(index > 0) {
    // ... load one or more locale .cdb files for the newly selected locale ...
}
```

`ClearLocaleTexts()` (`gframe/data_manager.cpp:46-54`) runs **unconditionally**, before
loading anything - including before switching back to the base language (`index == 0`,
which loads nothing at all). It nulls every card's `_locale_strings` pointer and clears the
separate `locales` map outright. This is the operation an earlier draft of this facade had
no equivalent for.

`DataManager::GetStrings()` (`gframe/data_manager.h:111-115`) returns the *entire* locale
`CardString` if a locale pointer is linked at all, or the entire base `CardString` if it is
not - never a mix of the two. A locale entry that is linked but has an empty `name` does
**not** fall back to the base name; `GetName()` separately turns that empty result into a
`"???"` placeholder (a presentation concern this module does not reproduce - see
`card-database.md#locale`). Only the sixteen `desc[]` strings get per-slot fallback
(`CardDataM::GetDesc`, `gframe/data_manager.h:116-124`): each slot falls back to base
independently exactly when the locale layer's own slot for that card is empty.

`ParseLocaleDB` (`gframe/data_manager.cpp:180-228`) reuses `locales[code]` across every file
loaded into the same active locale (nothing but `ClearLocaleTexts()` resets it), and
`GetWstring` (`:77-96`) unconditionally writes each column - clearing the target string when
the column is empty rather than leaving a prior value in place. So a later file's row for a
code an earlier file in the same active locale already touched replaces it completely, field
by field, even with an empty value - the same last-file-wins rule `ParseDB` uses for the
base layer, applied here to the locale layer instead. `ParseLocaleDB`/`ParseDB`'s shared
`indexes` map also links a locale row to a base row whichever order they arrive in, which
upstream's real startup path (`gframe/data_handler.cpp:28`'s `LoadDatabases()`, then
`gframe/game.cpp:2647`'s locale load afterward) never actually exercises in reverse.

### The corrected design

- **Three maps**, not one: `base_text_` (immutable once loaded, independent of any locale
  operation), `locale_text_` (the active locale layer; a code present here - regardless of
  whether its fields are empty - is "linked", in upstream's terms), and `records_` (the
  materialized, effective view `find()`/iteration expose, recomputed by `resolve_text()`
  whenever a load or clear touches a code).
- **`clear_locale()`** discards `locale_text_` and recomputes every code it had touched back
  to its `base_text_` value - the data-layer half of `ClearLocaleTexts()`.
- **`name`/`text` are linked as a pair**, exactly matching `GetStrings()`: once a code is
  linked, both come from the locale layer even if one or both are empty, never falling back
  to base field-by-field. This module does not reproduce the `"???"` placeholder - that is
  `GetName()`'s own presentation fallback, a layer above the raw `CardString` selection this
  facade's job stops at (see `card-database.md#locale`).
- **The sixteen auxiliary strings keep their own per-slot fallback**, matching `GetDesc()`,
  deliberately *not* unified with the name/text rule above - that unification was the
  invented behavior this decision replaces.
- **A later `load_locale()` call for a code the active locale already touched fully
  replaces its entry**, matching `ParseLocaleDB`'s `locales[code]` reuse - not a per-field
  merge across files.
- **Base-before-locale only** is retained from the original decision, for the same reason:
  verified against the two real call sites (`data_handler.cpp:28`, `game.cpp:2647`/`:4001`),
  every real startup loads every base database before any locale data, and order-independent
  linking is real `DataManager` capability but dead code in practice. A locale row for a
  code not already loaded via `load_database()` is ignored, not queued.

### Why this is not "blindly clone DataManager"

The remaining, deliberate divergence is narrow and already stated in
`docs/architecture/card-database.md`: this module does not reproduce `GetName()`'s `"???"`
placeholder substitution (a UI-facing default, not a `CardString`-level fact), it does not
reproduce the Irrlicht `IReadFile` locale-load path, and it does not track *which* locale is
active by name - `clear_locale()` is a pure data-layer state transition, and choosing which
files correspond to which language stays a caller's job, per this module's own scope rule.
Everything else in this decision is upstream's actual, source-verified fallback and
lifecycle behavior, reproduced because the earlier "cleaner" version of this facade turned
out to be an invented rule this module's own purpose - exposing authoritative EDOPro
card-data semantics - gives it no license to invent.

---

## Decision 5 — Card code 0 is a load failure, not data

**`CardDatabase::load_database()` rejects a file containing a `datas` row with `id = 0`,
atomically (the whole file fails, matching Decision 3), rather than storing it as
`CardCode::None`.**

### What upstream actually does

Nothing in `DataManager::ParseDB` special-cases `id = 0` - if a real `.cdb` row had it, it
would load like any other code. But 0 is not a value real `.cdb` data uses: it is
upstream's own synthesized "not a real card" sentinel, constructed only in memory, never
read from a database row. `CardDataC::getRealCode()` (`gframe/data_manager.h:87-90`) treats
`code == 0` as meaning "this is a dummy entry; use `alias` instead" by its own comment
("dummy entries have a code of 0 with the alias corresponding to the actual code"), and
`DeckManager::GetDummyOrMappedCardData` (`gframe/deck_manager.cpp:17-28`) is exactly that
construction: `tmp->code = 0; tmp->alias = code;` for a code the loaded catalogue does not
recognize. 0 is upstream's placeholder for "no real card", used pervasively, never a `.cdb`
primary key in practice.

### Why this module enforces it

This module's own `CardCode::None = 0` already carries the identical meaning throughout its
public API - `alias == CardCode::None` means "no alias" (`card_record.h`). If a real card
could also have `code == CardCode::None`, that convention would become ambiguous for
exactly that one card: does `alias == CardCode::None` on card 0 mean "card 0 has no alias",
or would it even be reachable, given `CardCode::None` is also `find()`'s and every consumer's
spelling for "no card"? Rejecting a code-0 row keeps `CardCode::None` an unambiguous
sentinel, consistent with both upstream's own convention and this module's public API
contract - not a guess, and not general schema validation: it is the one row shape that
would make this module's own stated invariant false.

---

## Status

`data/` builds and tests standalone (`card-database` CI job); nothing outside it depends on
it yet. `edopro_next_client` continues to build with no SQLite or card-data dependency
(`semantic-client` CI job, unchanged). Wiring this into a deck model, into search, or into
the legacy `ygoprodll` build is explicitly the next slice's problem, not this one's.

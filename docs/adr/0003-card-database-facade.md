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

Four decisions turned out to be load-bearing:

1. a new top-level module, not `client/` and not `gframe/`;
2. SQLite via CMake's own `FindSQLite3`, not the project's vcpkg bundle;
3. a stronger per-file load guarantee than `DataManager::ParseDB` actually gives;
4. locale overlay that is per-field and base-first, not `DataManager`'s whole-string-swap,
   order-independent linking.

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

## Decision 4 — Locale overlay is per-field and requires the base card first

**A locale value replaces the base value one field at a time - a non-empty locale `name`
overrides, an empty one falls back to the base `name`, and likewise independently for
`text` and each of the sixteen auxiliary strings. A locale row for a card code not already
loaded via `load_database()` is ignored.**

### What upstream actually does

`DataManager::GetStrings()` (`gframe/data_manager.h:111`) returns the *entire* locale
`CardString` if a locale pointer is set at all, or the entire base `CardString` if it is
not - never a merge of the two. So a locale entry that exists but has an empty `name`
column makes `GetName()` return `"???"`, not the base-language name, because the whole
locale struct was selected over the whole base struct field-for-field-indiscriminately.
Only the sixteen `desc[]` strings get per-slot fallback (`CardDataM::GetDesc`,
`gframe/data_manager.h:116`), and that inconsistency - str1..16 fall back per slot, `name`
and `text` do not - is the shape of one implementation choice (a nullable pointer to a
whole `CardString`, chosen so `ClearLocaleTexts()` can drop every locale string in one
pass over `indexes`) leaking into what looks like a data contract, not a documented locale
format. `ParseLocaleDB`/`ParseDB`'s shared `indexes` map also links a locale row to a base
row whichever order they arrive in, which upstream's real startup path
(`gframe/data_handler.cpp:28`'s `LoadDatabases()`, then `gframe/game.cpp:2647`'s locale load
afterward) never actually exercises in reverse.

### Why this module diverges, twice

**Per-field fallback.** Extending the `desc[]` slots' own per-slot fallback to `name` and
`text` too is not a new rule; it is the *one* consistent rule where upstream has two
(Decision framing: "where this model reproduces a legacy quirk, that is deliberate and
marked; where it diverges, the divergence is named and justified" -
[semantic-model.md](../architecture/semantic-model.md)). A caller of this facade has no way
to observe upstream's internal pointer-swap mechanism and no reason to want its
side effect.

**Base-before-locale only.** Verified against the two real call sites
(`data_handler.cpp:28`, `game.cpp:2647`/`:4001`): every real startup loads every base
database, then loads locale data on top. Order-independent linking is real capability in
`DataManager` but dead code in practice. Implementing it here would mean carrying a second,
pending-link data structure to support an ordering nothing in this project's own source
ever uses. If a future caller needs locale-before-base, that is a real, then-justified
reason to revisit this decision - not a hypothetical to build against now.

---

## Status

`data/` builds and tests standalone (`card-database` CI job); nothing outside it depends on
it yet. `edopro_next_client` continues to build with no SQLite or card-data dependency
(`semantic-client` CI job, unchanged). Wiring this into a deck model, into search, or into
the legacy `ygoprodll` build is explicitly the next slice's problem, not this one's.

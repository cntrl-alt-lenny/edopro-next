# The card database facade

What a Project Ignis `.cdb` file actually contains, exactly how upstream's `DataManager`
turns a row into a card, and how `data/` reads the same data into
`edopro_next::data::CardRecord` - reproducing every genuine schema semantic, and leaving
out only what exists solely because `DataManager` is an Irrlicht-era, globally-mutable
client component.

Everything below was read from source at upstream `54ea755a` (`docs/UPSTREAM.md`'s pinned
base commit; unchanged since). Line numbers are given for orientation and will drift with
upstream merges; the file and the function are the durable references, per the convention
[semantic-model.md](semantic-model.md) established.

See [ADR 0003](../adr/0003-card-database-facade.md) for *why* this is a new module with
its own SQLite dependency, rather than an addition to `client/` or `gframe/`. This document
is only about what the data means.

---

## 0. What this module is not

It is not a legality checker, not a search index, not a UI formatter, and not
`DataManager`. It does not decide whether a card is banned, does not resolve an alias to
"the" canonical printing, does not localize an attribute name to a display string, and does
not touch Irrlicht, `ocgcore`, or any global. It reports what a `.cdb` row says. Everything
downstream of that fact - legality, search, presentation - is later M3 work or a caller's
own concern.

Real `.cdb` files (Project Ignis's `BabelCDB`) are fetched at runtime, exactly like card
artwork and CardScripts, and are **not committed to this repository** - CLAUDE.md forbids
it, and this module's own tests build tiny synthetic databases at runtime for that reason
(`data/tests/test_card_database.cpp`).

---

## 1. The schema this module reads

Two tables, joined on `id`. This is not a guess at the public BabelCDB schema; it is
exactly the column list `DataManager::ParseDB`'s own `SELECT` names
(`gframe/data_manager.cpp:24`):

```sql
SELECT datas.id, datas.ot, datas.alias, datas.setcode, datas.type, datas.atk, datas.def,
       datas.level, datas.race, datas.attribute, datas.category,
       texts.name, texts.desc, texts.str1, ..., texts.str16
FROM datas, texts
WHERE texts.id = datas.id
ORDER BY texts.id;
```

`data/`'s own query (`data/src/card_database.cpp`'s `kSelectStmt`) is character-for-character
the same column list, in the same order, differing only in whitespace.

### 1.1 The join is an inner join, on purpose

A card row present in `datas` with no matching `texts` row - or vice versa - satisfies
neither side of `WHERE texts.id = datas.id` and is not returned by the query at all. This
module reproduces that exactly: `CardDatabase::load_database()` never sees, and therefore
never loads, a card missing from either table. This is real schema behaviour, not an
oversight this module papers over - a `.cdb` entry without text data is not a usable card
either way.

### 1.2 The locale schema is `texts` alone

`DataManager::ParseLocaleDB`'s query (`gframe/data_manager.cpp:28`) reads only
`id,name,desc,str1..str16` from `texts` - no `datas` table at all, because a locale file
supplies translated strings, not card mechanics. `CardDatabase::load_locale()`'s query
matches it exactly. See §4.

---

## 2. Field by field

Column indices below are the `SELECT`'s own order (0-based), matching both
`sqlite3_column_*`'s `iCol` parameter in `DataManager::ParseDB` and in
`data/src/card_database.cpp`'s `decode_row()`.

| # | Column | `CardRecord` field | Notes |
|---|---|---|---|
| 0 | `datas.id` | `code` | The passcode. Primary key. |
| 1 | `datas.ot` | `scope` | Bitmask: `SCOPE_OCG`/`SCOPE_TCG`/... (`gframe/data_manager.h:21-34`). Opaque here - legality is not this module's job. |
| 2 | `datas.alias` | `alias` | 0 (`CardCode::None`) means no alias. |
| 3 | `datas.setcode` | `setcodes` | Packed; see §2.1. |
| 4 | `datas.type` | `type` | Bitmask: `TYPE_MONSTER`/`TYPE_SPELL`/`TYPE_LINK`/... Opaque, except for one read at load time - see §2.2. |
| 5 | `datas.atk` | `attack` | Signed. `-1`/`-2` are real "?" values. |
| 6 | `datas.def` | `defense` **or** `link_marker` | See §2.2. |
| 7 | `datas.level` | `level`, `left_scale`, `right_scale` | Packed; see §2.3. |
| 8 | `datas.race` | `race` | 64-bit bitmask; see §2.4. |
| 9 | `datas.attribute` | `attribute` | Bitmask. |
| 10 | `datas.category` | `category` | Opaque; consumed by upstream's search filters (`gframe/menu_handler.h`, `gframe/deck_con.cpp`), not by card mechanics. |
| 11 | `texts.name` | `name` | UTF-8. |
| 12 | `texts.desc` | `text` | UTF-8. Named `text`, not `desc`, in `CardRecord` - see the field's own doc comment in `card_record.h` for why. |
| 13-28 | `texts.str1`..`str16` | `strings[0..15]` | UTF-8. Per-card meaning assigned by CardScripts, not by the schema. |

### 2.1 Setcode: a packed uint64, unpacked to non-zero slots only

`DataManager::ParseDB:127-136` reads `datas.setcode` as one `uint64_t` and unpacks it into
up to four 16-bit values, one per 16-bit slot, low slot first:

```cpp
uint64_t setcodes = sqlite3_column_int64(pStmt, 3);
for(int i = 0; i < 4; i++) {
    uint16_t setcode = (setcodes >> (i * 16)) & 0xffff;
    if(setcode)
        cd.setcodes.push_back(setcode);
}
```

A zero slot is skipped, not preserved as a placeholder - a card with setcodes in slots 0
and 2 produces a two-element list, not four elements with two zeros. `data/`'s
`decode_row()` does the identical unpack.

**What is deliberately not reproduced:** upstream then appends a trailing `0` and points a
raw `uint16_t*` (`setcodes_p`) at the vector's data (`ParseDB:133-136`) - a C-array
null-terminator, because `CardDataC::setcodes_p` is handed to `ocgcore`'s C API
(`OCG_CardData`) as a null-terminated array. `CardRecord::setcodes` is a plain
`std::vector<std::uint16_t>` with no sentinel; this module has no C API to feed, and a
sentinel value in a C++ container's data is exactly the kind of ocgcore-interop plumbing
this facade has no reason to carry.

### 2.2 Link monsters: the defense column is not a defense

`DataManager::ParseDB:140-144`:

```cpp
if(cd.type & TYPE_LINK) {
    cd.link_marker = cd.defense;
    cd.defense = 0;
} else
    cd.link_marker = 0;
```

For a card whose `type` includes the Link bit, the raw `datas.def` value is not a defense
stat at all - it is the link-marker bitmask (which of the eight link-arrow positions this
card has), and a Link monster's real defense is always 0 by the rules of the game. This
module reproduces the identical branch, reading the Link bit's value
(`TYPE_LINK = 0x4000000`) from `ocgcore`'s own public header
(`ocgcore/ocgapi_constants.h:58`) as a single, cited constant private to
`data/src/card_database.cpp` - not copied from `gframe/`, and not exposed in `data/`'s
public API, because a caller of this facade needs to know the *result*
(`defense`/`link_marker`), never the bit that produced it.

### 2.3 Level and Pendulum scale share one packed int - and `level`'s destination is unsigned

`DataManager::ParseDB:146-153`:

```cpp
int level = sqlite3_column_int(pStmt, 7);
if(level < 0)
    cd.level = -(level & 0xff);
else
    cd.level = level & 0xff;
cd.lscale = (level >> 24) & 0xff;
cd.rscale = (level >> 16) & 0xff;
```

The low byte is the level/rank magnitude; the sign of the *whole* packed value, not of the
low byte alone, decides the sign of the intermediate result; bits 16-23 and 24-31 are the
right and left Pendulum scale.

**The destination type is the load-bearing fact here, and it is easy to misread this snippet
without checking it.** `level` (the local variable) is `int`, so `-(level & 0xff)` is a
genuine negative `int` when the packed value is negative. But `cd.level` - the field the
result is assigned into - is declared `uint32_t` in *both* `CardData` and `CardDataC`
(`gframe/data_manager.h:44-69`), not `int32_t`. Assigning a negative `int` to a `uint32_t`
field is well-defined C++ (reduction modulo 2³²), so the value `CardDataC::level` actually
holds after this line, for a card whose intermediate result is `-7`, is
`static_cast<std::uint32_t>(-7)` = `0xfffffff9` = `4294967289` - not `-7`. This is not a
storage-layout accident either: `DataManager::CardReader` (`gframe/data_manager.cpp:556-560`)
`memcpy`s a `CardDataC` into an `OCG_CardData` (`ocgcore/ocgapi_types.h:38-51`), whose own
`level` field is `uint32_t` too - the rules engine's own public API receives this exact
wrapped bit pattern unchanged. And it is observable, not just internal: `gframe/deck_con.cpp`'s
level filter (`filter_lv`, itself `uint32_t`) compares `data._data.level` directly with
`<`/`<=`/`>`/`>=`, so a "negative"-encoded level participates in deck search as an enormous
unsigned number, not a small negative one - exposing a signed `-7` from this facade would
make its data disagree with upstream's own observable filtering behaviour, not just its
storage type. `CardRecord::level` is therefore `std::uint32_t`, and
`data/src/card_database.cpp` computes the same signed intermediate result upstream does, then
converts it to `std::uint32_t` explicitly at the same point upstream's implicit assignment
does - reproducing the bit pattern, not the mathematically "cleaner" signed value.

Pendulum scale is unaffected by this: `(level >> 24) & 0xff` and `(level >> 16) & 0xff` are
already small non-negative `int` values (0-255) by the time they are computed - the `& 0xff`
mask discards any sign-extension bits before the result is ever assigned anywhere - so
converting them to `uint32_t` changes nothing. `left_scale`/`right_scale` stay `std::uint32_t`
in this record, decoded with the same signed arithmetic right shift upstream uses (well-defined
in C++20, which is what `level` is decoded with here) - not a value reinterpreted as unsigned
first, which would change the result for a negative packed value.

**A consequence worth stating plainly, verified by tracing the formula rather than assumed:**
because the sign of the intermediate result comes from the *whole* packed int, a negative
level is not simply `-(desired level)`. To reach an intermediate `-7` (stored as `4294967289`),
the packed column value must be `-249` (`-249`'s bit pattern is `0xFFFFFF07`, so
`raw & 0xff == 7`, giving `-7` before the unsigned conversion). And because a negative packed
value's upper three bytes are all `0xFF` by sign extension, `left_scale`/`right_scale` come
out as `255` for *any* negative-level card - a mechanical side effect of the shared packing,
not a second, independent encoding. `data/tests/test_card_database.cpp`'s
`negative_level_unpacks_bit_for_bit_like_upstream` fixes this exact case (`level = -249` in
the synthetic row, `record->level == static_cast<std::uint32_t>(-7)`) so a future change to
this formula cannot silently start producing a different number for the same bytes. Callers
should treat `left_scale`/`right_scale` as meaningful only when `type` carries the Pendulum
bit; this module does not make that judgement itself, matching how it leaves every other
`type`-flag interpretation to the caller.

### 2.4 Race is a genuine 64-bit value

`DataManager::ParseDB:154` reads `datas.race` with `sqlite3_column_int64`, and
`CardDataC::race` is itself `uint64_t` (`gframe/data_manager.h:64`). A race value using bit
32 or above is current schema data, not a forward-compatibility extension this module
invented - the column has always been read as 64 bits.

---

## 3. Loading multiple databases: last file wins, in full

`DataManager::ParseDB:111` reads:

```cpp
auto ptr = &cards[code];
```

`cards` is keyed by card code. `operator[]` returns the existing slot if `code` was already
loaded by an earlier file, or creates one if not - either way, every field of that slot is
then unconditionally overwritten with the row just read. There is no per-field merge and no
"first database wins": the last `LoadDB` call to mention a given code determines that
card's data in full, and a code unique to an earlier file is untouched by a later file that
never mentions it.

`CardDatabase::load_database()` reproduces exactly this contract - `std::map::insert_or_
assign` on the live catalogue, once the whole new file has parsed successfully (see §5) -
verified by `data/tests/test_card_database.cpp`'s
`later_database_completely_overwrites_a_duplicate_code`, which checks both halves: the
shared code reflects only the second file, and the code unique to the first file survives.

This matches upstream's real loading order too:
`DataHandler::LoadDatabases()` (`gframe/data_handler.cpp:28`) loads `./cards.cdb` first,
then every `./expansions/*.cdb`, then zip-archived `.cdb` files - each a `LoadDB` call
capable of overwriting anything the previous ones defined.

---

## 4. Locale: a separate, clearable layer

`CardDatabase` keeps three maps, not one - `base_text_` (what `load_database()` loaded,
untouched by any locale operation), `locale_text_` (the active locale layer), and
`records_` (the effective view `find()`/iteration return, recomputed from the other two by
`resolve_text()` whenever either changes). Full rationale for this shape, and for the bug it
replaces, is in
[ADR 0003, Decision 4](../adr/0003-card-database-facade.md#decision-4-locale-is-a-separate-clearable-overlay-layer-with-upstreams-actual-fallback-rules).
This section states the resulting externally observable rules.

### The lifecycle

Traced from `Game::ApplyLocale` (`gframe/game.cpp:3987-4020`):

```
load base databases
    -> load_database() one or more times
selected locale begins
    -> clear_locale()                          (ClearLocaleTexts, unconditional)
    -> load_locale() one or more times, for that locale's files
effective strings now reflect that locale
user changes locale (or returns to the base language)
    -> clear_locale()                          (ClearLocaleTexts, unconditional - even
                                                 when returning to base, which loads nothing)
    -> load_locale() one or more times for the new locale, or none at all
```

`clear_locale()` is the data-layer half of `ClearLocaleTexts()`
(`gframe/data_manager.cpp:46-54`): it discards `locale_text_` entirely and restores every
touched code's `name`/`text`/`strings` to its `base_text_` value. It is harmless and
deterministic when no locale is active.

### name/text: linked as a pair, not per field

Once any `load_locale()` call has mentioned a code at all (since the last `clear_locale()`),
that code's `name` and `text` come from the locale layer **as a pair** - even if one or both
are empty - and do not fall back to the base value individually. This matches
`CardDataM::GetStrings()` (`gframe/data_manager.h:111-115`), which returns the whole linked
`CardString` or the whole base one, never a mix. `DataManager::GetName()`'s separate
`"???"`-on-empty placeholder is not reproduced - that is a presentation default layered on
top of `GetStrings()`, not part of the `CardString` it selects, and this module's job stops
at reporting what the selected `CardString` actually holds.

### The sixteen auxiliary strings: per-slot fallback, deliberately different

Each of `strings[0..15]` falls back to its own base value independently, exactly when the
locale layer's same slot for that code is empty - matching `CardDataM::GetDesc()`
(`gframe/data_manager.h:116-124`). This is *not* the same rule as name/text above, and this
module does not unify them: that unification was the invented behavior an earlier version
of this facade shipped, and external review of the resulting bug is why Decision 4 exists in
its current form.

### Multiple files in one active locale: last file wins, in full

A later `load_locale()` call for a code the active locale already touched **completely
replaces** that code's locale entry, field by field, even with an empty value - the same
last-file-wins rule §3 documents for the base layer, applied here to `locale_text_`.
`DataManager::ParseLocaleDB` (`gframe/data_manager.cpp:180-228`) reuses `locales[code]`
across every file loaded into one active locale, and its `GetWstring` helper
(`:77-96`) unconditionally writes each column - explicitly clearing the target string when a
column is empty, never leaving a prior file's value in place.

This is a different operation from switching locales: loading two files for the *same*
active locale accumulates into one overlay (last file wins per code); calling
`clear_locale()` between two locale loads discards the first overlay entirely before the
second begins. `data/tests/test_card_database.cpp` keeps these as separate tests
(`later_locale_file_in_the_same_active_locale_overwrites_the_earlier_one` vs.
`switching_locale_after_clear_does_not_leak_the_previous_locale`) precisely so the two are
never conflated.

### A locale row for an unknown code is still ignored

Unchanged from the original decision: a `load_locale()` row for a code not already present
via `load_database()` is ignored, not queued for a base row that might arrive later. Verified
against the two real call sites (`gframe/data_handler.cpp:28`, `gframe/game.cpp:2647`/
`:4001`): every real startup loads every base database before any locale data, so
upstream's own order-independent linking is real `DataManager` capability but dead code in
practice.

---

## 5. Failure policy: read-only, and atomic against the catalogue

Every open is `SQLITE_OPEN_READONLY` (`data/src/card_database.cpp`'s `open_readonly()`) -
this module never creates, never writes, and a missing path fails to open rather than
silently producing an empty database.

A missing file, a file that is not a SQLite database, and a valid SQLite file missing a
required table or column all fail at the same two points upstream's own `ParseDB`/
`ParseLocaleDB` would: `sqlite3_open_v2` (missing file / unreadable) or `sqlite3_prepare_v2`
(anything that makes the fixed column list in §1 not resolve - SQLite validates a
database's header lazily, on first real access, which is why an invalid-but-openable file
still fails here and not at open time). Every failure path returns a `LoadResult` with
`ok == false` and a non-empty, diagnosable `error` string; none of them touch `base_text_`,
`locale_text_`, or `records_` at all, in either `load_database()` or `load_locale()`
independently (each stages into a private local map first, per §3/§4), which is a
**stronger** guarantee than `DataManager::ParseDB`/`ParseLocaleDB` provide - see
[ADR 0003, Decision 3](../adr/0003-card-database-facade.md#decision-3-a-load-is-atomic-against-the-catalogue-which-is-stronger-than-upstream)
for why upstream's own version of this can leave a catalogue part-old, part-new, part
neither, and why this module does not copy that. `clear_locale()` performs no filesystem or
SQLite operation - it only discards in-memory state and recomputes `records_` from
`base_text_` - so it has no `LoadResult`-style failure state to report and is not a
`LoadResult`-returning operation; it is not declared `noexcept`, though, since the string
copies that restore each touched code's text can themselves allocate.

`data/tests/test_card_database.cpp` covers all of: a missing file, a non-SQLite file, a
`.cdb`-shaped-but-wrong file with no `datas`/`texts` tables at all, a schema missing exactly
one required column, that a failed `load_database()` leaves a previously loaded catalogue's
size and contents completely unchanged (with a subsequent successful load still working
normally afterward), and separately that a failed `load_locale()` leaves the *currently
active locale* - not just the base layer - completely unchanged
(`failed_locale_load_leaves_the_active_locale_unchanged`).

### Pointer/reference stability

`find()` returns a pointer into `records_`, never into a temporary, and no operation in this
module ever erases a loaded card - so a pointer once obtained stays valid for the lifetime of
the owning `CardDatabase`. It is not a snapshot, though: a later `load_database()`,
`load_locale()`, or `clear_locale()` call can update the `CardRecord` that pointer refers to
in place (a base field changing on reload, or the text fields changing because the active
locale changed). A caller that needs a point-in-time value should copy `*find(code)` rather
than hold the pointer across a call that can mutate this catalogue.

---

## 6. Text encoding

`DataManager` stores `std::wstring` and branches on `WCHAR_MAX` to decide whether to read
SQLite's UTF-16 accessor (`sqlite3_column_text16`) or its UTF-8 one plus a manual decode
(`gframe/data_manager.cpp:77-96`) - because it needs a wide string on Windows and a 32-bit
`wchar_t` elsewhere. `CardRecord` stores `std::string`, UTF-8 throughout, using SQLite's
`sqlite3_column_text`/`sqlite3_column_bytes` unconditionally - the same accessor upstream's
own non-Windows branch already uses, just without the wstring conversion this module has no
reason to perform. See `card_record.h`'s own note on why legacy wide-string/`epro::*` types
do not appear in this module at all.

---

## 7. Card code 0

`CardDatabase::load_database()` rejects a file whose `datas` table has a row with `id = 0` -
the whole file fails (§5's atomicity), rather than that row being stored. Nothing in
`DataManager::ParseDB` special-cases this; a real `.cdb` row with id 0 would load like any
other. But 0 is not a value real `.cdb` data uses in practice - it is upstream's own
synthesized "not a real card" sentinel, constructed only in memory:
`CardDataC::getRealCode()` (`gframe/data_manager.h:87-90`) reads `code == 0` as "this is a
dummy entry, use `alias` instead" by its own comment, and
`DeckManager::GetDummyOrMappedCardData` (`gframe/deck_manager.cpp:17-28`) is exactly that
construction (`tmp->code = 0; tmp->alias = code;`) for a code the loaded catalogue does not
recognize.

This module's own `CardCode::None = 0` already carries that identical meaning throughout its
public API (`alias == CardCode::None` means "no alias"). Enforcing that a loaded card can
never have `code == CardCode::None` keeps that sentinel unambiguous, rather than merely
documenting an invariant the loader did not actually check. See
[ADR 0003, Decision 5](../adr/0003-card-database-facade.md#decision-5-card-code-0-is-a-load-failure-not-data).

---

## 8. What is out of scope for this slice, on purpose

- **Search.** `category`, `type`, `race`, and friends are carried verbatim; indexing or
  filtering them is the "fast search" M3 item, not this one.
- **Legality.** `scope` (`ot`) is exposed as a raw bitmask. Whether a scope value makes a
  card playable in a given format is a deck/duel-rules decision this module does not make -
  compare `gframe/deck_manager.cpp`'s `CHECK_UNOFFICIAL`, which is exactly that kind of
  decision, made above this layer.
- **Alias resolution.** `alias` is carried verbatim. Deciding what a nonzero alias *means*
  for banlist matching (`CardDataC::IsInArtworkOffsetRange`, `gframe/data_manager.h:76`) is
  deck/banlist logic, not a database fact, and is not reproduced here.
- **The Irrlicht `IReadFile` load path.** `DataManager::LoadDB(irr::io::IReadFile*)`
  supports reading a `.cdb` out of a packed archive via a custom SQLite VFS
  (`gframe/ireadfile_sqlite.h`). This module reads real filesystem paths only
  (`std::filesystem::path`); archive-backed loading, if ever needed, is a caller-side
  concern layered on top, not something this facade's SQLite boundary should know about.
- **Global SQLite configuration.** `DataManager`'s constructor calls
  `sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)` and registers a process-wide custom VFS
  (`gframe/data_manager.cpp:32-36`). This module does neither - mutating SQLite's global
  threading mode or VFS registry from a library used by potentially unrelated callers in the
  same process is exactly the kind of global state CLAUDE.md's module boundaries exist to
  avoid, and this module's own tests (which create and read SQLite files directly,
  concurrently with nothing) do not need it either.
- **Which locale is active, and where its files live.** `clear_locale()`/`load_locale()` are
  a pure data-layer state transition - discard the overlay, or add to it from one file.
  There is no locale name, language code, or directory-scanning logic here (compare
  `Game::ApplyLocale`'s `Utils::FindFiles(locale, {"cdb"})` over `./config/languages/<code>/`,
  `gframe/game.cpp:4000`): choosing which files correspond to which language, and calling
  `clear_locale()` at the right moment, is a caller's job.

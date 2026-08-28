# ADR 0004 — A presentation-independent deck model and `.ydk` codec

## Context

M3's second item (`docs/ROADMAP.md`) is "Deck model reading and writing `.ydk`" - a value
type for a Yu-Gi-Oh deck, plus the ability to read and write Project Ignis's `.ydk` text
format, with no legality, search, or QML deck-builder work bundled in. `docs/architecture/
deck-model.md` is the source-verified account of what upstream's `.ydk` parser and writer
actually do; this ADR is the narrower record of the four choices in that account that were
genuinely debatable, not merely "what upstream does" restated.

## Decision 1 — `Deck` stores `data::CardCode` values, lives in `data/`, and needs no `CardDatabase`

Upstream's own `Deck` (`gframe/deck.h`) is `std::vector<const CardDataC*>` × 3 - a pointer
into a live, globally-loaded catalogue (`gDataManager`). Constructing one requires that
catalogue to already contain every card in the deck.

### Options considered

1. **Store `const CardRecord*` into a `CardDatabase`** - the closest analogue to upstream's
   own shape. Rejected: it reproduces the exact coupling (a deck cannot be represented, let
   alone parsed, without a loaded database) the M3A facade's own separation from `client/`
   was built to avoid one layer up, and it makes "parse a `.ydk` with no database loaded"
   (a real scenario - see `docs/architecture/deck-model.md`§6, "unknown non-zero codes")
   impossible to express at all rather than just a corner case.
2. **Store raw `std::uint32_t` codes** - no dependency on anything, but reintroduces exactly
   the "which module's numeric card-code type is this" ambiguity `data::CardCode`
   (`data/include/edopro_next/data/card_code.h`) exists to resolve, and loses the `None`
   sentinel's type-level meaning that Decision 3 depends on.
3. **Store `data::CardCode`** (chosen). Already a defined, documented, project-owned type
   with an established "not a real card" sentinel (`CardCode::None`); reusing it costs
   nothing new and gains a type-checked distinction between "a card identity" and "any
   `uint32_t`".

### Why `data/`, not a new top-level module or `client/`

`data/` already owns presentation-independent card data and already defines `data::CardCode`
- putting the deck model here reuses that type directly with no cross-module dependency to
introduce. A new top-level module would exist solely to hold a three-field struct and a
parser, for no boundary benefit `data/` doesn't already provide. `client/` is the duel-
protocol semantic model (`docs/architecture/semantic-model.md`) and must stay that - a deck
list belongs to deck construction, not to an in-progress duel, and coupling `client/` to
deck/card-database concerns would violate the separation `client/`'s own design deliberately
maintains.

### The cost, and why it is acceptable

`edopro_next_deck` (the new CMake target, `data/CMakeLists.txt`) is a **separate** static
library from `edopro_next_data`, specifically so it does not link SQLite - reading or
writing a `.ydk` never requires a card database, and the module boundary needing to prove
that (via a second, independent link graph - `data/tests/test_deck_ydk.cpp`'s own
executable links only `edopro_next_deck`) rather than merely asserting it in a comment is
the whole point of Decision 1. The cost is one more CMake target and one more test
executable in the same directory; both reuse the existing generic `tests/test_support.cpp`
harness (`data/CMakeLists.txt`), so the marginal build-system cost is small.

## Decision 2 — The codec follows the file's own section markers; type-based reclassification is out of scope

Upstream has two distinct behaviours for turning `.ydk` text into sections
(`docs/architecture/deck-model.md`§3): `LoadCardList` with `extralist == nullptr` (file
markers inert; a later, `CardDatabase`-dependent step, `LoadDeck`, classifies every code as
Main or Extra by its actual card type), and `LoadCardList` with `extralist` provided (file
markers determine an initial split, which `LoadDeck` **still** partially overrides
afterward for any Fusion/Synchro/Xyz/Link/Ritual card it finds in the "wrong" section).
Neither upstream mode is achievable without a loaded database.

### Options considered

1. **Reproduce `LoadDeck`'s type-based classification inside the parser** - would give the
   closest match to `separated = false` behaviour, but requires linking `edopro_next_deck`
   against `edopro_next_data`, defeating Decision 1, and conflates "read this text" with
   "know Yu-Gi-Oh's card-type rules" in one function - the reclassification is real game-
   knowledge (what counts as an Extra-Deck card), not file-format knowledge.
2. **Follow the file's explicit markers only, treat `#extra`/`!` as always functional**
   (chosen). `parse_ydk` behaves as if `extralist` were always supplied - `"#main"` is
   inert, `"#extra"` and any `!`-prefixed line are always functional, exactly as
   `docs/architecture/deck-model.md`§2.3/§3 documents - and no card-type check of any kind
   runs. This needs no database, matches what the deck builder's own primary read/write
   flows already produce and consume in practice (`gframe/deck_con.cpp`'s file-open and
   drag-drop paths use `separated = true`), and represents exactly what the file says,
   nothing inferred.
3. **Auto-detect Extra Deck cards by code range or heuristic, without a real database** -
   rejected outright: inventing a classification upstream itself does not use anywhere would
   silently disagree with the genuine rule in some case, which is worse than not classifying
   at all.

### Consequence

`edopro_next::data::Deck`'s section membership, for a file with explicit markers, matches
what a human (or the deck builder) put there - not necessarily what `LoadDeck`'s type check
would move it to. A future layer with a `CardDatabase` available can implement that
reclassification as an explicit `Deck -> Deck` transformation on top of this codec; that is
a strictly better place for it than inside text parsing, and this decision deliberately
leaves room for it without prejudging its design.

## Decision 3 — Card code 0 is excluded during parsing, not stored

`docs/architecture/deck-model.md`§5 traces both of upstream's own behaviours for a code that
resolves to "not a real card" - `LoadCardList` stores a literal `0` line with no special
case at all; the downstream, database-dependent `LoadDeck` either drops it (default mode) or
keeps it as an untyped placeholder card (separated mode) - and finds that they disagree with
each other, mode by mode.

### Options considered

1. **Store it, matching raw `LoadCardList`** - simplest, but stores `CardCode::None` as an
   ordinary deck entry, contradicting that value's own established meaning everywhere else
   in `data/` (`card_code.h`; `CardDatabase::load_database()` refuses to load a `.cdb` row
   with `id = 0` for the identical reason, `docs/architecture/card-database.md`§7). Every
   consumer of `Deck` would need its own "is this actually `None`" special case to stay
   correct - reintroducing, at the value-model level, precisely the kind of sentinel-
   checking `CardCode::None`'s type-level meaning exists to avoid needing.
2. **Reject the whole file as malformed** - too strong: nothing about a stray `0` line makes
   the rest of the file unreadable, and upstream itself never treats it as fatal in either
   mode.
3. **Exclude it from the resulting sections, report it via a non-fatal diagnostic** (chosen).
   Matches neither upstream mode exactly, and is not intended to: it is the choice
   consistent with this project's own `CardCode::None` sentinel, made once, at the one layer
   (parsing) where "this text does not represent a real card" is legitimately a text-level
   fact, independent of which downstream policy (§Decision 2) would otherwise apply.

An overflowed value that truncates to exactly `0` (`4294967296 -> 0` via
`static_cast<uint32_t>`, itself matching upstream's own truncation - `docs/architecture/
deck-model.md`§2.5) is excluded by the identical rule, since after truncation it is
genuinely indistinguishable from a literal `"0"` line - not a separately invented special
case.

## Decision 4 — The writer never depends on a card database or UI state

`MakeYdkEntryString` (`gframe/deck_manager.cpp:469-472`) optionally emits a
`"# <CardName>\n"` line before each code, driven by `gGameConfig->addCardNamesToDeckList`
and a `gDataManager->GetName(code)` lookup; `SaveDeck` always emits `"#created by
<nickname>"`, sourced from `mainGame->ebNickName`, live UI state.

### Options considered

1. **Reproduce both, taking a `CardDatabase*`/name-lookup callback and a nickname string** -
   rejected: couples the writer to a database dependency (breaking Decision 1) for a feature
   that (`docs/architecture/deck-model.md`§4) has zero effect on loading - any `#`-prefixed
   line is just a comment to every version of the parser. It is presentation/export sugar,
   not deck semantics, and does not belong on this codec's core boundary.
2. **Drop the name-comment feature; expose creator as an optional, caller-supplied string**
   (chosen). `YdkWriteOptions::creator` is a plain `std::optional<std::string>` this codec
   never sources a value for itself - the *mechanism* for upstream-compatible output is
   available (the exact line it produces matches `SaveDeck`'s own format), but the *policy*
   of "what nickname" is entirely the caller's, keeping `data/` free of any notion of "the
   current user". A caller wanting upstream's name-commented export can build it on top,
   using its own `CardDatabase::find(code)->name` before calling `serialize_ydk`, without
   this codec needing to support it directly.

## Status

Accepted. Implemented in `data/include/edopro_next/data/deck.h`, `data/include/
edopro_next/data/ydk.h`, `data/src/ydk.cpp`, and pinned by `data/tests/test_deck_ydk.cpp`.

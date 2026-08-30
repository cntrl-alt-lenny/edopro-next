# Deck legality / policy foundation (`policy/`)

A presentation-independent module implementing the deck-validation semantics
`gframe/deck_manager.cpp`'s `DeckManager::CheckDeckSize`/`CheckDeckContent`/
`CheckCards` and `gframe/generic_duel.cpp`'s `GenericDuel::PlayerReady`
implement upstream - read from source, not assumed, and cited line-by-line
below. It exists so that a future deck-builder UI can show whether a deck is
valid without QML, `ui/`, or `data/` ever computing that themselves - see
CLAUDE.md's "THE RULES ENGINE MUST NOT BECOME THE UI, THE UI MUST NOT
IMPLEMENT GAME RULES" and [ADR 0007](../adr/0007-deck-legality-policy-module.md)
for the full module-boundary reasoning.

## 0. What this module is not

- Not a legality *UI*. Nothing here is wired into `ui/`'s QML in this slice.
- Not the complete upstream rule set. It reproduces `CheckDeckSize`,
  `CheckDeckContent` and `CheckCards` specifically - not every rule
  `gframe/` enforces anywhere (e.g. it does not touch anything about an
  in-progress duel, side-decking between games, or `LoadDeck`'s own
  Main/Extra *classification* step - see §7 below for why that is a
  deliberately separate concern this module does not perform).
- Not automatic Main/Extra classification. This module validates a `Deck`
  whose section split already exists; it never moves a card between
  sections. See `docs/architecture/deck-model.md` for why classification,
  if it is ever built, is intentionally a separate `Deck -> Deck`
  transformation.
- Not a ruleset author. `ValidationPolicy` (§6) has no default value; a
  caller must supply every field explicitly. This module never invents "the"
  rules of Yu-Gi-Oh.

## 1. Where the source lives

| Concept | Upstream source | This module |
|---|---|---|
| Deck size | `gframe/deck_manager.cpp:238-258` (`CheckDeckSize`), `gframe/network.h:24-38` (`DeckSizes`) | `DeckSizePolicy`, `SectionSizeRange` (`validation_policy.h`) |
| Content validation | `gframe/deck_manager.cpp:157-236` (`CheckCards`, `CheckDeckContent`) | `validate_deck()` (`deck_validation.h`/`.cpp`) |
| Validation sequencing | `gframe/generic_duel.cpp:366-391` (`PlayerReady`) | `validate_deck()`'s own top-level order |
| Banlist file format | `gframe/deck_manager.cpp:35-108` (`LoadLFListSingle`/`LoadLFList`/`LoadLFListFolder`), `gframe/deck_manager.h:18-31` (`LFList`) | `LfList`, `parse_lflist()`, `load_lflist()` (`lf_list.h`/`.cpp`) |
| Type/scope bits | `ocgcore/ocgapi_constants.h`, `gframe/data_manager.h` (`SCOPE_*`, `TYPE_SKILL`) | duplicated `constexpr` citations in `deck_validation.cpp` |

## 2. Top-level validation order

`validate_deck()` reproduces `PlayerReady`'s own sequencing
(`gframe/generic_duel.cpp:372-383`) - `CheckDeckSize` and `CheckDeckContent`
never call each other or order themselves; `PlayerReady` is what actually
decides which runs first:

1. **Deck size** (`CheckDeckSize`) - Main, then Extra, then Side, first
   failure wins. Skill cards are subtracted from the counted Main size
   (`deck_manager.cpp:240-241`: `sizes.main != (deck.main.size() - skills)`).
   Comparison is inclusive (`network.h:31-32`'s
   `operator==(Sizes, size_t)`: `count < min` fails, `count <= max` required)
   - reproduced by `SectionSizeRange::contains()`.
2. **Unknown-card condition** - only when `content_checking_enabled`. See §7.
3. **Content validation** (`CheckDeckContent`), in its own exact internal
   order (`deck_manager.cpp:204-236`):
   1. Forbidden types, tested across Main, Extra, and Side (`:206-207`).
   2. Legend monsters, counted across **Main + Extra combined** (`:208-209`).
   3. Legend spells, Main only (`:210-211`).
   4. Legend traps, Main only (`:212-213`).
   5. Skill count > 1 in Main (`:214-215`).
   6. A null `LFList*` short-circuits here, before any per-card pass runs at
      all (`:217-218`). See §5.
   7. Main cards, in file order; then Extra; then Side (`:219-236`). Each
      card, in order: allowed-card/scope check -> section-placement check ->
      shared copy-count cap -> LFList/whitelist limitation
      (`deck_manager.cpp:157-203`'s `CheckCards`).

## 3. Copy counting and aliasing

One shared `std::map<CardCode, int>` is threaded across all three
`check_cards()` calls (Main, Extra, Side) - reproducing
`banlist_content_t ccount` being declared once in `CheckDeckContent`
(`:216`) and passed by reference into every `CheckCards` call
(`:219,227,236`). A fourth copy of the same card spread across all three
sections is caught by this shared counter, not three independent ones.

**Copy-count key**: `record->alias ? record->alias : code`
(`deck_manager.cpp:192`) - alias-preferred. An alias and its original share
one slot, hard-capped at 3 (`:195-196`), independent of any banlist.

**Banlist lookup key** (`GetLimitationIterator`, `deck_manager.h:23-30`):
**code-first**, falling back to alias only on a miss, and only if the list
is not a whitelist, or the code/alias pair is within the artwork-offset
window (`CARD_ARTWORK_VERSIONS_OFFSET = 10`,
`data_manager.h:74-85`'s `IsInArtworkOffsetRange`). This is a genuinely
*different* resolution order from the copy-count key above - both are
reproduced exactly as upstream keeps them distinct
(`policy/src/deck_validation.cpp`'s `limitation_for()` vs. its
`count_key` computation).

**Absence from a list**: for an ordinary (blacklist-style) list, a card
absent from `content` is unrestricted, beyond the universal 3-copy cap. For
a `whitelist`, absence is itself illegal (`deck_manager.cpp:199`:
`curlist->whitelist && is_end`).

## 4. Scope / `CHECK_UNOFFICIAL` - a preserved quirk, not a bug fix

`gframe/deck_manager.cpp:165`:

```cpp
#define CHECK_UNOFFICIAL(cit) if (cit->ot > 0x3) return ret.type = DeckError::UNOFFICIALCARD, ret;
```

This is a **magnitude** comparison, not a bitmask test. Applied in the
`OcgOnly`/`TcgOnly`/`OcgAndTcg` modes (`:166-179`), it means ANY card whose
scope carries a bit beyond `SCOPE_OCG|SCOPE_TCG` (0x3) - e.g. a Legend
(`0x400`), a Speed-duel card, or a Prerelease card that is *also* OCG/TCG
legal - is rejected as "unofficial" in those three modes, even though it is
genuinely OCG/TCG legal. Only `WithPrerelease` (`:180-183`) and `Any`
(`:184-186`) use a real bitwise test. `deck_validation.cpp` reproduces this
exact magnitude test (`record->scope > 0x3`) rather than "fixing" it to
`!(scope & ~0x3)` - this is upstream's own, real, observed behavior, and a
faithful reproduction of it is the whole point of this module (matching this
project's established practice elsewhere, e.g. the level-magnitude
wrap-around in `data/src/card_database.cpp`).

`ALLOWED_CARDS_ANY` performs no scope check at all (`:184-186`).

## 5. Null `LFList*` vs. a concrete "N/A" list - two different states

`gframe/deck_manager.cpp:217-218`:

```cpp
banlist_content_t ccount;
if(!lflist)
    return ret;
```

A **null** `LFList*` causes `CheckDeckContent` to return **before** any
`CheckCards` pass ever runs - scope, section-placement, the three-copy cap,
and the banlist check are **all** skipped, and the deck passes (assuming
forbidden-type/Legend/Skill already passed).

Upstream's own synthetic **"N/A" list** (`deck_manager.cpp:101-107`,
appended after every real file, `hash == 0` as its sentinel) is a
**concrete, non-null** `LFList*` with empty `content` and `whitelist ==
false`. Selecting it still runs every `CheckCards` pass in full - scope,
zone, and the 3-copy cap all still apply; only the banlist-specific
per-card limit never fires, since `content` is empty.

`ValidationPolicy::lflist` (`validation_policy.h`) models this as
`std::optional<LfList>` specifically so these two states cannot be
collapsed into one: `std::nullopt` reproduces the null-pointer
short-circuit; an `LfList` value - even a default-constructed, empty one -
reproduces the concrete "N/A" behavior. `test_deck_validation.cpp`'s
`nullLflistSkipsScopeZoneCopyCountAndBanlist` and
`concreteEmptyLflistStillEnforcesScopeZoneAndCopyCount` pin the difference
directly, using the identical zone/scope/copy-count setups for both.

## 6. `ValidationPolicy` - no implicit ruleset

Upstream receives `DeckSizes`, `forbiddentypes`, `DuelAllowedCards`, the
selected `LFList`, and `no_check_deck_content` from live host/session state
(`gframe/network.h`'s `HostInfo`, resolved in
`gframe/generic_duel.cpp:373-381`). This project's UI has no
networking/hosting session at this slice to derive them from, so
`ValidationPolicy` (`validation_policy.h`) has **no default constructor**
that would silently pick a value for any of them. A caller must choose every
field explicitly - including, notably, `lflist` (see §5) and
`rituals_belong_in_extra` (see §7). A future UI/session layer may offer
named convenience presets (e.g. "standard OCG/TCG, 40-60/0-15/0-15"); that
is explicitly deferred, not built here.

## 7. Ritual placement: a validation-time boolean, not the loader's three-state enum

`gframe/deck.h`'s loader-side `RITUAL_LOCATION::{DEFAULT, MAIN, EXTRA}`
belongs to `LoadDeck`'s **classification** step (deciding which section a
freshly-loaded card lands in) - a different concern from this module, which
validates a `Deck` whose section split already exists. `CheckDeckContent`
itself receives only a resolved **boolean**
(`deck_manager.cpp:204`: `bool rituals_in_extra`), derived in
`generic_duel.cpp:379` from a duel-rule flag
(`host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32)`).
`ValidationPolicy::rituals_belong_in_extra` models exactly that boolean, not
the three-state loader enum - this module does not implement automatic
classification at all (§0), so the loader's `DEFAULT` (Rush-conditional)
behavior has no equivalent here; a caller who needs it must resolve it to a
boolean before calling `validate_deck()`.

## 8. Unknown-card semantics - a deliberately different claim from upstream's

This project's `edopro_next::data::Deck` can legitimately contain a
`CardCode` with no matching `CardDatabase` entry (see `deck.h`/`ydk.h`).
Upstream's own `UNKNOWNCARD` is a byproduct of its *loading*/network path
and legacy dummy-card machinery (`DeckManager::GetDummyOrMappedCardData`,
`deck_manager.cpp:17-28`), not something `CheckDeckContent` computes
itself - and *which* code upstream would report, if any, depends on which
load mode produced the deck:

- `separated = false` (`extralist == nullptr`): an unresolvable code is
  **dropped** from the deck entirely, and only the **last** such code
  survives as `errorcode` (`deck_manager.cpp:350-356`) - multiple unknown
  codes collapse to one, arbitrarily.
- `separated = true`: an unresolvable code becomes a zeroed dummy
  (`type == 0`) that is **kept** in the deck, with `errorcode` never set at
  all for it (`:359-363`) - so it can silently reach `CheckDeckContent`
  itself, where its zeroed `ot`/`type` fields can produce a *different*,
  misleading error (e.g. `TCGONLY`) instead of `UNKNOWNCARD`.

`validate_deck()` does not recreate this machinery. It detects an unknown
code directly through `database.find()`, and reports the **first** one
found scanning Main (in order), then Extra (in order), then Side (in
order) - a deliberate, simple, fully-deterministic choice of this module's
own. This is **not** a claim of matching any specific upstream load mode's
`errorcode` value - it is a different, honestly-scoped claim: "this module's
own validation found this specific code unresolvable first," full stop.

## 9. LFList grammar

`parse_lflist()`/`load_lflist()` (`lf_list.h`/`.cpp`) reproduce
`DeckManager::LoadLFListSingle` (`gframe/deck_manager.cpp:35-87`) line by
line:

- `#...` and blank lines are skipped (`:50`).
- `!Name` opens a new named section, closing and emitting the previous one
  (if its hash is nonzero) first (`:52-60`). `name` is stored as plain UTF-8
  `std::string` here, not upstream's `std::wstring` - nothing in this
  module touches Irrlicht text rendering, and the underlying bytes are
  identical either way (matching `CardRecord::name`'s own UTF-8
  convention).
- `$whitelist` is a **prefix** match (`:62`: `str.rfind(key.data(), 0,
  key.size()) == 0`), not exact equality - `"$whitelistTRUE"` also matches.
  It runs unconditionally, even before any `!Name` line - which has zero
  observable effect, since the next real `!Name` line unconditionally
  resets `whitelist = false` (`:58`), and nothing is ever emitted for a
  section that never opens (`:66-67,84-85`).
- A content line before any `!Name` header is dropped silently
  (`:66-67`: `if(!lflist.hash) continue;`), exactly like a malformed one.
- A content line is `<code> <count>`, split on the **first** space
  (`:68-70`). The count field runs from that space up to (but not
  including) the first character after it that is not a digit or `-`
  (`:71-73`) - tolerating trailing text (e.g. a comment) after a valid
  count, but **not** tolerating a *second* space: a second space is itself
  "not a digit or `-`", so it immediately ends the count field, leaving
  only the first space character itself to be parsed as the count - which
  fails, dropping the whole line. This is upstream's own literal behavior,
  not a hypothetical edge case - `test_lf_list.cpp`'s
  `multipleSpacesBetweenCodeAndCountDropsTheLine` pins it.
- Code 0 short-circuits before the count is even parsed (`:76-77`) - a
  malformed count on a code-0 line is irrelevant, silently skipped, exactly
  like upstream.
- A duplicate code **overwrites** its `content` entry (`:79`,
  `std::unordered_map::operator[]`), but the hash update runs
  **unconditionally for every accepted line**, including a duplicate
  (`:80`) - so the final `content` value and the hash's accumulated history
  can genuinely disagree about how many times a code appeared.
  `duplicateCodeOverwritesContentButHashAccumulatesBoth` pins this
  algebraically (see below).
- Multiple files load in a fixed order - `./expansions/lflist.conf`, then
  `./lflist.conf`, then every `*.conf` in `./lflists/`
  (`deck_manager.cpp:97-100`) - and upstream appends its own synthetic
  "N/A" list after all of them (`:101-107`). This module's `load_lflist()`
  reads exactly one file; multi-file loading order is a caller concern this
  module does not impose (there is no equivalent of upstream's fixed,
  three-source search path here, since this project has no equivalent
  runtime convention for *which* files exist yet).

## 10. The hash-domain divergence - the one place this module diverges from upstream

`gframe/deck_manager.cpp:80`:

```cpp
lflist.hash = lflist.hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
```

The first XOR term - `(code << 18) | (code >> 14)` - is a true 32-bit
rotate-left-by-18 (18+14 == 32) with compile-time-constant shift amounts,
always well-defined.

The second term - `(code << (27 + count)) | (code >> (5 - count))` -
depends on `count`, a value parsed straight from the file
(`std::stol`, effectively an `int32_t`). **C++ requires each shift amount
to be in `[0, 31]` for a 32-bit operand** ([expr.shift]); a shift amount
outside that range, or negative, is undefined behavior - not "wraps", not
"masks to 5 bits" - genuinely undefined, regardless of what any particular
compiler happens to emit for it. Solving `0 <= 27 + count <= 31` and
`0 <= 5 - count <= 31` simultaneously gives:

```
count in [-26, 4]
```

as the **only** domain in which upstream's own expression is defined at
all. Every real banlist count in actual use (0/1/2, conventionally 3 for
"no additional restriction") sits comfortably inside this range - the
divergence below only matters for a hand-edited or adversarial file.

**This module's deliberate choice**: outside `[-26, 4]`, the content line
is rejected entirely - excluded from **both** `content` and `hash` - the
same treatment as a syntactically malformed line
(`LfListIgnoredLine`, `lf_list.h`). This is a fail-closed choice, not an
invented "safe" hash formula for a domain upstream itself never defined:
there is no such thing as "the correct hash" for a count where upstream's
own source has no defined behavior to match, so this module refuses to
compute one rather than inventing new semantics and calling them
source-equivalent. `hashUnsafeCountDomainFailsClosed`
(`test_lf_list.cpp`) pins the exact boundary (`4` and `-26` accepted; `5`
and `-27` rejected) and an extreme out-of-domain value.

## 11. Why `policy/` is a sibling of `data/`, not part of it

`data/`'s own docs repeatedly and explicitly describe legality as belonging
in "an explicitly separate, explicitly reviewed layer above `data/`" -
[card-database.md](card-database.md)'s §8: *"Whether a scope value makes a
card playable in a given format is a deck/duel-rules decision this module
does not make"*; [card-search.md](card-search.md)'s §12: *"belongs, if
built, in an explicitly separate, explicitly reviewed layer above
`data/`."* Keeping `policy/` a sibling (not a subdirectory of `data/`)
preserves `data/`'s own identity as "a data boundary, not a rules
boundary" (its own header doc comments) rather than growing its scope. See
[ADR 0007](../adr/0007-deck-legality-policy-module.md) for the full
decision, including why this target depends explicitly on both
`edopro_next_data` and `edopro_next_deck`.

## 12. Why QML never owns legality

CLAUDE.md: *"THE RULES ENGINE MUST NOT BECOME THE UI. THE UI MUST NOT
IMPLEMENT GAME RULES ... UI code never decides legality."* `validate_deck()`
is the one and only place this project computes whether a deck is valid. A
future UI layer may render a `DeckValidationError` (§ below); it must never
count copies, inspect `TYPE_*`/`SCOPE_*` bits, read a `LFList`, or decide
validity itself - exactly the same boundary `CardEntry`'s `isXyz`/`isLink`
flags already establish for presentation classification (`ui/src/
deckbuilder/card_entry.h`), extended here to policy instead of display.

## 13. Explicit non-goals of this slice

No QML/Qt wiring of any of this. No "collect every finding" validator mode
- `validate_deck()` returns the single first error, in source order, by
design (see [ADR 0007](../adr/0007-deck-legality-policy-module.md)). No
automatic Main/Extra classification. No mutation/normalization of a `Deck`.
No `.ydke`. No artwork. No structured search filters or legacy sigil
grammar. No controller work. No networking/`HostInfo` integration. No
upstream `.ydk` interoperability harness. No `gframe/`/`ocgcore/` changes.

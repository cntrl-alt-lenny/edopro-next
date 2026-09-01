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
| Content validation | `gframe/deck_manager.cpp:157-237` (`CheckCards`, `CheckDeckContent`) | `validate_deck()` (`deck_validation.h`/`.cpp`) |
| Validation sequencing | `gframe/generic_duel.cpp:366-397` (`PlayerReady`) | `validate_deck()`'s own top-level order |
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
   Comparison is inclusive (`network.h:31-32` only declares
   `operator==(Sizes, size_t)`; the actual comparison is defined in
   `netserver.cpp:18-22`: `count < min` fails, `count <= max` required)
   - reproduced by `SectionSizeRange::contains()`.
2. **Unknown-card condition** - only when `content_checking_enabled`. See §7.
3. **Content validation** (`CheckDeckContent`), in its own exact internal
   order (`deck_manager.cpp:204-237`):
   1. Forbidden types, tested across Main, Extra, and Side (`:206-207`).
   2. Legend monsters, counted across **Main + Extra combined** (`:208-209`).
   3. Legend spells, Main only (`:210-211`).
   4. Legend traps, Main only (`:212-213`).
   5. Skill count > 1 in Main (`:214-215`).
   6. A null `LFList*` short-circuits here, before any per-card pass runs at
      all (`:216-218`). See §5.
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
`OcgOnly`/`TcgOnly`/`OcgAndTcg` modes (`:166-178`), it means ANY card whose
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

`gframe/deck_manager.cpp:216-218`:

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
  (if its hash is nonzero) first (`:52-60`). `name` is an UTF-8-by-convention
  byte string (`std::string`, matching `CardRecord::name`'s own convention),
  not upstream's decoded `std::wstring` (`BufferIO::DecodeUTF8`,
  `gframe/bufferio.h`) - nothing in this module touches Irrlicht text
  rendering, so there is no reason to carry a wide string, or to decode at
  all. For **well-formed** UTF-8 input this is text-equivalent to what
  upstream's decoder would produce (a lossless round-trip) - it is **not**
  a claim that the two in-memory byte representations (`std::string` vs.
  `std::wstring`) are themselves identical. For **malformed** UTF-8, this
  is a documented divergence, not an equivalence: this module passes the
  bytes through unchanged, while upstream's decoder does not (it silently
  substitutes a NUL for an unrecognized leading byte, and abandons decoding
  the rest of the string entirely once a multi-byte sequence's continuation
  bytes run out early) - see `LfList::name`'s own doc comment
  (`lf_list.h`) for the full account.
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

## 10. Conversion order first, hash-domain divergence second

### 10.1 Upstream's own conversion order is reproduced exactly - for both fields

`gframe/deck_manager.cpp:75-78`:

```cpp
auto code = static_cast<uint32_t>(std::stoul(str.substr(0, p)));
if(code == 0)
    continue;
auto count = static_cast<int32_t>(std::stol(str.substr(p, c)));
```

Both fields are parsed at `std::stoul`/`std::stol`'s full native width, then
narrowed via an unconditional `static_cast` - **with no range check in
between**, on either field. That narrowing is source semantics, not a
hazard this module corrects for: `parse_lflist()` performs the identical
two-step parse-then-narrow sequence, in the identical order, for both
`code` and `count` (`lf_list.cpp`). An earlier revision of this module
inserted a range check on the *wide*, not-yet-narrowed value before
either narrowing cast ran - this was a genuine overreach beyond the one
authorized divergence (§10.2 below), silently rejecting lines upstream's
own narrowing would have accepted (see §10.3). It has been removed; this
module's only intentional difference from upstream's conversion behavior
is the hash-safety check in §10.2, and that check applies strictly *after*
narrowing.

**The code field has no divergence of its own at all.** There is no
shift-undefined-behavior risk from a code value by itself - `code` is only
ever the *operand* being shifted (`code << shift_amount`), never the shift
amount itself, and a shift of a `uint32_t` operand by any amount in
`[0, 31]` is well-defined regardless of the operand's value. So an
out-of-`uint32_t`-range code (e.g. `4294967297`, which narrows to `1`) is
reproduced exactly as upstream's own `static_cast<uint32_t>(std::stoul(...))`
would produce it - including applying that line's restriction to whatever
card the narrowed code happens to identify, which can be a real, different,
legitimate card. This is upstream's own real (if surprising) observable
behavior, not a bug this module fixes; see `data/src/ydk.cpp`'s identical
`std::stoul` call and `docs/architecture/deck-model.md`§2.5 for this
project's own established precedent of reproducing this exactly, including
its ABI-dependent edge cases (§10.3), rather than "fixing" it into a
platform-independent grammar.

### 10.2 The one deliberate hash-semantic divergence: refusing to hash a narrowed count that would be shift-undefined-behavior

(A second, separate, narrower divergence - in section-name encoding, not in
content/hash parsing - is documented in §9's own `!Name` bullet; the two
are independent and this section covers only the first.)

`gframe/deck_manager.cpp:80`:

```cpp
lflist.hash = lflist.hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
```

Here, `code` and `count` are upstream's own **narrowed** `uint32_t`/`int32_t`
values from §10.1 - the ones the `static_cast`s already produced, not the
wide `std::stoul`/`std::stol` results.

The first XOR term - `(code << 18) | (code >> 14)` - is a true 32-bit
rotate-left-by-18 (18+14 == 32) with compile-time-constant shift amounts,
always well-defined regardless of `code`'s value.

The second term - `(code << (27 + count)) | (code >> (5 - count))` -
depends on the narrowed `count`. **C++ requires each shift amount to be in
`[0, 31]` for a 32-bit operand** ([expr.shift]); a shift amount outside
that range, or negative, is undefined behavior - not "wraps", not "masks to
5 bits" - genuinely undefined, regardless of what any particular compiler
happens to emit for it. Solving `0 <= 27 + count <= 31` and
`0 <= 5 - count <= 31` simultaneously, for this **already-narrowed**
`count`, gives:

```
count in [-26, 4]
```

as the **only** domain in which upstream's own expression is defined at
all, once `count` is the value upstream's narrowing actually produces.
Every real banlist count in actual use (0/1/2, conventionally 3 for "no
additional restriction") sits comfortably inside this range.

**This module's deliberate choice**: when the narrowed `count` falls
outside `[-26, 4]` - whether because the source text's own magnitude was
already outside that range, or because upstream's own narrowing wrapped a
wide value there (§10.3) - the content line is rejected entirely, excluded
from **both** `content` and `hash`, the same treatment as a syntactically
malformed line (`LfListIgnoredLine`, `lf_list.h`). This is a fail-closed
choice, not an invented "safe" hash formula for a domain upstream itself
never defined: there is no such thing as "the correct hash" for a
*narrowed* count where upstream's own source has no defined behavior to
match, so this module refuses to compute one rather than inventing new
semantics and calling them source-equivalent. `hashUnsafeCountDomainFailsClosed`
(`test_lf_list.cpp`) pins the exact boundary on already-in-range values
(`4` and `-26` accepted; `5` and `-27` rejected) and an in-range-but-large
out-of-domain value (`999999999`); `wideCountThatNarrowsToAnUnsafeValueIsStillRejectedAfterNarrowing`
pins that the same rejection applies when a *wide* value narrows into that
unsafe domain, not only when the source text was already small.

### 10.3 The narrowing itself: toolchain evidence, not an assumption

For an out-of-range value, "narrowing to a signed type" was **implementation-defined**
behavior under C++17 (the standard `gframe/` itself is compiled as -
CLAUDE.md) and became standardized, well-defined, two's-complement modular
reduction only as of C++20 (P0907R4) - the standard this project's own
`policy/` module targets. This project does not claim a single portable
guarantee spans both: it relies on the fact that **the actual compiler this
project's CI builds both `gframe/` and `policy/` with (GCC, per
`docs/BASELINE.md`) documents this exact modular-reduction behavior as its
own implementation-defined behavior**, independent of language standard
version - so the observable result is identical for both the C++17
upstream build and the C++20 `policy/` build on the toolchains this project
actually uses. A hypothetical C++17 build on a different compiler with
different implementation-defined behavior for this conversion is a real,
narrow gap this document does not paper over - matching
`docs/architecture/deck-model.md`§2.5's own established practice for the
identical question about `std::stoul`/`std::stoll`'s native-width parsing.

Unsigned-to-unsigned narrowing (the code field, `static_cast<uint32_t>`
from `std::stoul`'s `unsigned long`) has always been well-defined modular
reduction, in every C++ standard version - there is no toolchain caveat
for that half of §10.1.

Two ABI-dependent cases are pinned directly, mirroring
`data/tests/test_deck_ydk.cpp`'s own
`two_to_the_32_is_excluded_one_way_or_another_depending_on_unsigned_long_width`:

- `wideCountThatNarrowsIntoTheSafeDomainIsAcceptedLikeUpstreamWould` -
  `4294967296` (2³²) as a count. On a 64-bit `long` platform, `std::stol`
  succeeds and narrows to `0` (inside the safe domain - accepted and
  hashed normally). On a 32-bit `long` platform, `std::stol` itself throws
  `std::out_of_range` (caught, reported as malformed) - the `if constexpr`
  branches on `std::numeric_limits<long>::max()`, never assuming the
  64-bit outcome is universal.
- `wideCodeThatNarrowsToAnotherCardsCodeIsAppliedLikeUpstreamWould` /
  `wideCodeThatNarrowsToZeroIsTreatedAsTheOrdinaryCodeZeroNoOp` - the
  identical question for `4294967297`/`4294967296` as the code field,
  branching on `std::numeric_limits<unsigned long>::max()` against
  `std::numeric_limits<std::uint32_t>::max()`.

`codeMinusOneWrapsViaUnsignedArithmeticMatchingUpstream` pins a case that
is **not** ABI-dependent: `std::stoul` accepts a leading `-` without
throwing, parsing the digits as a magnitude and negating via the unsigned
type's own modular arithmetic - `"-1"` becomes `0xFFFFFFFF` after
narrowing regardless of `unsigned long`'s platform width, since `2^64` is
itself a multiple of `2^32`.

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

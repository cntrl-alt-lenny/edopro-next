# The deck model and `.ydk` codec

What a `.ydk` file actually contains, exactly how upstream's `DeckManager` reads and writes
one, and how `data/` reads and writes the same text into `edopro_next::data::Deck` -
reproducing the file-structural grammar in full, and deliberately leaving out the
card-type-based reclassification and legality machinery that lives one layer above it in
upstream.

Everything below was read from source at upstream `54ea755a` (`docs/UPSTREAM.md`'s pinned
base commit; unchanged since). Line numbers are given for orientation and will drift with
upstream merges; the file and the function are the durable references, per the convention
[semantic-model.md](semantic-model.md) established.

See [ADR 0004](../adr/0004-deck-model-ydk-codec.md) for *why* this is `data::CardCode`
codes rather than card pointers, and why the codec follows the file's own section markers
rather than reproducing upstream's type-based classification. This document is only about
what the format means and what upstream's own code actually does with it.

---

## 0. What this module is not

It is not a legality checker, not a deck builder, not a card-type classifier, and not
`DeckManager`. It does not check deck size, does not enforce a three-copy limit, does not
apply a banlist, and does not decide whether a card belongs in the Extra Deck by its type.
It reports what a `.ydk` file's own markers say, and nothing more. It also never opens a
`CardDatabase`: a syntactically valid card code is stored whether or not anything currently
loaded recognises it, and `edopro_next_deck` (`data/CMakeLists.txt`) does not link SQLite -
see §8.

---

## 1. `Deck`: three ordered lists of card codes, not pointers

Upstream's own `Deck` (`gframe/deck.h`):

```cpp
struct Deck {
    using Vector = std::vector<const CardDataC*>;
    Vector main;
    Vector extra;
    Vector side;
    void clear() { main.clear(); extra.clear(); side.clear(); }
};
```

A pointer into a live, globally loaded catalogue (`gDataManager`) - constructing one
requires that catalogue to already contain the card. `edopro_next::data::Deck` stores
`edopro_next::data::CardCode` (`data/include/edopro_next/data/deck.h`) instead: the same
three ordered lists, but a value, not a pointer, so a `.ydk` can be parsed with no database
loaded at all - see §6 for why that matters for round-tripping and §5 for why it removes a
whole mechanism (dummy/alias entries) upstream needs and this module does not.

Order and multiplicity both matter and are both preserved by construction: `main`/`extra`/
`side` are plain `std::vector`s, never deduplicated, never sorted, never counted into a
map. Three entries for the same code are three vector elements, in the order the file (or
the caller) put them in.

---

## 2. `LoadCardList`: the actual `.ydk` parser

`DeckManager::LoadCardList` (`gframe/deck_manager.cpp:272-318`, `static`, private to that
file) is the only code that reads `.ydk` text. Reproduced here in full because every
decision below cites a specific piece of it:

```cpp
static bool LoadCardList(const epro::path_string& name, cardlist_type* mainlist = nullptr,
                          cardlist_type* extralist = nullptr, cardlist_type* sidelist = nullptr,
                          uint32_t* retmainc = nullptr, uint32_t* retsidec = nullptr) {
    FileStream deck{ name, FileStream::in };
    if(deck.fail())
        return false;
    cardlist_type res;
    std::string str;
    bool is_side = false;
    bool is_extra = false;
    uint32_t sidec = 0;
    while(std::getline(deck, str)) {
        auto pos = str.find_first_of("\n\r");
        if(str.size() && pos != std::string::npos)
            str.erase(pos);
        if(str.empty())
            continue;
        if(str[0] == '#') {
            if(!extralist || str != "#extra")
                continue;
            is_extra = true;
        }
        if(str[0] == '!') {
            is_side = true;
            continue;
        }
        if(str.find_first_of("0123456789") != std::string::npos) {
            uint32_t code = 0;
            try { code = static_cast<uint32_t>(std::stoul(str)); }
            catch (...) { continue; }
            res.push_back(code);
            if(is_side) {
                if(sidelist)
                    sidelist->push_back(code);
                sidec++;
            } else {
                if(mainlist && !is_extra)
                    mainlist->push_back(code);
                if(extralist && is_extra)
                    extralist->push_back(code);
            }
        }
    }
    if(retmainc)
        *retmainc = static_cast<uint32_t>(res.size() - sidec);
    if(retsidec)
        *retsidec = sidec;
    return true;
}
```

`edopro_next::data::parse_ydk` (`data/src/ydk.cpp`) reproduces this function's file-
structural behaviour exactly, with one mode collapsed to a single choice - see §3 - and one
deliberate exclusion - see §5.

### 2.1 Opening the file: the only genuine failure

`deck.fail()` (line 274) is the only thing that makes `LoadCardList` return `false` - a
missing file or a permission error. No line of file *content* ever causes it to fail; every
malformed or unrecognised line is silently skipped and the function still returns `true`.
`edopro_next::data::load_ydk()` mirrors this split: opening/reading the file is the one
thing that can fail (`YdkLoadResult::ok`/`error`); `parse_ydk()` itself, working on text
already in memory, has no failure state at all - see §7.

### 2.2 CRLF: stripped by hand, because nothing else does it

`FileStream` is `std::fstream` on Linux (`gframe/file_stream.h`, the non-Windows branch) -
opened with no text-mode translation, so a CRLF-terminated file leaves a trailing `\r` on
every line `std::getline` (which splits on `\n` only) extracts. Lines 282-284 strip it by
hand: find the first of `\n` or `\r` in the line and erase from there. Since `getline` never
leaves an embedded `\n` in what it returns, this is equivalent to "find the first `\r`,
truncate there" - which is also what would happen to a line with a stray `\r` in the
*middle*, not just at the end, matching `find_first_of` rather than `find_last_of` or a
fixed "strip exactly one trailing character".

`edopro_next::data::parse_ydk` (§below) splits its input on `\n` itself (there is no stream
to `getline` from - it takes `std::string_view` directly), then applies the identical
`find_first_of("\n\r")`-truncation rule to each extracted line - and `load_ydk()` reads its
file with `std::ios::binary`, so no OS/runtime text-mode translation happens before that
rule runs, matching `FileStream`'s own lack of translation on Linux.

### 2.3 Section markers

- **`#main`** never appears as a special case anywhere in this function. It is ordinary `#`-
  prefixed comment text, skipped by the same branch as any other comment. A file that omits
  it entirely parses identically - the "current section is main" state is simply the
  parser's initial state (`is_extra = is_side = false`), not something `#main` sets.
- **`#extra`** is functional *only when `extralist` is non-null* (line 288:
  `if(!extralist || str != "#extra") continue;`). When the caller passed no `extralist`,
  every `#`-prefixed line - `"#extra"` included - just hits `continue`, and `is_extra` is
  never set by the file at all; section membership is then decided entirely by
  `LoadDeck`'s card-type reclassification afterward (§4). When `extralist` *is* provided and
  the line is the exact string `"#extra"`, `is_extra` becomes `true`; there is no explicit
  `continue` after that assignment, but the line falls through to the `'!'` check (false)
  and the digit check (`"#extra"` has none), so the net effect - no card added, loop
  continues - is identical to an explicit `continue`.
- **`!`, not specifically `"!side"`** - any line whose first character is `!` sets
  `is_side = true` and continues (line 292-294). A real file only ever writes `"!side"`
  (§4), but the check does not test the rest of the string.
- **Neither flag ever resets.** Once `is_extra` or `is_side` becomes `true`, nothing in this
  function sets it back to `false`. In particular, a `!`-prefixed line seen before an
  `"#extra"` marker permanently latches `is_side`; a later `"#extra"` still sets `is_extra`,
  but the `if(is_side) {...} else {...}` split (line 301) checks `is_side` first, so every
  following card still lands in side, not extra. Pinned by
  `a_hash_extra_marker_appearing_after_bang_side_does_not_reopen_extra`
  (`data/tests/test_deck_ydk.cpp`) - a case no real upstream-written file produces (§4's
  canonical order never puts `!side` before `#extra`), tested because the parser's own
  control flow allows it, not because it is expected in practice.

`edopro_next::data::parse_ydk` always behaves as if `extralist` were provided - see §3 for
why - so `#main` is inert exactly as above, `#extra` is always functional, and `!`/latching
behave identically to the transcription above.

### 2.4 The digit pre-check, then `std::stoul`, then no code-0 special case

Line 296: `str.find_first_of("0123456789") != std::string::npos` - a line is only ever
*attempted* as a number if it contains at least one digit anywhere in it. A line with none
(ordinary comment text that isn't `#`-prefixed for some reason, stray punctuation, etc.)
never reaches `std::stoul` and is simply not a candidate at all.

Line 298: `code = static_cast<uint32_t>(std::stoul(str))`, wrapped in `try`/`catch(...)`
that just `continue`s on any exception. **There is no `code == 0` check anywhere in this
function.** (A `code == 0` check does exist elsewhere in `deck_manager.cpp`, in the
unrelated LFList/banlist-file parser a few dozen lines earlier in the same file - a
different function reading a different file format, not `.ydk`.) A line that parses to `0`
is pushed into `res` and into whichever of `mainlist`/`extralist`/`sidelist` the current
`is_extra`/`is_side` state selects, exactly like any other code. See §5 for what this
module does instead, and why.

`std::stoul`'s exact semantics matter here and are cited from the standard, not memory - see
§2.5.

### 2.5 `std::stoul`: verified empirically, not assumed

Confirmed against the actual toolchain this project builds with (Linux x86-64, g++ 15.2.0,
64-bit `unsigned long`, matching `docs/BASELINE.md`), and pinned by
`data/tests/test_deck_ydk.cpp`:

- Leading whitespace (including tab) is skipped before parsing begins.
- An optional leading `+` or `-` is accepted. `-` does **not** throw: the digits are parsed
  as a magnitude and then negated using the *unsigned* type's modular arithmetic - `-1`
  becomes `0xFFFFFFFFFFFFFFFF` as a 64-bit `unsigned long`, which `static_cast<uint32_t>`
  then truncates to `0xFFFFFFFF`. This is mathematically identical to wrapping directly at
  32 bits (2⁶⁴ is a multiple of 2³²), so the result does not depend on `unsigned long`'s
  platform width for this case.
- Trailing non-numeric text is silently ignored - `stoul` parses the *leading* valid number
  and stops; it does not require the whole string to be numeric. `"123abc"` parses to `123`.
- A decimal point stops the parse at the point: `"12.5"` parses to `12`.
- Base is always 10 (`LoadCardList` never passes a base argument) - a leading `"0x"` is not
  hex. `"0x1A"` parses `"0"`, then stops at `'x'`; it does not parse as `26`.
- `std::invalid_argument` is thrown when the first non-whitespace, non-sign character is not
  a digit at all (e.g. a would-be-numeric-looking string that is actually just punctuation) -
  caught by `catch(...)`, the line is skipped, no diagnostic is possible from inside
  `LoadCardList` (it has none to give).
- `std::out_of_range` is thrown when the parsed magnitude exceeds `unsigned long`'s own
  range (64 bits here) - e.g. a 30+ digit number - also caught and skipped. This is
  different from the `code == 4294967296` case above: that value fits comfortably in a
  64-bit `unsigned long` and only overflows on the subsequent `static_cast<uint32_t>`, which
  is defined wraparound, not an exception.

`edopro_next::data::parse_ydk` calls `std::stoul` the same way, on the same platform
baseline, and inherits these exact semantics rather than approximating them - see
`data/tests/test_deck_ydk.cpp`'s §H-labelled tests (`leading_numeral_followed_by_letters_
parses_the_leading_number`, `a_leading_minus_sign_wraps_via_unsigned_arithmetic_matching_
upstream`, `a_hex_looking_prefix_parses_as_decimal_zero_not_as_hexadecimal`,
`a_number_far_beyond_any_native_integer_width_is_reported_as_malformed`, and others) for
each case pinned individually rather than inferred.

---

## 3. Explicit sections vs. legacy auto-classification: why this codec always uses the former

`LoadDeckFromFile` (`gframe/deck_manager.cpp:319-329`) is the actual entry point, and calls
`LoadCardList` with `extralist` set to `separated ? &extralist : nullptr` - so upstream
itself has two distinct file-reading modes, controlled by one caller-supplied bool:

- **`separated = false` (the parameter's own default, `gframe/deck_manager.h:80`):**
  `"#extra"` is inert (§2.3); everything before any `!`-prefixed line goes into one combined
  `mainlist`. Section membership - what ends up in the Extra Deck - is decided afterward, by
  `LoadDeck` (`gframe/deck_manager.cpp:330-392`), which looks up each code's `CardDataC` via
  `gDataManager` and classifies it as Extra-Deck by its actual card type
  (`TYPE_FUSION`/`TYPE_SYNCHRO`/`TYPE_XYZ`/Link monster) or by `isRitualMonster()` combined
  with the caller's `RITUAL_LOCATION` policy. This mode **requires a live, loaded card
  database** to produce a correct split at all.
- **`separated = true`:** `"#extra"` is functional, so `LoadCardList` itself splits main
  from extra using the file's own markers. `LoadDeck` is still called afterward
  (`LoadDeckFromFile` always calls it) and **still** reclassifies - notably, a card placed
  in the file's `#main` section that turns out to be a Fusion/Synchro/Xyz/Link card gets
  moved to `deck.extra` by `LoadDeck` regardless of which section the file put it in
  (`is_extra_deck_card`, `gframe/deck_manager.cpp:335-348`). So even upstream's "explicit"
  mode does not, on its own, fully determine final section membership without a database -
  it only determines the boundary `LoadCardList` reports back to `LoadDeck` as a starting
  point.

Real callers overwhelmingly use `separated = true`: it is what `DeckManager::LoadDeckFromFile`
receives from the deck builder's own primary flows (`gframe/deck_con.cpp`'s file-open and
drag-drop paths), which is where hand-authored and tool-authored `.ydk` files actually get
read. `separated = false` is used by at least one quick-load path at duel start
(`gframe/menu_handler.cpp:314`) and one call that takes the parameter's default
(`gframe/menu_handler.cpp:1114`).

**This codec always behaves as if `separated = true` was requested - `"#extra"` is always
functional - and never performs the `LoadDeck` reclassification step at all.** Concretely:
a card's section in `edopro_next::data::Deck` is exactly what the file's own markers say,
full stop. No `CardDatabase`, no card type, no Ritual/Rush/Link check, ever runs inside
`parse_ydk`.

This is deliberate, for two independent reasons:

1. **Scope.** Type-based classification needs a loaded `CardDatabase`, coupling a "read this
   text file" operation to "have the entire card catalogue open" - exactly the coupling this
   module's separation from `edopro_next_data` (§8) exists to avoid, and exactly what the
   task defining this slice of work explicitly forbids putting in the raw parser.
2. **Correctness of representation.** A `.ydk`'s explicit sections are real, load-bearing
   information a file can contain (what the deck builder itself writes and reads,
   overwhelmingly the common real-world case per the call-site survey above) - collapsing
   that into "one flat list, reclassify later" would silently discard it for any consumer
   that only has this codec.

`LoadDeck`'s type-based reclassification is real, useful upstream behaviour and is not lost
- it is simply not part of this codec. A future, explicitly-reviewed layer with access to a
`CardDatabase` can implement it as a transformation from one `Deck` to another
(`Deck -> Deck`), which is a strictly better foundation for it than baking the same logic
into the text parser, and keeps this codec usable by a caller that has no database loaded at
all (§6).

---

## 4. `SaveDeck`: the writer, and the canonical section order

Two overloads (`gframe/deck_manager.cpp:436-451` and `:453-468`) differing only in whether
the source is a `Deck` or three raw code lists; both produce identical output shape:

```cpp
deckfile << "#created by " << BufferIO::EncodeUTF8(mainGame->ebNickName->getText()) << "\n#main\n";
// ... one MakeYdkEntryString(code) per main card ...
deckfile << "#extra\n";
// ... one MakeYdkEntryString(code) per extra card ...
deckfile << "!side\n";
// ... one MakeYdkEntryString(code) per side card ...
```

`#main`/`#extra`/`!side` are written **unconditionally** - nothing guards them on the
corresponding section being non-empty, so a deck with an empty Extra Deck still gets a bare
`"#extra\n"` line immediately followed by `"!side\n"`. `edopro_next::data::serialize_ydk`
reproduces this exactly (§6), including for a completely empty `Deck`.

`MakeYdkEntryString` (`gframe/deck_manager.cpp:469-472`):

```cpp
std::string DeckManager::MakeYdkEntryString(uint32_t code) {
    if(gGameConfig->addCardNamesToDeckList)
        return epro::format("# {}\n{}\n", BufferIO::EncodeUTF8(gDataManager->GetName(code)), code);
    return epro::to_string(code) + "\n";
}
```

Two writer-metadata features exist upstream, and neither is reproduced by this codec's core
writer, on purpose:

- **`"#created by <nickname>"`** comes from `mainGame->ebNickName` - live UI state this
  module has no access to and should not depend on (`data/` has no notion of "the current
  user"). `edopro_next::data::YdkWriteOptions::creator` exposes the *mechanism* (an optional
  string, emitted as that exact line when present, omitted entirely when absent) - see §6 -
  without this codec ever sourcing a value for it itself. Not emitted byte-for-byte
  verbatim, though: `serialize_ydk` strips any `'\n'`/`'\r'` from `creator` first. Without
  that, a caller-supplied string containing an embedded newline would not stay one cosmetic
  comment line - the text after the break becomes a new line of real file content, able to
  match a section marker or parse as a bare card code on a later `parse_ydk()` of this
  codec's own output (`creator = "x\n999"` would otherwise inject a phantom code-`999` Main
  Deck card - `data/tests/test_deck_ydk.cpp`'s
  `a_creator_containing_a_newline_cannot_inject_a_card_line` pins that this cannot happen).
  This was caught by external review of this PR, not found during initial implementation.
- **The optional `"# <CardName>\n<code>\n"` form** requires `gDataManager->GetName(code)` -
  a card-database name lookup. This is presentation/export sugar with zero effect on
  loading (§2.3/§2.4: any `#`-prefixed line is just a comment to the parser, name-comment or
  otherwise), so it does not belong on the semantic writer at all. This codec does not offer
  it, in any form - not as an option, not via a caller-supplied name callback - keeping
  `serialize_ydk` free of any card-name-lookup dependency, matching `parse_ydk`'s freedom
  from `CardDatabase` (§3, §6). A caller that wants upstream's optional commented export can
  build it on top, using its own `CardDatabase::find(code)->name` before or after calling
  this codec; that composition does not need to live inside `data/`.

`card->getRealCode()` (`gframe/data_manager.h:87-90`, `return code ? code : alias;`) is how
`SaveDeck`'s `Deck`-taking overload recovers the *original* code for a card that was loaded
as a dummy placeholder (§5) - `code` for such an entry is always `0` by construction, so
`getRealCode()` falls through to `alias`, which `GetDummyOrMappedCardData` set to the
originally-requested code. `edopro_next::data::Deck` never needs this indirection: it stores
the original `CardCode` directly, so there is no dummy/alias pair to reconstruct from.

---

## 5. Card code 0: a deliberate divergence, precisely scoped

Two different pieces of upstream code touch a code that resolves to "not a real card", and
they disagree with each other - which is exactly why this needs to be a documented decision
rather than "matching upstream", singular:

- **`LoadCardList` itself (§2.4): no special case at all.** A literal `"0"` line is pushed
  into the raw list like any other code, in whichever section the current `is_extra`/
  `is_side` state selects.
- **`LoadDeck` (`gframe/deck_manager.cpp:330-392`), the downstream, `CardDatabase`-dependent
  step:** for each raw code, it tries `gDataManager->GetCardData(code)`; since `CardCode::None
  == 0` is never a real loaded `.cdb` row (`docs/architecture/card-database.md`§7 - a `.cdb`
  row with `id = 0` is itself rejected as a load failure), this lookup fails for a code-0
  entry exactly as it would for any other code the catalogue does not recognise, and
  `LoadDeck` falls back to `GetDummyOrMappedCardData(code)` - constructing a dummy
  `CardDataC` with `code = 0` regardless of what the *original* requested code was (§4). What
  happens next is **mode-dependent**, controlled by `loadalways` (`= !!extralist`, i.e. tied
  to the same `separated` flag as §3): in non-`loadalways` mode the dummy is dropped
  (`errorcode = code; continue;` - line 352-355); in `loadalways` mode the dummy is *kept* as
  an untyped placeholder and lands in `deck.main` (the extra/main split at line 359 requires
  `cd->code != 0`, which a dummy never satisfies).

Neither of those is "the" correct behaviour for a boundary that has no `CardDatabase` to
consult in the first place - `LoadDeck`'s entire mechanism is downstream of exactly the
CardDatabase-dependent reclassification step §3 already excludes from this codec. What this
codec actually does is decided independently, from `CardCode::None`'s own established
meaning: **a line that parses to code `0` is excluded from the resulting `Deck` - not
stored in any section - and reported via `YdkIgnoredLine{ .reason = "card code 0 is not a
real card" }`.**

This is a genuine divergence from `LoadCardList`'s raw behaviour (which stores it
structurally, §2.4), justified by staying strictly at the text-parsing layer this codec
occupies: `CardCode::None == 0` is this project's own "not a real card" sentinel throughout
`data/` (`card_code.h`; enforced as a load failure by `CardDatabase::load_database()`,
`docs/architecture/card-database.md`§7), and a `Deck` that could contain it would force
every consumer to special-case "is this entry actually `None`" the way `LoadDeck`'s
`cd->code != 0` checks do - reintroducing, in the value model, exactly the kind of ad hoc
sentinel-checking `CardCode::None`'s enum-level meaning already exists to avoid. Both of
upstream's two behaviours are mode-dependent artifacts of the dummy/alias mechanism (§4)
this module's value-based `Deck` has no use for; this codec's own behaviour is neither of
them, chosen instead for internal consistency with the rest of `data/`.

Also excluded, by the same reasoning and the same mechanism: a code that overflows
`uint32_t` when truncated (`static_cast<uint32_t>` wraps `4294967296` to `0`) is
**indistinguishable, after truncation, from a literal `"0"` line**, and is excluded for the
identical reason - not because this codec detects the overflow specially, but because the
truncated value genuinely is `0`. A magnitude that overflows `std::stoul`'s own return type
(`unsigned long`, 64-bit on this baseline) is a different case entirely: it throws
`std::out_of_range` before truncation ever happens, and is reported as `"malformed card
code"`, not as code 0. `data/tests/test_deck_ydk.cpp` pins both cases separately
(`two_to_the_32_wraps_to_zero_and_is_excluded_by_the_code_zero_policy` vs.
`a_number_far_beyond_any_native_integer_width_is_reported_as_malformed`) precisely so they
are never conflated.

---

## 6. The codec's public shape, and the round-trip contract

`data/include/edopro_next/data/ydk.h`:

- `parse_ydk(std::string_view) -> YdkParse { Deck deck; vector<YdkIgnoredLine> ignored; }` -
  pure text-to-value parsing, no filesystem, no failure state (§2.1: nothing at this layer
  can fail on content, matching `LoadCardList` itself).
- `load_ydk(path) -> YdkLoadResult { bool ok; string error; Deck deck; vector<YdkIgnoredLine>
  ignored; }` - adds the one genuine failure mode, file open/read, on top of `parse_ydk`. On
  failure, `deck` and `ignored` are left empty rather than partially populated; there is no
  in-place mutation of a caller-owned `Deck` for a failed load to leave half-applied in the
  first place - the codec returns a fresh value, and a caller that only assigns it when
  `ok == true` (`data/tests/test_deck_ydk.cpp`'s
  `loading_a_missing_file_fails_and_leaves_an_existing_deck_untouched`) gets transactional
  behaviour for free, without this codec needing a separate "transaction" concept.
- `serialize_ydk(Deck, YdkWriteOptions = {}) -> std::string` - pure, deterministic
  serialization (§4): the same `Deck` and `options` always produce byte-identical output
  (`serializing_the_same_deck_twice_produces_byte_identical_output`), using a plain `'\n'`
  throughout regardless of host platform, matching upstream's own `deckfile << "...\n"`
  (never `"\r\n"`).
- `save_ydk(path, Deck, YdkWriteOptions = {}) -> YdkSaveResult { bool ok; string error; }` -
  writes `serialize_ydk`'s output to `path` in binary mode, so the emitted `\n` bytes reach
  disk unchanged regardless of host text-mode translation, and reports write failure
  explicitly rather than discarding it.

**Round-trip contract:** for a `Deck` produced by `parse_ydk`/`load_ydk`,
`parse_ydk(serialize_ydk(deck)) == deck` - identical card order, identical duplicates,
identical section membership, including for unknown non-zero codes and for a completely
empty `Deck`. Comments and incidental whitespace are not expected to survive (this is not a
lossless text/AST round-trip - the semantic `Deck` is the thing being round-tripped, not the
source text), and none of the tests expect them to.
`parse_then_serialize_then_parse_again_yields_an_identical_deck` pins this directly.

**Unknown non-zero codes need no special mechanism to survive.** Because `Deck` stores
`CardCode` values, not pointers into a catalogue, a code this codec has never seen resolved
by any database round-trips exactly like any other - there is no dummy/alias construction to
reproduce (§4), and no `CardDatabase` is ever consulted during either parse or write.

---

## 7. Filesystem semantics

`load_ydk`/`save_ydk` take a `std::filesystem::path` directly - no "deck name" API that
constructs a path relative to a working directory or a legacy `./deck/` convention
(`DeckManager::GetDeckPath`, `deck_folder`), and no dependency on `mainGame`, `gGameConfig`,
or any UI nickname/profile state. Where a deck file actually lives, and how a user names one,
is an application-layer concern above this codec, not something the codec decides for
itself - matching how `CardDatabase` takes a `std::filesystem::path` and has no opinion on
where `.cdb` files are discovered from (`docs/architecture/card-database.md`§8).

`load_ydk` opens read-only (`std::ios::binary`, no create flag implied by `std::ifstream`'s
default open mode); `save_ydk` opens with `std::ios::trunc`, overwriting any existing file at
that path - there is no partial-write or backup-then-replace behaviour, matching
`FileStream::out`'s own plain-truncate semantics (`gframe/file_stream.h`).

### Detecting a failure the stream's own state does not surface for free

Two failure modes here are easy to get wrong with `<iostream>`'s ordinary-looking API, and
both were caught by external review, not by initial implementation - verified empirically
(`/dev/full`, `/proc/self/mem`, both real Linux devices built for exactly this kind of test)
rather than taken on the reviewer's word:

- **`load_ydk` does not read via `output << file.rdbuf()`.** That inserter operates
  directly on the streambuf, bypassing `basic_istream::read()`'s own sentry/state-update
  machinery - so a genuine mid-read I/O error below the streambuf can leave `file` reporting
  `good()` with a silently truncated (or, confirmed empirically, completely empty) result.
  `load_ydk` instead reads through `file.read()` in a fixed-size, sign-correct loop
  (`while(file.read(chunk, sizeof(chunk)) || file.gcount() > 0)`), which does update `file`'s
  state correctly, and checks `file.bad()` only after that loop - verified both for byte-
  exact correctness across file sizes that do and do not land on a chunk boundary (including
  empty), and for actually detecting a forced read error where the `rdbuf()` form did not.
- **`save_ydk` calls `file.flush()` before checking whether the write succeeded.**
  `ofstream::write()` can leave the stream reporting `good()` even though the data never
  reached the destination: a full filesystem can accept the write into the stream's own
  buffer and only fail once that buffer actually flushes - which, without an explicit
  `flush()`, would happen silently at the `ofstream`'s destruction, after `save_ydk` had
  already returned `ok == true`. Confirmed empirically against `/dev/full` (a device that
  always accepts an open and a `write()`, but always fails the underlying flush): checking
  `file`'s state immediately after `write()` reported `good()`; the same check after an
  explicit `flush()` correctly reported the failure.

---

## 8. What is out of scope for this slice, on purpose

- **Legality.** Deck size limits, the three-copy rule, forbidden/limited/semi-limited
  checks, `LFList`/banlist parsing, `DuelAllowedCards` scope restrictions, Skill/Legend
  limits, Ritual-placement validation - all of `CheckDeckContent`/`CheckDeckSize`
  (`gframe/deck_manager.cpp:204-258`) and friends. None of it is reproduced, referenced, or
  implicitly baked into `Deck`'s shape; a `Deck` with 60 duplicate copies of one code is a
  perfectly valid value this module will parse, store, and write back out unchanged.
- **Fast search.** Not part of this codec at all - the next, separate M3 roadmap item.
- **Card-type classification.** `LoadDeck`'s Extra-Deck reclassification (§3) and Ritual/Rush
  handling (`RITUAL_LOCATION`, `isRitualMonster()`/`isRush()`) are real upstream behaviour,
  deliberately not reimplemented here - see §3 for the reasoning and where such a layer
  would belong if built later.
- **YDKe / Base64 import-export.** `ExportDeckYdke`/`ImportDeckYdke`
  (`gframe/deck_manager.cpp`) and `ImportDeckBase64Omega` are a completely different
  serialization (a `ydke://` URI carrying raw little-endian `uint32` arrays, Base64-encoded),
  unrelated to the `.ydk` text format this module reads and writes, and not needed by
  anything in this slice.
- **The card-name-comment writer feature, and creator-nickname sourcing.** See §4 - the
  mechanism (`YdkWriteOptions::creator`) is exposed; the policy of what value to put there,
  and the name-lookup-dependent comment form, are not.

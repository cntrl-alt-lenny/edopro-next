# Fast card search

What upstream's deck-builder search actually does, exactly how `data/` reproduces its
genuinely reusable parts, and what it deliberately leaves out - the compact sigil
mini-language, the type-based reclassification-adjacent UI plumbing, and everything that is
legality rather than search.

Everything below was read from source at upstream `54ea755a` (`docs/UPSTREAM.md`'s pinned
base commit; unchanged since). Line numbers are given for orientation and will drift with
upstream merges; the file and the function are the durable references, per the convention
[semantic-model.md](semantic-model.md) established.

See [ADR 0005](../adr/0005-card-search-structured-query.md) for the load-bearing decisions:
an explicit, rebuilt-on-demand snapshot instead of a live index, and a structured query type
instead of upstream's sigil syntax. This document is the fuller account those decisions are
built on.

---

## 0. What this module is not

It is not a legality checker, not the QML deck builder, not `DeckBuilder`, and not a
UI-syntax parser. It does not check deck size, does not enforce a three-copy limit, does not
consult an `LFList`, does not know what a whitelist is, and has no concept of "is this card
legal right now." It never touches Qt, QML, Irrlicht, `ocgcore`, `gframe`, or `DeckManager`.
`SearchQuery::scope` is the raw `CardRecord::scope` bitmask, exposed as data a caller may
filter on - not a policy this module interprets on its own. See §2 for exactly which parts
of upstream's own `CheckCardProperties` this excludes, and why.

---

## 1. Upstream search, traced from source

### 1.1 `FilterCards`: the entry point, and its sigil mini-language

`DeckBuilder::FilterCards` (`gframe/deck_con.cpp:1059-1190`) is what actually runs when the
deck builder's search box changes. The text in `mainGame->ebCardName` is first split on
`||` (`Utils::TokenizeString`, `gframe/utils.h:294-305`) into independent OR-terms; each
term's results are computed separately (with a per-term result cache, `searched_terms`) and
then unioned. Within one term, `&&` splits into AND-subterms - a card must satisfy all of
them. Within one subterm, an optional prefix selects a mode:

| Prefix | Effect | Constant |
|---|---|---|
| `!!` | Negate the rest of this subterm's result | `SEARCH_MODIFIER_NEGATIVE_LOOKUP` |
| `@` | Archetype/setcode search instead of name/text | `SEARCH_MODIFIER_ARCHETYPE_ONLY` |
| `$$` | Rules-text only | `SEARCH_MODIFIER_TEXT_ONLY` |
| `$` | Name only | `SEARCH_MODIFIER_NAME_ONLY` |

(`gframe/deck_con.h:39-44`, checked in that order - `$$` before `$` - so `$$` is not
misread as `$` plus a stray `$`.) What remains after stripping a prefix is further split on
`*` into tokens (`gframe/deck_con.cpp:1129`).

This is a compact, keyboard-driven, UI-specific language, tightly coupled to the Irrlicht
deck builder's single text box. `SearchQuery` (`data/include/edopro_next/data/search_query.h`)
does not expose it - see ADR 0005, Decision 2, and §5 below for exactly what it exposes
instead and why none of the sigils survive as the *fundamental* API, even though most of
what they *do* has a structured equivalent.

### 1.2 `CheckCardProperties`: search filters and legality filters, mixed together

`DeckBuilder::CheckCardProperties` (`gframe/deck_con.cpp:1191-1324`) is a single function
that both filters on ordinary static card metadata (monster/spell/trap type, race,
attribute, ATK/DEF/Level/Scale, effect category, Link markers) *and* enforces policy that
has nothing to do with what a card *is*:

- An unconditional top-of-function gate: `TYPE_TOKEN` cards, `SCOPE_HIDDEN` cards, and any
  card whose `ot` is not purely `SCOPE_OFFICIAL` are excluded unless "show anime cards" is
  checked or the active `LFList` is a whitelist.
- A large `switch` on `filter_lm` (`limitation_search_filters`, `gframe/deck_con.h:20-38`)
  that consults `filterList->GetLimitationIterator` - i.e. reads the currently loaded
  `LFList`'s ban/limit/semi-limit counts - and separately branches on `SCOPE_OCG`/
  `SCOPE_TCG`/`SCOPE_ANIME`/`SCOPE_ILLEGAL`/etc. as named legality categories, not raw bits.

This module reproduces only the first half. §2 states exactly which pieces of
`CheckCardProperties` have a `SearchQuery` equivalent and which are deliberately absent.

### 1.3 `CheckCardText` and `Utils::ContainsSubstring`: text matching, and one real quirk

`DeckBuilder::CheckCardText` (`gframe/deck_con.cpp:1341-1362`) reads `CardDataM::GetStrings()`
(`gframe/data_manager.h:111-115`) for `uppercase_name`/`uppercase_text` - **precomputed at
load time**, once per card, in both `DataManager::ParseDB` and `ParseLocaleDB`
(`gframe/data_manager.cpp:158-164,198-204`), never recomputed per search. It never reads
`CardString::desc[16]` (the sixteen auxiliary strings) at all - name and rules text are the
only searchable fields upstream's own deck search uses, which is why `SearchQuery`'s
`TextScope` only ever offers `Name`/`Text`/`NameOrText` (§4).

`Utils::ContainsSubstring` (`gframe/utils.cpp:664-674`) is documented ("Returns true if and
only if all tokens are contained in the input") but its actual implementation requires more
than that:

```cpp
bool Utils::ContainsSubstring(epro::wstringview input, const std::vector<epro::wstringview>& tokens) {
    if (input.empty() || tokens.empty())
        return false;
    std::size_t pos1, pos2 = 0;
    for (const auto& token : tokens) {
        if((pos1 = input.find(token, pos2)) == epro::wstringview::npos)
            return false;
        pos2 = pos1 + token.size();
    }
    return true;
}
```

Each token's search starts at `pos2`, the end of the *previous* token's match - so tokens
must appear left-to-right, non-overlapping, in the same order the query gave them. This is
an artifact of writing the check with one forward-advancing `find()`, not a stated design
choice anywhere in the surrounding code or comments. `CardSearchIndex` deliberately does not
reproduce the ordering requirement - see §7.

### 1.4 `SortList`: a UI column sort, not a relevance ranking

`DeckBuilder::SortList` (`gframe/deck_con.cpp:1397-1432`) partitions `results` into "cards
whose *exact* uppercase name is itself one of the active search terms" (moved to the front,
via `searched_terms.find(GetUppercaseName(...))`) and everything else, then independently
sorts *each partition* by whichever comparator the sort-type dropdown selected -
`DataManager::deck_sort_lv`/`_atk`/`_def`/`_name`/`_passcode_descending`
(`gframe/data_manager.cpp:712-765`), all of which are card-type-aware column comparators
(monster type, then the chosen stat, then a few more tiebreakers, finally passcode) built
for a sortable-column grid, not a relevance score. `CardSearchIndex`'s own ranking (§8) is a
new, independently designed scheme informed by this but not copied from it - see ADR 0005,
Decision 1.

### 1.5 `GetSetCode` and `CardSetcodes`: archetype names vs. numeric setcodes

`DataManager::GetSetCode` (`gframe/data_manager.cpp:401-421`) resolves a *human, localized
archetype name string* (e.g. what a player types after `@`) into the numeric setcode(s)
whose *localized set-name string* - from `_setnameStrings`, a strings resource loaded
independently of any `.cdb`, not part of `CardRecord`/`CardDatabase` at all - contains it as
a substring. This module has no equivalent and does not attempt one: resolving an archetype
*name* needs a data source `data/` does not yet have. `SearchQuery::setcodes` (§4) is a
purely numeric filter over `CardRecord::setcodes` (which *is* schema data), not a name
lookup - see §9 for the full separation.

`CardSetcodes` (`gframe/deck_con.cpp:1325-1331`) and `check_set_code` (`:1332-1340`) are
what actually resolves *which* card's setcodes to check once a numeric filter exists:

```cpp
static const auto& CardSetcodes(const CardDataC& data) {
    if(data.alias) {
        if(auto _data = gDataManager->GetCardData(data.alias); _data)
            return _data->setcodes;
    }
    return data.setcodes;
}
```

An aliased card (an alternate-artwork printing or errata, `card_record.h`) is matched using
the setcodes of the card it is an alias *of*, when that card is loaded - not its own
(usually empty) `setcode` row. `CardSearchIndex::rebuild()` reproduces this exactly, once
per card, into `Entry::effective_setcodes` (§6), rather than resolving it per query.

---

## 2. What has a `SearchQuery` equivalent, and what is deliberately absent

| Upstream (`CheckCardProperties`) | `SearchQuery` equivalent |
|---|---|
| Monster type + subtype bitmask (`filter_type`/`filter_type2`) | `type` (`BitmaskFilter`) |
| Race (`filter_race`) | `race` (`BitmaskFilter64`) |
| Attribute (`filter_attrib`) | `attribute` (exact match) |
| ATK/DEF/Level/Scale numeric filters | `attack`/`defense`/`level`/`left_scale`/`right_scale` (`NumericFilter`) |
| Effect category (`filter_effect`) | `category` (`BitmaskFilter`) |
| Link markers (`filter_marks`) | `link_marker` (`BitmaskFilter`) |
| `ot`/scope, used both as data and as legality | `scope` (`BitmaskFilter`) - raw data only, see §0 |

| Upstream policy (`CheckCardProperties`/`filter_lm`) | `SearchQuery` equivalent |
|---|---|
| `TYPE_TOKEN`/`SCOPE_HIDDEN` auto-exclusion, anime-mode gate | **None.** Not search - visibility policy. A caller wanting to exclude tokens can use `type` with a mask that excludes `TYPE_TOKEN`. |
| `LFList` ban/limit/semi-limit counts, whitelist | **None.** Legality, explicitly out of scope (§0, CLAUDE.md). |
| `LIMITATION_FILTER_OCG`/`TCG`/`ANIME`/`ILLEGAL`/etc. named scope categories | **None**, as named categories - `scope` exposes the same underlying bits as a raw filter, with no "this means legal" interpretation attached. |

---

## 3. Snapshot and rebuild: an explicit, not observed, lifecycle

`CardSearchIndex::rebuild(const CardDatabase&)` copies everything `search()` needs - into
`Entry` values, keyed by `CardCode`, never a `CardRecord*` - out of the database's *current*
state and keeps no reference back to it. A later `load_database()`, `load_locale()`, or
`clear_locale()` call on that same `CardDatabase` is invisible to a previously built
`CardSearchIndex` until `rebuild()` runs again.

This was a genuine three-way choice:

1. **A live index holding `CardRecord*`, updated automatically as the database changes** -
   rejected: it requires either an observer/subscription mechanism (real complexity for a
   need this slice does not have) or accepting that pointers held by search results could be
   invalidated by an unrelated database mutation - a footgun `CardDatabase::find()`'s own
   pointer-stability contract (`card_database.h`) does not create for *its* callers, and this
   module should not create either.
2. **Recompute normalized strings on every search()** - rejected on measurement: §10 shows a
   full rebuild over 22,000 synthetic cards costs under 300 ms, and a search that is already
   fast (single-digit milliseconds) has no need to pay that cost on every keystroke.
3. **An explicit snapshot, rebuilt only when the caller calls `rebuild()`** (chosen). Simple,
   easy to reason about, and correct by construction: `search()` never touches
   `CardDatabase` at all, so there is no way for it to observe a half-updated database
   mid-mutation. The cost is that a caller who forgets to call `rebuild()` after loading a
   locale gets stale results - `data/tests/test_card_search.cpp`'s
   `search_reflects_the_active_locale_only_after_an_explicit_rebuild` and
   `a_database_override_is_invisible_to_search_until_rebuilt` pin this staleness
   *deliberately*, as the documented contract, not as a bug.

See ADR 0005, Decision 1 for the fuller reasoning.

---

## 4. The structured query, field by field

`data/include/edopro_next/data/search_query.h`:

- **`text` / `text_scope`.** Empty `text` means no text constraint at all - every entry
  passing the other filters matches (§8's `NoTextQuery` tier; see §11 for why this, not "no
  results," was chosen). Non-empty `text` is normalized once (§7) and split on whitespace
  into tokens; see §7 for exactly how tokens are matched. `TextScope` is `Name`/`Text`/
  `NameOrText` - never the sixteen auxiliary strings, matching §1.3.
- **`exact_code`.** A ranking hint, not a filter - see §8 and ADR 0005, Decision 3.
- **`type`/`category`/`link_marker`/`scope`: `BitmaskFilter{require_all_bits}`.** "This
  field's bits include every bit in `require_all_bits`" - the same shape as upstream's own
  `(data.type & filter) != filter` checks, named for what it does.
- **`race`: `BitmaskFilter64`.** Same shape, 64-bit, matching `CardRecord::race`'s real
  width (`docs/architecture/card-database.md`§2.4).
- **`attribute`: exact match**, matching upstream's own single-attribute selector.
- **`attack`/`defense`/`level`/`left_scale`/`right_scale`: `NumericFilter{value, comparison}`.**
  Three comparisons - `EqualTo`/`AtLeast`/`AtMost` - not upstream's six-way per-field
  filter-type scheme (exact / at-least / at-most / at-most-excluding-"?" /
  at-least-excluding-"?" / exactly-"?", `gframe/deck_con.cpp:1202-1226`). See §9 for why the
  "?" sentinel needs no special case here. `defense` never matches a Link monster (§9);
  `left_scale`/`right_scale` never match a non-Pendulum card (§9) - both mirroring
  `CheckCardProperties`'s own unconditional exclusions.
- **`setcodes`.** "At least one of these" (OR), alias-resolved once at `rebuild()` time
  (§1.5, §6) - not an archetype-name lookup (§1.5, §9).
- **`limit`.** Applied after ranking (§8), never before.

---

## 5. Why a structured type instead of the sigil language

`||`/`&&`/`*`/`$`/`$$`/`@`/`!!` (§1.1) each *do* something a future QML caller plausibly
wants - OR of independent queries, AND of constraints, multi-word matching, scope
restriction, archetype search, negation - but as a compact string grammar meant to be typed
in one text box by a keyboard-driven user, not as a programmatic API. A caller building a
structured filter UI (checkboxes, dropdowns, numeric range fields - the eventual QML deck
builder this module exists to support) would have to *construct a string in this grammar*
to express something it already has as typed values, then have this module parse that
string back apart - manufacturing a serialization round-trip with no reason to exist.

`SearchQuery` expresses the same *capabilities* (§4) as plain typed fields instead. Some of
the sigils' semantics carry over directly with a structured name (`$`/`$$` -> `text_scope`,
`*`-separated tokens -> whitespace-separated tokens in `text`, `@` -> `setcodes`); `||` has
no equivalent at all (a caller wanting the union of two independent queries calls `search()`
twice and merges - no reason for this module to do it for them); `!!` (negation) and `&&`
(AND across *sub-terms*, as opposed to AND across *tokens within one field*, which `text`
already provides) are not offered in this slice - see §12's deferred list.

A parser that accepts upstream's exact string grammar and *produces* a `SearchQuery` remains
possible to add later, for a power-user compatibility mode - but only if it turns out to be
small and genuinely wanted; ADR 0005, Decision 2 explains why this PR does not build one
speculatively.

---

## 6. `Entry`: what the snapshot actually stores

`CardSearchIndex`'s private `Entry` (`card_search_index.h`) holds exactly what `search()`
needs and nothing else: `CardCode`, the two normalized searchable strings, every static
metadata field `SearchQuery` can filter on, and `effective_setcodes` (§1.5's alias
resolution, resolved once at `rebuild()` time). It does **not** copy the sixteen auxiliary
strings (never searched, §1.3), does **not** keep `alias` itself (only its *resolved
consequence*), and does **not** keep a `CardRecord*` or any other pointer back into the
`CardDatabase` it was built from - see §3 for why that separation is load-bearing, not
incidental.

---

## 7. Normalization

`edopro_next::data::normalize_search_text` (`text_normalize.h`/`.cpp`) reproduces
`Utils::ToUpperChar`'s exact `wchar_t` table (`gframe/utils.h:320-358`) codepoint for
codepoint: the specific Latin-1 accented ranges it folds to `A`/`E`/`I`/`O`/`U`/`N`, the two
inverted-punctuation special cases (`¡`->`!`, `¿`->`?`), and - critically - **anything else
is left alone**. A codepoint above 255 (any non-Latin script, emoji, CJK) passes through
completely unchanged; a Latin-1 codepoint *not* in the explicit table (Æ, ß, Ø, Þ, Ð, Ç, and
their lowercase forms) also passes through unchanged, because upstream's own fallback -
`std::toupper(static_cast<int>(c))` - only affects those characters when the active C locale
says so, and the default `"C"` locale (verified empirically, no `setlocale` call anywhere in
this codebase's search path) does not. This module implements that same observable
behaviour directly (`c >= 'a' && c <= 'z'`) rather than calling the locale-dependent
`std::toupper`, so the result never depends on the embedding process's global locale state -
a deliberate strengthening, not a behavioural difference from what upstream actually does in
practice.

Working in UTF-8 rather than `wchar_t` sidesteps a real platform difference in upstream's
own representation (`wchar_t` is 16 bits on Windows, 32 on Linux) without changing any
result: every codepoint the fold table cares about fits in one UTF-16 code unit, so upstream's
narrower Windows `wchar_t` and this module's `char32_t` agree on every input regardless.
Malformed UTF-8 (a stray continuation byte, a truncated sequence) is not rejected - the
leading byte is treated as its own one-byte "codepoint" and decoding continues, so this
function can never throw or loop, which matters because it sits on a public boundary that
does not get to assume its input was pre-validated.

Query text is normalized once, then split on ASCII whitespace into tokens
(`whitespace_tokens`, `card_search_index.cpp`) - deliberately simpler than
`Utils::TokenizeString`'s generic multi-separator splitter, and, unlike
`Utils::ContainsSubstring` (§1.3), **order-independent**: `contains_all_tokens` checks each
token's presence anywhere in the field, independently, not left-to-right from where the
previous token matched. This is a deliberate divergence, chosen because the ordering
requirement in upstream's version is traceable to *how* `ContainsSubstring` happens to be
implemented (a single forward-advancing `find()`), not to any stated intent, and because
order-independent "all these words, anywhere" matches ordinary search-box expectations more
directly than upstream's implicit left-to-right requirement would.

---

## 8. Ranking

`MatchKind` (`search_result.h`) is declared in priority order and `search()` sorts by it
directly, then by a deterministic tie-break:

1. **`ExactCode`** - `SearchQuery::exact_code` was set and this entry is that code, having
   independently passed every other active filter (`text` included) - see §4 and ADR 0005,
   Decision 3 for why this is a ranking override, not an implicit filter.
2. **`ExactName`** - the normalized query equals the entry's normalized name exactly.
3. **`NamePrefix`** - the entry's normalized name starts with the normalized query.
4. **`NameMatch`** - every token is present somewhere in the normalized name (order-independent, §7).
5. **`TextMatch`** - no name-tier match, but every token is present in the normalized text.
   Only reachable under `TextScope::Text`/`NameOrText`.
6. **`NoTextQuery`** - `text` was empty; every filter-passing entry lands here uniformly.

Within one tier, results are ordered by normalized name ascending, then `CardCode` ascending
- a total order, so the same index and query always produce byte-for-byte-identical output
(pinned by `the_same_index_and_query_return_identical_ordered_results_every_time`,
`data/tests/test_card_search.cpp`). `limit` (§4) truncates this already-ranked sequence,
never influences what gets ranked.

This is a new scheme, informed by but not copied from `SortList` (§1.4), which sorts by a UI
column selection, not by relevance to a text query - see ADR 0005, Decision 1.

---

## 9. Numeric filters: sentinels, Link monsters, Pendulum scales

`NumericFilter` has three comparisons - `EqualTo`/`AtLeast`/`AtMost` - deliberately smaller
than upstream's six-way per-field filter-type scheme (§4). The "?" ATK/DEF sentinel (`-1`/
`-2`, real displayed values, not decode errors - `card_record.h`) needs no special case: a
realistic `AtLeast` threshold already excludes it via ordinary signed comparison (`-2 >=
2500` is false), exactly reproducing upstream's own observable result for that case without
this module knowing what "?" means; a caller specifically wanting only "?" cards uses
`EqualTo` with `-1` or `-2` directly.

`defense` never matches a Link monster (`type & TYPE_LINK`, `0x4000000` -
`ocgcore/ocgapi_constants.h:58`, same citation precedent as `card_database.cpp`'s
`kTypeLinkBit`) - a Link monster's `defense` column is not a defense value at all
(`card_record.h`; `docs/architecture/card-database.md`§2.2), matching
`CheckCardProperties`'s own unconditional `|| (data.type & TYPE_LINK)` exclusion. Similarly,
`left_scale`/`right_scale` only ever match a card with `TYPE_PENDULUM` set (`0x1000000`),
matching `CheckCardProperties`'s own `|| !(data.type & TYPE_PENDULUM)` exclusion.

`level`'s stored type is `uint32_t`, and a negative packed level wraps to a large unsigned
value rather than staying negative - a deliberate, source-faithful M3A decision
(`docs/architecture/card-database.md`§2.3), not something this module treats specially.
Comparing that wrapped value against a caller's `NumericFilter` (widened to `int64_t`,
which preserves the exact ordering a `uint32_t`-vs-`uint32_t` comparison would give, since
widening an unsigned value into a larger signed type never changes its relative order) means
a huge wrapped level participates in an `AtLeast` filter exactly as it would for upstream's
own `uint32_t`-typed comparison - `data/tests/test_card_search.cpp`'s
`a_negative_encoded_level_participates_as_a_huge_unsigned_value` pins this directly, the same
case `card-database.md`'s own `-249 -> 4294967289` example uses.

---

## 10. Performance: measured, not assumed

The roadmap item is "fast search," which does not automatically mean a complex index -
measured first, per CLAUDE.md's validation-proportionate-to-what-changed guidance and this
task's own explicit instruction not to reach for an inverted index "because 'search engine'
sounds like it should have one."

**Dataset:** `data/tests/bench_card_search.cpp` generates 22,000 synthetic cards (a real
Yu-Gi-Oh card pool is on the order of 13,000 unique cards; 22,000 gives comfortable margin)
with realistic-length names (two-word adjective+noun plus an id suffix) and rules text
(2-5 concatenated sentence fragments drawn from a small fixed pool, roughly 150-400
characters each) - deterministic (a fixed RNG seed), so the benchmark's own numbers are
reproducible across runs, and entirely synthetic - no real card names or text.

**Measured** (this development environment: WSL2/Ubuntu on the machine this session ran on,
g++ 15.2.0, `-O2`; not a dedicated benchmarking rig, and not necessarily representative of
every target platform - reported as one honest data point, not a guaranteed number):

| Operation | Time |
|---|---|
| `CardDatabase::load_database` (22,000 rows) | ~254 ms |
| `CardSearchIndex::rebuild` (cold) | ~291 ms |
| `CardSearchIndex::rebuild` (warm, repeat) | ~291 ms |
| Exact-name query (no match in this synthetic set - full scan, worst case) | ~8.1 ms |
| Name-prefix query (~1,100 hits) | ~7.9 ms |
| Broad text-scan query, `TextScope::Text` (~8,000 hits) | ~11.0 ms |
| Filtered query (type+ATK+level, no text, ~8,000 hits) | ~6.2 ms |
| Ranked broad name query (~1,500 hits) | ~7.3 ms |

Every query - including the worst-case full scan with no match at all - lands in the
single-digit-to-low-double-digit millisecond range, comfortably inside normal UI
responsiveness budgets, with **no index beyond a `std::vector` of precomputed, normalized
strings and a linear scan**. `rebuild()`'s ~300 ms is a one-time cost paid when a database or
locale actually changes, not per search - see §3. On this evidence, an inverted index,
n-gram/token index, SQLite FTS, or any third-party search library would add real complexity
and a new dependency (CLAUDE.md's "no new dependencies without justification") to solve a
problem this measurement shows does not exist at this scale. None was added.

`bench_card_search` is a plain executable, not a `ctest` case (`data/CMakeLists.txt`) - wall
clock numbers are not CI pass/fail criteria, matching this task's explicit instruction; it
exists to be re-run and re-measured, not to gate a build.

---

## 11. The empty-query contract

An empty `SearchQuery::text` means "every entry passing the other active filters matches,"
ranked uniformly as `NoTextQuery` (§8) - not "match nothing." This was a deliberate choice
between two reasonable options: a caller building a "browse the whole (optionally filtered)
catalogue" view - the deck builder's initial, no-search-typed-yet state - gets that for free
by passing a `SearchQuery` with only static filters set and no `text`, rather than needing a
separate "list everything" code path alongside `search()`.
`empty_text_query_returns_every_card_as_no_text_query` (`data/tests/test_card_search.cpp`)
pins this as tested contract.

---

## 12. Deliberately deferred

- **Legality of any kind** - see §0/§2. Not begun, not planned for this module ever;
  belongs, if built, in an explicitly separate, explicitly reviewed layer above `data/`.
- **Archetype-name resolution** (`@name` -> setcodes, §1.5) - needs a set-name string
  resource this project's `data/` module does not yet represent at all. `setcodes` (§4)
  already covers the numeric half.
- **`||`/`&&`/`!!` compositional operators, and a compatibility parser for upstream's exact
  string grammar** - see §5. A caller wanting OR composes multiple `search()` calls itself;
  negation and a legacy-syntax parser are both plausible small additions later, not built
  speculatively here.
- **Fuzzy matching, edit distance, phonetic search, stemming, autocomplete/prediction** -
  none of this exists upstream either; not part of this slice.
- **Asynchronous/background search, debouncing** - §13; this module is synchronous by
  design, and fast enough (§10) that the eventual UI layer may not need either.
- **Inverted/token index or any third-party search dependency** - considered and rejected on
  measurement, §10.

---

## 13. Threading

`CardSearchIndex` has no internal synchronization: `rebuild()` and `search()` are ordinary,
non-atomic member functions. `search()` only reads `entries_` and never mutates it, so
multiple threads may call `search()` concurrently against one already-built snapshot; calling
`rebuild()` concurrently with any `search()` (or with another `rebuild()`) on the same
instance is not a guarantee this class makes, and is the caller's responsibility to
serialize, like any other ordinary C++ value type with no stated thread-safety contract. No
worker thread, task queue, or asynchronous API is introduced in this slice - §10's
measurements suggest the eventual UI layer may not need one at all, and if it turns out to,
that decision belongs with the caller that actually has latency/responsiveness requirements
to weigh, not baked into this module speculatively.

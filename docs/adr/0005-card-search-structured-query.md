# ADR 0005 — Fast card search: an explicit snapshot and a structured query

## Context

M3's third item (`docs/ROADMAP.md`) is "Fast search over the full card pool" - a
presentation-independent way to find cards in a loaded `CardDatabase`, fast enough for a
future QML deck builder's search box, with no legality, no deck mutation, and no UI of any
kind bundled in. `docs/architecture/card-search.md` is the source-verified account of what
upstream's deck-builder search actually does; this ADR is the narrower record of the three
choices in that account that were genuinely debatable.

## Decision 1 — An explicit, rebuilt-on-demand snapshot, not a live index

`CardDatabase` can change after a `CardSearchIndex` is built - another `load_database()`,
a `load_locale()`, a `clear_locale()` - and names/text are search inputs, so this matters.

### Options considered

1. **A live index holding `CardRecord*`, kept in sync automatically** - the closest
   analogue to `CardDatabase::find()`'s own pointer-stability contract, but it needs either
   an observer/subscription mechanism on `CardDatabase` (real complexity for a need this
   slice does not have - `CardDatabase` has no such mechanism today, and adding one only for
   this would grow that module for a caller-count-of-one) or accepting stale/dangling
   pointers as a known risk, which is exactly the kind of footgun `find()`'s own stability
   contract exists to avoid handing to *its* callers.
2. **Recompute normalized strings on every `search()` call, no persistent snapshot at all** -
   simplest to reason about, but rejected on measurement:
   `docs/architecture/card-search.md`§10 shows normalizing and indexing 22,000 synthetic
   cards costs under 300 ms - fine to pay once when the catalogue changes, wasteful to pay on
   every keystroke of an already-fast (single-digit-millisecond) search.
3. **An explicit, private snapshot, rebuilt only when the caller calls `rebuild()`**
   (chosen). `search()` never touches `CardDatabase` at all, so it cannot observe a
   half-updated database mid-mutation, and results identify cards by `CardCode` only - never
   a pointer into snapshot- or database-owned storage - so a result stays meaningful across a
   `rebuild()` or even after the index is destroyed.

### Consequence

A caller that changes `CardDatabase` and forgets to call `rebuild()` gets stale results, not
an error - `docs/architecture/card-search.md`§3 and
`data/tests/test_card_search.cpp`'s `search_reflects_the_active_locale_only_after_an_
explicit_rebuild`/`a_database_override_is_invisible_to_search_until_rebuilt` pin this as the
documented contract. The eventual application layer is responsible for calling `rebuild()`
after any card-data change it cares about being searchable - the same shape as
`CardDatabase::clear_locale()`/`load_locale()` already puts on its own callers for locale
selection (`docs/architecture/card-database.md`§4).

## Decision 2 — A structured query type, not upstream's sigil mini-language

Upstream's search box accepts one string in a compact grammar - `||`/`&&`/`*`/`$`/`$$`/`@`/
`!!` (`docs/architecture/card-search.md`§1.1) - built for a single keyboard-driven Irrlicht
text field.

### Options considered

1. **Expose the sigil grammar directly - a `search(std::string_view query)` that parses the
   legacy syntax** - rejected: a future QML caller with typed filter state (checkboxes,
   dropdowns, numeric range fields - the deck builder this module exists to support) would
   have to *construct* a string in this grammar to express values it already has typed, then
   have this module parse that string back apart. That is a serialization round-trip
   invented for no reason, and it would make this module's fundamental contract a string
   grammar instead of a type.
2. **A structured `SearchQuery` value type expressing the same underlying capabilities**
   (chosen) - `text`/`text_scope` for `$`/`$$`/plain search, `setcodes` for `@`,
   whitespace-separated tokens in `text` for `*`, typed optional fields for every static
   filter (`docs/architecture/card-search.md`§4). `||` (union of independent queries) has no
   field at all - a caller wanting it calls `search()` twice and merges, which needs nothing
   from this module. `&&` across sub-terms and `!!` negation are not offered in this slice
   (see §12's deferred list) - both are plausible small additions later, layered on top of
   `SearchQuery`, not reasons to expose the string grammar now.
3. **Build a compatibility parser from the legacy string grammar into `SearchQuery`, in this
   same PR, so power users keep exact keyboard parity** - deferred, not rejected outright:
   this task's own scope explicitly says only to build it if source research shows it is
   "tiny and genuinely necessary." Having now traced the grammar in full
   (`docs/architecture/card-search.md`§1.1), it is small but not *trivial* - it needs its own
   token/precedence handling and its own test coverage for exactly the kind of edge cases
   this project treats seriously (empty tokens, prefix-vs-`$$`-vs-`$` disambiguation,
   `!!`/`@` combined with empty token sets). Bundling it into the PR that establishes the
   *structured* API risks conflating two different reviews - "is the query model right" and
   "is this string parser source-faithful" - so it is left for a follow-up slice if a
   keyboard-parity power-user mode is actually wanted once the structured API has UI
   consumers.

## Decision 3 — `exact_code` is a ranking hint within the unified result set, not a filter

`SearchQuery::exact_code` exists so a caller does not have to parse `text` as a number
itself (this task's own explicit instruction). Upstream's closest analogue -
`FilterCards`'s `trycode`/`BufferIO::GetVal` check (`gframe/deck_con.cpp:1096-1106`) - treats
a numeric search *term* as a complete substitute for that term's normal matching, but that
happens per-`||`-term in a list of independently-OR'd terms, a shape `SearchQuery` does not
have (Decision 2). `exact_code` needed its own, independently reasoned semantics.

### Options considered

1. **`exact_code` restricts the entire search to just that one code** - the naive reading of
   "exact code lookup," and the first implementation attempt here. Rejected once its actual
   consequence became clear: it makes `exact_code` combined with `text`/other filters
   *replace* them instead of narrowing among them, so `{text: "Dragon", exact_code: X}` would
   silently stop being "Dragon cards, with X pinned to the top" and become "only X, if X
   happens to be a dragon" - a surprising interaction for two fields that read as
   independently composable.
2. **`exact_code` is a pure ranking override: every entry is still evaluated against every
   other active filter and `text` normally; the one entry (if any) whose code matches
   `exact_code` is reported with `MatchKind::ExactCode` - the top-priority tier - instead of
   whatever tier its own text match would have earned** (chosen). This matches the ranking
   framing this task's own instructions used for `MatchKind` ("exact card-code hit, *if
   explicitly queried*" - listed as simply the top tier of one unified ranking, not a
   separate query mode), composes predictably with every other field (it can only ever
   *promote* a result that already qualifies, never manufacture one that does not), and
   keeps `search()` a single, uniform evaluation pass over every candidate rather than two
   different algorithms selected by whether `exact_code` happens to be set.

### Consequence

`exact_code` naming a code that does not exist, or that fails `text`/another active filter,
contributes nothing and excludes nothing else - the rest of the query still runs normally.
`data/tests/test_card_search.cpp`'s `exact_code_restricts_to_that_one_code_and_ranks_it_
first`, `exact_code_for_a_missing_code_does_not_narrow_an_otherwise_unconstrained_query`, and
`exact_code_for_a_missing_code_with_a_non_matching_text_query_matches_nothing` pin these
three cases directly - the corrected shape survived the first, wrong implementation being
caught by exactly the deliberately-adversarial test suite this project's tests are meant to
be.

## Status

Accepted. Implemented in `data/include/edopro_next/data/{search_query.h,search_result.h,
card_search_index.h,text_normalize.h}`, `data/src/{card_search_index.cpp,text_normalize.cpp}`,
and pinned by `data/tests/test_card_search.cpp`.

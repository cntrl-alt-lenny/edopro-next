# ADR 0012 — The deck builder's search filters follow upstream's filter window

## Context

Upstream's deck builder has a filter window beside its search box (`gframe/game.cpp:661-743`):
card type and sub-type, attribute, race, four number boxes (ATK, DEF, Level/Rank, Scale), an
effect-category popup, a link-marker popup, a "limit" list and a "show non-official cards"
switch. `DeckBuilder::StartFilter` reads them (`gframe/deck_con.cpp:1040-1058`) and
`DeckBuilder::CheckCardProperties` applies them (`:1191-1324`). Before this round the deck
builder here searched by text only, although `data::SearchQuery` already had most of the
matching fields ([ADR 0005](0005-card-search-structured-query.md)).

[card-search.md](../architecture/card-search.md) §2.2 and §2.3 record upstream's behaviour
for every control, input form and default exclusion, quoted at file and line. This ADR records
the decisions made in reproducing it.

## Decision 1 — Numeric filters reproduce every upstream input form; `data/` grows to do so

Upstream turns the text of each number box into one of six comparisons (`parse_filter`,
`deck_con.cpp:21-51`, and `BufferIO::GetVal`, `gframe/bufferio.h:240-249`). card-search.md
§2.1 called `NumericFilter`'s three comparisons a "deliberate simplification" that needed no
special case for the "?" sentinel. Checking every form against it (card-search.md §2.2) showed
that is not so:

- "at most" (`<=`, `<`) on ATK and DEF rejects a negative ("?") stat
  (`deck_con.cpp:1204-1205`, `:1210-1211`); `AtMost` alone keeps it;
- "?" means ATK or DEF equal to **-2**, not -1 (`:1205`, `:1211`), and on Level or Scale it
  matches no card at all (`:1218`, `:1224`);
- after a recognised first character, the number is read in wrapping unsigned 32-bit
  arithmetic, and trailing characters make it **0** rather than cancelling the filter
  (`bufferio.h:240-249`): "1500a" means "equal to 0";
- text starting with anything else (including a space or a minus sign) sets no filter.

Options:

1. **Keep the three comparisons and record each difference as a divergence.** Rejected: the
   user types the same text into the same box and gets different cards, for common inputs
   like `<=1000`.
2. **Mirror the six filter types as an enum in `data/`.** Rejected: strict `>` and `<` are
   `AtLeast n + 1` and `AtMost n - 1` on integers, and "?" is `EqualTo(-2)` or an unmatchable
   value, so four of the six add states without adding results.
3. **Add one flag and a parser** (chosen). `NumericFilter` gains `excludes_negative`, set for
   "at most" on ATK/DEF; `data::parse_numeric_filter(text, field)`
   (`numeric_filter_text.h`) reproduces `parse_filter` and `GetVal`, including the 32-bit
   width each box uses (`int32_t` for ATK/DEF, `uint32_t` for Level/Scale,
   `gframe/deck_con.h:115-122`). "?" on Level/Scale becomes `EqualTo(-1)`, which no widened
   unsigned field can equal.

`data/tests/test_numeric_filter_text.cpp` checks every form (prefixes × numbers × oddities,
184 inputs) on all four boxes against a transcription of upstream's parser, comparisons and
card decode, requiring the same set of cards. The text becomes a comparison in `data/`, not
in `ui/`, so the rule has one home and no Qt dependency.

## Decision 2 — Spell and Trap sub-types compare the whole type word

Upstream's Monster sub-type is an all-bits check (`deck_con.cpp:1196`), but its Spell and Trap
sub-types compare the whole type word: `if(filter_type2 && data._data.type != filter_type2)`
(`:1233`, `:1240`). "Normal Spell" is `TYPE_SPELL` (`game.cpp:3396`), so it finds only cards
whose type is exactly `TYPE_SPELL`. card-search.md §2.1 recorded `type`'s all-bits shape as a
deliberate uniform rule; with it, "Normal Spell" would find every Spell. `SearchQuery` gains
`type_equals`, and the deck builder uses it for Spell and Trap sub-types. `type` keeps its
all-bits meaning.

## Decision 3 — The banlist-dependent part lives in `policy/`, as one function

The cards upstream hides before any filter (`deck_con.cpp:1192-1193`) and the whole limit list
(`:1254-1322`) depend on the selected banlist: a whitelist shows non-official cards and hides
every card not on it, and banned/limited/semi-limited/unlimited count against the list.
`policy::deck_search_admits(record, lflist, show_non_official, filter)`
(`policy/include/edopro_next/policy/deck_search_filter.h`) reproduces those lines together,
and `limitation_filter_choices()` reproduces which choices the list offers
(`Game::ReloadCBLimit`, `game.cpp:3412-3442`). `data/` stays free of legality
(card-search.md §0).

The limit list also holds card-pool categories (OCG, TCG, Anime, Rush, ...) that read only the
card's scope. They are kept in the same function because upstream runs them inside the same
branch, followed by the same whitelist check (`:1320-1321`), and splitting them would make the
whitelist behaviour depend on two layers agreeing. `LfList` lookups go through
`policy::limitation_for`, moved from `deck_validation.cpp` to `lf_list.h` so validation and
search share one copy of `LFList::GetLimitationIterator` (`gframe/deck_manager.h:23-30`).

This project has a "No banlist" choice, which upstream does not (it always has a list,
`deck_con.cpp:74`, `:513`). With no list, the four banlist counts are not offered, since
there is nothing to count against; the other choices behave as with an empty, non-whitelist
list. `policy/tests/test_deck_search_filter.cpp` compares both functions with a
transcription of upstream over every scope-bit combination, token or not, every count
(including through an alias), a blacklist, a whitelist and no list, both switch states and all
17 filters.

The search applies `deck_search_admits` to `CardSearchIndex`'s ranked results before the
deck builder's 200-result cap, so a card it drops never takes one of the 200 places.

## Decision 4 — Search hides what upstream hides, by default

Round 020 left tokens visible in search, though they cannot be added (ADR 0011). This round
reproduces upstream's default visibility instead:

- tokens and hidden cards are never listed, whatever the switch or list says;
- non-official cards (any scope bit outside OCG, TCG and Prerelease) are listed only while
  "Show non-official cards" is on or the selected list is a whitelist;
- with a whitelist, only the cards on it are listed unless the limit list is set to "All
  cards".

Options: keep showing everything (rejected: the search would list cards upstream's never
shows, including tokens that the add buttons then refuse), or hide tokens only (rejected: a
partial copy of one upstream line is a second, weaker rule). The switch starts off, as
upstream's does (`show_unofficial`, default `false`, `gframe/game_config.inl:73`). Upstream
saves the switch in its configuration (`game.cpp:2557`); this project does not persist it
yet. That is a recorded divergence.

## Decision 5 — Labels are this project's; values are upstream's

Upstream takes every label in the filter window from its string resource (`strings.conf`,
through `DataManager::GetSysString`), which this repository does not contain and does not
commit. The values behind the labels (type bits, attribute bits, race bits, category bits,
marker bits, limit filters) are upstream's, in upstream's order, cited in
`ui/src/deckbuilder/search_filters.cpp`. The labels are:

- card types, sub-types, attributes and limit choices: English names of the constants they
  select;
- races: the names `ocgcore/ocgapi_constants.h:71-103` gives. Upstream always offers bits
  0-31, and bits 32-63 only when its strings name them (`game.cpp:3449-3462`). This project
  offers bits 0-31 and `RACE_YOKAI` (bit 62), the one higher race ocgcore defines;
- effect categories: "Category N (0xBIT)". Upstream's names for the 32 bits are strings
  1100-1131 (`game.cpp:722`), which are not in this repository, and this project does not
  invent names for them. The filter works; the labels are not descriptive.

## Decision 6 — What deliberately differs from upstream's filter window

- **When the search runs.** Upstream runs it on Enter, on a combo box change, on the category
  and marker OK buttons, and on typing in ATK/DEF only once the text is longer than two
  characters (`deck_con.cpp:471-516`). Here every change re-runs it. The cards found for a
  given set of choices are the same.
- **Clear.** Upstream's Clear resets the filters and the name box and empties the result list
  until the next search (`deck_con.cpp:1363-1397`). Here Clear resets the same filters and the
  search text, and the list shows every visible card, as it does whenever nothing is typed
  (card-search.md §11). Neither resets the non-official switch.
- **The limit choice when the list of choices changes.** Upstream rebuilds the list on a
  banlist change and on the switch, keeping the selected position if it was below 8
  (`deck_con.cpp:607-612`). Positions mean different filters in the whitelist and
  non-whitelist lists (`game.cpp:3412-3442`). Here the chosen filter is kept if it is still
  offered, and otherwise falls back to the first choice. What Irrlicht's combo box selects
  after upstream clears and refills it was not read.
- **A number typed alone in the search box.** Upstream treats a search term that is a known
  card code as that card, bypassing every filter (`deck_con.cpp:1096-1106`). That belongs to
  the search-text grammar, which is a later round, and is not reproduced.
- **Order.** Results are ranked as card-search.md §8 describes; upstream sorts by a
  user-chosen column (§1.4). Unchanged by this round.

## Consequences

- card-search.md §2.1 no longer calls the numeric comparison a simplification, and records
  `type_equals` (§2.1, §2.2, §9). §1.2 and §2 name `policy::deck_search_admits` as the home of
  the banlist-dependent half.
- [ADR 0011](0011-extra-deck-classification.md)'s note that the search lists tokens is
  superseded by Decision 4.
- A future search-text grammar round (`@`, `$`, `!!`, `||`, `&&`, code lookup) adds to the text
  side only; the filter side is complete apart from the labels in Decision 5.

## Status

Accepted. Implemented in `data/include/edopro_next/data/{search_query.h,numeric_filter_text.h}`,
`data/src/{card_search_index.cpp,numeric_filter_text.cpp}`,
`policy/include/edopro_next/policy/{deck_search_filter.h,lf_list.h}`,
`policy/src/{deck_search_filter.cpp,lf_list.cpp,deck_validation.cpp}`,
`ui/src/deckbuilder/{search_filters,search_results_model}.{h,cpp}` and
`ui/qml/screens/DeckBuilderScreen.qml`; pinned by `data/tests/test_numeric_filter_text.cpp`,
`data/tests/test_card_search.cpp`, `policy/tests/test_deck_search_filter.cpp`,
`ui/tests/test_deckbuilder.cpp` and `ui/tests/test_deckbuilder_screen.cpp`.

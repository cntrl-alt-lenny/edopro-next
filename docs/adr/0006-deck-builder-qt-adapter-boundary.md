# ADR 0006 — Deck builder: a Qt adapter layer, and one canonical `Deck`

## Context

M3D1 (`docs/architecture/deck-builder-ui.md`) needed to give the reviewed, Qt-free `data/`
facades (`CardDatabase`, `CardSearchIndex`, `Deck`/`.ydk` codec - M3A/M3B/M3C) a real QML
screen without breaking CLAUDE.md's core rule that the rules/data engine must not become the
UI and the UI must not implement engine-adjacent logic. Two decisions in that design were
genuinely debatable enough to record here rather than leave as unexplained code shape.

## Decision 1 — A Qt C++ adapter layer between `data/` and QML, not QML-direct access

### Options considered

1. **Expose `data/` types to QML directly** (e.g. register `CardDatabase`/`CardSearchIndex`
   as QML types, hand `CardRecord*`/`SearchResult` straight to delegates). Rejected: this
   would force `data/` to either link Qt (breaking its own reviewed Qt-free boundary,
   `card-database.md`, `card-search.md`, `deck-model.md`) or expose raw pointers into
   snapshot-owned storage to QML's garbage-collected/duck-typed JS environment, which has no
   way to honor the C++ lifetime contracts those pointers carry (`CardSearchIndex::search()`'s
   own documented pointer-free-by-design result shape, [ADR 0005](0005-card-search-structured-query.md)
   Decision 1 exists specifically to avoid this class of problem on the C++ side; handing a raw
   pointer to QML would reopen it one layer up).
2. **A thin Qt C++ adapter layer (`ui/src/deckbuilder/`) that is the only code including both
   a Qt header and a `data/` header** (chosen). `data/` stays exactly as reviewed - no new
   dependency, no new public API surface added for this UI's sake. QML only ever sees Qt
   primitives, `Q_GADGET` value snapshots (`CardEntry`), or `QAbstractListModel` role data -
   never a `data/` type or pointer.

### Consequence

Every `data/` value crossing into QML is copied into a small, purpose-built Qt shape at the
adapter boundary (`CardEntry`, `DeckSectionModel`'s roles, `SearchResultsModel`'s roles) -
one extra small class per shape, in exchange for `data/` never needing to know Qt exists and
QML never needing to reason about C++ object lifetime at all.

## Decision 2 — One canonical `Deck` owned by `DeckController`; views hold pointers into it, never a second copy

### Options considered

1. **Each QML-facing model owns its own copy of the relevant section** (e.g.
   `DeckSectionModel` holds its own `std::vector<CardCode>`, synced from `DeckController` on
   every change via an explicit copy or a signal/slot re-fetch). Rejected: this creates two
   sources of truth that a missed signal, a partial update, or a future bug could desynchronize
   - exactly the "second model of canonical deck contents" CLAUDE.md's presentation/rules
   separation and this task's own instructions explicitly rule out for QML, and rejecting it
   for a sibling C++ model for the identical reason.
2. **`DeckSectionModel` holds a raw, non-owning `const std::vector<CardCode>*` directly into
   `DeckController`'s own `Deck` member, and `DeckController` calls `notifyInserted`/
   `notifyRemoved`/`notifyReset` immediately after every mutation it performs** (chosen). There
   is exactly one `Deck` value in the process for a given editing session; the three section
   models are views over slices of it, never independent state.

### Consequence

This is only safe because `DeckController::deck_` is a plain member whose *address* never
changes for the controller's lifetime, even when reassigned wholesale
(`deck_ = std::move(result.deck)` in `loadDeck()`, `deck_.clear()` in `newDeck()`) - only
`Deck`'s own three vectors' contents change. `DeckSectionModel` would dangle if `DeckController`
ever stored `Deck` behind a pointer/`unique_ptr` and reallocated it; it does not, and this ADR
records that constraint as load-bearing for anyone changing `DeckController`'s storage later.
The corresponding cost: `DeckController` is the *only* code path permitted to mutate `deck_`,
and it must remember to call the matching `notify*` after every mutation - there is no
automatic change-detection mechanism enforcing this at compile time. Reviewed and accepted as
proportionate for a single-writer class this small; `ui/tests/test_deckbuilder.cpp`'s
`addRemoveOrderAndMultiplicity`/`loadPreservesOrderDuplicatesAndUnknownCodes` exercise the
model-visible effects of every mutating call, which would fail if a `notify*` call were ever
dropped for an existing operation.

A card's deck section is, by the same reasoning as
[ADR 0004](0004-deck-model-ydk-codec.md)'s explicit-sections decision, always the caller's
(ultimately the user's) explicit choice - `DeckController::addCard(code, section)` takes
`section` as a required parameter and never inspects `CardEntry::isMonster`/`isLink`/
`isPendulum` to choose or override it. Those classification bits exist on `CardEntry` solely
for presentation (`docs/architecture/deck-builder-ui.md`§7, §10) - the same "classify for
display, never for section-membership" split `CardSearchIndex` already established for its own
static filters ([ADR 0005](0005-card-search-structured-query.md)).

## Status

Accepted. Implemented in `ui/src/deckbuilder/{card_entry,card_catalog,search_results_model,
deck_section_model,deck_controller}.{h,cpp}`, wired into `ui/qml/screens/DeckBuilderScreen.qml`,
and pinned by `ui/tests/test_deckbuilder.cpp`. Full design detail:
[deck-builder-ui.md](../architecture/deck-builder-ui.md).

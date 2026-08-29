# The deck builder UI: Qt/QML adapter boundary

What `ui/src/deckbuilder/` and `ui/qml/screens/DeckBuilderScreen.qml` actually do, why the
Qt/data dependency runs in one direction only, who owns the one canonical `Deck` for an
editing session, and what this slice (M3D1) deliberately leaves for later. This document is
about the new Qt-facing layer only; it does not re-derive what `data/` itself does or why -
that is already covered by [card-database.md](card-database.md), [deck-model.md](deck-model.md)
and [card-search.md](card-search.md), and this layer is reviewed against those contracts, not
a reimplementation of them.

---

## 0. What this slice is not

It is not the complete M3 "Deck builder UI" roadmap item. There is no legality, no banlist,
no deck-size or copy-count limit, no automatic Main/Extra classification, no artwork, no
archetype-name search, no controller/gamepad navigation, and no full keyboard parity with
upstream. `docs/ROADMAP.md`'s M3 entry stays unchecked. See §12.

---

## 1. Upstream archaeology this layer is built against

Read from source before writing any of `ui/src/deckbuilder/`, at the same pinned upstream
base `docs/UPSTREAM.md` records:

- **`gframe/deck_con.cpp`, class `DeckBuilder`** - the Irrlicht deck-builder screen.
  `Initialize()` (`:54-94`) builds every Irrlicht widget and loads the current deck via
  `DeckManager::LoadDeck`; `Terminate()` (`:95-128`) tears widgets down and optionally
  returns to the menu; `SetCurrentDeckFromFile()` (`:129-135`) and `ImportDeck()` (`:136-146`)
  are the file-open paths; `FilterCards()`/`CheckCardProperties()` (`:1059-1340`) implement
  the sigil search grammar `card-search.md`§1.1 already documents in full;
  `push_main`/`push_extra`/`push_side`/`pop_main`/`pop_extra`/`pop_side` (`:1577-1672`) are
  the section-mutation primitives, each taking an explicit `DeckType` destination - there is
  no upstream function that decides a card's section from its type at push time either;
  classification-on-load is a separate, later step (§7, and `deck-model.md`§3).
- **`gframe/deck_manager.{h,cpp}`** - `LoadCardList`/`SaveDeck`/`LoadDeck`, already fully
  documented in [deck-model.md](deck-model.md). Nothing here re-derives that; `edopro_next_deck`
  (`data/`) is the only code in this project that parses or writes `.ydk` text, including from
  this UI layer (§9).
- **`gframe/data_handler.cpp`** - `DataHandler::LoadDatabases()` (`:28-39`) loads
  `./cards.cdb` if present, then every `.cdb` under `./expansions/` (both fixed, relative,
  working-directory-based conventions), then `LoadArchivesDB()` (`:40-52`) scans configured
  archive files for further `.cdb` entries - three different discovery mechanisms, all
  ultimately calling `DataManager::LoadDB` once per file, in order, with **no rollback and no
  atomicity across files**: a later file's rows simply overwrite an earlier file's for the
  same card id (`card-database.md`§4's overlay precedence). This UI layer's bootstrap (§4)
  keeps that same "each path loads in order, last file wins, one failure does not stop the
  rest" shape, deliberately without reproducing the fixed-relative-path/archive-scanning
  *discovery* mechanism itself (§4).

No Irrlicht widget, layout constant, or pixel-level UI decision from `deck_con.cpp` is carried
into the QML screen (§10) - only the *operations* (search, add-to-section, remove, load, save,
new) and the *data shapes* they act on.

---

## 2. Dependency direction: `data/` is Qt-free; the adapter is the only seam

```
edopro_next_data / edopro_next_deck / edopro_next_search   (Qt-free, reviewed in M3A/M3B/M3C)
                              ^
                              |  plain C++ calls only
                              |
                    ui/src/deckbuilder/*             (Qt: QObject, QAbstractListModel, Q_GADGET)
                              ^
                              |  Q_PROPERTY / Q_INVOKABLE / context properties
                              |
                         ui/qml/*.qml                (QML: visual layout, selection, focus)
```

`data/CMakeLists.txt` links no Qt library, before or after this PR - unchanged by this slice.
`ui/src/deckbuilder/` is the only code that includes both a Qt header and an
`edopro_next/data/*.h` header in the same translation unit. QML never includes, links, or
directly names a `data/` type: every value that crosses into QML is either a Qt primitive
(`quint32`, `QString`, `bool`), a `Q_GADGET` value snapshot (`CardEntry`, §5), or a
`QAbstractListModel` role value (§6, §7) - never a `CardRecord*`, `CardSearchIndex&`, or raw
`std::vector<CardCode>`.

---

## 3. New C++ classes and who owns what

| Class | Owns | Never does |
|---|---|---|
| `CardCatalog` (`card_catalog.h/.cpp`) | The process's one `CardDatabase` and matching `CardSearchIndex`. | Decide where `.cdb` files live (§4); mutate a `Deck`. |
| `SearchResultsModel` (`search_results_model.h/.cpp`) | A `QAbstractListModel` of the current query's `CardSearchIndex::search()` results. | Own card data (resolves through `CardCatalog` at display time); mutate the deck (§8 pins this). |
| `DeckSectionModel` (`deck_section_model.h/.cpp`) | Nothing - a read-only view. Holds a raw `const std::vector<CardCode>*` straight into `DeckController`'s own `Deck`. | Mutate its section; exist independently of a `DeckController` (`QML_UNCREATABLE`). |
| `DeckController` (`deck_controller.h/.cpp`) | **The single canonical `edopro_next::data::Deck` for this editing session** (§7), current file path, dirty flag, last error. | Decide a card's section (§7); parse/serialize `.ydk` itself (delegates to `edopro_next::data::load_ydk`/`save_ydk`). |
| `CardEntry` + `make_card_entry()` (`card_entry.h/.cpp`) | Nothing - a `Q_GADGET` value snapshot, copied by value, no lifetime coupling to the `CardDatabase` that produced it. | Get constructed with invented data for an unknown code - `known = false` and every other field at its default (§7). |

This is deliberately five small, single-purpose classes rather than folding any of this into
`AppContext`. `AppContext` (`ui/src/appcontext.{h,cpp}`) is untouched by this PR; `main.cpp`
constructs `CardCatalog`/`DeckController` itself and publishes them as separate
`rootContext()` properties (`cardCatalog`, `deckController`) alongside whatever `AppContext`
already provides, rather than routing deck-builder state through it.

`DeckSectionModel`'s raw pointer into `DeckController::deck_` is safe because `deck_` is a
plain member, never reallocated even when reassigned wholesale (`deck_ = std::move(...)` in
`loadDeck()`/`newDeck()`) - only its *contents* change, never its address, for as long as the
owning `DeckController` is alive. `DeckController::rebindModels()` re-points all three models
whenever the catalog changes, and is the one place that wiring happens.

---

## 4. Card database bootstrap: explicit paths, no fabricated fallback

`--card-db <path>` (repeatable, `main.cpp`) is this slice's entire discovery mechanism -
deliberately not upstream's fixed `./cards.cdb` + `./expansions/*.cdb` + archive-scan
convention (§1), because that convention is a property of upstream's installed-application
layout, not something this project's dev/CI/packaging story has established yet, and inventing
a new fixed-path convention here would be exactly the kind of unreviewed assumption CLAUDE.md's
honesty rules warn against. Every path supplied loads in order via
`CardCatalog::loadDatabases()` -> `CardDatabase::load_database()` (`card_catalog.cpp`),
preserving `data/`'s own documented last-file-wins overlay precedence (`card-database.md`§4) -
the same order-matters shape as upstream's own loader, just explicitly supplied rather than
discovered.

No path supplied is a fully supported, honest state: `CardCatalog::loaded()` is `false`,
`DeckBuilderScreen.qml` shows an explicit "No card database loaded" empty state (§10) with the
exact `--card-db` instructions, and nothing crashes or presents fabricated data. A path that
fails to load does not stop the remaining paths from being attempted (matching upstream's own
per-file resilience), but every failure is collected into `lastError` - never silently dropped.
No `.cdb` is bundled, generated, or downloaded by this PR.

---

## 5. Search-index rebuild lifecycle

`CardCatalog::loadDatabases()` builds every supplied path into a **fresh** `CardDatabase`,
swaps it into `database_` only once every path has been attempted, and then calls
`searchIndex_.rebuild(database_)` **unconditionally** against that just-swapped-in value. This
means:

- A load with zero successes still rebuilds - but against a **freshly-constructed, empty**
  `CardDatabase`, not whatever `database_` happened to hold from a previous call. Earlier text
  in this document described this as rebuilding "against whatever `database_` already
  contained" - that was true of the original in-place-`load_database()` implementation, but is
  no longer true after the fix below, and the distinction is not cosmetic: an in-place reload
  would leave a prior call's codes searchable forever, and an all-failed in-place reload would
  leave `loaded()` reporting stale data as if it were still current. External review caught
  exactly this (`ui/tests/test_deckbuilder.cpp`'s `reloadingTheCatalogNeverLeavesStaleResults`
  now pins the adversarial case directly: a code present only in an earlier successfully-loaded
  database must be completely gone, not merely unmatched by name, after a later call that does
  not include it). Either way - build fresh or mutate in place - the index can never lag behind
  a database state that no longer exists, which is the invariant that actually matters here.
- `loadedChanged()` fires once per call, after the rebuild - `SearchResultsModel::refresh()`
  (connected to it) always re-runs against the *current* index, never a stale one, matching
  `CardSearchIndex`'s own documented "results are stale until the caller calls `rebuild()`"
  contract (`card-search.md`§3, [ADR 0005](../adr/0005-card-search-structured-query.md) Decision
  1) - the caller here being `CardCatalog`, unconditionally, every time.
- `DeckSectionModel` also listens for `loadedChanged()` directly (§8.1) - a deck row's
  displayed name/known-state is resolved from `CardCatalog` at display time, not stored, so it
  needs its own notification path independent of `SearchResultsModel`'s.

No locale management is added - `CardDatabase::load_locale()`/`clear_locale()` are not called
anywhere in this slice, since nothing in this PR's scope needs a non-default locale.

---

## 6. Search model

`SearchResultsModel::refresh()` builds a `SearchQuery{ .text = queryText_, .limit = 200 }` and
calls `CardCatalog::searchIndex().search()` - `data/`'s own linear-scan implementation,
unmodified and un-wrapped, matching M3D1's own instruction that the measured single-digit-
millisecond scan (`card-search.md`§10) needs no async worker, debounce, or second index for a
QML text field. Every keystroke updates `queryText` (`Q_PROPERTY` binding from
`searchField.text`), which re-runs `refresh()` synchronously on the UI thread.

Exposed roles: `CardCodeRole`, `NameRole`, `SummaryRole` (a short, concise metadata line - see
`search_results_model.cpp`'s `build_summary()`: ATK/DEF for a normal monster, ATK for a Link
monster, nothing for a non-monster), `MatchKindRole`. No normalization, no SQL, and no second
search algorithm exist in QML or anywhere in this UI layer - `queryText` is handed to
`SearchQuery::text` verbatim; `CardSearchIndex` owns all matching/ranking logic. The 16
auxiliary `str1..str16` text columns are never searched (`data/`'s own established scope,
`card-database.md`).

Only one filter exists in this slice: free text. No structured filter fell out naturally
enough to include without expanding scope, so none was added (`SearchQuery`'s other typed
fields - `exact_code`, static metadata filters - are simply left unset).

---

## 7. Canonical deck ownership and section-edit semantics

`DeckController` holds exactly one `edopro_next::data::Deck` - three ordered
`std::vector<CardCode>` (`main`/`extra`/`side`), unchanged in shape from `deck-model.md`§1.
There is no second copy anywhere: `DeckSectionModel` reads through a pointer into this same
object (§3), and `deck()` (C++-only, used by tests) returns a `const&`, never a copy a caller
could drift from the real one.

- **`addCard(code, section)`** appends to the end of the named section's vector. No dedup, no
  reorder, no cap - three calls with the same code produce three vector entries, matching
  `deck-model.md`§1's "order and multiplicity both matter" invariant.
- **`removeAt(section, index)`** erases exactly that one vector element. Removing one copy of
  a triplicated code leaves the other two, in their original relative order.
- **The caller always names the section explicitly.** `DeckController` has no logic anywhere
  that inspects a `CardEntry`'s `isMonster`/`isLink`/`isPendulum` fields to choose or veto a
  destination - those fields exist on `CardEntry` purely for the preview pane (§11) and the
  search result's summary line (§6), never for section routing. This is the direct
  continuation of upstream's own `push_main`/`push_extra`/`push_side` shape (§1): the
  *destination* is always an explicit parameter, never inferred inside the push/add call
  itself.

`explicitSectionChoiceIsNeverReclassified` (`ui/tests/test_deckbuilder.cpp`) pins this: adding
the same code to all three sections keeps it in all three, rather than a hypothetical
Fusion/Synchro/Xyz/Link check moving it to Extra the way upstream's separate `LoadDeck`
reclassification step would (`deck-model.md`§3) - a step this slice does not implement or call,
matching `edopro_next_deck` itself.

### 7.1 Selection: one shared notion of "what is selected"

`DeckBuilderScreen.qml` originally let the search results list and each of the three
`DeckSectionList`s track their own `ListView.currentIndex` completely independently, with
`selectedSection`/`selectedDeckRow`/`selectedResultRow` merely *recording* whichever one last
reported a change - nothing ever told the *other* lists to stop showing their own, now-stale,
highlighted row. External review found this: selecting a `Main` entry, then an `Extra` entry,
left both visibly highlighted at once, and neither Escape nor a fresh deck ever cleared a list's
own `currentIndex` once set.

The fix makes this screen the single source of truth for selection, driven entirely
imperatively rather than through a declarative binding back to it:

- `DeckSectionList` exposes `currentIndex` as a plain `property alias` (`ui/qml/components/
  DeckSectionList.qml`) - readable and imperatively *writable* from the owning screen, but
  never the target of a declarative one-way binding *from* the screen. A `currentIndex:
  someExpression`-style binding would be permanently broken the first time a click assigns to
  the same property directly - real, ordinary QML behaviour (writing to a property removes its
  earlier binding) - which is exactly what the original `resultsList.currentIndex:
  root.selectedResultRow` binding did to itself on the very first click, silently stopping
  `resultsList` from ever reflecting a later `root.selectedResultRow = -1` again.
- `DeckBuilderScreen.selectDeckEntry(section, row)` and its `resultsList.onCurrentIndexChanged`
  counterpart each clear every *other* list's `currentIndex` to `-1` (and the corresponding
  `selectedResultRow`/`selectedSection`/`selectedDeckRow` bookkeeping) before recording the new
  selection - so at most one row is ever highlighted anywhere on the screen.
- `clearAllSelection()` is the single function that clears everything at once: all four lists'
  `currentIndex`, `hasPreview`, and every selection-tracking property. `Escape`'s `Shortcut`,
  the New button (after `deckController.newDeck()`), and the Open dialog's `onAccepted` (only on
  a *successful* load - a failed one leaves the current deck, and therefore its selection,
  genuinely unchanged) all call it - so a model reset can never leave a stale highlight or a
  preview referring to a card that may no longer even exist in that position.
- `removeSelectedDeckEntry()` calls `clearAllSelection()` after removing, rather than trying to
  select a "next" row - after a removal, index-based neighbours are not a meaningful concept to
  guess at (the section may now be empty, or the row that slides into the removed index is a
  different card entirely), so a deterministic, unambiguous cleared state was chosen over a
  guess that could silently show the wrong card as "still selected."

`ui/tests/test_deckbuilder_screen.cpp` pins this against the real, QML-engine-loaded
`DeckBuilderScreen` (via `TestHarness.qml`, §10.2) rather than only the underlying C++ models -
see `selectingOneSectionClearsAllOthers`, `clearAllSelectionClearsEveryList`,
`newDeckClearsStaleSelection`, and `loadDeckClearsStaleSelection`.

### 7.1.1 A second, independent auto-selection source: `ListView`'s own default

Fixing the above was not the whole story. Visual verification (§13) then caught a screenshot of
an *otherwise untouched* deck - no click, no scenario, nothing - with its first `Main` entry
already highlighted, "Remove selected" already enabled, and the preview pane already showing
that card's details. The cause is independent of anything above: Qt Quick's `ListView` itself
defaults `currentIndex` to `0`, not `-1`, the instant a non-empty `model` is set - with no click,
no binding, no code in this project doing anything at all. Both `resultsList`
(`DeckBuilderScreen.qml`) and the internal `listView` inside every `DeckSectionList`
(`DeckSectionList.qml`) hit this the moment they first receive a populated model - a card being
added, a `.ydk` loading, or simply the search results that are already visible before any text
is typed (empty query text matches everything, §6). Neither `DeckController` nor
`DeckSectionModel` has any concept of "highlighted" at all - it is a pure QML `ListView`
property that no C++-only adapter test could ever have exercised, which is exactly why this
survived every earlier round of review and testing.

Fixed by giving each of the four `ListView`s an explicit `currentIndex: -1` at the point they
are declared - a plain literal, not a binding to anything the selection-contract fix above would
need to keep re-asserting; it only has to win the very first, one-time default before any real
selection - imperative or otherwise - ever happens. `populatingASectionNeverAutoSelectsItsFirstRow`
and `populatingSearchResultsNeverAutoSelectsTheFirstRow` (`ui/tests/test_deckbuilder_screen.cpp`)
pin both list types directly.

---

## 8. Unknown card codes

A `.ydk` may legitimately contain a non-zero code the currently loaded database does not
recognize - a card from an expansion not supplied via `--card-db`, most obviously.
`make_card_entry()` (`card_entry.cpp`) returns `known = false` with every other field at its
type's default when `CardDatabase::find(code)` returns null; it never fabricates a name, never
skips the code, and never mutates the `Deck` to drop or renumber it.

`DeckSectionModel::data()` exposes `KnownRole` alongside `CardCodeRole`/`NameRole`.
`DeckBuilderScreen.qml`'s section delegates use this to render `known == false` entries as
their bare numeric code plus a literal "Unknown card" label, rather than an empty or blank
row - the code itself is never hidden. Because `DeckController::deck_` stores `CardCode`
values, not `CardDatabase`-resolved pointers (`deck-model.md`§1), an unknown code round-trips
through add/remove/save/reload exactly like a known one - there is no separate "unresolved
reference" representation to keep in sync or lose.

`loadPreservesOrderDuplicatesAndUnknownCodes` (`ui/tests/test_deckbuilder.cpp`) pins this
directly: loading a `.ydk` with no catalog loaded at all leaves a nine-digit unknown code
intact in `side`, reported as `known == false` by the model, and
`savedDeckRoundTripsThroughParseYdk` confirms saving reproduces it byte-identically via
`parse_ydk`. This holds regardless of whether *any* `CardCatalog` has ever loaded a database at
all - `DeckBuilderScreen.qml`'s deck pane, unlike its search pane, does not gate on
`hasCatalog` (§10.1): an unknown-code entry is fully selectable and removable with zero
databases loaded, exercised end-to-end (real screen, real QML bindings, not just the
`DeckController`/`DeckSectionModel` pair in isolation) by
`unknownCardDeckIsSelectableWithNoCatalog` (`ui/tests/test_deckbuilder_screen.cpp`).

### 8.1 A deck row's display refreshes when its catalog reloads

`DeckSectionModel::data()` resolves `NameRole`/`KnownRole` from `CardCatalog` fresh on every
call rather than storing them (§7) - which means a *previous* resolution can go stale the
moment the bound `CardCatalog` reloads, with nothing telling any observing `ListView` to
re-read it. `DeckSectionModel::bind()` connects to `CardCatalog::loadedChanged()` (disconnecting
any previous catalog's connection first, so rebinding to a different `CardCatalog` instance
never leaves a dangling connection to the old one) and re-emits `dataChanged()` for
`NameRole`/`KnownRole` across every row it currently holds whenever that signal fires -
`CardCodeRole` is never included, since the underlying code itself never changes on a reload.
`deckRowsRefreshWhenCatalogReloads` (`ui/tests/test_deckbuilder.cpp`) pins both directions this
can move: an unknown code becoming known once its database loads, and a known code's *name*
changing between two differently-named catalogs for the same code - each transition is
confirmed to actually emit `dataChanged`, not just to eventually read correctly the next time
something else happens to call `data()`.

### 8.2 The preview pane also refreshes on a catalog reload

The preview pane's `entry:` binding (§10) reads `cardCatalog.cardCount` purely to establish an
explicit QML binding dependency on `CardCatalog`'s reload signal before calling
`cardCatalog.cardDetails(...)` - a bare method-call expression has no notify signal of its own
for QML's binding engine to react to, so without this, the previewed card's `CardEntry` would
only ever be recomputed when `previewCode`/`hasPreview` themselves changed, not when the
catalog underneath an already-selected code changed. `previewCode` itself stays the identity
that survives a reload - never a `CardRecord*` or a cached `CardEntry`.

---

## 9. Open/save/new and the dirty-state contract

`DeckController` wraps `edopro_next::data::load_ydk`/`save_ydk` directly - it contains no
`.ydk` parsing or serialization logic of its own, and QML never touches either function.
`ui/qml/screens/DeckBuilderScreen.qml`'s `FileDialog`s (`QtQuick.Dialogs` / QuickDialogs2,
matching this being a Qt Quick, not Widgets, application - see `ui/CMakeLists.txt`'s
`QuickDialogs2` component) collect a `QUrl` and hand it straight to
`loadDeck()`/`saveDeckAs()`.

**Dirty-state contract**, implemented exactly as tested by
`dirtyStateTransitionsMatchContract` (`ui/tests/test_deckbuilder.cpp`):

| Event | `dirty` after |
|---|---|
| `DeckController` freshly constructed | `false` |
| `newDeck()` | `false` |
| `addCard()` / `removeAt()` | `true` |
| `saveDeck()` / `saveDeckAs()` succeeds | `false` |
| `saveDeck()` / `saveDeckAs()` fails | unchanged (stays `true` if it was) |
| `loadDeck()` succeeds | `false` |
| `loadDeck()` fails | unchanged, and `deck_` itself is untouched (§9.1) |

`currentPath`/`currentFileName` are empty until the first successful load or save;
`saveDeck()` (plain "Save") returns `false` without attempting anything when `currentPath` is
still empty, and `DeckBuilderScreen.qml` responds by opening the Save As dialog instead - there
is no silent no-op "Save" on a brand-new deck.

### 9.0 Save routes on `currentPath`, not on `saveDeck()`'s return value alone

`saveDeck()` returning `false` means two different things - "there is no `currentPath` yet" and
"there is one, but the write genuinely failed" - and only the first should ever open Save As.
The Save button and `Ctrl+S` both originally called `saveDeck()` unconditionally and opened
Save As whenever it returned `false`, which meant a real disk/permission failure on an
already-named deck silently turned into an unexplained Save As prompt instead of surfacing
`deckController.lastError` (already rendered in the deck pane) - a second, competing UI for the
same failure, and one that also discarded the actual error message a user would need to
understand what went wrong. `DeckBuilderScreen.saveOrSaveAs()` fixes the routing itself: it
checks `deckController.currentPath.length === 0` *before* calling `saveDeck()` at all, so Save
As only ever appears for a deck that has genuinely never been saved anywhere, and any other
`saveDeck()` failure is left entirely to the existing `lastError` banner.

### 9.1 Failed load leaves the current deck untouched

`loadDeck()` relies entirely on `load_ydk()`'s own transactional contract (`deck-model.md`§6):
a failed load returns a fresh, empty `YdkLoadResult` and never mutates a caller-owned value, so
`DeckController` simply never assigns `deck_` on the failure path - there is no separate
"backup the old deck first, restore on failure" mechanism to get wrong, because there is
nothing in this function that touches `deck_` before the `result.ok` check succeeds.
`failedLoadDoesNotDestroyCurrentDeck` pins this.

### 9.2 New/Open while dirty: explicit confirmation, no silent discard

`DeckController::newDeck()` itself discards unconditionally - it does not ask. The "do not ask"
responsibility sits one layer up, in `DeckBuilderScreen.qml`'s `confirmThen()` helper: every
call site that can destroy unsaved work (`New`, `Open`, and the matching `Ctrl+O` shortcut)
routes through it, which checks `deckController.dirty` and only proceeds immediately when
`false`; when `true`, it opens a small `Dialog` ("Discard unsaved changes?" / Discard / Cancel)
and defers the actual action until the user picks Discard. This is a QML-owned *decision to
ask*, not a second copy of dirty-state truth - it reads `deckController.dirty` directly, never
maintaining its own flag. The `Discard` standard button carries Qt's `DestructiveRole`, which
fires `discarded()`, not `accepted()` - confirmed empirically against this project's actual Qt
6.8.3 build (a standalone offscreen QML case that clicked the button and logged which signal
fired), not assumed from memory. Both this dialog and §9.3's use `onDiscarded`, not `onAccepted`
- the original code used `onAccepted`, which meant clicking Discard closed the dialog but never
actually ran the pending action, found by external review.

### 9.3 New/Open protect the current screen; closing the window did not

Everything above only guards `DeckBuilderScreen`'s own New/Open actions. Closing the
application window bypassed both entirely: `DeckController` (and therefore its `dirty` flag)
lives for the whole application's lifetime as a `main.cpp`-owned object, not something scoped to
the Decks screen, so a dirty deck could be silently discarded by closing the window - including
after navigating away to a different screen - even though New and Open both already asked.
`Main.qml`'s `ApplicationWindow` now handles `Window`'s own `closing(CloseEvent close)` signal:
if `deckController.dirty`, it sets `close.accepted = false` and opens the same kind of
Discard/Cancel confirmation dialog `DeckBuilderScreen` uses, calling `Qt.quit()` from
`onDiscarded`. This reads `deckController.dirty` directly at the window level - the same single
source of truth, never a second copy - and does not interact with `main.cpp`'s own `--capture`
mechanism, which quits via `QGuiApplication::exit()`/`quit()` directly rather than through a
window close request, so it never triggers this guard.

---

## 10. QML layout and components added

- **`DeckBuilderScreen.qml`** replaces the `NotImplementedScreen` previously shown for "Decks"
  in `Main.qml`. Three-pane layout at the default 1280x800 (search | deck sections | preview),
  each pane a `ColumnLayout` inside one `RowLayout` filling the screen; collapses to a usable,
  non-clipping arrangement at the 960x600 minimum (§13 - this needed one real fix, see below).
  See §10.1 for what happens with no catalog loaded - it is **not** the empty-state-replaces-
  everything shape this document originally described.
- **`DeckSectionList.qml`** - one Main/Extra/Side section: a titled, counted (`"Main (12)"`)
  `ListView` over a `DeckSectionModel`, keyboard-selectable, Delete/Backspace-removes-selected.
  Exposes `currentIndex` as a plain alias for the owning screen's selection contract (§7.1).
- **`CardPreview.qml`** - a textual/metadata card preview bound to a `CardEntry`: name,
  code, description (word-wrapped), and ATK/DEF/Level or ATK/LINK-markers where the entry's
  own `isMonster`/`isLink`/`isPendulum` flags say they are meaningful. No artwork, no image
  placeholder pretending to be one. Displays `entry.raceDisplay`, never `entry.race` directly -
  see §10.3.
- All new QML uses only existing `Theme.qml` tokens - no raw hex colours, no gradients.

### 10.1 No catalog loaded: the deck editor stays functional; only search degrades

M3D1's own original requirement was that a deck file must remain representable even when its
card codes cannot be resolved (§8) - true of the C++ layer from the start, but not of the QML:
the entire three-pane `RowLayout` (search, deck sections, *and* preview) was originally hidden
behind `visible: root.hasCatalog`, with a full-page "No card database loaded" message shown in
its place. That meant New/Open/Save, Main/Extra/Side, and the preview pane were all unreachable
with no `--card-db` supplied at all - a mouse user could not even open an existing `.ydk`,
despite `DeckController`/`edopro_next_deck` never needing a `CardDatabase` for any of that.

The fix scopes the empty state to the search pane alone. The outer `RowLayout` is now always
visible; only the search pane's own inner content switches, via two sibling `ColumnLayout`s
gated on `hasCatalog`/`!hasCatalog`:

- **No catalog:** the "No card database loaded" message, the `--card-db` instructions, and
  `cardCatalog.lastError` (§10.4) - no search field, no results list, no Add-to-section buttons,
  since none of them have anything to act on without a catalog.
- **Has a catalog:** the search field, results list, and Add-to-section buttons, unchanged from
  before.

The deck pane (New/Open/Save, Main/Extra/Side, Remove selected) and the preview pane were never
actually coupled to `hasCatalog` at the C++ level - `CardCatalog::cardDetails()` already returns
`known: false` for every code when nothing is loaded (an empty `CardDatabase` simply never
`find()`s anything), which is exactly the honest "Unknown card" rendering `DeckSectionList`/
`CardPreview` already use for any unresolved code (§8). Removing the outer `visible:
root.hasCatalog` gate was the entire fix; no new C++ was needed for this part.
`noCatalogDeckEditorStaysFunctional` and `unknownCardDeckIsSelectableWithNoCatalog`
(`ui/tests/test_deckbuilder_screen.cpp`) pin this against the real screen.

### 10.2 Testing the real screen: `TestHarness.qml`

`ui/tests/test_deckbuilder_screen.cpp` loads the actual `DeckBuilderScreen.qml` through a real
`QQmlApplicationEngine` - not just the C++ adapter classes `test_deckbuilder.cpp` already
covers in isolation, which cannot see a bug that only exists in the QML itself (every issue
§7.1 and §10.1 describe was invisible to the adapter-only suite). `ui/tests/TestHarness.qml`
hosts the screen inside an actual `StackLayout` as the current page, matching `Main.qml`'s own
shape closely enough that `StackLayout.isCurrentItem` resolves to `true` the same way it does
for real use - without it, `isActiveScreen` (§11) would never see a `StackLayout` ancestor at
all. `TestHarness.qml` is registered under its own `qt_add_qml_module` call in `ui/tests/
CMakeLists.txt`, sharing the `"EdoproNext"` URI with `edopro_next_shell`'s own module - Qt's
`qmltyperegistrar` generates each target's `CardCatalog`/`DeckController`/`DeckSectionModel`/
`SearchResultsModel` registration scoped to that target's own compiled-in resources, so the two
modules never collide despite the shared URI string.

Selection is simulated the way a real click actually drives it - by setting a
`DeckSectionList`'s own `currentIndex` property (found via `findChild()`, using an `objectName`
added to `mainList`/`extraList`/`sideList`/`resultsList`/`cardPreview` for exactly this purpose,
not read by any production code), which triggers the same `entryActivated` ->
`DeckBuilderScreen.selectDeckEntry()` chain a click does through ordinary Qt property-change
notification. Calling `selectDeckEntry()` itself directly (an earlier version of this test did)
looks equivalent but is not: `selectDeckEntry()` only clears *other* lists' selection, on the
assumption that a click already set the clicked list's own `currentIndex` before it runs - skip
that step and a test can silently pass while asserting against a `currentIndex` nothing ever
set. Other screen functions (`clearAllSelection`, `removeSelectedDeckEntry`) genuinely are
invoked directly via `QMetaObject::invokeMethod`, and native `FileDialog` interaction is avoided
entirely, since simulating it would be fragile and platform-dependent for no additional
coverage - these are the *same* functions the real buttons and shortcuts call.

### 10.3 A 64-bit `race` value cannot be handed to QML as a raw number

`CardEntry::race` is a `qulonglong` - Project Ignis's `RACE_*` bitmask constants run up to bit
62 (e.g. `RACE_YOKAI = 0x4000000000000000`) - and QML/JavaScript numbers are IEEE-754 doubles.
`CardPreview.qml` originally concatenated `entry.race` straight into a string expression
(`entry.attribute + " / " + entry.race`). A tiny empirical Qt 6.8.3 program (a `Q_GADGET`
matching `CardEntry`'s exact `race` property declaration, passed `0x4000000000000000ULL`
through the identical QML marshalling path) confirmed the actual failure mode is subtler than
plain precision loss: `2^62` itself round-trips through a `double` **exactly** (`===
4611686018427387904` holds, and `toString(16)` reproduces `4000000000000000` bit-for-bit) - but
JavaScript's *default decimal string* form of that exact double is `"4611686018427388000"`, not
`"4611686018427387904"`, because both decimal strings parse back to the identical double at
this magnitude (the gap between adjacent representable doubles near `2^62` is `2^10 = 1024`) and
the engine is free to pick either as its canonical shortest-round-trip form. The number is never
actually corrupted internally; a human reading the preview pane would still see the wrong
digits.

The fix keeps `race` as a real, exact `qulonglong` on `CardEntry` (still available to any future
C++ caller) and adds `raceDisplay` (`QString`), computed once in `make_card_entry()` via
`QString::number(record->race, 16)` - a direct 64-bit-to-string conversion with no `double`
round-trip anywhere in the path. `CardPreview.qml` reads `raceDisplay`, never `race`, for
display. Hex, not decimal: `race` is a bitmask, and no human-readable race-name table exists
yet (§12). Audited: every other `CardEntry` field QML reads is 32-bit or narrower
(`code`/`attack`/`defense`/`level`/`leftScale`/`rightScale`/`linkMarker`/`attribute`/`type`),
comfortably within a double's 53-bit exact-integer range - `race` is the only field this applies
to.

### 10.4 `cardCatalog.lastError` is visible in both the empty and the usable screen

With multiple `--card-db` paths, one can fail to load while another still succeeds - `hasCatalog`
becomes `true` from the successful one, but `cardCatalog.lastError` still holds the failed
path's message (§4). The no-catalog message (§10.1) is not the only place that error is
rendered: the "has a catalog" search pane also shows it, directly above the search field, so a
partial load failure stays visible in the screen a user actually sees rather than disappearing
the moment any single database loads successfully - silently losing it there would have been
exactly the kind of dropped failure CLAUDE.md's honesty rules exist to prevent.

### 10.5 A real layout defect found and fixed during visual verification (§13)

The deck pane's
header row originally placed the current filename/dirty `SectionHeading` and the New/Open/Save
buttons in one `RowLayout`. At 960px width the three fixed-size buttons left the
`Layout.fillWidth` heading almost no room, and - because `SectionHeading` had no `elide` set -
its text simply overflowed underneath the (opaque) `New` button rather than truncating, so only
a sliver of the first letter remained visible. Fixed two ways: `SectionHeading` now sets
`elide: Text.ElideRight` (a component-level fix, protecting every usage, not just this one),
and the header itself was restructured into two rows - the heading alone on its own
full-width line, the three buttons in a `RowLayout` beneath it - so the two no longer compete
for the same horizontal space at any width this shell supports.

### 10.6 The preview pane's width, and a second missing-`wrapMode` defect

A second, independently-found layout defect: the three panes' widths were originally
`Layout.preferredWidth: parent.width * 0.34` (search), `* 0.36` (deck), and a bare
`Layout.fillWidth: true` (preview) - percentages that silently assumed `parent.width` was close
to the *window's* width. It is not: this `RowLayout` is `DeckBuilderScreen`'s own content, which
sits beside the shell's nav rail (`Main.qml`), so its actual available width is the *screen's*
width, already smaller than the window by the rail's own share. At the default 1280×800 window
this left the preview pane only **138px** wide - measured directly at runtime during visual
verification, not estimated from the screenshot - nowhere near enough for a title-sized card
name or the ATK/DEF grid.

That narrowness alone would just look cramped; what made it a real defect is that the "Card
code " label (`CardPreview.qml`) had no `wrapMode` (`Text`'s own default is `Text.NoWrap`) and
no `elide` either, so at 138px it did not wrap or truncate - it silently overflowed rightward,
off the edge of the preview pane and, since preview is the rightmost element in the row, off the
edge of the window itself. The card title, which *does* set `wrapMode: Text.WordWrap`, still
overflowed too: word-wrap only breaks *between* words, and a single word like "Dragon" at
title-point size can still be wider than a 138px box on its own line. The same missing-`wrapMode`
mistake was found a second time in the no-catalog message's own heading (§10.1) - "No card
database loaded" - which has nothing to do with pane width at all and overflowed into the deck
pane's own space purely for lack of `wrapMode: Text.WordWrap`.

Fixed three ways: the search/deck panes' shares were reduced to `0.32`/`0.34` (from `0.34`/
`0.36`), the preview pane gained an explicit `Layout.minimumWidth: 260` - a hard floor Qt Quick
Layouts treats as a harder constraint than a sibling's `preferredWidth`, so the other two panes
shrink first if space is ever genuinely tight - and the "Card code " `Text` gained
`elide: Text.ElideRight` as a defensive fallback for any width this pane could still end up at.
Both this and the no-catalog heading's missing `wrapMode: Text.WordWrap` were fixed directly; a
full sweep of every other `Text` item in `DeckBuilderScreen.qml`/`CardPreview.qml`/
`DeckSectionList.qml` confirmed every remaining one already had `wrapMode` or `elide` set, or is
a short, fixed-length label (`"ATK / DEF"`, `"Level"`, ...) with no realistic overflow risk.

---

## 11. Keyboard and launch affordances

Per-screen `Shortcut` items in `DeckBuilderScreen.qml`, all guarded by
`StackLayout.isCurrentItem` so they only fire while Decks is the visible page (the shell's
`StackLayout` keeps every page instantiated even when hidden, so an unguarded `Shortcut` would
fire from any screen): `Ctrl+F` focuses search, `Ctrl+O` opens (through `confirmThen`),
`Ctrl+S` saves (`saveOrSaveAs()`, §9.0 - falling back to Save As only when there is no current
path yet), `Ctrl+Shift+S` always opens Save As, `Escape` calls `clearAllSelection()` (§7.1) -
visibly clearing every list's highlighted row and the preview pane, not only this screen's own
bookkeeping variables the way an earlier version did. Search results and deck entries are both
`ListView`s with `activeFocusOnTab: true` and visible `highlighted`/focus styling, so Tab/
Shift+Tab reaches them in the natural focus chain: no custom focus-traversal logic was written.
This is explicitly not a claim of full keyboard or controller parity with upstream - only the
core interactions this slice's own functionality needs to be usable without a mouse.

`main.cpp` also adds `--start-screen <name>` and `--capture <path>[--capture-width/-height]`,
purely for launching directly to a screen and producing a real screenshot headlessly (§13) -
not a routing system (`Main.qml`'s `StackLayout.currentIndex` still normally starts at Home and
is driven by the nav rail), and not part of ordinary interactive use.

---

## 12. What remains before the M3 roadmap item can be checked

- Legality of any kind: deck-size limits, the three-copy rule, `LFList`/banlist checks.
- Automatic Main/Extra classification from card type (upstream's `LoadDeck` reclassification,
  `deck-model.md`§3) - a `Deck -> Deck` transformation layered on top of `edopro_next_deck`,
  deliberately not built here or inside this codec.
- Artwork: no image loading, downloading, or caching of any kind.
- The legacy sigil search grammar / archetype-name resolution (`card-search.md`§1.1) - only
  plain text search is wired up.
- Full keyboard and controller parity (§11 covers only the core interactions).
- `.ydke`/Base64 import-export (`deck-model.md`§8).
- Any end-to-end proof that a deck saved here loads unchanged in real upstream EDOPro - this
  slice's tests confirm round-tripping through this project's own `parse_ydk`/`load_ydk`
  (§8), which is format-compatible by construction (`deck-model.md`), but no test here launches
  actual upstream EDOPro.

## 13. Visual verification performed for this slice

### 13.1 First pass (static states only)

Screenshots were captured with `--capture` against a small, clearly-synthetic runtime `.cdb`
(seven cards, names prefixed "Synthetic", including one Link monster, one Pendulum monster, and
one deliberately long name) generated outside the repository and never committed, at three
configurations: 1280x800 with the database loaded, 960x600 (this shell's documented minimum)
with the database loaded, and the empty "no database" state. All three were inspected directly
(not merely "the process exited 0") for clipping, overlapping text, contrast, unwanted
horizontal scroll, description wrapping, and legible Main/Extra/Side counts - the header-row
defect in §10.5 was found this way, at the 960x600 configuration, and confirmed fixed by
recapturing after the change. At the time, interaction states (a selected search result, a
selected deck entry) were deliberately not captured, judging a scripted-input capture mode out
of proportion to the slice at hand - the selection/highlight styling itself was instead verified
by reading the QML and exercised indirectly by the adapter-only Qt Test suite of the time.

### 13.2 Second pass: interaction states, and what only they could find

External review correctly rejected §13.1's limitation as insufficient once the screen's
selection contract (§7.1) became load-bearing behaviour, not just styling. A scratch,
never-committed capture harness (a modified copy of `main.cpp`, built only inside the disposable
WSL rsync tree this project's local builds already use, never touching the real source tree) was
extended with a `--capture-scenario` flag that drives one of a few interaction sequences - via
the same `objectName`-tagged `findChild()` + property-set/`QMetaObject::invokeMethod()` approach
`ui/tests/test_deckbuilder_screen.cpp` uses - immediately before capturing. Nine screenshots were
produced and each inspected directly: the two static states re-verified, a `.ydk` with unknown
codes loaded with **no catalog at all**, a selected search result, a selected `Main` entry,
switching selection from `Main` to `Extra`, `clearAllSelection()` (the Escape shortcut's own
action) clearing a prior selection, `New`'s combined `newDeck()` + `clearAllSelection()`, and the
application-close dirty-confirmation dialog (§9.3).

This pass is what actually found §7.1.1's `ListView`-default-`currentIndex` defect and §10.6's
preview-pane-width/missing-`wrapMode` defects - neither visible in any static screenshot from
§13.1, since both only manifest once a list actually holds data or a selection actually happens.
Both were fixed and the full nine-screenshot set was recaptured and re-inspected to confirm: no
stale highlight anywhere, the preview pane tracking exactly one selection at a time, the
dirty-close dialog appearing with the correct text and buttons, and every text element - the
card title, the `raceDisplay` hex value, the description, the "No card database loaded" heading
- rendering fully within its pane with no overflow past the window's own edge.

One acknowledged gap in this pass, not judged worth a third capture round: the `new-replaces`
scenario's setup script omitted the actual `deckController.newDeck()` call before invoking
`clearAllSelection()`, so its screenshot ended up visually identical to the `escape-clears` one
rather than showing an emptied deck. The underlying behaviour is not in doubt - it is exactly
what `newDeckClearsStaleSelection` (`ui/tests/test_deckbuilder.cpp`) already asserts, including
`mainCount() == 0` after the call - so this is a gap in one scratch script's screenshot coverage,
not in the verification of the feature itself.

The capture harness's own process reliably fails to exit after `QGuiApplication::quit()` once an
actual selected/highlighted delegate has been rendered offscreen - confirmed harmless and
specific to this harness: the frame is captured and written to disk correctly, in well under two
seconds, before the hang; both threads then sit idle in the kernel (`do_sys_poll`, 0% CPU - not
spinning) rather than the process ever doing further work. Never observed via the shipped
`--capture` path (which this harness only extends), and irrelevant to the shell's real behaviour
either way; worked around by never blocking on the scratch process's own exit when scripting a
capture, launching it in the background and unconditionally terminating it after a fixed settle
time instead.

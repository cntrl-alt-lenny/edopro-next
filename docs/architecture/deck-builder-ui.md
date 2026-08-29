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

`CardCatalog::loadDatabases()` calls `searchIndex_.rebuild(database_)` **unconditionally**,
once, at the end of the call - after every path has been attempted, regardless of whether any
or all of them failed. This means:

- A load with zero successes still rebuilds (against whatever `database_` already contained,
  including "nothing" on the very first call) - the index can never lag behind a database
  state that no longer exists.
- `loadedChanged()` fires once per call, after the rebuild - `SearchResultsModel::refresh()`
  (connected to it) always re-runs against the *current* index, never a stale one, matching
  `CardSearchIndex`'s own documented "results are stale until the caller calls `rebuild()`"
  contract (`card-search.md`§3, [ADR 0005](../adr/0005-card-search-structured-query.md) Decision
  1) - the caller here being `CardCatalog`, unconditionally, every time.

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
`parse_ydk`.

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
maintaining its own flag.

---

## 10. QML layout and components added

- **`DeckBuilderScreen.qml`** replaces the `NotImplementedScreen` previously shown for "Decks"
  in `Main.qml`. Three-pane layout at the default 1280x800 (search | deck sections | preview),
  each pane a `ColumnLayout` inside one `RowLayout` filling the screen; collapses to a usable,
  non-clipping arrangement at the 960x600 minimum (§13 - this needed one real fix, see below).
  An honest empty state (§4) replaces the three-pane layout entirely when no catalog is loaded.
- **`DeckSectionList.qml`** - one Main/Extra/Side section: a titled, counted (`"Main (12)"`)
  `ListView` over a `DeckSectionModel`, keyboard-selectable, Delete/Backspace-removes-selected.
- **`CardPreview.qml`** - a textual/metadata card preview bound to a `CardEntry`: name,
  code, description (word-wrapped), and ATK/DEF/Level or ATK/LINK-markers where the entry's
  own `isMonster`/`isLink`/`isPendulum` flags say they are meaningful. No artwork, no image
  placeholder pretending to be one.
- All new QML uses only existing `Theme.qml` tokens - no raw hex colours, no gradients.

**A real layout defect found and fixed during visual verification (§13):** the deck pane's
header row originally placed the current filename/dirty `SectionHeading` and the New/Open/Save
buttons in one `RowLayout`. At 960px width the three fixed-size buttons left the
`Layout.fillWidth` heading almost no room, and - because `SectionHeading` had no `elide` set -
its text simply overflowed underneath the (opaque) `New` button rather than truncating, so only
a sliver of the first letter remained visible. Fixed two ways: `SectionHeading` now sets
`elide: Text.ElideRight` (a component-level fix, protecting every usage, not just this one),
and the header itself was restructured into two rows - the heading alone on its own
full-width line, the three buttons in a `RowLayout` beneath it - so the two no longer compete
for the same horizontal space at any width this shell supports.

---

## 11. Keyboard and launch affordances

Per-screen `Shortcut` items in `DeckBuilderScreen.qml`, all guarded by
`StackLayout.isCurrentItem` so they only fire while Decks is the visible page (the shell's
`StackLayout` keeps every page instantiated even when hidden, so an unguarded `Shortcut` would
fire from any screen): `Ctrl+F` focuses search, `Ctrl+O` opens (through `confirmThen`),
`Ctrl+S` saves (falling back to Save As if there is no current path yet), `Ctrl+Shift+S` always
opens Save As, `Escape` clears the current search-result/deck-entry selection. Search results
and deck entries are both `ListView`s with `activeFocusOnTab: true` and visible
`highlighted`/focus styling, so Tab/Shift+Tab reaches them in the natural focus chain: no
custom focus-traversal logic was written. This is explicitly not a claim of full keyboard or
controller parity with upstream - only the core interactions this slice's own functionality
needs to be usable without a mouse.

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

Screenshots were captured with `--capture` against a small, clearly-synthetic runtime `.cdb`
(seven cards, names prefixed "Synthetic", including one Link monster, one Pendulum monster, and
one deliberately long name) generated outside the repository and never committed, at three
configurations: 1280x800 with the database loaded, 960x600 (this shell's documented minimum)
with the database loaded, and the empty "no database" state. All three were inspected directly
(not merely "the process exited 0") for clipping, overlapping text, contrast, unwanted
horizontal scroll, description wrapping, and legible Main/Extra/Side counts - the header-row
defect in §10 was found this way, at the 960x600 configuration, and confirmed fixed by
recapturing after the change. Interaction states (a selected search result with the preview
pane populated, a selected deck entry) were not captured as screenshots - doing so would need
either a scripted-input capture mode or a second launch-time affordance beyond `--start-screen`,
which was judged out of proportion to this slice; the selection/highlight styling itself was
instead verified by reading the QML (`highlighted: ListView.isCurrentItem`, the same pattern
`NavButton.qml` already establishes elsewhere in this shell) and exercised indirectly by the
Qt Test suite's own assertions on the underlying models. This is a deliberate, stated limit on
what was visually verified, not an oversight.

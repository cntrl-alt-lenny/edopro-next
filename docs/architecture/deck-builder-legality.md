# Deck builder legality: what upstream does, and the `ValidationPolicy` boundary

Source-verified account of how `gframe/deck_con.cpp`'s `DeckBuilder` (the Irrlicht deck
editor) and `gframe/generic_duel.cpp`'s `GenericDuel::PlayerReady` (duel entry) each treat
deck legality, how the two differ, and what each of `policy::ValidationPolicy`'s six fields
would have to be sourced from if this project's QML deck builder is ever wired to
`validate_deck()`. Ends in a **Recommended boundary** section - a proposal for Brain and the
owner to ratify, not a decision this document makes.

This document is upstream archaeology plus an input to a future decision. It changes no
code. `docs/architecture/deck-legality.md` already documents `CheckDeckSize`/
`CheckDeckContent`/`CheckCards`/the LFList grammar in full, source-cited detail; this
document does not re-derive that material; it cites the same functions only where the
*calling context* (editor vs. duel entry) is the actual question.

---

## 1. Investigation question 1, answered first

**No - `gframe/deck_con.cpp`'s `DeckBuilder` never calls `DeckManager::CheckDeckContent` or
`DeckManager::CheckDeckSize`, anywhere.** A repository-wide search of every `.cpp`/`.h` under
`gframe/` for both names finds exactly one call site for each, and it is the same one:
`gframe/generic_duel.cpp:373` (`CheckDeckSize`) and `:380` (`CheckDeckContent`), both inside
`GenericDuel::PlayerReady` - the server-side handler for a player toggling "ready" in a duel
room's lobby. `deck_con.cpp` (1703 lines) contains neither name at all. The deck editor
instead performs its own separate, weaker, bypassable set of checks, detailed in §2 - built
from different data, checked at a different moment, and enforcing a different (usually
looser, sometimes simply different) rule than final validation does. This is the central fact
this document exists to establish: **"the deck editor checks legality" and "the deck editor
calls the same legality check duel entry uses" are not the same claim, and upstream's own
source shows only the first is true, not the second.**

---

## 2. What upstream's deck editor actually does about legality

### 2.1 Three independent mechanisms, not one

`DeckBuilder` has no single "is this deck legal" function at all. It has three separate,
independently-triggered mechanisms, none of which is `CheckDeckContent`/`CheckDeckSize`:

| Mechanism | What it gates | Where |
|---|---|---|
| `push_main`/`push_extra`/`push_side` | Whether one card may be *added* to one section right now | `deck_con.cpp:1577-1648` |
| `check_limit` | Whether one more copy of a card would exceed a banlist/hard-3 cap | `deck_con.cpp:1673-1698` |
| `CheckCardProperties` | Whether a card *appears in the search results at all* | `deck_con.cpp:1191-1324` |

None of the three is a whole-deck validator; each fires per-card, per-action, while the deck
is being edited.

### 2.2 `push_main`/`push_extra`/`push_side`: fixed literals, not `DeckSizes`

`push_main` (`deck_con.cpp:1577-1609`):

```cpp
if(pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
    return false;
if((pointer->type & (TYPE_LINK | TYPE_SPELL)) == TYPE_LINK)
    return false;
...
if(!forced && !mainGame->is_siding) {
    if(main_and_extra_legend_count_monster >= 1 && (pointer->ot & SCOPE_LEGEND) && (pointer->type & TYPE_MONSTER))
        return false;
    if(main_legend_count_spell >= 1 && (pointer->ot & SCOPE_LEGEND) && (pointer->type & TYPE_SPELL))
        return false;
    if(main_legend_count_trap >= 1 && (pointer->ot & SCOPE_LEGEND) && (pointer->type & TYPE_TRAP))
        return false;
    if(main_skill_count >= 1 && (pointer->type & TYPE_SKILL))
        return false;
    if(container.size() >= 60)
        return false;
}
```

`push_extra` (`:1610-1636`) and `push_side` (`:1637-1648`) mirror this shape with `15`/`15`
as their own hard caps. Three things this project's own `policy/` behaves differently from,
by design of upstream's editor, not by omission of ours:

- **`60`/`15`/`15` are maxima only, with no minima**, and they are literals baked into the
  editor - not `DeckSizes`/`host_info.sizes`, which the editor never reads or holds a
  variable for at all. `SectionSizeRange` (`validation_policy.h`) models a min *and* max per
  section, matching duel entry's shape, not the editor's.
- **The Legend/Skill counts are cached counters** (`main_and_extra_legend_count_monster`,
  etc.), maintained incrementally by `RefreshLimitationStatusOnAdded`/`OnRemoved`
  (`deck_con.cpp:1475-1576`) as an optimization over rescanning the whole deck on every push -
  not a call into `DeckManager::TypeCount`/`CountLegends` at check time, though they wrap the
  same static helpers when initialized (`RefreshLimitationStatus`, `:1456-1474`).
- **Fusion/Synchro/Xyz/Link-non-spell cards are unconditionally rejected from Main** (and
  their Extra-Deck counterparts have their own type gate) regardless of `forced` - this is
  section-placement-by-type-classification happening *at push time* in the editor, which is
  exactly the "automatic Main/Extra classification" this project's `policy/` and
  `edopro_next_deck` both deliberately do not perform (`deck-legality.md`§0,
  `deck-builder-ui.md`§0/§7). The editor's push functions conflate "which section is this
  card allowed in" (a classification concept) with "does adding it violate a count cap" (a
  validation concept) in one function; this project's own architecture keeps those two
  concerns in different layers (`edopro_next_deck` for classification, if ever built;
  `policy/` for validation), so this is a genuine three-way split (editor: fused together;
  duel entry: neither - `CheckDeckContent` trusts the caller's Main/Extra split completely;
  this project: two separate, not-yet-connected pieces) rather than a two-way comparison.

**A correction this document prompted in this project's own `deck-builder-ui.md`.**
That document previously stated: *"there is no upstream function that decides a card's
section from its type at push time either; classification-on-load is a separate, later
step."* Read narrowly - no single function *reclassifies/redirects* a card to a different
section the way `LoadDeck`'s own loader-side classification does - that holds. But
`push_main`/`push_extra` themselves, quoted above, plainly do decide-by-type **at push
time**: `push_main` returns `false` outright for any Fusion/Synchro/Xyz/non-Spell-Link card,
and `push_extra` returns `false` for anything that is not a Ritual/Link-non-Spell/Fusion/
Synchro/Xyz card. The caller's own cascade
(`if (!push_main(pointer, ...) && !push_extra(pointer, ...)) push_side(pointer);` -
`deck_con.cpp:725`, similarly at `:665-669,701`) relies on exactly these type gates to land a
card in its correct section through trial and rejection, not routing. `deck-builder-ui.md`§1
has since been corrected to say precisely this - no single function *chooses* a destination,
but `push_main`/`push_extra` do gate on type at push time, and the caller's cascade is what
turns that gating into effective section placement.

### 2.3 `check_limit`: the editor's own banlist gate, not `CheckCards`

```cpp
bool DeckBuilder::check_limit(const CardDataC* pointer) {
    uint32_t limitcode = pointer->alias ? pointer->alias : pointer->code;
    int found = 0;
    int limit = filterList->whitelist ? 0 : 3;
    auto endit = filterList->content.end();
    auto it = filterList->GetLimitationIterator(pointer);
    if(it != endit)
        limit = it->second;
    if(limit == 0)
        return false;
    const auto& deck = current_deck;
    for(auto* plist : { &deck.main, &deck.extra, &deck.side }) {
        for(auto& pcard : *plist) {
            if(pcard->code == limitcode || pcard->alias == limitcode) {
                if((it = filterList->content.find(pcard->code)) != endit)
                    limit = std::min(limit, it->second);
                else if((it = filterList->content.find(pcard->alias)) != endit)
                    limit = std::min(limit, it->second);
                found++;
            }
            if(limit <= found)
                return false;
        }
    }
    return true;
}
```
(`deck_con.cpp:1673-1698`)

This answers "would one more copy of this card still be legal against `filterList`" by
**re-scanning the entire current deck live**, every time, rather than threading a shared
`banlist_content_t ccount` the way `CheckDeckContent`/`CheckCards` do
(`deck-legality.md`§3). The two are checking a materially similar rule (alias-preferred copy
counting, banlist-limit-vs-hard-3-cap, whitelist-absence-is-illegal) but are two independent
implementations, not one shared function - `check_limit` has no forbidden-type check, no
scope/`CHECK_UNOFFICIAL` check, no section-placement check, and no deck-size check at all; it
answers exactly one narrower question than `CheckCards` does.

`filterList` - the object `check_limit` and `CheckCardProperties` both read - is **not**
`host_info.lflist`. It is the deck editor's own currently-selected combo-box item:

```cpp
filterList = &gdeckManager->_lfList[mainGame->cbDBLFList->getSelected()];
```
(`deck_con.cpp:74`, re-assigned identically at `:513` and `game.cpp:2447`)

`cbDBLFList` is populated by `Game::RefreshLFLists()` (`game.cpp:2430-2449`) from
`gdeckManager->_lfList` - the same in-memory vector `DeckManager::LoadLFList()`
(`deck_manager.cpp:97-108`) fills once at startup from `./expansions/lflist.conf`,
`./lflist.conf`, every `*.conf` under `./lflists/`, plus a synthetic, `hash == 0` "N/A" entry
appended last (`:101-107`) - the identical loading upstream also does for `host_info.lflist`
resolution (`deck-legality.md`§9). The **selection** persists across sessions as a
**hash**, not a path or index, in a single shared config value:

```cpp
gGameConfig->lastlflist = gdeckManager->_lfList[mainGame->cbDBLFList->getSelected()].hash;
```
(`deck_con.cpp:127`, on `Terminate()`)

restored by scanning for a matching hash the next time the list repopulates:

```cpp
if (gGameConfig->lastlflist == list.hash) {
    cbHostLFList->setSelected(hostIndex);
    cbDBLFList->setSelected(deckIndex);
}
```
(`game.cpp:2442-2445`)

Two things worth naming explicitly:

- **`lastlflist` is the *same* config value `cbHostLFList` (the "create a duel room" screen's
  own banlist selector) uses** (`game.cpp:2439,2443`; written again at
  `duelclient.cpp:235`: `cscg.info.lflist = gGameConfig->lastlflist = ...`). Selecting a
  banlist in the deck editor and selecting one when hosting a duel room share one persisted
  value - there is no independent "my preferred editor banlist" setting.
- **The editor's selection is saved on `Terminate()` from whatever `cbDBLFList` currently
  shows, unconditionally** - there is no "was the deck actually checked against this list"
  step anywhere in that path; it is a pure UI-state save, not a validation record.

### 2.4 The `forced` bypass: Shift, or a persistent global setting

`push_main`/`push_extra`/`push_side` (§2.2) and `check_limit` (§2.3) are both skippable.
Every mouse-driven add path computes:

```cpp
const bool forceInput = gGameConfig->ignoreDeckContents || event.MouseInput.Shift;
```
(`deck_con.cpp:624`)

and passes `forceInput`/`forced` straight through to `push_*`, and separately guards
`check_limit` itself the same way at two of its three call sites (`:641,756`) - but **not**
at the third. `:719`'s click-to-add path guards `check_limit` with `gGameConfig->
ignoreDeckContents` alone:

```cpp
if(!pointer || (!gGameConfig->ignoreDeckContents && !check_limit(pointer)))
    break;
if (event.MouseInput.Shift) {
    push_side(pointer, -1, gGameConfig->ignoreDeckContents);
} else {
    ...
```
(`:719-725`)

`event.MouseInput.Shift` is read two lines later, but only to choose whether the card goes to
Side or the Main/Extra cascade - it does not bypass `check_limit` at this call site.
`ignoreDeckContents` is a single, persistent, global boolean settings checkbox
(`chkIgnoreDeckContents`, "Ignore deck contents [for deckbuilding]" -
`gDataManager->GetSysString(12119)`, `game.cpp:1561,1669`), defaulting to `false`
(`game_config.inl:70`) - not a per-deck flag, not scoped to one editing session, and not reset
between decks. Once a user enables it, every one of §2.2's and §2.3's checks is permanently
inert for that installation until they disable it again. Holding Shift achieves the identical
bypass for `push_*` (§2.2) and for `check_limit` at the drag-based add paths (`:641,756`) -
but **not** at `:719`'s click-to-add path, where `check_limit` still runs on Shift alone and
is skipped only when `ignoreDeckContents` is also set.

### 2.5 Import/Open bypass §2.2 and §2.3 entirely - not merely "forced"

`SetCurrentDeckFromFile`/`DeckManager::LoadDeckFromFile` (`deck_con.cpp:129-135`) and
`ImportDeck`/`DeckManager::ImportDeckYdke`/`ImportDeckBase64Omega` (`:136-146`) both mutate
`current_deck`'s three vectors **directly** - neither calls `push_main`, `push_extra`,
`push_side`, or `check_limit` at any point. This is not "forced input" in the `forceInput`
sense above; it is a code path that never reaches either check at all, forced or not. A
`.ydk` file opened in the editor, or a `ydke://`/Base64 string pasted from the clipboard, can
contain any Main/Extra/Side split, any card multiplicity, and any card codes whatsoever, and
the editor will display and let the user keep editing it exactly as loaded.

### 2.6 `SaveDeck` performs no check of any kind

```cpp
bool DeckManager::SaveDeck(epro::path_stringview name, const Deck& deck) {
    ...
    serializeDeck(deck.main);
    deckfile << "#extra\n";
    serializeDeck(deck.extra);
    deckfile << "!side\n";
    serializeDeck(deck.side);
    return true;
}
```
(`deck_manager.cpp:436-452`; the `cardlist_type` overload at `:453-468` is identical in
shape). `SaveDeck` writes exactly whatever `current_deck` currently holds. It calls neither
`CheckDeckSize`/`CheckDeckContent` nor `check_limit`/`push_*`. There is no "your deck is
invalid, save anyway?" prompt anywhere in this path, unlike the *dirty-state* discard
confirmations this project's own `deck-builder-ui.md`§9.2/9.3 documents for its own,
unrelated concern (losing unsaved edits).

### 2.7 What the user actually sees (investigation question 5)

**Nothing, while editing.** None of §2.2-2.4's rejections produce a dialog, a tooltip, or a
status-bar message - a blocked add is simply a silent no-op (`return false`/`break` with no
further code run). The only editor-level surface at all is `CheckCardProperties`'s search
filter (`filter_lm`, `deck_con.cpp:1254-1322`, driven by a `cbLimit` combo,
`:1043,1385`): a user can narrow the *search results pane* to only Banned/Limited/
Semi-Limited/Unlimited/OCG/TCG/etc. cards relative to whichever `filterList` is currently
selected - this changes what appears in the list to pick cards from, and has no relationship
to whether any card **already in the deck** is legal.

**The only place upstream ever renders an actual legality error message to the user at all**
is `DuelClient::ClientEvent`'s handling of `STOC_ERROR_MSG`/`ERROR_TYPE::DECKERROR`
(`duelclient.cpp:470-554`) - the client-side reaction to the `STOC_ERROR_MSG` packet
`GenericDuel::PlayerReady` sends when `CheckDeckSize`/`CheckDeckContent` reject a deck
(`generic_duel.cpp:388`; the preceding line, `:387`, sends a different packet -
`STOC_HS_PLAYER_CHANGE`, marking the player not-ready - not the error itself). It switches
on the returned `DeckError::DERR_TYPE` and builds a
specific localized string per case (`LFLIST` -> sysstring 1407, `OCGONLY` -> 1413,
`UNKNOWNCARD` -> 1415, `MAINCOUNT` -> 1417 with the actual min/max/current counts
interpolated, etc.; `duelclient.cpp:487-548`), then calls `mainGame->PopupMessage(text)`
(`:549`) - a modal popup **in the duel-room/lobby screen**, at the moment a player clicks
Ready, never in the deck-editor screen the deck was actually built in. A user can spend an
entire editing session with a deck the editor never once objected to, then only discover it
is rejected minutes or days later, in a completely different screen, when they try to use it.

---

## 3. What upstream does at duel entry

`GenericDuel::PlayerReady` (`generic_duel.cpp:366-397`):

```cpp
void GenericDuel::PlayerReady(DuelPlayer* dp, bool is_ready) {
    ...
    if(is_ready) {
        DeckError deck_error = DeckManager::CheckDeckSize(dueler.pdeck, host_info.sizes);
        if(deck_error.type == DeckError::NONE && !host_info.no_check_deck_content) {
            if(dueler.deck_error) {
                deck_error.type = DeckError::UNKNOWNCARD;
                deck_error.code = dueler.deck_error;
            } else {
                bool rituals_in_extra = host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32);
                deck_error = DeckManager::CheckDeckContent(dueler.pdeck, gdeckManager->GetLFList(host_info.lflist),
                                                           static_cast<DuelAllowedCards>(host_info.rule), host_info.forbiddentypes, rituals_in_extra);
            }
        }
        if(deck_error.type != DeckError::NONE) {
            ...
            NetServer::SendPacketToPlayer(dp, STOC_ERROR_MSG, deck_error);
            return;
        }
    }
    ...
}
```

Sequencing matches `deck-legality.md`§2 exactly (size, then unknown-card, then content) -
this is the function that establishes that order; `CheckDeckSize`/`CheckDeckContent`
themselves never call each other.

Three things worth naming precisely about *this* call site, beyond what `deck-legality.md`
already documents about the functions themselves:

- **`dueler.pdeck` is populated over the network, independently of any local editor state.**
  `GenericDuel::UpdateDeck` (`generic_duel.cpp:406-...`) receives a raw deck buffer from the
  client and calls `DeckManager::LoadDeckFromBuffer` (`:423`), storing any unresolvable code
  in `dueler.deck_error` (`generic_duel.h:63-66`'s `duelist` struct). This is a **third**,
  independent load path, distinct from both the editor's file-open (§2.5) and its own live
  `current_deck` - a client sends whichever deck it currently has selected for the room, and
  the server reconstructs it from bytes, with its own unknown-card handling
  (`deck-legality.md`§8) layered on top.
- **Every non-`lflist` `ValidationPolicy`-shaped input comes from `HostInfo`**
  (`network.h:40-61`), populated once, at duel-room-creation time, from the "Create Game"
  dialog's own widgets - never from anything the deck editor reads or writes:

  ```cpp
  cscg.info.rule = mainGame->cbRule->getSelected();
  ...
  cscg.info.lflist = gGameConfig->lastlflist = mainGame->cbHostLFList->getItemData(...);
  cscg.info.duel_flag_low = mainGame->duel_param & 0xffffffff;
  cscg.info.duel_flag_high = (mainGame->duel_param >> 32) & 0xffffffff;
  cscg.info.no_check_deck_content = mainGame->chkNoCheckDeckContent->isChecked();
  ...
  static constexpr DeckSizes nolimit_deck_sizes{ {0,999},{0,999},{0,999} };
  if(mainGame->chkNoCheckDeckSize->isChecked()) {
      sizes = nolimit_deck_sizes;
  } else {
      TOI(sizes.main.min, mainGame->ebMainMin->getText(), 40);
      TOI(sizes.main.max, mainGame->ebMainMax->getText(), 60);
      TOI(sizes.extra.min, mainGame->ebExtraMin->getText(), 0);
      TOI(sizes.extra.max, mainGame->ebExtraMax->getText(), 15);
      TOI(sizes.side.min, mainGame->ebSideMin->getText(), 0);
      TOI(sizes.side.max, mainGame->ebSideMax->getText(), 15);
  }
  cscg.info.forbiddentypes = mainGame->forbiddentypes;
  ```
  (`duelclient.cpp:229-263`) - `40/60/0/15/0/15` are the **text fields' own placeholder
  fallbacks if parsing fails**, not a compiled-in "the" default the engine falls back to on
  its own; a host who clears the field and the parse throws gets this value from `TOI`'s own
  `catch` clause, nothing more authoritative. `mainGame->forbiddentypes` is itself a
  persistent-per-installation setting (`gGameConfig->lastDuelForbidden`, restored at
  `game.cpp:1297`, saved at `:2523`), chosen on a separate duel-rules screen
  (`game.cpp:3135`'s `forbiddentypes = flag2`) - reachable from the "Create Game" flow, not
  from the deck editor.
- **`rituals_in_extra` is derived from a negotiated duel-rule bit, which by construction only
  exists once a room's rules are set** - `host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL
  >> 32)`, matching `deck-legality.md`§7 exactly.

---

## 4. Direct comparison

| Dimension | Deck editor (`deck_con.cpp`) | Duel entry (`PlayerReady`) |
|---|---|---|
| Calls `CheckDeckSize`/`CheckDeckContent`? | **Never.** | Yes, in that order, gated on `is_ready`. |
| Deck-size rule | Fixed literals `60`/`15`/`15`, **maxima only**, enforced per-push in `push_main`/`extra`/`side` (§2.2). | `host_info.sizes` (`DeckSizes`, min *and* max per section), host-chosen per room (§3). |
| Copy-count/banlist rule | `check_limit`, live-rescanned per add, against `filterList` = the editor's own persistently-selected `cbDBLFList` item (§2.3). | `CheckCards`'s shared `ccount` map (`deck-legality.md`§3), against `host_info.lflist` resolved via `GetLFList()` - independently selected, on the host dialog's own combo. |
| Allowed-card scope (`DuelAllowedCards`) | No enforcement at all. `filter_lm`'s OCG/TCG/etc. options only narrow the *search* pane (§2.7); nothing stops mixing scopes already in the deck. | Enforced per card in `CheckCards`, against `host_info.rule` - host-chosen, editor never sees it. |
| Forbidden types | Only an unconditional "≤1 Skill card" cap baked into `push_main` (§2.2) - not configurable, not the general bitmask. | `host_info.forbiddentypes`, a host-configured, arbitrary `TYPE_*` bitmask (§3). |
| Ritual placement | Outside `is_siding`: `CardDataC::isRush()`, a static per-card property, read in `push_main`/`push_extra`'s own Ritual branch (`deck_con.cpp:1582,1615` - see §2.2's quoted code, not §2.4/§3, which don't cite these lines). Inside `is_siding` (mid-match side-decking only): a real, live duel's field flag. | Always a negotiated `duel_flag_high` bit, resolved once per room, regardless of card type (§3). |
| Content checking on/off | No such concept; the editor never performs content checking either way. | `host_info.no_check_deck_content`, host-chosen. |
| Bypassable? | Yes - Shift-held, or the persistent `ignoreDeckContents` setting (§2.4); or entirely sidestepped via file-open/clipboard-import (§2.5). | No caller-facing bypass once `is_ready` fires (`no_check_deck_content` only skips the content half, never the size half). |
| Can the resulting deck be saved regardless? | Yes, unconditionally (§2.6). | N/A - this is not a save path. |
| User-visible error surface | None, ever, in the editor itself (§2.7). | A modal popup with a specific per-error-type message, but only in the lobby screen, only at Ready-time (§2.7). |

**Answering investigation question 4 directly: yes.** A user can build and save a deck in
upstream that duel entry would reject - trivially via file-open or clipboard-import (§2.5,
which never runs *any* check), and even through ordinary interactive editing via Shift-held
adds or the global `ignoreDeckContents` setting (§2.4). This is not a hypothetical edge case;
it is the documented, intended behavior of two independently-implemented mechanisms that were
never required to agree.

---

## 5. `ValidationPolicy` field-by-field: what each would have to be sourced from

| Field | Real editor-time counterpart? | Where the value would have to come from |
|---|---|---|
| `deck_sizes` (`DeckSizePolicy`) | **No.** The editor's own `60`/`15`/`15` caps (§2.2) are maxima-only literals with no minima, enforced per-push, not a `DeckSizes`-shaped value read from anywhere. | Duel-entry's real counterpart (`host_info.sizes`) is host-chosen per room and has no meaning without a room. This project has no room concept; a value would have to be an explicit ruleset this project's own UI defines and names, not a value read off any upstream state. |
| `allowed_cards` (`AllowedCardPool`) | **No.** `filter_lm`'s OCG/TCG/etc. options (§2.7) only filter the search pane; nothing in the editor tracks or enforces "the deck's current mode." | Purely a duel-room concept (`host_info.rule`, chosen on `cbRule`, `duelclient.cpp:229`). No editor-time state to source it from at all today. |
| `forbidden_types` (`uint32_t`) | **Partial, narrow overlap only.** The editor's unconditional Skill-card cap (`main_skill_count >= 1`, §2.2) happens to enforce a stricter, always-on special case of what a host *could* also achieve generally via this field - but it is not driven by any bitmask value, configurable, or generalizable to other `TYPE_*` bits. | `mainGame->forbiddentypes`, a persistent per-installation setting (`lastDuelForbidden`) chosen on a duel-rules screen the deck editor never shows (§3). |
| `rituals_belong_in_extra` (`bool`) | **No, outside of mid-match side-decking.** Plain editor mode uses `CardDataC::isRush()` - a static fact about the *card*, not a chosen ruleset (§2.4). Only `is_siding` mode (an active duel already in progress) reads a real field flag. | `host_info.duel_flag_high`'s `DUEL_EXTRA_DECK_RITUAL` bit, negotiated per room (§3) - only exists once two players are already matched. |
| `content_checking_enabled` (`bool`) | **No.** The editor has no on/off concept for content checking; it never performs it regardless. | `host_info.no_check_deck_content`, a host-dialog checkbox (`duelclient.cpp:239`). |
| `lflist` (`std::optional<LfList>`) | **Yes - the one field with a genuine, always-present, real editor-time analogue.** `filterList` (§2.3), backed by the identical `_lfList`/file-grammar this project's `LfList`/`parse_lflist()` already reproduce byte-for-byte. | `cbDBLFList`'s current selection. Two caveats: (1) upstream's own editor selection is advisory only (`check_limit`/search-filter), never a final gate - selecting a list does not stop `check_limit`'s bypasses (§2.4) or file-import (§2.5) from producing a deck that violates it; (2) the null-vs-concrete-"N/A" distinction this field exists to preserve (`validation_policy.h`, `deck-legality.md`§5) has **no equivalent selection state in the editor** - `cbDBLFList` always has some concrete item selected, defaulting to index `0`, never "nothing"; upstream's actual null-`LFList*` case only arises at duel entry, when `GetLFList(host_info.lflist)` fails to find a hash match (e.g. a host's persisted `lastlflist` hash no longer corresponds to any currently-loaded file) - a session-resolution failure mode, not a UI gesture. |

---

## 6. Investigation questions, answered explicitly

1. **Does the deck editor validate the whole deck at all, via `CheckDeckContent`/
   `CheckDeckSize`?** No - see §1. It performs three separate, narrower, bypassable
   mechanisms instead (§2.1-2.3).
2. **Where does the editor's active banlist come from, how is it chosen/persisted, what is in
   it?** `_lfList`, loaded once at startup from `./expansions/lflist.conf`, `./lflist.conf`,
   every `*.conf` under `./lflists/`, plus a synthesized zero-hash "N/A" entry
   (`deck_manager.cpp:97-108`); the editor's current selection is `cbDBLFList`'s combo index
   into that vector, persisted across sessions as a **hash** in `gGameConfig->lastlflist`
   (§2.3) - the same config value the "host a duel room" screen's own selector shares.
3. **Where do the non-banlist `ValidationPolicy` inputs come from - are any meaningful with no
   duel and no host?** All five are duel-room/`HostInfo` concepts with no live editor-time
   state behind them (§5) - `deck_sizes`, `allowed_cards`, `forbidden_types`, and
   `content_checking_enabled` have no editor analogue whatsoever; `rituals_belong_in_extra`
   has a same-shaped-but-differently-sourced analogue (`isRush()`) only outside a duel, and a
   real one only inside a live, already-matched duel's side-decking mode.
4. **Can a user build and save a deck upstream would reject at duel entry?** Yes - see §4's
   closing paragraph. This is not an edge case; several ordinary paths (file-open, clipboard
   import, Shift-held adds, the global "ignore deck contents" setting) all produce it by
   design.
5. **What does the user see, and when?** Nothing in the editor itself, ever (§2.7) - blocked
   adds are silent no-ops. The only actual error message anywhere is a modal popup rendered in
   the lobby screen, triggered only by `PlayerReady`'s rejection, with per-error-type text
   (`duelclient.cpp:470-554`) - structurally and temporally disconnected from the screen where
   the deck was built.

---

## 7. Recommended boundary

**Confidence: medium.** High confidence in everything upstream-descriptive above (§1-6, all
directly cited); medium, not high, confidence in the recommendation itself, because it
requires a genuine judgment call upstream's own source does not resolve - see the closing
paragraph of this section for the one fact that would most change it.

### The shape of the problem

`policy::ValidationPolicy` correctly has no default (ADR 0007 Decision 3) because upstream
itself has none outside a live host session (§3, §5). This document's finding sharpens that:
upstream's *editor* does not merely lack a default for these fields - it has no concept of
five of the six at all (§5). Building this project's deck-builder legality UI is therefore
not "find where upstream's editor keeps this value and read it the same way" for five of six
fields; there is no such place to read. Only `lflist` has a real, direct editor-time upstream
analogue to follow.

### Recommendation

1. **`lflist`: give this project's deck builder a real, user-visible "Banlist" selector**,
   directly analogous to `cbDBLFList` - backed by `policy::load_lflist()`/`parse_lflist()`
   (already built, already tested), populated from whatever `.conf` files this project's own
   `--card-db`-style explicit-path convention supplies (matching `deck-builder-ui.md`§4's
   already-established "no fabricated fallback" precedent for the card database itself), with
   a "no banlist selected" state that maps to `std::nullopt` - **not** defaulting to some
   concrete list the way `cbDBLFList` always has *something* selected. This is the one field
   this document found strong upstream precedent for, and the one place the null-vs-"N/A"
   distinction (`deck-legality.md`§5) actually has a UI decision to be faithful to: this
   project's selector can and should make "no banlist" an honest, explicit, distinct choice
   from "the N/A list," which upstream's own combo-box-always-has-a-selection UI cannot
   cleanly do.
2. **For the other five fields, do not invent a silent default inside `policy/` or `ui/`.**
   Since upstream itself has no default for these outside a live room, and `docs/ROADMAP.md`
   places "Lobby and network screens" at M4 - a full milestone after the current M3 deck
   builder work - this project faces a genuine choice, not a discovery:
   - **(a) Defer legality entirely** until M4's networking/hosting layer gives a real
     session/host concept to source these five fields from, matching upstream's own actual
     boundary (the editor genuinely does not check them either) most faithfully - at the cost
     of leaving M3's roadmap legality item unbuilt for another milestone.
   - **(b) Introduce a small, explicitly-named, user-visible "ruleset" choice now** (e.g. a
     single "Ruleset: Standard OCG/TCG" selector exposed in the deck builder's own UI, whose
     values this project chooses and documents as its own, not upstream's) to supply
     `deck_sizes`/`allowed_cards`/`forbidden_types`/`rituals_belong_in_extra`/
     `content_checking_enabled`, while `lflist` (§ point 1 above) stays independently
     selectable. This lets `validate_deck()` be wired in now, satisfying the roadmap item,
     but requires this project to originate a ruleset upstream does not provide outside a
     live session - the same category of move ADR 0007 Decision 3 explicitly refused to make
     silently *inside* `policy/`. Doing it in `ui/` instead does not remove that concern; it
     only changes which layer owns the decision. If chosen, the ratifying brief must require
     this choice to be visible and named in the UI, never a hidden default a user cannot see
     or change - and must record it as a documented, deliberate, this-project's-own choice
     (a candidate ADR, not this document - recording a decision is not what this document
     does; see its own opening framing, above, as archaeology plus an input to a future
     decision).
   - **(c) A fuller session-shaped abstraction that anticipates M4's eventual `HostInfo`
     equivalent now** - considered and not recommended: it asks this project to design a
     shape for a concept (a duel/session ruleset) that does not exist anywhere in this
     codebase yet and whose real requirements will only be known once M4's actual networking
     design is done. Building it now risks a second, incompatible shape needing to be
     reconciled with M4's real one later - over-engineering for this slice.
   - I lean toward **(b)**, on the strength of `lflist`'s real precedent and the fact that a
     visible, honestly-labeled "Standard OCG/TCG" choice is a small, legible piece of new
     surface area, not a silent invention - but this is exactly the kind of call this
     document's own brief says is Brain/the owner's to ratify, not mine to decide.
3. **Whichever option is chosen, the boundary itself does not move**: `validate_deck()`
   remains the only place this project computes legality (`deck-legality.md`§12); any ruleset
   object §5/point 2 introduces is caller-supplied plain data constructed in `ui/`'s C++
   adapter layer (matching ADR 0006's existing boundary exactly - never in QML, never inside
   `policy/` itself), and QML only ever renders the resulting `DeckValidationError`.
   Upstream's own editor/duel-entry split (§4) is itself a case *against* treating a
   deck-builder-time check as authoritative or blocking in the same way duel entry's is -
   whatever this project builds should be presented as advisory ("this deck would not be
   accepted at duel entry: ..."), matching how loosely upstream's own two mechanisms actually
   relate, not as a hard gate the editor enforces upstream never did either.

**Scheduling is one relevant signal for this recommendation, not the only one.** How soon
M4's "Lobby and network screens" item is actually expected to start affects the calculus: if
imminent, the ruleset concept option (b) asks this project to invent would be built, then
almost immediately superseded by a real one, favoring option (a). If M4 remains distant,
(b)'s cost (a small, own, documented ruleset surface) is easier to justify to unblock M3's
legality item now. But the deeper tension this document establishes does not dissolve either
way: for five of `ValidationPolicy`'s six fields, upstream itself has no concept of the value
at all outside a live host session (§5) - so option (b) is not "read what upstream would do
and copy it sooner," it is this project originating rules Yu-Gi-Oh's own upstream client
never had to define outside a duel room, the same category of move CLAUDE.md's "the UI must
not implement game rules" and ADR 0007 Decision 3 both caution against doing silently.
`docs/ROADMAP.md` places M4 after the remainder of M3 with no closer date; that timing
informs the schedule side of the trade-off, but does not by itself resolve which side of it
this project should be on.

---

## Open questions

- Whether the M3 roadmap intends "legality" to mean full `validate_deck()` wiring, or a lesser
  visible subset (e.g. banlist-only, matching the one field with real upstream precedent) -
  not resolved by anything read for this document.
- Whether a future `ui/` ruleset type (if option (b) above is chosen) should live beside
  `DeckController` in `ui/src/deckbuilder/`, or as a new small class of its own - a design
  question this document deliberately leaves open, since no QML or interaction design is
  in its own scope (see this document's opening framing, above).
- **Not actually open, on inspection: whether `policy::ValidationPolicy` should ever grow a
  *named preset* factory function.** Both `validation_policy.h`'s own doc comment and ADR
  0007 Decision 3 already answer this - a future UI/session layer may define named
  convenience presets once it has a ruleset selection concept to attach them to, and that is
  explicitly out of scope for `policy/` itself. Listed here only because a prior draft of
  this document posed it as unresolved; it is not.

# Main or Extra Deck: upstream's rule and this project's one copy of it

Where upstream EDOPro decides whether a card goes in the Main Deck or the Extra Deck, what it
decides in each case, and how this project reproduces that. The rule itself lives in
`policy/include/edopro_next/policy/deck_placement.h`; the decisions about where it lives and
when the deck builder applies it are in [ADR 0011](../adr/0011-extra-deck-classification.md).

Every quotation below was read from `gframe/` at the upstream base recorded in
`docs/UPSTREAM.md`, and from `ocgcore/` at submodule commit `46779fbe` (v11.0-86). Line
numbers refer to those files.

---

## 1. The places upstream decides

| Where | Function | What it decides |
|---|---|---|
| Loading any deck | `DeckManager::LoadDeck`, `gframe/deck_manager.cpp:330-392` | Main or Extra for every non-side code, through its `is_extra_deck_card` lambda (§2, §5) |
| Validating at duel entry | `DeckManager::CheckDeckContent`, `gframe/deck_manager.cpp:204-236` | whether a card already in Main or Extra is allowed there (§4) |
| Adding a card in the deck builder | `DeckBuilder::push_main`/`push_extra`, `gframe/deck_con.cpp:1577-1636`, and the call sites that try them in turn (§3) | whether a section accepts the card; the caller's order of attempts turns that into a placement |

The first two agree on a common rule (§4). The third is a different piece of code: for most
type-bit combinations it agrees with that rule under `RITUAL_LOCATION::DEFAULT`, and for six
families of them it does not (§3.3).

---

## 2. The rule: `is_extra_deck_card`

`gframe/deck_manager.cpp:335-348`:

```cpp
auto is_extra_deck_card = [&](auto* card) {
	if(card->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
		return true;
	if(card->type & (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER))
		return true;
	if(card->isRitualMonster()) {
		if(rituals_in_extra == RITUAL_LOCATION::DEFAULT) {
			return card->isRush();
		} else {
			return rituals_in_extra == RITUAL_LOCATION::EXTRA;
		}
	}
	return false;
};
```

The order matters: Fusion, Synchro and Xyz are asked first, then Link, then Ritual. A Ritual
Monster that is also a Fusion (or Synchro, Xyz, Link Monster) is therefore Extra under every
`RITUAL_LOCATION`, including `MAIN`.

### 2.1 The Link line means "Link and Monster"

Line 338 reads `card->type & (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER)`. The lambda is
only ever called as `is_extra_deck_card(cd)` (`:359`), so `card == cd`. The parenthesised `&&`
has type `bool`, which is `0` or `1`, and `TYPE_MONSTER` is `0x1`
(`ocgcore/ocgapi_constants.h:33`: `#define TYPE_MONSTER     0x1`). So the line is
`card->type & 1` when the card is a Link Monster, and `card->type & 0` otherwise: true exactly
when the card is both a Link card and a Monster. A Link Spell (`TYPE_SPELL | TYPE_LINK`) is not
an Extra Deck card; nor is a card with the Link bit and no Monster bit of any other shape.
`CheckDeckContent` spells the same test plainly: `cit->type & TYPE_LINK && cit->type &
TYPE_MONSTER` (`:220`, `:231`).

### 2.2 Ritual Monsters, Rush, and `RITUAL_LOCATION`

`gframe/data_manager.h:92-98`:

```cpp
bool isRitualMonster() const {
	return (type & (TYPE_MONSTER | TYPE_RITUAL)) == (TYPE_MONSTER | TYPE_RITUAL);
}

bool isRush() const {
	return ot & SCOPE_RUSH;
}
```

with `#define SCOPE_RUSH       0x200` (`gframe/data_manager.h:29`). A Ritual Spell is not a
Ritual Monster, so no `RITUAL_LOCATION` moves it. `gframe/deck.h:19-23`:

```cpp
enum class RITUAL_LOCATION : uint8_t {
	DEFAULT,
	MAIN,
	EXTRA,
};
```

| `RITUAL_LOCATION` | Ritual Monster, not Rush | Ritual Monster, Rush |
|---|---|---|
| `DEFAULT` | Main | Extra |
| `MAIN` | Main | Main |
| `EXTRA` | Extra | Extra |

### 2.3 Where each `RITUAL_LOCATION` comes from

`LoadDeck`, `LoadDeckFromBuffer` and `LoadDeckFromFile` all default the argument to
`DEFAULT` (`gframe/deck_manager.h:77-80`). The only places that pass anything else convert a
duel-rule boolean, always the same way:

- `gframe/generic_duel.cpp:421-423`, the server receiving a player's deck:
  `bool rituals_in_extra = host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32);` then
  `LoadDeckFromBuffer(..., rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN)`.
- `gframe/deck_manager.cpp:407`, `LoadSide` (side-decking between games), the same
  conversion of its `bool rituals_in_extra` parameter, which `generic_duel.cpp:426` passes.
- `gframe/menu_handler.cpp:314-315` and `:810-812`, the client loading its selected deck when
  the player readies up in a room:
  `mainGame->dInfo.HasFieldFlag(DUEL_EXTRA_DECK_RITUAL) ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN`.

`DUEL_EXTRA_DECK_RITUAL` is `0x800000000` and is part of `DUEL_MODE_RUSH`
(`ocgcore/ocgapi_constants.h:414`, `:417`). Every other load (the deck builder's own opens,
`.ydke` import, Omega import, WindBot) takes `DEFAULT` (§5.2).

---

## 3. Adding a card in upstream's deck builder

### 3.1 The two push functions

`gframe/deck_con.cpp:1577-1588` (`push_main`, the part before its count and Legend limits):

```cpp
bool DeckBuilder::push_main(const CardDataC* pointer, int seq, bool forced) {
	if(pointer->isRitualMonster()) {
		if(mainGame->is_siding) {
			if(mainGame->dInfo.HasFieldFlag(DUEL_EXTRA_DECK_RITUAL))
				return false;
		} else if(pointer->isRush() && !forced)
			return false;
	}
	if(pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
		return false;
	if((pointer->type & (TYPE_LINK | TYPE_SPELL)) == TYPE_LINK)
		return false;
```

`gframe/deck_con.cpp:1610-1621` (`push_extra`, same part):

```cpp
bool DeckBuilder::push_extra(const CardDataC* pointer, int seq, bool forced) {
	if(pointer->isRitualMonster()) {
		if(mainGame->is_siding) {
			if(!mainGame->dInfo.HasFieldFlag(DUEL_EXTRA_DECK_RITUAL))
				return false;
		} else if(!pointer->isRush() && !forced)
			return false;
	} else if(pointer->type & TYPE_LINK) {
		if(pointer->type & TYPE_SPELL)
			return false;
	} else if((pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) == 0)
		return false;
```

After these type gates, when neither `forced` nor side-decking, `push_main` also refuses a
second Legend card of the same kind, a second Skill, and a 61st card (`:1590-1601`), and
`push_extra` refuses a second Legend Monster and a 16th card (`:1623-1628`). `push_side`
(`:1637-1648`) has no type gate at all, only a 15-card limit.

### 3.2 The order of attempts

No single function chooses a section; each call site tries sections in an order and stops at
the first that accepts:

- Right-click on a search result (`:717-727`): after `check_limit` (the copy limit,
  `:719`), Shift sends it to `push_side` (`:721-722`); otherwise
  `if (!push_main(pointer, -1, gGameConfig->ignoreDeckContents) && !push_extra(pointer, -1, gGameConfig->ignoreDeckContents)) push_side(pointer);` (`:725-726`).
- Middle-click (`:744-771`; not while side-decking, `:747-748`), after `check_limit` unless
  `forceInput` (`:756`), where `forceInput` is
  `gGameConfig->ignoreDeckContents || event.MouseInput.Shift` (`:624`). It never passes
  `forced`. On a search result (`:767-769`):
  `if(!push_extra(pointer) && !push_main(pointer)) push_side(pointer);`. On a card already in
  Main (`:758-760`): `if(!push_main(pointer)) push_side(pointer);`. On one in Extra
  (`:761-763`): `if(!push_extra(pointer)) push_side(pointer);`. On one in Side
  (`:764-766`): `if(!push_side(pointer) && !push_extra(pointer)) push_main(pointer);`.
- Dragging a card onto a section (`:664-669`) calls that section's push with `forceInput`;
  if it refuses, a card dragged out of a deck section goes back to that section
  (`:672-678`), and one dragged from the search results is not added.
- Dropping card-name text onto the deck (`:875-878`):
  `push_main(dragging_pointer, hovered_seq, true) || push_extra(dragging_pointer, hovered_seq + is_lastcard, true);`
- A card dragged from the Side deck (`click_pos == 3`) and released with the right button
  (`:729-736`): `if(!push_extra(dragging_pointer)) push_main(dragging_pointer);`; from Main or
  Extra (`:730-733`) it goes to `push_side`.
- While side-decking, right-click on a Side card (`:700-702`): `if(push_extra(pointer) || push_main(pointer))`.

Every unforced attempt that chooses between Main and Extra tries them in one of two orders:
Main then Extra (right-click on a search result, `:725`) or Extra then Main (middle-click on
a search result, `:768`; a card from Side released with the right button, `:735-736`; a
middle-click on a Side card once Side refuses it, `:765-766`; and, while side-decking only,
`:701`). The other unforced attempts (`:759`, `:762`, `:674-678`) try only the section the
card came from (`:759` and `:762` then fall back to Side), and so choose nothing by type.

Tokens never reach any of these: the search result filter rejects them first,
`gframe/deck_con.cpp:1192`:
`if(data._data.type & TYPE_TOKEN || data._data.ot & SCOPE_HIDDEN || ...) return false;`.
An unknown code cannot be a search result either, since results come from the loaded
database.

### 3.3 What that adds up to

For an ordinary right-click or middle-click add, outside side-decking, with `forced` false
and the section not full ("unforced" below), the ordinary card shapes land like this:

| Card | `push_main` | `push_extra` | Lands in |
|---|---|---|---|
| Fusion, Synchro, Xyz (not Ritual) | refuses (`:1585`) | accepts (`:1620` not taken) | Extra |
| Link Monster (not Ritual) | refuses (`:1587`) | accepts (`:1617-1619`) | Extra |
| Link Spell | accepts | refuses (`:1618`) | Main |
| Ritual Monster, not Rush | accepts | refuses (`:1615`) | Main |
| Ritual Monster, Rush | refuses (`:1582`) | accepts | Extra |
| Any other card | accepts | refuses (`:1620`) | Main |

That is `is_extra_deck_card` under `RITUAL_LOCATION::DEFAULT`, row for row, but it is a list of
the shapes someone thought of, not of every combination of type bits. The exhaustive statement
follows.

**The order of the two attempts does not matter** (§3.2 lists both orders and every place each
occurs). No card is accepted by both functions: `push_extra` accepts only a Rush Ritual Monster
(which `push_main` refuses at `:1582`), a card that is not a Ritual Monster, with the Link bit
and not the Spell bit (which `push_main` refuses at `:1587`), or a card that is not a Ritual
Monster and not Link, with a Fusion, Synchro or Xyz bit (which `push_main` refuses at
`:1585`). So the two orders always agree on which of Main and Extra takes a card. When both
refuse it, the two attempts place nothing, and the call sites that have a fallback push it to
Side (`:725-726`, `:768-769`); `:735-736` has none. Dragging onto a section and the text drop
use `forced`, which is out of this table (§6).

**Where the deck builder and this project's rule differ.** The rule is the lambda under
`RITUAL_LOCATION::DEFAULT` (§2); the deck builder is the unforced cascade above. Over every
combination of the eight type bits either one reads (Monster, Spell, Trap, Fusion, Ritual,
Synchro, Xyz, Link: 256) in both scopes (Rush: `ot & SCOPE_RUSH`; not Rush), 512 combinations
in all, they disagree on exactly 156, which fall into six families and no others:

| | Card (type bits; scope) | `push_main` | `push_extra` | Deck builder | This project | Combinations |
|---|---|---|---|---|---|---|
| A | Link bit; no Monster, Spell, Fusion, Synchro or Xyz bit; either scope | refuses (`:1587`) | accepts (Link branch, `:1617-1619`) | Extra | Main | 8 |
| B1 | Link, Spell and Monster bits; no Ritual, Fusion, Synchro or Xyz bit; either scope | accepts (`:1587` needs Link without Spell) | refuses (`:1618`) | Main | Extra | 4 |
| B2 | Ritual Monster with Link and Spell bits; no Fusion, Synchro or Xyz bit; not Rush | accepts | refuses (non-Rush Ritual Monster, `:1615`) | Main | Extra | 2 |
| C1 | Ritual Monster with the Link bit, no Spell, Fusion, Synchro or Xyz bit; not Rush | refuses (`:1587`) | refuses (`:1615`) | Side | Extra | 2 |
| C2 | Ritual Monster with at least one Fusion, Synchro or Xyz bit (any Link, Spell, Trap bits); not Rush | refuses (`:1585`) | refuses (`:1615`) | Side | Extra | 56 |
| D | not a Ritual Monster; Link and Spell bits and at least one Fusion, Synchro or Xyz bit; either scope | refuses (`:1585`) | refuses: the Link branch is taken and refuses the Spell (`:1618`), so the Fusion/Synchro/Xyz test is never reached | Side | Extra | 84 |

The Trap bit is read by neither rule and is free in every family (in A the Ritual bit is free
too, since without the Monster bit it is not a Ritual Monster); "Ritual Monster" is the
Monster and Ritual bits together (`isRitualMonster()`, §2.2). A Ritual Monster in Rush scope
never differs: `push_main` refuses it (`:1582`), `push_extra` accepts it, and the lambda says
Extra. Tokens never reach the push functions (§3.2), and no other `SCOPE_*` bit is read by
either rule, so nothing else enters the count. The remaining 356 combinations agree.

The count 156 = 8 + 4 + 2 + 2 + 56 + 84 and the rows above are not asserted by this document
alone: `policy/tests/test_deck_placement.cpp`,
`deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies`, transcribes
`push_main` and `push_extra` from the lines quoted in §3.1, runs all 512 combinations, and
requires the set of cards the cascade and `belongs_in_extra_deck(..., RushInExtra)` disagree
on to be exactly the union of these six families, each with the two outcomes and the count
shown. It also checks that the two call-site orders agree and that no card is accepted by both
pushes. Changing the transcription, the families or the rule without the other two makes it
fail.

**Which is right at duel entry.** In every one of the six families the section this project
chooses is the section `LoadDeck` gives the card whatever `RITUAL_LOCATION` the duel uses
(the test checks this for every differing card), so it is where the server's re-split puts the
card on the way into a duel (§5.4). The deck builder's answer is either Side (C1, C2, D: the
card is not placed at all) or a section the re-split moves the card out of (A, B1, B2). What
duel entry then does with a Ritual Monster in B2, C1 or C2 is §4's hybrid: `LoadDeck` says
Extra, and `CheckDeckContent`'s Extra callback accepts it only when the duel has
`DUEL_EXTRA_DECK_RITUAL`. Section 6 records what this project does about all of this.

**Whether any card has these shapes is not claimed.** This record says nothing about which of
the 156 combinations occur in any card database; the test covers every combination, which does
not depend on the answer. (An earlier version of this section said that two of these shapes
were not found in a local Project Ignis install. That was a read-only count over data this
project does not commit, it covered only two of the six families, and it cannot be reproduced
in CI, so it is removed rather than restated.)

Three more differences are not about type at all, and are listed in §6 as decisions: the
61st-card, 16th-card and Legend/Skill refusals that push a card to Side instead; `forced`
(`gGameConfig->ignoreDeckContents`, or a text drop) letting a Rush Ritual Monster into Main
and a non-Rush one into Extra; and side-decking, where both push functions follow the
duel's `DUEL_EXTRA_DECK_RITUAL` flag instead of `isRush()`.

---

## 4. Validating at duel entry

`gframe/deck_manager.cpp:219-234`:

```cpp
ret = CheckCards(deck.main, lflist, allowedCards, ccount, [&](const CardDataC* cit)->DeckError {
	if ((cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) || (cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
		return { DeckError::EXTRACOUNT };
	if(cit->isRitualMonster() && rituals_in_extra)
		return { DeckError::EXTRACOUNT };
	return { DeckError::NONE };
});
if (ret.type) return ret;
ret = CheckCards(deck.extra, lflist, allowedCards, ccount, [&](const CardDataC* cit)->DeckError {
	if(cit->isRitualMonster()) {
		if(!rituals_in_extra)
			return { DeckError::EXTRACOUNT };
	} else if (!(cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) && !(cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
		return { DeckError::EXTRACOUNT };
	return { DeckError::NONE };
});
```

The Main callback rejects exactly the cards `is_extra_deck_card` sends to Extra under
`rituals_in_extra ? EXTRA : MAIN`: Fusion/Synchro/Xyz, Link Monster, then Ritual Monster when
`rituals_in_extra`. Term for term the same rule.

The Extra callback asks the Ritual question **first**. For every card except one shape, "the
Extra callback accepts it" equals "`is_extra_deck_card` says Extra". The exception is the
Ritual Monster that is also Fusion/Synchro/Xyz/Link Monster: with `rituals_in_extra` false,
`LoadDeck` puts it in Extra (type first) and `CheckDeckContent` then rejects it there (Ritual
first). That is upstream disagreeing with itself; this project reproduces it rather than
choosing a side (ADR 0011, Decision 1).

---

## 5. Loading a deck

### 5.1 The loop

`gframe/deck_manager.cpp:349-364`, the Main-list loop (the Side-list loop at `:379-390` is
the same without the lambda, and the Extra-list loop is §5.3):

```cpp
for(auto code : mainlist) {
	if(!(cd = gDataManager->GetCardData(code))) {
		cd = gdeckManager->GetDummyOrMappedCardData(code);
		if((!cd || cd->code == 0) && !loadalways) {
			errorcode = code;
			continue;
		}
	}
	if(!cd || cd->type & TYPE_TOKEN)
		continue;
	else if((!extralist || cd->code != 0) && is_extra_deck_card(cd))  {
		deck.extra.push_back(cd);
	} else {
		deck.main.push_back(cd);
	}
}
```

with `const bool loadalways = !!extralist;` (`:334`).

- **Tokens** (`cd->type & TYPE_TOKEN`, `:357`) are dropped from every list, in every mode,
  before the lambda is asked. `TYPE_TOKEN` is `0x4000` (`ocgcore/ocgapi_constants.h:46`).
- **Unknown codes.** `GetDummyOrMappedCardData` (`gframe/deck_manager.cpp:17-28`) has two
  phases. While `load_dummies` is true it returns a placeholder for the code, created once and
  cached, with `code = 0`, `alias` set to the original code and every other field zero
  (`:20-27`); the id-mapping file is not consulted. Upstream stops loading placeholders once
  its card repositories have finished updating (`gframe/game.cpp:2682`,
  `gframe/data_handler.cpp:159-160`); from then on the function returns the card an id-mapping
  file maps the code to, or null (`:18-19`; `DataManager::GetMappedCardData`,
  `gframe/data_manager.cpp:349-354`). So:
  - without an extra list (`loadalways` false): a code that resolves to a placeholder or to
    null is dropped and becomes the returned error code (`:352-354`);
  - with an extra list: a placeholder is kept and, since its `code` is `0`, never
    classified (`:359`), so it stays in the list it came from; with placeholders off, an
    unmapped code gives null and is dropped silently (`:357`).
- **Mapped codes** are classified by the card they map to, but only once placeholders are
  off; while they are on, a mapped code becomes a placeholder like any other unknown code.

### 5.2 The two modes, and which caller uses which

`LoadDeckFromFile` (`gframe/deck_manager.cpp:319-329`) passes `separated ? &extralist :
nullptr`, and `LoadCardList` only honours the `#extra` marker when it has an extra list to
fill (`:287-290`).

| Mode | Extra list | What the file's `#extra` does | Callers | `RITUAL_LOCATION` |
|---|---|---|---|---|
| not separated | none | nothing: every non-side code is classified by the lambda | `menu_handler.cpp:314`, `:810` (ready in a room); `menu_handler.cpp:1114` (a `.ydk` dropped on the main menu); `LoadDeckFromBuffer` (`deck_manager.cpp:259-271`): the server at `generic_duel.cpp:423`, `LoadSide` at `deck_manager.cpp:407`, Omega import at `:595` | duel flag at the room paths, the server and `LoadSide`; `DEFAULT` at `:1114` and Omega |
| separated | yes | its cards go to Extra unclassified (§5.3) | the deck builder's opens (`deck_con.cpp:409`, `:422`, `:521`, `:889`; `menu_handler.cpp:537`, `:543`); `.ydke` import (`deck_manager.cpp:552`); WindBot (`windbot.cpp:69`) | `DEFAULT` |

### 5.3 The separated mode's Extra list

`gframe/deck_manager.cpp:365-378` pushes every resolvable, non-token code from the Extra list
into `deck.extra` with no type check at all. So separated mode corrects Main to Extra (a
Fusion under `#main` moves to Extra) and never Extra to Main (a Normal Monster under `#extra`
stays in Extra). [ydk-interoperability.md](ydk-interoperability.md) proves the Main-to-Extra
move against the real `LoadDeckFromFile` in CI.

### 5.4 What reaches duel entry

A client sends its deck with Main and Extra concatenated into one list
(`gframe/menu_handler.cpp:43-48`:
`BufferIO::Write<uint32_t>(pdeck, static_cast<uint32_t>(deck.main.size() + deck.extra.size()));`
then every Main code, then every Extra code). The server rebuilds it with
`LoadDeckFromBuffer` in the not-separated mode (`generic_duel.cpp:423`) before
`CheckDeckContent` runs, so upstream re-derives every card's section from its type, under the
duel's own `RITUAL_LOCATION`, and drops tokens and unknown codes (§5.1), on the way into a
duel. A deck whose split disagrees with the rule in the editor is, at a real upstream duel
entry, re-split rather than rejected - except for the Ritual hybrid of §4, which the re-split
itself puts where the Extra callback rejects it. See §6, "Legality messages", for what this
means for this project's legality messages.

---

## 6. This project

### Where the rule lives

`policy/include/edopro_next/policy/deck_placement.h`, next to the validation it shares a
rule with (ADR 0011, Decision 1):

- `RitualPlacement::{RushInExtra, Main, Extra}` mirrors `RITUAL_LOCATION::{DEFAULT, MAIN,
  EXTRA}`; `ritual_placement_for(bool)` is upstream's `flag ? EXTRA : MAIN` conversion.
- `is_ritual_monster`, `is_rush`, `is_token`, `is_extra_deck_type` are `isRitualMonster()`,
  `isRush()`, the token test and the lambda's first two lines.
- `belongs_in_extra_deck(record, rituals)` is the lambda, in its own order.
- `classify_card(database, code, rituals)` adds the loop's token and unknown handling and
  returns `Main`, `Extra`, `Token` or `Unknown`.

### What uses it

- `policy::validate_deck()`: the Main zone check calls `belongs_in_extra_deck` directly; the
  Extra zone check is built from `is_ritual_monster` and `is_extra_deck_type` in upstream's
  Ritual-first order (§4). Its behaviour is unchanged and its tests are untouched.
- `DeckController::placementFor()` / `addCardToDeck()` (`ui/src/deckbuilder/`), under
  `RitualPlacement::RushInExtra` (§3.3). The screen's add button names the section the
  controller reports and calls `addCardToDeck()`; QML inspects no card type.

### Decisions and divergences

All recorded, with reasons, in [ADR 0011](../adr/0011-extra-deck-classification.md):

- **Adding uses the lambda under `DEFAULT`, not the push functions.** In the six families of
  §3.3 (156 of the 512 type-bit and scope combinations) this project follows the lambda: A goes
  to Main (upstream's deck builder: Extra); B1 and B2 go to Extra (deck builder: Main); C1, C2
  and D go to Extra (deck builder: Side, that is, not placed). The reason is in ADR 0011,
  Decision 2: in every family the lambda's answer is where `LoadDeck`, and so the server's
  re-split at duel entry, puts the card, and the cascade's is not.
- **No capacity or Legend/Skill fallback.** Upstream sends a card to Side when Main has 60
  cards, Extra has 15, or a Legend/Skill limit is reached; this editor always places by type
  and lets advisory legality report the count (ADR 0010's non-blocking model, and
  `deck-builder-ui.md` §7's "no cap").
- **No copy-limit gate on add.** Upstream's `check_limit` (`deck_con.cpp:719`) refuses a
  fourth copy; this editor adds it and legality reports it (as before this round).
- **No `forced` mode.** Upstream's `ignoreDeckContents` and text drop let a Ritual Monster
  into the "other" section; here the explicit `addCard(code, section)` primitive is the only
  way to place a card against the rule, and the screen offers it only for Side.
- **No side-decking.** This editor has no between-games side-decking, so the push functions'
  `is_siding` branch has no counterpart.
- **Tokens are never placed, including in Side.** Upstream never offers one (§3.2); here the
  search does show tokens (card search is out of this round's scope), so both add buttons are
  disabled for one and `addCardToDeck()` refuses it.
- **Opening a `.ydk` does not reclassify.** ADR 0004, Decision 2 stands: the deck shows what
  the file says. Upstream's deck builder opens in separated mode and would move a
  Fusion/Synchro/Xyz/Link Monster (or, under `DEFAULT`, a Rush Ritual Monster) out of `#main`
  into Extra and drop tokens. Here such a card stays in Main and advisory legality reports it
  when a banlist is selected; saving writes back exactly what was opened plus the user's
  edits.
- **Legality messages describe the deck as arranged, not what duel entry would do.** The
  computation is unchanged (`policy::validate_deck()` on the sections as the editor holds
  them), but §5.4 shows the server does not validate that arrangement: it re-derives Main and
  Extra from card type under the duel's `RITUAL_LOCATION`, and `LoadDeck` drops tokens
  (`deck_manager.cpp:357`), before it counts or checks anything. So the deck builder's
  banner no longer says a deck "would not be accepted at duel entry" or is "legal for duel
  entry". Errors read "Fails as arranged: ..." and a pass reads "Passes as arranged under this
  ruleset and banlist." The placement error, which used to say a card "belongs in the Main
  deck, not the Extra deck" even for an Extra Deck card sitting in Main, now says only that the
  card "is in a section that does not accept it (whether a card goes in the Main deck or the
  Extra deck follows from its type)". Why each old claim was false, and when the new ones are
  true, is in ADR 0011, Decision 5. Not changed: the disclosure that no banlist is selected,
  and "Deck meets this ruleset's size and type limits", which do not speak about duel entry.

### Tests

- `policy/tests/test_deck_placement.cpp`: every case in §2-§5 at the rule level (Fusion,
  Synchro, Xyz, Link Monster, Link without Monster, Ritual Monster under each
  `RitualPlacement` with and without Rush, Ritual Spell, Rush non-Ritual, the Ritual hybrid,
  tokens including a token with an Extra Deck bit, unknown codes, and an empty database),
  with hand-written expectations; and one test over all 256 combinations of the eight type
  bits either rule reads, in both scopes and both values of the duel flag, that checks the
  lambda against independent transcriptions of `CheckDeckContent`'s two callbacks and runs
  `validate_deck()` on each card in each section. It counts the 120 hybrid disagreements
  exactly. A second test over all 512 (type bits, scope) combinations transcribes upstream's
  `push_main` and `push_extra` and requires the cards on which the deck builder and this
  project differ to be exactly the six families of §3.3, in both call-site orders.
- `ui/tests/test_deckbuilder.cpp`: `addCardToDeck()` for each card kind, Ritual and Rush
  Ritual placement, refusal of tokens, unknown codes, code 0 and a missing catalog, and an
  opened `.ydk` keeping its sections. `ui/tests/test_deckbuilder_screen.cpp`: the real
  screen's add buttons.

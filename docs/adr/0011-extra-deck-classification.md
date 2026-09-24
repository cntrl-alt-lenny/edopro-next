# ADR 0011 — One definition of the Extra Deck rule, and when the deck builder applies it

## Context

The M3 deck builder let the user choose Main, Extra or Side for every card by hand
(ADR 0006). Automatic Main/Extra classification was the first item the roadmap listed as
still missing, and [ADR 0004](0004-deck-model-ydk-codec.md) (Decision 2, "Consequence") left
it for a later `Deck`-level step with a card database available. `policy/` already held half
of upstream's rule, privately, for validation (`is_unconditionally_extra_deck_type()` and
`is_ritual_monster()` in `deck_validation.cpp`), so adding a second copy for the deck
builder would have given the project two definitions that could disagree.

Upstream decides Main versus Extra in three places, quoted and compared in
[deck-placement.md](../architecture/deck-placement.md): `LoadDeck`'s `is_extra_deck_card`
lambda, `CheckDeckContent`'s two zone callbacks, and the deck builder's `push_main`/
`push_extra` cascade. They share one rule, with a `RITUAL_LOCATION` choosing where Ritual
Monsters go, but disagree with each other on some card shapes (§3.3 and §4 there).

## Decision 1 — The rule lives in `policy/`, as the lambda, and validation calls it

`policy/include/edopro_next/policy/deck_placement.h` defines `belongs_in_extra_deck(record,
RitualPlacement)`, a line-for-line reproduction of the lambda, with its primitives
(`is_extra_deck_type`, `is_ritual_monster`, `is_rush`, `is_token`) and `classify_card()`,
which adds the load loop's token and unknown-code handling.

### Options considered

1. **`data/`.** Rejected: `data/` is a data boundary that never decides game rules
   (`deck.h`, `card_record.h`), and ADR 0004 kept even the deck model free of card types.
2. **A new module.** Rejected: the rule's only existing user is `policy::validate_deck()`,
   and a new module would have to be depended on by `policy/` for no boundary benefit.
3. **`policy/`** (chosen). It already depends on `data::CardRecord`/`CardDatabase`, is
   Qt-free, and already held half the rule. Putting the whole rule here lets validation
   *call* it instead of keeping a copy.

`validate_deck()`'s Main zone check now calls `belongs_in_extra_deck()` directly: upstream's
Main callback is term for term the lambda under `rituals_in_extra ? EXTRA : MAIN`
(deck-placement.md §4). Its Extra zone check keeps upstream's Ritual-first order, built from
the same primitives, because upstream's Extra callback genuinely disagrees with the lambda for
a Ritual Monster that is also Fusion/Synchro/Xyz/Link: reproducing that is what "source is the
arbiter" requires, and choosing either side would change validation behaviour. No
`validate_deck()` test expectation moved. `test_deck_placement.cpp` checks every combination of
the eight relevant type bits against independent transcriptions of both callbacks, so the two
cannot drift apart without a failing test.

### How the Ritual and Rush cases are expressed

`RitualPlacement::{RushInExtra, Main, Extra}` mirrors `RITUAL_LOCATION::{DEFAULT, MAIN,
EXTRA}` value for value, under names that say what each does. `ritual_placement_for(bool)` is
upstream's own `flag ? EXTRA : MAIN` conversion. There is no default: every caller names the
mode it reproduces, as `ValidationPolicy` requires every field. `ValidationPolicy` itself still
takes the resolved boolean (ADR 0007, Decision 4, unchanged), because `CheckDeckContent` does.

## Decision 2 — Adding a card places it by the lambda under `RITUAL_LOCATION::DEFAULT`

`DeckController::addCardToDeck(code)` puts the card where
`classify_card(database, code, RitualPlacement::RushInExtra)` says. `placementFor(code)`
exposes the same answer, and the screen's add button shows it ("Add to Main"/"Add to Extra").

Why `DEFAULT`: outside side-decking, `push_main` refuses a Rush Ritual Monster and `push_extra`
refuses any other Ritual Monster (`deck_con.cpp:1578-1583`, `:1611-1616`), so upstream's
right-click lands them exactly as `DEFAULT` does, whatever the duel rules later say. The
selected ruleset's `ritualsBelongInExtra` is therefore not consulted: upstream's editor does
not consult `DUEL_EXTRA_DECK_RITUAL` either, except while side-decking.

Why the lambda rather than a reproduction of the push functions: the push cascade is not one
rule but a sequence of acceptance tests whose outcome depends on the call site, capacity,
Legend/Skill counts, `forced` and side-decking. Reproducing it would put a second definition
of "belongs in Extra" in the project - exactly what this round must not do.

The cascade and the lambda do not agree everywhere. Over every combination of the eight type
bits either reads (256) in both Rush scopes, they differ on 156 of 512, in six families
(deck-placement.md §3.3 has the table, with the upstream lines for each; every call-site order
upstream uses gives the same answer, §3.2). Each is a divergence, recorded here:

| | Card | Upstream deck builder | This project (and `LoadDeck`) |
|---|---|---|---|
| A | Link bit; no Monster, Spell, Fusion, Synchro or Xyz bit | Extra | Main |
| B1 | Link, Spell and Monster bits; not Ritual, no Fusion/Synchro/Xyz | Main | Extra |
| B2 | Ritual Monster with Link and Spell; no Fusion/Synchro/Xyz; not Rush | Main | Extra |
| C1 | Ritual Monster with Link, no Spell, Fusion, Synchro or Xyz; not Rush | Side | Extra |
| C2 | Ritual Monster with a Fusion, Synchro or Xyz bit; not Rush | Side | Extra |
| D | not a Ritual Monster; Link, Spell and a Fusion, Synchro or Xyz bit | Side | Extra |

`policy/tests/test_deck_placement.cpp` transcribes `push_main`/`push_extra` and requires this
set to be exactly right, so the record, the transcription and the rule cannot drift apart
unnoticed. This ADR makes no claim about whether any card in any database has one of these
bit combinations: the test covers every combination, which does not depend on the answer.
(Round 020 said that two of them were not found in a local Project Ignis install. That was an
unreproducible count over uncommitted data, covering two of the six families; it is removed.)

Options for these families, decided in round 021 (no behaviour changed: this is round 020's
behaviour, now argued over the whole set):

1. **Follow the lambda** (chosen). In every family the lambda's answer is the section
   `LoadDeck` gives the card whatever `RITUAL_LOCATION` the duel uses (the test checks it for
   every differing card), and so the section the server's re-split puts the card in at duel
   entry (deck-placement.md §5.4). The cascade's answer is either Side, meaning not placed
   (C1, C2, D), or a section the re-split then moves the card out of (A, B1, B2). The editor
   therefore shows the split a deck will have once loaded, and there is still one definition.
2. **Reproduce the push functions for the add action.** Rejected: it needs a second definition
   of which cards are Extra Deck cards, to be kept in step with the first by another test,
   and it would place cards where upstream's own load and validation then contradict it, or
   nowhere. That is what a UI cascade does when it is a sequence of refusals; it is not a
   rule to copy.
3. **Refuse to add the families where the cascade says Side.** Rejected: for D the lambda and
   `CheckDeckContent`'s Extra callback agree that the card is an Extra Deck card and valid in
   Extra, so refusing it would make a deck that upstream accepts unbuildable with the add
   button. For C1 and C2 whether duel entry accepts the card depends on the duel's
   `DUEL_EXTRA_DECK_RITUAL` flag (deck-placement.md §4), which the editor does not know.

A change of behaviour here, if it is ever wanted, is a new decision with its own round.

Further divergences from the push cascade, each deliberate:

- **No Side fallback when a section is full or a Legend/Skill limit is reached**, and **no
  copy-limit refusal** (`check_limit`): the card is placed by type and advisory legality
  reports the count, keeping ADR 0010's non-blocking model.
- **No `forced` placement against the rule.** `DeckController::addCard(code, section)`
  remains the explicit, unclassified primitive (used by the screen for Side, by tests, and
  available to a future drag-and-drop); the screen offers no Main/Extra override.
- **No side-decking mode**, since this editor has no between-games side-decking.
- **Tokens are never placed**, in Main, Extra or Side. Upstream never offers a token to add
  (`deck_con.cpp:1192`); this project's search does list tokens (search is out of this
  round's scope), so both add buttons are disabled for one and `addCardToDeck()` refuses it.

## Decision 3 — A card deliberately put in the "wrong" section stays there

The screen no longer has separate "Add to Main" and "Add to Extra" buttons, so an ordinary
add cannot misplace a card. A misplaced card can still come from an opened file or from a
caller of `addCard(code, section)`; it stays where it was put, and advisory legality reports
it when a banlist is selected, as before. Upstream refuses the push instead (unless
`forced`). Refusing here would make a file-loaded misplacement impossible to reproduce by
hand and would turn an advisory check into a blocking one, which ADR 0010 rejected.

## Decision 4 — Opening a `.ydk` still follows the file (ADR 0004, Decision 2 stands)

Upstream's deck builder opens files in separated mode, so `LoadDeck` moves a
Fusion/Synchro/Xyz/Link Monster (or, under `DEFAULT`, a Rush Ritual Monster) listed under
`#main` into Extra, never the other way, and drops tokens (deck-placement.md §5). This project
does not reclassify on open:

- it would silently rewrite a user's file on the next save, and the dirty-state contract
  (deck-builder-ui.md §9) has no notion of "changed by loading";
- the reclassification is one-directional, so it would not even produce a split that
  satisfies the rule (a Normal Monster under `#extra` stays there);
- advisory legality already reports a misplaced card, and upstream's own server re-splits a
  deck by type before validating it anyway (deck-placement.md §5.4).

This is a divergence from upstream's deck builder, recorded here. No `Deck -> Deck`
reclassifier is built, because nothing would call it.

## Decision 5 — Legality messages describe the deck as arranged, not what duel entry would do

Round 020 recorded (deck-placement.md §5.4) that upstream's server does not validate the
arrangement a client built. The client sends Main and Extra as one list
(`gframe/menu_handler.cpp:43-48`); `GenericDuel::UpdateDeck` rebuilds the deck from it with
`LoadDeckFromBuffer(..., rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN)`
(`gframe/generic_duel.cpp:421-423`), the not-separated mode of `LoadDeck`, which classifies
every non-Side card by the lambda under that location, drops every token (`deck_manager.cpp:
357`) and drops unknown codes; `PlayerReady` then runs `CheckDeckSize` and `CheckDeckContent`
on the rebuilt deck (`generic_duel.cpp:373-381`). `policy::validate_deck()` runs on the sections
as the editor holds them, tokens included. Round 021 went through every message the deck
builder shows against that:

| Message (before) | What it claimed | At upstream duel entry |
|---|---|---|
| "%1 belongs in the Main deck, not the Extra deck" (the placement error) | the card would be rejected; and that it belongs in Main | The server moves it by type before checking, so it is not rejected, except the Ritual hybrid of deck-placement.md §4. The error is also raised for an Extra Deck card sitting in *Main*, for which "belongs in the Main deck" was the opposite of the truth. |
| Main/Extra/Side "has N cards, fewer than / exceeding" | the deck would be rejected for that count | The count is taken after the re-split and the token drop, so it differs from the editor's whenever a card is misplaced or a token is present: 61 cards in Main of which one is a Fusion is over the limit in the editor and 60 + 1 at duel entry. |
| every other error (scope, copy limit, banlist, Legend, Skill, forbidden type, unknown code) | the deck would be rejected | These do not depend on the section, but the editor counts a token the server drops (a fourth copy of a token is an editor error and no error there), and the editor's database is not the room's. |
| "Deck is legal for duel entry under this ruleset and banlist." | duel entry would accept the deck | Would hold for a deck without a token: if `validate_deck()` passes, every Main card is non-Extra by the lambda and every Extra card is one the Extra callback accepts and the lambda calls Extra under either `RITUAL_LOCATION`, so the re-split changes nothing. It fails for a deck with a token: the server drops it and counts one card fewer. |

Options: (1) keep the wording wherever it happens to be true: rejected, since no message
qualifies once a token is possible (an opened `.ydk` keeps one, ADR 0004 Decision 2) and the
banner cannot tell the user which case they are in. (2) Make the editor validate what the
server would, re-splitting and dropping tokens first: rejected here, because it changes the
legality computation, which this round does not touch, and it would report a deck legal while
its sections are wrong, against ADR 0004 Decision 2. (3) **Say what the check did** (chosen):
every message states the result of the check on the deck *as arranged* and makes no claim
about duel entry.

- Errors read `Fails as arranged: <reason>`; a pass reads `Passes as arranged under this
  ruleset and banlist.` "As arranged" means the deck's own Main, Extra and Side sections with
  every card counted, which is exactly what `validate_deck()` checks.
- The placement error reads `%1 is in a section that does not accept it (whether a card goes
  in the Main deck or the Extra deck follows from its type).` It does not name the section the
  card belongs in: the error does not say which section failed, and a Ritual hybrid in a duel
  without `DUEL_EXTRA_DECK_RITUAL` is accepted by neither section. What it says is `CheckDeckContent`'s own rule (deck-
  placement.md §4) and is true.
- The reasons themselves ("has 39 cards, fewer than the minimum of 40", "is TCG-only, not
  allowed under this ruleset", ...) are unchanged: each is a fact about the deck as arranged
  and the selected ruleset.
- Not changed: the "No banlist is selected" disclosures and "Deck meets this ruleset's size and
  type limits", which say nothing about duel entry.

`ui/tests/test_deckbuilder.cpp`'s `everyLegalityMessageSpeaksAboutTheDeckAsArrangedOnly` pins
the exact text of every message and that none mentions duel entry.

## Consequences

- One definition: `policy/deck_placement.h`. `validate_deck()` and the deck builder both
  reach it; `ui/qml/` inspects no card type.
- [ADR 0006](0006-deck-builder-qt-adapter-boundary.md)'s statement that a card's section is
  always the caller's explicit choice is superseded for the add action by Decision 2; it still
  holds for `addCard(code, section)`.
- [ADR 0007](0007-deck-legality-policy-module.md)'s "this module does not classify a `Deck`"
  is superseded to this extent: `policy/` now holds the per-card rule, but still never moves a
  card between a `Deck`'s sections.
- [ADR 0010](0010-deck-builder-ruleset-and-legality-ui.md)'s wording of the advisory banner
  ("Would not be accepted at duel entry: <reason>", "Deck is legal for duel entry ...") is
  superseded by Decision 5, added in round 021.

## Status

Accepted. Decision 2's record of the deck builder's differences and Decision 5 were added in
round 021, which corrected round 020's statement that there were two. Implemented in
`policy/include/edopro_next/policy/deck_placement.h`,
`policy/src/deck_placement.cpp`, `policy/src/deck_validation.cpp`,
`ui/src/deckbuilder/deck_controller.{h,cpp}` and `ui/qml/screens/DeckBuilderScreen.qml`;
pinned by `policy/tests/test_deck_placement.cpp` (including the push-cascade transcription
of Decision 2), `ui/tests/test_deckbuilder.cpp` (including every message of Decision 5) and
`ui/tests/test_deckbuilder_screen.cpp`.

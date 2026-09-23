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
Monsters go, but disagree with each other on a few card shapes (§3.3 and §4 there).

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
of "belongs in Extra" in the project - exactly what this round must not do. For every card
shape the cascade and the lambda agree (deck-placement.md §3.3) except two, recorded here as
divergences:

- **A card with the Link bit and neither Monster nor Spell**: upstream's deck builder puts it
  in Extra, `LoadDeck` in Main; this project puts it in Main.
- **A non-Rush Ritual Monster that is also Fusion/Synchro/Xyz/Link**: upstream's right-click
  puts it in Side (both pushes refuse it), `LoadDeck` in Extra; this project puts it in Extra.

Neither shape was found in the card databases of a local Project Ignis install (a read-only
count; not reproducible in CI).

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

## Consequences

- One definition: `policy/deck_placement.h`. `validate_deck()` and the deck builder both
  reach it; `ui/qml/` inspects no card type.
- [ADR 0006](0006-deck-builder-qt-adapter-boundary.md)'s statement that a card's section is
  always the caller's explicit choice is superseded for the add action by Decision 2; it still
  holds for `addCard(code, section)`.
- [ADR 0007](0007-deck-legality-policy-module.md)'s "this module does not classify a `Deck`"
  is superseded to this extent: `policy/` now holds the per-card rule, but still never moves a
  card between a `Deck`'s sections.
- Not addressed: the legality banner calls a misplaced card something that "would not be
  accepted at duel entry", although upstream's server re-splits a deck before validating it
  (deck-placement.md §5.4). The wording belongs to ADR 0010's legality presentation and is
  left for a decision outside this round.

## Status

Accepted. Implemented in `policy/include/edopro_next/policy/deck_placement.h`,
`policy/src/deck_placement.cpp`, `policy/src/deck_validation.cpp`,
`ui/src/deckbuilder/deck_controller.{h,cpp}` and `ui/qml/screens/DeckBuilderScreen.qml`;
pinned by `policy/tests/test_deck_placement.cpp`, `ui/tests/test_deckbuilder.cpp` and
`ui/tests/test_deckbuilder_screen.cpp`.

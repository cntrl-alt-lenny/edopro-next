# 020-extra-deck-classification: put cards in the Main or Extra Deck the way upstream does

Tier: 2
Mode: implementation
Supersedes: none

## Goal

In the new deck builder, a card goes into the Main Deck or the Extra Deck by
the same rule upstream EDOPro applies. The rule has exactly one definition in
this project, it lives outside the UI, and every place it is used says which
upstream behaviour it reproduces.

## Context

Why this round is next: automatic Main/Extra classification is the first item
the M3 roadmap entry lists as still missing from the deck builder
(`docs/ROADMAP.md`, M3, the unchecked "Deck builder UI in QML" item).
[ADR 0004](../../adr/0004-deck-model-ydk-codec.md) (Decision 2,
"Consequence") deliberately left it out of the `.ydk` codec, as a later,
explicit `Deck -> Deck` step with a card database available. Everything it
needs exists now: card data, a deck model, and a deck builder with a tested
Qt adapter.

It is Tier 2 because it reproduces an upstream game rule (which cards belong
in the Extra Deck). This project's real defects have been mismatches with
upstream that compiled and tested clean (`AGENTS.md`, Evidence).

Where upstream decides this. Read these yourself and quote what you read; do
not work from this brief's summary:

- `gframe/deck_manager.cpp`, `DeckManager::LoadDeck`: the `is_extra_deck_card`
  lambda and the loop that uses it. Note:
  - its Ritual handling, which depends on a `RITUAL_LOCATION` argument and
    on `CardDataC::isRush()` (`gframe/data_manager.h`);
  - its token and unknown-card handling;
  - how the loop behaves differently when an explicit extra list is
    supplied.
- `gframe/deck_con.cpp`: how upstream's own deck builder places a card the
  user adds (`push_extra`, `push_main` and `push_side`, and the order in
  which the call sites try them).
- How `RITUAL_LOCATION` reaches these functions, and from where.

What this project already has:

- `policy/src/deck_validation.cpp` mirrors half of that lambda as
  `is_unconditionally_extra_deck_type()` and handles Ritual placement
  separately.
- `policy/include/edopro_next/policy/validation_policy.h` and
  `docs/architecture/deck-legality.md` §7 explain why validation takes a
  resolved boolean for Rituals, not the loader's three-state enum. They also
  say explicitly that `policy/` validates an existing split and does not
  classify one.
- `docs/architecture/deck-model.md` §3 and ADR 0004, Decision 2: why the
  codec follows the file's section markers.
- `docs/architecture/deck-builder-ui.md` §7 (section-edit semantics), §12
  (what remains) and §14 (legality integration), with ADRs 0006 and 0010.
- `docs/architecture/ydk-interoperability.md` and ADR 0008: the existing
  proof that upstream's loader reclassifies a card written under `#main`.

Not worth reading: `client/`, the duel path, `ocgcore/`.

## The problem, in parts

1. **Establish upstream's behaviour exactly**, at each point where it
   decides Main versus Extra:
   - adding a card in its deck builder;
   - loading a deck in each of its load modes;
   - the Ritual cases under each `RITUAL_LOCATION` value, including Rush
     cards;
   - tokens and unknown cards.

   Record it in the relevant architecture document with file-and-line
   citations.
2. **One definition of "belongs in the Extra Deck".** The project must not
   end up with two copies of the rule that could disagree: the new one and
   the one already in `policy/`. You choose where the definition lives
   (`data/`, `policy/`, or a new presentation-independent place) and how the
   Ritual and Rush cases are expressed, and you argue it in an ADR. It needs
   a card database. It carries no Qt type and no UI concept.
3. **The deck builder uses it.** When the user adds a card, it goes where
   upstream's deck builder would put it. What happens to a card the user
   deliberately places in the "wrong" section is your call. So is whether
   opening a `.ydk` reclassifies what the file says. Upstream has different
   behaviours for these, and ADR 0004 chose to follow the file's markers.
   Each such choice is recorded as a decision: an upstream divergence, if it
   is one, is never silent. The UI renders the result and forwards user
   intent; it decides nothing itself.
4. **The records follow.** Bring these up to date:
   - the deck builder's documents (§12 of `deck-builder-ui.md` also still
     lists legality as missing, which stopped being true with round 015);
   - the roadmap M3 item's "still missing" text;
   - the deck builder row in `docs/capabilities.md`.

   Leave the M3 checkbox unchecked unless every part of that item is done.

## Scope and non-goals

In scope:

- `data/` or `policy/` (wherever part 2 lands), with their tests;
- `ui/src/deckbuilder/`, `ui/qml/` and `ui/tests/`;
- `docs/architecture/`, a new ADR (and a superseding note on an older ADR if
  one changes), `docs/ROADMAP.md`, `docs/capabilities.md` and the
  regenerated README block.

Out of scope:

- `gframe/`, `integration/`, `ocgcore/`, `client/`;
- the `.ydk` codec's parsing rules, unless the ADR argues for a change;
- `.github/workflows/` and repository settings;
- framework files;
- the other missing deck-builder parts: artwork, the sigil search grammar,
  structured filters, and keyboard/controller parity.

## Invariants

- The UI implements no game rule (`AGENTS.md`). The classification is
  computed outside `ui/qml/`; `data/` and `policy/` stay free of Qt.
- Upstream source is the arbiter of upstream semantics. Quote what you read,
  at file and line. An unrecorded divergence is a defect, whatever its merit
  (`AGENTS.md`, Evidence).
- `policy::validate_deck()`'s existing behaviour and its tests are
  unchanged, unless the ADR argues for a change and the report lists every
  test whose expectation moved.
- The `.ydk` round-trip contract (`deck-model.md` §6) and the existing
  `data/` tests hold.
- No card database, artwork or CardScripts are committed. Test fixtures are
  synthetic, as the existing ones are.
- No new dependency.

## Acceptance criteria

1. The architecture record states upstream's behaviour for every case in
   part 1, with citations a reader can re-check.
2. The Extra Deck rule has exactly one definition in the project. Every
   use, including `policy/`'s validation, reaches it or is shown to be
   identical to it by a test that would fail if they diverged.
3. Tests cover every classification case in part 1, and each could fail:
   show one failing against a deliberately wrong rule. That includes the
   Link-without-Monster, Ritual/`RITUAL_LOCATION` and Rush cases, tokens, and
   unknown codes.
4. In the deck builder, adding a Fusion, Synchro, Xyz or Link Monster puts
   it in the Extra Deck, and adding an ordinary monster, spell or trap puts
   it in the Main Deck. A `ui/` test drives this through the adapter.
5. The ADR records every decision from parts 2 and 3, and every divergence
   from upstream, with its reason.
6. The records in part 4 are true, and
   `python3 tools/generate_readme_status.py --check` passes.

## Required evidence

1. Seat start:
   - the real output of `python3 tools/fw.py start --role builder --round
     020-extra-deck-classification`;
   - `git submodule update --init` right after it;
   - `git submodule status`;
   - the operating system, compiler and Qt version, each from a command.
2. `AGENTS.md`'s evidence-table cycle for every module you changed:
   - `data/` and/or `policy/`: configure with `-DEDOPRO_NEXT_WERROR=ON`,
     build, `ctest`;
   - `ui/`: `-DEDOPRO_NEXT_UI_TESTS=ON`, build, `ctest`, and the offscreen
     clean-QML-load check.

   Give real output and exit status. This machine's Qt may differ from CI's
   pinned version; say so if it does.
3. The upstream passages you relied on, quoted, with file and line.
4. For acceptance criterion 3: the literal change that makes the rule wrong,
   and the literal failing test output.
5. The Python suite (`python3 -m unittest discover -s tests -v`, with totals
   and each skip named), the three generator `--check` commands and
   `python3 tools/fw.py check`.
6. Every sentence removed from an architecture document, ADR, the roadmap or
   `docs/capabilities.md`, listed with what replaced it.
7. CI check conclusions at your final commit.
8. What you did not run, and what you did not verify. That includes
   behaviour through upstream's own GUI, which this round cannot exercise.

# 023-filter-layout-correction: the search filters fit the minimum window, and the gaps round 022's review found are closed

Tier: 2
Mode: implementation
Supersedes: none. This round corrects round 022 (022-deck-builder-filters)
before it merges. Its brief branch is cut from that round's final commit, so
this round's work lands on top of round 022's and both merge together.

## Goal

At the shell's documented minimum window size, the deck builder with its
search filters is laid out without overlap and its search results stay
usable. The four smaller gaps round 022's Verifier found are closed.

## Context

Round 022 added upstream's search filters to the deck builder. Its search
semantics held up under review and are not reopened here. Its Verifier report
(`docs/rounds/022-deck-builder-filters/verifier.md`) found:

1. **Layout, introduced by round 022.** At 960×600, the minimum
   `docs/architecture/deck-builder-ui.md` §13.1 documents, with the filters
   shown (the default), several controls run past the column divider into
   the deck column: the right-hand filter column (Sub-type, Type/race, DEF,
   Scale), the Clear button, the Limit list and "Link markers…". The search
   results shrink to about one visible row. At 1280×800 nothing overlaps.
   Brain reproduced this with `--capture --start-screen decks` against a
   synthetic database.
2. **Test gap.** The `ui/` test fixture's only Pendulum card has equal left
   and right scales, so wiring the Scale box to `right_scale` passes every
   `ui/` test. Upstream's box reads the left scale only
   (`gframe/deck_con.cpp:1222-1224`).
3. **Record.** ADR 0012, Decision 6, says upstream runs the search "on the
   category and marker OK buttons". Only the marker OK does
   (`deck_con.cpp:346-353` against `:465`). The same sentence also omits
   that Level and Scale re-run the search on every change (`:498-501`), and
   that the name box does once it is longer than two characters (`:494`).
4. **Wiring.** `ui/src/deckbuilder/search_filters.cpp` parses the Level box
   with the ATK field kind and no test notices. The two differ only for
   numbers of 2³¹ or more, but the wiring should name the right field and
   be pinned.
5. **Stale row.** The "Windows / macOS builds" row in
   `docs/capabilities.md` still says `ui/` on macOS has no recorded evidence.
   Rounds 019, 020, 022 and this one's seats have built and tested `ui/` on
   macOS with Qt 6.

Worth reading:

- `deck-builder-ui.md` §10.5-§10.7 (earlier layout fixes at 960×600) and
  §13 (how visual verification is done here);
- ADR 0012;
- `ui/qml/screens/DeckBuilderScreen.qml`;
- round 022's two reports.

## The problem, in parts

1. At 960×600 and at 1280×800, with the filters shown, nothing in the deck
   builder overlaps or runs past its column. The search results show more
   than one row at 960×600. How you achieve that is your call; argue it in
   `deck-builder-ui.md`. Every filter stays reachable and usable, including
   by keyboard.
2. A `ui/` test fails if the Scale box reads the right scale.
3. ADR 0012, Decision 6, states upstream's triggers as the source has them.
4. The Level box is parsed as a Level field, and a test fails if it is not.
5. The capabilities row states the macOS `ui/` evidence that now exists,
   with sources.

## Scope and non-goals

In scope:

- `ui/qml/`, `ui/src/deckbuilder/` and `ui/tests/`;
- ADR 0012;
- `docs/architecture/deck-builder-ui.md`;
- `docs/capabilities.md`.

Out of scope:

- any change to what a search returns;
- `data/` and `policy/` code;
- `gframe/`, `integration/`, `ocgcore/`, `client/`;
- `.github/workflows/`, settings and framework files;
- `docs/state.md`, which Brain updates after the merge.

## Invariants

- Everything in round 022's brief still holds
  (`docs/rounds/022-deck-builder-filters/brief.md`, Invariants).
- The UI implements no game rule and no matching (`AGENTS.md`).
- Existing test expectations do not change silently. List every change with
  its before and after.
- Nothing personal in any tracked document or image.

## Acceptance criteria

1. Captures at 960×600 and at 1280×800, with the filters shown and a
   synthetic database loaded, show no overlap and more than one result row
   at 960×600. You look at them yourself.
2. Deliberately wrong wiring (Scale to the right scale; Level parsed as ATK)
   makes a `ui/` test fail. Show both.
3. ADR 0012 and `docs/capabilities.md` state only what the source and the
   evidence support.
4. The `ui/` cycle, the Python suite, the generator checks and
   `fw.py check` are green, and CI is green.

## Required evidence

1. Seat start:
   - the real output of `python3 tools/fw.py start --role builder --round
     023-filter-layout-correction`;
   - `git submodule update --init` right after it;
   - `git submodule status`;
   - the operating system, compiler and Qt version, each from a command.
2. The `ui/` cycle from `AGENTS.md`'s evidence table (configure with
   `-DEDOPRO_NEXT_UI_TESTS=ON`, build, `ctest`, offscreen clean-QML-load),
   with real output and exit status.
3. The capture command lines, the pixel size of each capture from a command,
   and what you checked in each and what you did not. Do not commit the
   synthetic database.
4. For acceptance criterion 2: each literal change and its literal failing
   output.
5. The Python suite (totals, each skip named), the generator `--check`
   commands and `python3 tools/fw.py check`.
6. Every sentence removed from ADR 0012, `deck-builder-ui.md` or
   `docs/capabilities.md`, with what replaced it.
7. CI conclusions at your final commit, and what you did not run or verify.

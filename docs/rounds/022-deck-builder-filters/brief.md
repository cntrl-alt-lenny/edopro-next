# 022-deck-builder-filters: search the card pool with upstream's filters

Tier: 2
Mode: implementation
Supersedes: none

## Goal

The deck builder's search offers the filters upstream EDOPro's deck builder
offers, and each one finds the same cards upstream would. Where it
deliberately does not, that is a recorded decision. The UI only collects the
user's choices and shows results: matching happens in `data/`, and anything
that depends on a banlist happens in `policy/`.

## Context

Why this round is next: "structured filters beyond plain text" is one of the
open parts of M3's deck-builder item (`docs/ROADMAP.md`). The data layer
already has most of what is needed: `data::SearchQuery`
(`data/include/edopro_next/data/search_query.h`) filters by card type,
attribute, race, ATK, DEF, level, Pendulum scales, link markers, effect
category, raw scope bits and setcodes. The deck builder uses only its text
field (`ui/src/deckbuilder/search_results_model.cpp`).

The legacy sigil grammar (`@`, `$`, `$$`, `!!`, `&&`, `||`), and
archetype-name search, are **not** part of this round. They are a separate
later round: archetype names come from a string resource this repository
does not contain, and ADR 0005 (Decision 2) set conditions for building a
grammar parser at all.

It is Tier 2 because it reproduces upstream's search semantics, which this
project has got subtly wrong before (the `race` and `category` operator
mismatches recorded in `docs/architecture/card-search.md` §2.1).

Where upstream decides this. Read it yourself and quote what you rely on:

- `gframe/game.cpp` from about line 660: the filter window's controls and
  what each offers.
- `gframe/deck_con.cpp`:
  - `parse_filter` (`:21-51`), which turns the ATK, DEF, level and scale
    boxes' text into a comparison;
  - the event handling that reads the controls into `filter_*` values;
  - `CheckCardProperties` (`:1191` on), including the cards it hides before
    any filter applies: tokens, hidden cards, and non-official cards unless
    the anime box is ticked;
  - the `LIMITATION_FILTER_*` cases, which depend on the selected banlist.
- `gframe/deck_con.h`: `limitation_search_filters`.

What this project already has:

- `docs/architecture/card-search.md`: §1.2 on how upstream mixes search and
  legality filters; §2.1's operator audit, which calls the three-comparison
  `NumericFilter` a "deliberate simplification" of upstream's six-way
  scheme; §9 on sentinels.
- ADR 0005.
- `docs/architecture/deck-builder-ui.md` and ADRs 0006 and 0010, for the
  adapter boundary and how the selected banlist and ruleset reach the deck
  builder.

## The problem, in parts

1. **Establish upstream's filter behaviour exactly**:
   - every control;
   - every input form `parse_filter` accepts, including `?`, strict `>`
     and `<`, and text it does not recognise;
   - what `CheckCardProperties` does with each;
   - the cards it hides by default.

   Record it with file-and-line citations.
2. **Test §2.1's "deliberate simplification" claim against part 1.**
   Establish whether every input upstream accepts maps onto
   `NumericFilter`'s three comparisons with exactly the same result,
   including around the `?` sentinel and Link and Pendulum cards. Where it
   does not, either extend `data/` or record the divergence, and correct
   §2.1 to match what you found.
3. **Filters in the deck builder.** The user can filter by each upstream
   control that has a `SearchQuery` equivalent, typing numeric filters the
   way upstream accepts them. The QML collects choices and renders results.
   Turning text into a comparison, and all matching, happens outside
   `ui/qml/`.
4. **Banlist-status filters** (banned, limited, semi-limited, unlimited, and
   the related `LIMITATION_FILTER_*` cases). These depend on the banlist the
   deck builder has selected. If you build them, the answer comes from
   `policy/` and the UI decides nothing. If you leave any out, record why.
5. **Default visibility.** Decide, and record, whether search hides what
   upstream hides by default: tokens, hidden cards and non-official cards,
   and upstream's anime switch. Round 020 left tokens visible, but they
   cannot be added.
6. **The records follow:**
   - `card-search.md`;
   - `deck-builder-ui.md` (including §12's remaining list);
   - a new ADR for the decisions in parts 2 to 5;
   - the roadmap M3 item's "still missing" text;
   - `docs/capabilities.md`.

   Leave the M3 checkbox unchecked while other parts of that item remain.

## Scope and non-goals

In scope:

- `data/` (search) and its tests;
- `policy/`, only for part 4, with its tests;
- `ui/src/deckbuilder/`, `ui/qml/` and `ui/tests/`;
- `docs/architecture/`, a new ADR (and supersession notes on older ones),
  `docs/ROADMAP.md`, `docs/capabilities.md` and the regenerated README
  block.

Out of scope:

- the sigil grammar and archetype-name search (a later round);
- search ranking;
- card artwork;
- keyboard and controller parity beyond making the new controls reachable
  and usable by keyboard;
- `gframe/`, `integration/`, `ocgcore/`, `client/`;
- `.github/workflows/`, settings and framework files.

## Invariants

- The UI implements no game rule, and search does not become legality
  (`AGENTS.md`; `card-search.md` §0). `data/` stays free of legality, and
  `data/` and `policy/` stay free of Qt.
- Upstream source is the arbiter: quote it, at file and line. An unrecorded
  divergence is a defect (`AGENTS.md`, Evidence).
- Existing `data/`, `policy/` and `ui/` test expectations do not change
  silently. List every changed expectation with its before and after, and
  the reason.
- Search stays fast. If you change the search path, show the existing
  benchmark (`card-search.md` §10) before and after, on the same machine.
- No card database, artwork or string resource is committed. Test fixtures
  are synthetic.
- No new dependency.

## Acceptance criteria

1. The record states upstream's behaviour for every control, input form and
   default exclusion in part 1, with citations a reader can re-check.
2. Every upstream numeric input form has a test showing this project gives
   the same result as upstream, or is a recorded divergence. §2.1 no longer
   says anything part 2 found untrue.
3. The deck builder filters by every upstream control with a `SearchQuery`
   equivalent. A `ui/` test drives each one through the adapter and checks
   the results.
4. Part 4 and part 5 are each implemented as upstream does, or recorded as
   decisions with reasons, in the new ADR.
5. Tests could fail: show at least one filter test failing against a
   deliberately wrong comparison, and one against a deliberately wrong
   input parser.
6. The records in part 6 are true, and
   `python3 tools/generate_readme_status.py --check` passes.

## Required evidence

1. Seat start:
   - the real output of `python3 tools/fw.py start --role builder --round
     022-deck-builder-filters`;
   - `git submodule update --init` right after it;
   - `git submodule status`;
   - the operating system, compiler and Qt version, each from a command.
2. `AGENTS.md`'s evidence-table cycle for every module you changed (`data/`,
   `policy/` if touched, and `ui/`, with its offscreen clean-QML-load
   check), with real output and exit status. Say whether this machine's Qt
   differs from CI's pinned version.
3. The upstream passages you relied on, quoted, with file and line.
4. For acceptance criterion 5: each literal change, and its literal failing
   output.
5. The search benchmark before and after, if the search path changed.
6. The Python suite (with totals and each skip named), the three generator
   `--check` commands and `python3 tools/fw.py check`.
7. Every sentence removed from an architecture document, ADR, the roadmap or
   `docs/capabilities.md`, listed with what replaced it.
8. A capture of the deck builder with filters visible (`--capture`, with a
   synthetic database), looked at by you. Say what you checked visually and
   what you did not.
9. CI conclusions at your final commit, and what you did not run or verify.

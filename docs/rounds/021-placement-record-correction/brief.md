# 021-placement-record-correction: make round 020's upstream record exhaustive and pinned

Tier: 2
Mode: implementation
Supersedes: none. This round corrects round 020 (020-extra-deck-classification)
before it merges. Its brief branch is cut from that round's final commit, so
this round's work lands on top of round 020's, and both merge together.

## Goal

Everything round 020 says about how upstream places a card is exhaustive and
true, and a test that can fail keeps it true. Every message the deck builder
shows about duel entry is true as well.

## Context

Round 020 made the Main/Extra Deck rule one definition in `policy/`
(`policy/src/deck_placement.cpp`), used by `validate_deck()` and the deck
builder. Its code and tests hold up under review, and it is not reopened here.
Its Verifier report (`docs/rounds/020-extra-deck-classification/verifier.md`)
found three things wrong with the record around it, and Brain reproduced the
first independently:

1. **The comparison with upstream's deck builder is not exhaustive, though it
   says it is.** `docs/architecture/deck-placement.md` §3.3 says upstream's
   push functions (`gframe/deck_con.cpp:1577-1636`) and the `LoadDeck` lambda
   (`gframe/deck_manager.cpp:335-348`, under `RITUAL_LOCATION::DEFAULT`)
   differ "in only two shapes". ADR 0011, Decision 2, says "except two". A
   transcription of both over every combination of the eight type bits
   either rule reads, in both Rush scopes, finds more differing families.
   Every card with both the Link and the Spell bit that also has the Monster
   bit or a Fusion/Synchro/Xyz bit differs, as do further Ritual hybrids. The
   result is the same whichever order a call site tries the two push
   functions in. No test transcribes the push functions, so nothing pinned
   the §3.3 table.
2. **The "no such card in real databases" statement covers only the two
   shapes it knew about**, and it cannot be reproduced from the repository.
3. **The deck builder's legality messages say "Would not be accepted at duel
   entry"** for a card in the wrong section, and for some section-size
   errors. §5.4 of the same document shows that upstream's server re-splits a
   deck by card type before validating it, so for most misplaced cards that
   statement is false. Round 020 recorded this as an open question and left
   the text alone.

Worth reading: `deck-placement.md` §3, §5.4 and §6; ADR 0011; round 020's
two reports; `policy/tests/test_deck_placement.cpp`; and
`ui/src/deckbuilder/deck_controller.cpp`'s `formatLegalityError`. Re-read the
upstream functions yourself and quote what you rely on.

## The problem, in parts

1. **Exhaustive comparison.** Record every family of card types for which
   upstream's deck-builder placement differs from the rule this project
   uses, for every call-site order upstream uses. Correct §3.3 and ADR 0011's
   wording to match. Each difference is a recorded divergence with its
   reason, or the project's behaviour changes to follow upstream; argue
   whichever you choose in the ADR.
2. **Pinned by a test.** A committed test transcribes upstream's push
   functions (unforced, not side-decking, section not full) and checks the
   recorded difference set exactly, over every combination. The test fails
   if the record, the transcription or this project's rule changes without
   the others.
3. **Real-database statement.** Either remove it, or restate it precisely:
   which families were checked, against what (described, with no personal
   path), and that it cannot be reproduced in CI.
4. **True messages.** Every legality message the deck builder shows states
   only what is true of upstream, for the error it reports. You choose the
   wording and argue it against §5.4; the legality computation itself does
   not change.
5. **Small corrections the Verifier noted:**
   - the section citation in `policy/tests/test_deck_placement.cpp`'s
     `linkWithoutMonsterBelongsInMain` comment;
   - the `classify_card` header comment's claim that it reproduces the load
     loop's unknown-code handling. Make it precise.

## Scope and non-goals

In scope:

- `docs/architecture/deck-placement.md`, ADR 0011 and any other document
  that repeats the corrected statements;
- `policy/tests/` and comments in `policy/`;
- `ui/src/deckbuilder/deck_controller.cpp`'s message text, and `ui/tests/`
  for it.

Out of scope:

- any change to `policy/`'s rule or to `validate_deck()`'s results, unless
  part 1's argument concludes that behaviour must follow upstream. In that
  case stop and report before changing it: that is a new decision, not a
  correction;
- `gframe/`, `integration/`, `ocgcore/`, `client/`, `data/`,
  `.github/workflows/`, settings and framework files;
- `docs/state.md`, which Brain updates after the merge.

## Invariants

- Everything in round 020's brief still holds (`docs/rounds/020-extra-deck-classification/brief.md`,
  Invariants).
- Upstream source is the arbiter: quote it, at file and line.
- An unrecorded divergence is a defect (`AGENTS.md`, Evidence).
- Nothing personal in any tracked document.

## Acceptance criteria

1. The record lists every differing family, and the test proves the list is
   exactly right: no missing family and no extra one.
2. The test fails when any one of these is changed deliberately:
   - the transcription;
   - the recorded set;
   - this project's rule.

   Show each failing.
3. The real-database statement is removed, or it is precise and honest about
   what was checked.
4. No legality message states something about upstream duel entry that
   §5.4 contradicts, and a `ui/` test pins each changed message.
5. `policy/` and `ui/` builds and tests pass. The Python suite, the
   generator checks and `fw.py check` are green. CI is green.

## Required evidence

1. Seat start: the real output of `python3 tools/fw.py start --role builder
   --round 021-placement-record-correction`, of `git submodule update --init`
   right after it, and of `git submodule status`. Also the operating system,
   compiler and Qt version, each from a command.
2. The upstream passages relied on, quoted, with file and line.
3. The `policy/` and `ui/` cycles from `AGENTS.md`'s evidence table, with
   real output and exit status.
4. For acceptance criterion 2: each literal change, and its literal failing
   output.
5. The Python suite (with totals and each skip named), the generator
   `--check` commands and `python3 tools/fw.py check`.
6. Every sentence removed from a document, listed with what replaced it, and
   every changed message's old and new text.
7. CI conclusions at your final commit, and what you did not run or verify.

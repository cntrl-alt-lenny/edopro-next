<!-- fw-report
round: 020-extra-deck-classification
role: verifier
branch: verifier/020-extra-deck-classification
head: 3e4c678c8ec476037b73977e788b11e77a470a52
os: macOS 27.0
python: 3.9.6
written: 2026-09-24T06:58:57Z
-->
Reviewed commit: 3e4c678c8ec476037b73977e788b11e77a470a52 (origin/builder/020-extra-deck-classification; code/doc content is 06cca5aa, the report commit is on top). Commands I ran myself are listed under "Verified by me" below.

## Seat start

`python3 tools/fw.py start --role verifier --round 020-extra-deck-classification` -> exit 0:

```
seat ok: verifier, round 020-extra-deck-classification, branch verifier/020-extra-deck-classification at 3e4c678c8ec4
  brief: docs/rounds/020-extra-deck-classification/brief.md
  reviewing exactly 3e4c678c8ec476037b73977e788b11e77a470a52 from origin/builder/020-extra-deck-classification
  do not open docs/rounds/020-extra-deck-classification/builder.md until your first pass is finished
```

`git submodule update --init` -> exit 0 (first attempt, no retry needed; the clone's absolute path is replaced by `<clone>`):

```
Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
Cloning into '<clone>/ocgcore'...
Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
```

`git submodule status` -> exit 0:

```
 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
```

## Verified by me (macOS 27, arm64, Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2, Qt 6.11.1 via Homebrew; CI pins Qt 6.8.3 on Linux)

- `cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON`, build, `ctest --test-dir policy/build --output-on-failure` -> exit 0, 3/3 pass (lf_list, deck_validation, deck_placement).
- Same cycle for `data/` (unchanged by the round) -> exit 0, 3/3 pass.
- `ui/`, Release + `-DEDOPRO_NEXT_UI_TESTS=ON` (as CI does) -> configure/build/ctest exit 0, 2/2 pass; then Debug + `-DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON` in a build directory outside the clone -> exit 0, 2/2 pass (the only warning is the pre-existing `ld: duplicate libraries`).
- Offscreen clean-QML-load: macOS has no `timeout(1)`, so a Python wrapper reproduced CI's first assertion: the shell was still running at 20 s. stderr was not empty: one Qt platform notice (`qt.qpa.fonts ... missing font family "Sans Serif"`), not a QML diagnostic; the round's diff touches no font. Same notice round 019 recorded. CI's Linux job (which runs the check verbatim) succeeded at 3e4c678c (`gh run list`).
- Python: system 3.9.6 not used; Homebrew `python3.13 -m unittest discover -s tests -v` -> exit 0, 133 tests, OK (skipped=11: one Windows-ACL test, ten semantic-trace tests because no fresh `client/` binary; `client/` is untouched). `generate_readme_status.py --check`, `generate_messages.py --check`, `generate_protocol_constants.py --check`, `check_home_status.py` -> all exit 0. The round changes nothing under `tools/`, `tests/` or protocol tables, so golden reproduction was not run.

## Upstream cases re-read (gframe/, base recorded in docs/UPSTREAM.md)

I read every cited line myself. All of the following match the record `docs/architecture/deck-placement.md`:

- deck_manager.cpp:335-348 (lambda; order F/S/X, then Link, then Ritual by RITUAL_LOCATION); :338 reduces to "Link and Monster" (`(cd->type & TYPE_LINK && cd->type & TYPE_MONSTER)` is a bool, TYPE_MONSTER is 0x1, ocgapi_constants.h:33).
- :349-364 main loop (tokens dropped at :357 in every mode; `(!extralist || cd->code != 0) && is_extra_deck_card(cd)` at :359; unknown code -> error code without extra list, `loadalways` keeps a `code==0` placeholder in its own list with one; placeholders stop via `StopDummyLoading`, deck_manager.h:62-64, game.cpp:2682, data_handler.cpp:159-160). :365-378 extra list unclassified, tokens skipped. :379-390 side list.
- data_manager.h:92-98 (isRitualMonster, isRush), :29 SCOPE_RUSH 0x200; deck.h:19-23 RITUAL_LOCATION.
- RITUAL_LOCATION sources: generic_duel.cpp:421-423, deck_manager.cpp:407, menu_handler.cpp:314-315 and :810-812; all other loads take DEFAULT (deck_manager.h:77-80; menu_handler.cpp:1114; deck_manager.cpp:595, :552; deck_con.cpp:409/422/521/889; menu_handler.cpp:537/543; windbot.cpp:69).
- deck_con.cpp:1577-1636 push_main/push_extra gates; call sites :655-679, :700-702, :717-727, :756-769, :875-878; :1192 token filter.
- CheckDeckContent deck_manager.cpp:219-234 (Extra callback asks Ritual first; the Ritual+FSX/Link hybrid is an upstream self-disagreement), and section 5.4's claim that the server rebuilds the split by type before validating (generic_duel.cpp:373-380, :423; menu_handler.cpp:43-48) is correct.

One mismatch: see SHOULD FIX 1.

## Single definition and mutation testing

Every place classifying Extra Deck / extra-type cards in data/, policy/, ui/, client/, tools/, tests/ was grepped. The only definition is `policy/src/deck_placement.cpp` (`is_extra_deck_type`, `is_ritual_monster`, `is_rush`, `is_token`, `belongs_in_extra_deck`, `classify_card`). `policy/src/deck_validation.cpp` no longer has its own type bits; its Main check calls `belongs_in_extra_deck`, its Extra check calls the shared primitives. `ui/` reaches it only through `DeckController::placementFor`/`addCardToDeck`; QML inspects no type. `ui/src/deckbuilder/card_entry.cpp` and `CardPreview.qml` use type bits for presentation only (pre-existing, documented). Other type-bit uses in data/ (search filters, Link marker) are not Extra Deck classification.

Mutations, each reverted with `git checkout` (tree clean afterwards, tests green again):

| Mutation | Result |
|---|---|
| `kTypeSynchro` 0x2000 -> 0x1000 (rule) | deck_placement FAIL and deck_validation FAIL (consumer sees it) |
| Link no longer requires Monster | deck_placement FAIL |
| `RushInExtra` returns true for every Ritual | deck_placement FAIL |
| `kScopeRush` 0x200 -> 0x100 | deck_placement FAIL |
| `classify_card` token check disabled | deck_placement FAIL |
| validation Main check uses `RitualPlacement::Main` instead of the flag | deck_validation FAIL and deck_placement FAIL |
| validation Extra check drops the Ritual-first branch | deck_validation FAIL and deck_placement FAIL |
| UI `kAddRitualPlacement` -> `Main` | ui `deckbuilder` FAIL |
| UI `placementFor` maps Extra to Main | ui `deckbuilder` and `deckbuilder_screen` FAIL |

(Two of my first attempts did not compile under -Werror because a constant became unused; I discarded those runs and redid them by editing the constant's value.)

## Findings

- [SHOULD FIX] docs/architecture/deck-placement.md section 3.3 (and docs/adr/0011-extra-deck-classification.md Decision 2, "except two") -- the record says upstream's push cascade and the lambda differ "in only two shapes". That is false. I transcribed `push_main`/`push_extra` (unforced, not side-decking, section not full) and ran all 256 type-bit combinations of Monster/Spell/Trap/Ritual/Fusion/Synchro/Xyz/Link, in both scopes, against the lambda under DEFAULT. Beyond the two recorded shapes there are 88 more (card, scope) combinations, all with both the Link bit and the Spell bit set: (a) Link+Spell+Monster (with or without Trap) -> deck builder Main (push_main lets it through at :1587, push_extra refuses at :1618), lambda Extra; (b) Link+Spell plus any of Fusion/Synchro/Xyz -> both pushes refuse (push_main :1585, push_extra takes the Link branch at :1617-1618 and never reaches the FSX test), so right-click falls to Side, lambda Extra. The implementation follows the lambda, matching Decision 2's principle, so behaviour is as documented in spirit, but AGENTS.md calls an unrecorded divergence a defect and the record is presented as exhaustive. Failure path: a reader relying on "only two shapes" is misled. Root cause: no test transcribes the push functions (only `CheckDeckContent`'s callbacks, test_deck_placement.cpp), so the section 3.3 table is pinned by nothing. Fix the class: add the shapes, and add a push-cascade transcription test over the same 256 combinations so the record's table is checked, not asserted. Likely absent from real databases, but the round did not verify that (see UNPROVEN 1).
- [SHOULD FIX] docs/state.md lines 22 and 89 -- still say "automatic Main/Extra classification" is open in M3 and recommend it as the next slice; the round makes both stale (docs/ROADMAP.md and docs/capabilities.md were updated correctly). Brain-owned file; not in the brief's part 4 list, but the same class of drift last round's verifier found.
- [NOTE] policy/tests/test_deck_placement.cpp (`linkWithoutMonsterBelongsInMain` comment) cites "deck-placement.md section 6" for "A Link Spell - the one real shape"; section 6 says nothing of the kind (section 2.1 is the relevant one).
- [NOTE] `classify_card` returns `Unknown` for an unresolved code and does not reproduce upstream's mapped-code resolution (deck_manager.cpp:17-28, data_manager.cpp:349-354), which section 5.1 describes; the header comment says it "adds the load loop's token and unknown-code handling", which is only partly so (no placeholder-keeping). No behavioural consequence for add (search results come from the loaded database).
- [NOTE] The legality banner still says a misplaced card "would not be accepted at duel entry" although section 5.4 shows upstream re-splits by type first (except the Ritual hybrid). The round discloses this in the ADR and leaves it open; I agree it is out of scope, and it is now a documented, known inaccuracy in shipped UI text.
- [NOTE] Docs are otherwise true against evidence: ROADMAP M3 stays unchecked and lists artwork, sigil grammar/structured filters, keyboard/controller parity as missing; capabilities.md row matches; README block check passes; ADR 0011 records all four decisions and supersession notes are added to ADRs 0004/0006/0007; `validate_deck()` tests were not edited (the only removed test lines are two lines of a synthetic .cdb writer in `ui/tests/test_deckbuilder.cpp`, where the `ot` column now takes a scope field defaulting to 0).
- [UNPROVEN CLAIM] deck-placement.md section 3.3 and the builder report: "Neither shape occurs in the card databases of a local Project Ignis install ... 0 ... across 21 .cdb files". A read-only SQL count over data not in the repository; I cannot reproduce it, and the record itself says it is not reproducible in CI. It is also now known to cover only two of the divergent shape families.
- [UNPROVEN CLAIM] Builder's second-interpreter/CI statements are consistent with mine (133 tests OK under 3.13; CI success at 06cca5aa and 3e4c678c); the builder's specific mutation output text (e.g. "202 assertions failed") I did not reproduce byte for byte, only that each mutation fails.

## Pass two (builder report)

Agrees with my results for the module cycles, Python suite, generators, offscreen result (same font notice), and CI. The builder's mutation 4 (drop Fusion) observed through `ui/` corresponds to my rule mutation, which failed in policy and validation. It does not mention the third and fourth push-cascade shape families (SHOULD FIX 1).

## Not verified

- Behaviour through upstream's own GUI; upstream was read, not run. Nothing here ran `ocgcore` or `LoadDeck`.
- The "no such card in real databases" statement (no Project Ignis data used).
- Qt 6.8.3 on this machine (CI covers it) and Windows.
- Any visual rendering of the changed add buttons; only property-level tests were run.
- `gframe/` baseline and observer-fixture equivalence (untouched by the round, so not run); `client/` cycle (untouched); golden reproduction (`tools/`, `tests/` untouched); PR-body evidence check (`check_pr_evidence.py` found no PR body on stdin).

## Verdict

The Extra Deck rule has one definition in `policy/`, matches upstream's `is_extra_deck_card` lambda line for line, and both real consumers (`validate_deck()` and the deck builder) demonstrably depend on it: nine mutations of the rule or the consumers all made tests fail. All builds and tests I could run pass on macOS with Qt 6.11.1. The upstream record is accurate on every case I re-read, with one exception: its claim that the push cascade and the lambda differ in only two card shapes is incomplete (Link+Spell hybrids), and nothing tests that table. I found no BLOCKER; the two SHOULD FIX items (record enumeration plus a transcription test, and stale docs/state.md) are documentation and test-coverage repairs, not behaviour changes. Confidence: high on the code and tests, moderate on the record's completeness beyond what I brute-forced.

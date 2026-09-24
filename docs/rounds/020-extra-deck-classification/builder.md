<!-- fw-report
round: 020-extra-deck-classification
role: builder
branch: builder/020-extra-deck-classification
head: 06cca5aa6290d56033b3036993ba4385056e9bf9
os: macOS 27.0
python: 3.9.6
written: 2026-09-23T19:25:14Z
-->
# Builder report: 020-extra-deck-classification

All build and test evidence below is at commit `06cca5aa` (the last code/docs commit; this
report is committed on top of it by `fw.py report`), on macOS, from a fresh clone.

## Verified

### Seat start (required evidence 1)

- `python3 tools/fw.py start --role builder --round 020-extra-deck-classification` → exit 0
  ```
  seat ok: builder, round 020-extra-deck-classification, branch builder/020-extra-deck-classification at 1aaff4567bad
    brief: docs/rounds/020-extra-deck-classification/brief.md
    finish with: write docs/rounds/020-extra-deck-classification/builder.md, then python3 tools/fw.py report --role builder --round 020-extra-deck-classification --push
  ```
- `git submodule update --init` → exit 0
  ```
  Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
  Cloning into '<clone>/ocgcore'...
  Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
  ```
  (the clone's absolute path is replaced by `<clone>`.)
- `git submodule status` → exit 0
  ```
   46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
  ```
- Toolchain, each from a command: `sw_vers` → macOS 27.0 (build 26A428), `uname -m` arm64;
  `c++ --version` → Apple clang version 21.0.0 (clang-2100.3.34.2); `cmake --version` →
  4.4.3; `ninja --version` → 1.13.2; `sqlite3 --version` → 3.54.0; `qmake6 -query
  QT_VERSION` → 6.11.1. **This Qt differs from CI's pinned 6.8.3**
  (`.github/workflows/edopro-next.yml`, `version: "6.8.3"`); CI covers 6.8.3 on Linux (below).
  Python: `python3 --version` → 3.9.6; a second interpreter, Homebrew Python 3.13, was also
  used for the Python suite (below).

### Module cycles (required evidence 2)

Fresh `rm -rf {data,policy,ui}/build` first. Logs kept outside the repository.

- `data/` (unchanged by this round; run because the brief's invariant says its tests hold):
  `cmake -S data -B data/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON` →
  exit 0; `cmake --build data/build` → exit 0 (`[21/21] Linking CXX executable
  tests/bench/bench_card_search`); `ctest --test-dir data/build --output-on-failure` → exit 0
  ```
  1/3 Test #1: card_database ....................   Passed    0.35 sec
  2/3 Test #2: deck_ydk .........................   Passed    0.17 sec
  3/3 Test #3: card_search ......................   Passed    0.25 sec
  100% tests passed out of 3
  ```
- `policy/`: `cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DEDOPRO_NEXT_WERROR=ON` → exit 0; `cmake --build policy/build` → exit 0 (no compiler
  warnings in the log); `ctest --test-dir policy/build --output-on-failure` → exit 0
  ```
  1/3 Test #1: lf_list ..........................   Passed    0.17 sec
  2/3 Test #2: deck_validation ..................   Passed    0.26 sec
  3/3 Test #3: deck_placement ...................   Passed    0.20 sec
  100% tests passed out of 3
  ```
  `./policy/build/test_deck_placement` → exit 0, `13 tests, 0 failed, 0 assertions failed`
  (every test name printed `ok`).
- `ui/`: `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON
  -DEDOPRO_NEXT_UI_TESTS=ON` → exit 0; `cmake --build ui/build` → exit 0 (the only warning is
  the pre-existing `ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a',
  'data/libedopro_next_deck.a'`); `ctest --test-dir ui/build --output-on-failure` → exit 0
  ```
  1/2 Test #1: deckbuilder ......................   Passed    0.41 sec
  2/2 Test #2: deckbuilder_screen ...............   Passed    1.02 sec
  100% tests passed out of 2
  ```
  Running the binaries directly: `test_deckbuilder` → `Totals: 34 passed, 0 failed, 0
  skipped`, including `PASS : addToDeckPlacesExtraDeckMonstersInExtraAndEverythingElseInMain`,
  `addToDeckPlacesRitualMonstersAsUpstreamsDeckBuilderDoes`,
  `addToDeckRefusesTokensUnknownCodesAndMissingCatalog`,
  `openingAYdkKeepsTheFilesSectionsUnclassified`; `test_deckbuilder_screen` → `Totals: 22
  passed, 0 failed, 0 skipped`, including `PASS : addButtonsFollowTheControllersPlacement`.
- Offscreen clean-QML-load check. macOS has no `timeout(1)`, so CI's two assertions were
  reproduced with a script: start `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`,
  sleep 20 s, require the process still alive (`kill -0`), kill it, require empty stderr.
  Result: **still running after 20 s** (CI's first assertion holds); **stderr not empty**, one
  line, a Qt platform font notice, not a QML diagnostic:
  `qt.qpa.fonts: Populating font family aliases took 53 ms. Replace uses of missing font
  family "Sans Serif" with one that exists to avoid this cost.` `grep -ciE
  "qml|TypeError|ReferenceError"` on that stderr → `0`. Round 019's builder and verifier
  recorded the same macOS-only notice (`docs/rounds/019-presentation-tidy/verifier.md`, NOTE
  3). CI's Linux job, which runs the check verbatim, passed at `06cca5aa` (below).

### Acceptance criterion 3: tests fail against a wrong rule (required evidence 4)

Each mutation was applied to `policy/src/deck_placement.cpp`, the test run, and the file
restored from a copy; `git status` was clean afterwards and the tests passed again.

1. Link without Monster. Literal change:
   ```
   -	if((record.type & kTypeLink) && (record.type & kTypeMonster))
   +	if(record.type & kTypeLink)
   ```
   `./policy/build/test_deck_placement` → exit 1:
   ```
     ok   linkMonsterBelongsInExtra
     FAIL linkWithoutMonsterBelongsInMain
       .../policy/tests/test_deck_placement.cpp:162
       expected: !belongs_in_extra_deck(card(kTypeSpell | kTypeLink), rituals)
     FAIL linkWithoutMonsterBelongsInMain
       .../policy/tests/test_deck_placement.cpp:164
       expected: !belongs_in_extra_deck(card(kTypeLink), rituals)
     ...
     bad  linkWithoutMonsterBelongsInMain
     bad  validationZoneChecksAgreeWithTheRuleForEveryTypeCombination
     bad  extraDeckTypeIsTheUnconditionalHalfOnly
   13 tests, 3 failed, 202 assertions failed
   ```
2. Rush ignored under `DEFAULT`. Literal change:
   ```
   -			return is_rush(record);
   +			return false;
   ```
   → exit 1:
   ```
     FAIL ritualMonsterUnderRushInExtraFollowsRushScope
       expected: belongs_in_extra_deck(card(ritual, kScopeRush), RitualPlacement::RushInExtra)
     FAIL ritualMonsterUnderRushInExtraFollowsRushScope
       expected: belongs_in_extra_deck(card(ritual | kTypeEffect, kScopeOcgTcg | kScopeRush), RitualPlacement::RushInExtra)
     FAIL tokensAndUnknownCodesAreNeverPlaced
       expected: classify_card(database, CardCode{40}, RitualPlacement::RushInExtra) == CardPlacement::Extra
   13 tests, 2 failed, 3 assertions failed
   ```
3. Tokens placed. Literal change (two lines deleted from `classify_card`):
   ```
   -	if(is_token(*record))
   -		return CardPlacement::Token;
   ```
   → exit 1:
   ```
     FAIL tokensAndUnknownCodesAreNeverPlaced
       expected: classify_card(database, CardCode{30}, rituals) == CardPlacement::Token
     FAIL tokensAndUnknownCodesAreNeverPlaced
       expected: classify_card(database, CardCode{31}, rituals) == CardPlacement::Token
   13 tests, 1 failed, 6 assertions failed
   ```
4. Fusion dropped from the rule, observed through `ui/` (shows the deck builder and
   validation both reach the one rule). Literal change:
   ```
   -	if(record.type & (kTypeFusion | kTypeSynchro | kTypeXyz))
   +	if(record.type & ((kTypeFusion & 0u) | kTypeSynchro | kTypeXyz))
   ```
   `./ui/build/tests/test_deckbuilder` → exit 2:
   ```
   FAIL!  : TestDeckBuilder::addToDeckPlacesExtraDeckMonstersInExtraAndEverythingElseInMain() Compared values are not the same
      Actual   (controller.placementFor(code)): 0
      Expected (extra)                        : 1
   FAIL!  : TestDeckBuilder::noBanlistSelectionDisclosesSkippedChecksForExtraDeckMonsterInMain() Compared values are not the same
      Actual   (controller.isLegal()): 1
      Expected (false)               : 0
   Totals: 32 passed, 2 failed, 0 skipped, 0 blacklisted, 163ms
   ```
   `./ui/build/tests/test_deckbuilder_screen` → exit 1:
   ```
   FAIL!  : TestDeckBuilderScreen::addButtonsFollowTheControllersPlacement() Compared values are not the same
      Actual   (h.prop("selectedResultPlacement").toInt())       : 0
      Expected (static_cast<int>(DeckController::Section::Extra)): 1
   Totals: 21 passed, 1 failed, 0 skipped, 0 blacklisted, 554ms
   ```
   (A first attempt at this mutation, deleting `kTypeFusion` outright, did not compile under
   `-Werror` because the constant became unused; its build failure is not evidence and is
   mentioned only so the attempt is not hidden.)

### Validation unchanged (brief invariant)

- `git diff --stat 1aaff456 -- policy/tests/test_deck_validation.cpp data/` → empty: no
  `validate_deck()` test and nothing in `data/` changed; `deck_validation` passes (above).
- `git diff 1aaff456 -- ui/tests | grep '^-' | grep -v '^---'` → the only removed lines are
  two lines of the synthetic-`.cdb` writer in `test_deckbuilder.cpp` (the `ot` column now takes
  a new `scope` field defaulting to `0`, the value it hard-coded before). No existing
  assertion changed.
- `test_deck_placement`'s `validationZoneChecksAgreeWithTheRuleForEveryTypeCombination` runs
  `validate_deck()` on 1,024 one-card decks in each of Main and Extra (256 type-bit
  combinations × Rush/non-Rush × both values of `rituals_belong_in_extra`) against expectations
  transcribed independently from `gframe/deck_manager.cpp:220-223` and `:228-232`, and counts
  exactly 120 Ritual-hybrid disagreements between upstream's lambda and its Extra callback.

### Python and generators (required evidence 5)

- `python3 -m unittest discover -s tests -v` (Python 3.9.6) → exit 1:
  `Ran 133 tests ... FAILED (failures=1, skipped=11)`. The failure is
  `test_check_fails_on_a_stale_copy_and_update_repairs_it (test_readme_status.CommandLineTest)`
  at `tests/test_readme_status.py:234`, `AssertionError: 1 != 0`: the pre-existing Python 3.9
  incompatibility round 019's verifier recorded (`verifier.md`, NOTE 1); neither file is
  touched by this round.
- The same suite under Homebrew Python 3.13 (`python3.13 -m unittest discover -s tests -v`) →
  exit 0: `Ran 133 tests in 1.921s`, `OK (skipped=11)`. The 11 skips, by name:
  `test_unreadable_source_tree_fails_closed` (TestBinaryFreshness: "Windows ACL denial is
  required for this enumeration test"), and ten skipped because no fresh `client/`
  semantic-trace binary was built ("no semantic-trace binary is present and newer than every
  client source file"): TestSemanticGoldens `test_fixtures_exist`,
  `test_rendering_is_deterministic`, `test_traces_match_golden`; TestSemanticQuality
  `test_committed_fixtures_are_semantically_complete`,
  `test_coverage_accounts_for_every_packet`, `test_model_invariants_hold_at_the_end`,
  `test_no_environmental_leakage`, `test_no_packet_is_malformed_or_unknown`,
  `test_query_stream_coverage_is_real_and_clean`, `test_something_is_actually_decoded`.
- `python3 tools/generate_messages.py --check` → exit 0, `message table up to date (96 ids)`.
- `python3 tools/generate_protocol_constants.py --check` → exit 0, `protocol constants up to
  date (187 values)`.
- `python3 tools/generate_readme_status.py --check` → exit 0, `README status block is up to
  date`.
- `python3 tools/check_home_status.py` → exit 0, `home screen status matches docs/ROADMAP.md`.
- `python3 tools/fw.py check` → exit 0, `0 error(s), 0 warning(s)`.

### CI (required evidence 7)

- `gh run view 35908734315` → head `06cca5aa6290d56033b3036993ba4385056e9bf9`, conclusion
  `success`: Qt 6 shell (Linux): success; Regression harness (3.12): success; Regression
  harness (3.10): success; Semantic client model: success; Card and deck data: success;
  Upstream EDOPro baseline: skipped (it runs on pull requests, schedule and `master` only).
  This report's own commit comes after `06cca5aa` and changes only this file; its CI run is
  not recorded here.

### Personal data

- `git diff 1aaff456 | grep '^+' | grep -nE '/Users/|/private/|/tmp/|/Applications|<email
  pattern>|C:\\|/home/'` → no match (exit 1): no home-folder path, drive path or email
  address was added to a tracked file. The clone's own absolute path appears in the raw
  submodule output above and was replaced by `<clone>`.

### Upstream source relied on (required evidence 3)

Quoted in full, with line numbers, in `docs/architecture/deck-placement.md` (§2-§5); the
load-bearing passages, re-read at the pinned upstream base:

- `gframe/deck_manager.cpp:335-339` (the lambda's first half):
  ```cpp
  auto is_extra_deck_card = [&](auto* card) {
  	if(card->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
  		return true;
  	if(card->type & (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER))
  		return true;
  ```
  `:340-348`:
  ```cpp
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
- `gframe/deck_manager.cpp:357-363` (token skip, then classification):
  ```cpp
  	if(!cd || cd->type & TYPE_TOKEN)
  		continue;
  	else if((!extralist || cd->code != 0) && is_extra_deck_card(cd))  {
  		deck.extra.push_back(cd);
  	} else {
  		deck.main.push_back(cd);
  	}
  ```
- `gframe/data_manager.h:92-98`: `isRitualMonster()` is `(type & (TYPE_MONSTER | TYPE_RITUAL))
  == (TYPE_MONSTER | TYPE_RITUAL)`; `isRush()` is `ot & SCOPE_RUSH`; `:29` `#define SCOPE_RUSH
  0x200`.
- `gframe/deck.h:19-23`: `enum class RITUAL_LOCATION : uint8_t { DEFAULT, MAIN, EXTRA, };`
- `gframe/deck_con.cpp:1578-1588` (`push_main`'s type gates) and `:1611-1621`
  (`push_extra`'s), quoted in deck-placement.md §3.1; `:725`:
  `if (!push_main(pointer, -1, gGameConfig->ignoreDeckContents) && !push_extra(pointer, -1, gGameConfig->ignoreDeckContents))`;
  `:1192`: `if(data._data.type & TYPE_TOKEN || data._data.ot & SCOPE_HIDDEN || ...`.
- `gframe/deck_manager.cpp:220-223` and `:228-232` (`CheckDeckContent`'s zone callbacks),
  quoted in deck-placement.md §4.
- `gframe/generic_duel.cpp:421-423`:
  `bool rituals_in_extra = host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32);` and
  `LoadDeckFromBuffer(..., rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN)`;
  `gframe/menu_handler.cpp:43`: `BufferIO::Write<uint32_t>(pdeck, static_cast<uint32_t>(deck.main.size() + deck.extra.size()));`
- `ocgcore/ocgapi_constants.h:33` `#define TYPE_MONSTER     0x1`, `:46` `#define TYPE_TOKEN
  0x4000`, `:414` `#define DUEL_EXTRA_DECK_RITUAL 0x800000000`.

## Not verified

- **Behaviour through upstream's own GUI.** Nothing here ran upstream's Irrlicht deck builder;
  its push-cascade behaviour (deck-placement.md §3) is established by reading the source only.
  Nothing ran `LoadDeck` either in this round; its separated-mode Main-to-Extra move is proven
  in CI by the existing `ydk-interoperability.md` harness, not by anything new here.
- **`gframe/`, `integration/`, the upstream baseline and duel behaviour**: not touched and not
  built; no claim about duel behaviour is made.
- **Real card data.** All tests use synthetic cards. The statement that the two card shapes
  where upstream's push cascade and its lambda disagree (deck-placement.md §3.3) do not occur
  comes from a read-only SQL count over the card databases of a Project Ignis install present
  on this machine (a Link-bit card with neither Monster nor Spell: 0; a Ritual Monster with a
  Fusion, Synchro, Xyz or Link bit: 0, across 21 `.cdb` files). It is not reproducible in CI,
  and not a claim about every database a user may load. No card data was copied into the
  repository.
- **Qt 6.8.3 (CI's pin) and Windows.** Built and tested here only with Qt 6.11.1 on macOS;
  Qt 6.8.3 on Linux is covered by CI's success at `06cca5aa`; Windows was not built.
- **The offscreen check's empty-stderr assertion on macOS** does not pass verbatim (the font
  notice above); it passes on CI's Linux runner.
- **Visual check of the changed add buttons.** No screenshot was taken; the buttons' text and
  enabled state are verified only through `addButtonsFollowTheControllersPlacement`, which
  reads the real QML objects' properties.
- **Golden reproduction** (`--update` then `git diff --exit-code -- tests/golden`): not run;
  this round changes nothing under `tools/`, `tests/` or protocol tables.
- **`client/` cycle**: not run; `client/` is out of scope and unchanged.
- **The 11 skipped Python tests** did not run (reasons above).
- **This report's own commit's CI run** is not recorded.

## Changed

- `policy/include/edopro_next/policy/deck_placement.h`, `policy/src/deck_placement.cpp`
  (new): the one Extra Deck rule — `RitualPlacement` (mirrors `RITUAL_LOCATION`),
  `ritual_placement_for`, `is_ritual_monster`, `is_rush`, `is_token`, `is_extra_deck_type`,
  `belongs_in_extra_deck` (the lambda), `classify_card` (plus token/unknown handling).
- `policy/src/deck_validation.cpp`: the zone checks now reach the rule (Main: calls
  `belongs_in_extra_deck`; Extra: the shared primitives in upstream's Ritual-first order);
  its private type-bit copies and helpers are removed. Behaviour unchanged.
- `policy/CMakeLists.txt`: builds `deck_placement.cpp`; adds `test_deck_placement`.
- `policy/tests/test_deck_placement.cpp` (new): every classification case and the validation
  equivalence test.
- `policy/include/edopro_next/policy/validation_policy.h`: comment only (the field still is
  a boolean; how validation now reaches the three-state rule).
- `ui/src/deckbuilder/deck_controller.{h,cpp}`: `placementFor()`, `addCardToDeck()`,
  `kAddRitualPlacement`; header comment updated.
- `ui/qml/screens/DeckBuilderScreen.qml`: `selectedResultPlacement`, `addSelectedResultToDeck()`,
  `addSelectedResultToSide()`; "Add to Main"/"Extra"/"Side" buttons become one add button
  labelled with the controller's section plus "Add to Side", both disabled for an unplaceable
  card; header comment updated.
- `ui/src/deckbuilder/card_entry.cpp`: comment only.
- `ui/tests/test_deckbuilder.cpp`: `SyntheticCard.scope` (default 0) and four new tests.
- `ui/tests/test_deckbuilder_screen.cpp`: one new test.
- `docs/architecture/deck-placement.md` (new): upstream's behaviour for every case with
  citations; this project's rule, users, decisions and divergences.
- `docs/adr/0011-extra-deck-classification.md` (new): the decisions.
- `docs/adr/0004-…`, `0006-…`, `0007-…`: follow-up / "superseded in part" notes, additions only.
- `docs/architecture/deck-builder-ui.md`, `deck-legality.md`, `deck-builder-legality.md`,
  `deck-model.md`, `docs/ROADMAP.md`, `docs/capabilities.md`: updated (sentences below).
- `docs/state.md`: not changed.

### Sentences removed, and what replaced them (required evidence 6)

1. `deck-builder-ui.md` §0: removed "While legality validation, ruleset selection, and banlist
   selection are now integrated (see §14 and ADR 0010), there is still no automatic Main/Extra
   classification, no artwork, no archetype-name search, no controller/gamepad navigation, and
   no full keyboard parity with upstream." Replaced by: "Legality validation, ruleset selection
   and banlist selection are integrated (see §14 and ADR 0010), and adding a card places it in
   Main or Extra by upstream's rule (§7.2 and ADR 0011), but there is still no artwork, no
   archetype-name search, no structured filters, no controller/gamepad navigation, and no full
   keyboard parity with upstream."
2. `deck-builder-ui.md` §1: removed "But `push_main` and `push_extra` do gate on type:
   `push_main` rejects Fusion/Synchro/Xyz and non-Spell Link cards, and `push_extra` rejects
   anything that is not Ritual/Fusion/Synchro/Xyz/Link (`:1585-1588,1617-1621`) - so a card
   lands …" (it omitted the Ritual/Rush gates and the Link-Spell refusal). Replaced by the same
   sentence naming a Rush Ritual Monster (refused by `push_main`), a Link Spell and a non-Rush
   Ritual Monster (refused by `push_extra`), citing `:1578-1588,1611-1621` and
   deck-placement.md §3.
3. `deck-builder-ui.md` §7: "(`addSelectedResultTo()` only ever offers a code …" became "(the
   screen only ever offers a code …" (the function was renamed).
4. `deck-builder-ui.md` §7: removed the bullet "The caller always names the section
   explicitly. `DeckController` has no logic anywhere that inspects a `CardEntry`'s
   `isMonster`/`isXyz`/`isLink`/`isPendulum` fields to choose or veto a destination - those
   fields exist on `CardEntry` purely for the preview pane (§10.7) and the search result's
   summary line (§6), never for section routing. This is the direct continuation of upstream's
   own `push_main`/`push_extra`/`push_side` shape (§1): the destination is always an explicit
   parameter, never inferred inside the push/add call itself." Replaced by: "`addCard(code,
   section)` places a card exactly where its caller names. It never inspects the card, and it
   never moves one: the explicit, unclassified primitive, matching upstream's own
   `push_main`/`push_extra`/`push_side` shape (§1) in taking the destination as a parameter.
   `CardEntry`'s … fields exist purely for the preview pane (§10.7) and the search result's
   summary line (§6), never for section routing."
5. `deck-builder-ui.md` §7: removed "… keeps it in all three, rather than a hypothetical
   Fusion/Synchro/Xyz/Link check moving it to Extra the way upstream's separate `LoadDeck`
   reclassification step would (`deck-model.md`§3) - a step this slice does not implement or
   call, matching `edopro_next_deck` itself." Replaced by "… adding the same code to all three
   sections with `addCard` keeps it in all three.", plus the new §7.2.
6. `deck-builder-ui.md` §12: removed the bullets "Legality of any kind: deck-size limits, the
   three-copy rule, `LFList`/banlist checks." (no replacement: done since round 015, §14) and
   "Automatic Main/Extra classification from card type (upstream's `LoadDeck`
   reclassification, `deck-model.md`§3) - a `Deck -> Deck` transformation layered on top of
   `edopro_next_deck`, deliberately not built here or inside this codec." (replaced by §7.2 and
   ADR 0011). "The legacy sigil search grammar / archetype-name resolution
   (`card-search.md`§1.1) - only plain text search is wired up." became "… (`card-search.md`§1.1),
   and structured filters - only plain text search is wired up."
7. `deck-legality.md` §0: removed "Not automatic Main/Extra classification. This module
   validates a `Deck` whose section split already exists; it never moves a card between
   sections. See `docs/architecture/deck-model.md` for why classification, if it is ever built,
   is intentionally a separate `Deck -> Deck` transformation." Replaced by: "Not a deck
   reclassifier. `validate_deck()` validates a `Deck` whose section split already exists, and
   nothing in `policy/` moves a card between sections. Since round 020 the per-card Extra Deck
   rule itself does live in this module (`deck_placement.h`; see deck-placement.md and ADR
   0011): the zone checks below call it, and the deck builder uses it to place a card being
   added."
8. `deck-legality.md` §7: removed "… not the three-state loader enum - this module does not
   implement automatic classification at all (§0), so the loader's `DEFAULT`
   (Rush-conditional) behavior has no equivalent here; a caller who needs it must resolve it to
   a boolean before calling `validate_deck()`." Replaced by: "… not the three-state loader enum.
   `CheckDeckContent` never receives the loader's `DEFAULT` (Rush-conditional) mode, so
   validation has no use for it. The three-state enum does exist in this module since round
   020, as `RitualPlacement` in `deck_placement.h`, for classifying a card; the Main zone check
   calls that rule with `ritual_placement_for(rituals_belong_in_extra)`, upstream's own `flag ?
   EXTRA : MAIN` conversion (deck-placement.md §2.3, §4)."
9. `deck-builder-legality.md` §2.2: removed "… which is exactly the "automatic Main/Extra
   classification" this project's `policy/` and `edopro_next_deck` both deliberately do not
   perform (`deck-legality.md`§0, `deck-builder-ui.md`§0/§7)." and "…; this project's own
   architecture keeps those two concerns in different layers (`edopro_next_deck` for
   classification, if ever built; `policy/` for validation), so this is a genuine three-way
   split (editor: fused together; duel entry: neither - `CheckDeckContent` trusts the caller's
   Main/Extra split completely; this project: two separate, not-yet-connected pieces) rather
   than a two-way comparison." Replaced by: "Since round 020 this project has the
   classification half as one per-card rule in `policy/` (`deck_placement.h`), which the deck
   builder uses when adding a card and `validate_deck()` uses for its zone checks, and keeps
   count caps in validation only (deck-placement.md, ADR 0011)." plus a marked *Correction
   (round 020)*: the "duel entry: neither" claim was wrong, because the client sends Main and
   Extra as one list (`menu_handler.cpp:43-48`) and the server re-splits it by type
   (`generic_duel.cpp:423`) before `CheckDeckContent`.
10. `deck-model.md` §8: "… deliberately not reimplemented here - see §3 for the reasoning and
    where such a layer would belong if built later." became "… deliberately not reimplemented
    here - see §3 for the reasoning. The per-card rule now lives in `policy/` (…); opening a
    file still does not reclassify it."
11. `docs/ROADMAP.md`, M3 "Deck builder UI" item: "an explicit-choice Main/Extra/Side editor
    over one canonical `Deck`, `.ydk` open/save/new with a tested dirty-state contract," became
    "a Main/Extra/Side editor over one canonical `Deck` that adds a card to Main or Extra by
    upstream's Extra Deck rule (one definition, in `policy/`, which validation also calls - ADR
    0011) and to Side on request, `.ydk` open/save/new with a tested dirty-state contract
    (opening follows the file's own sections),"; "Still missing: automatic Main/Extra
    classification, artwork, the legacy sigil search grammar and structured filters beyond
    plain text, and full keyboard/controller parity." became "Still missing: artwork, the
    legacy sigil search grammar and structured filters beyond plain text, and full
    keyboard/controller parity."; the design links gained deck-placement.md and ADR 0011. The
    checkbox stays unchecked.
12. `docs/ROADMAP.md`, M3 exit paragraph: "… with advisory legality validation exists;
    automatic classification, structured/legacy search parity and full keyboard/controller
    parity remain, so the milestone is not complete." became "… with advisory legality
    validation and automatic Main/Extra placement exists; artwork, structured/legacy search
    parity and full keyboard/controller parity remain, so the milestone is not complete."
13. `docs/capabilities.md`, "Deck builder UI" row: "search, explicit Main/Extra/Side editing,
    `.ydk` open/save, and advisory, non-blocking legality … over a tested Qt adapter; no
    automatic Main/Extra classification, no artwork, no full keyboard/controller parity" became
    "search; adding a card puts it in Main or Extra by upstream's Extra Deck rule (computed by
    `policy/`, the same rule validation uses), or in Side on request; `.ydk` open/save,
    following the file's own sections; and advisory, non-blocking legality … over a tested Qt
    adapter; no artwork, no structured search filters, no full keyboard/controller parity",
    with links to deck-placement.md and ADR 0011 added.

ADRs 0004, 0006 and 0007 only gained notes; no sentence was removed from any ADR.

## Open questions

1. **The legality banner's "Would not be accepted at duel entry" for a misplaced card.**
   Reading the network path for this round showed that upstream's server re-splits a deck by
   type before `CheckDeckContent` runs (`menu_handler.cpp:43-48`, `generic_duel.cpp:423`;
   deck-placement.md §5.4). So for any card other than the Ritual hybrid, a card in the wrong
   Main/Extra section would be moved, not rejected, at a real upstream duel entry, and the
   existing message (round 015, ADR 0010) overstates it. I corrected the architecture record
   (deck-builder-legality.md, marked as a correction) but did not change the message or
   validation: it is ADR 0010's presentation, and changing it (for example validating the
   re-split deck) would move `validate_deck()`'s behaviour, which this brief keeps fixed. Brain
   should decide whether this is a follow-up round.
2. **Two card shapes where upstream disagrees with itself** (deck-placement.md §3.3, §4): a
   Link-bit card with neither Monster nor Spell, and a Ritual Monster that is also
   Fusion/Synchro/Xyz/Link. This project follows `LoadDeck`'s lambda for adding and reproduces
   `CheckDeckContent`'s Ritual-first Extra check for validation. Both are recorded in ADR 0011;
   neither shape was found in local card data.
3. **Tokens appear in search results.** Upstream's search hides them (`deck_con.cpp:1192`);
   this project's `CardSearchIndex` does not. This round disables both add buttons for a token
   instead of touching search, which the brief puts out of scope. Hiding tokens from results
   belongs with the structured-filters work.
4. **Records outside the brief's list that are now stale.** `docs/overview.md` says the
   deck builder has users "build Main/Extra/Side explicitly" and "shows no deck legality" (the
   latter stale since round 015); `docs/state.md` lists "Main/Extra classification" among what
   is missing (lines 22 and 89). Neither file is in this round's scope (state.md is Brain's), so
   neither was edited.
5. **ADR 0006 and ADR 0007** received "superseded in part" notes rather than being rewritten;
   Brain may prefer a different form.
6. **Python 3.9 failure** in `test_readme_status` is pre-existing and not addressed (the
   framework says Python 3.9+; CI's floor is 3.10).

<!-- fw-report
round: 021-placement-record-correction
role: builder
branch: builder/021-placement-record-correction
head: 55155a3d1506d00a4e684078e14315498cc68d5b
os: macOS 27.0
python: 3.9.6
written: 2026-09-24T07:23:24Z
-->
# Builder report: 021-placement-record-correction

Code and documentation evidence below is at commit `6a52b92ceba2a1fdb768c9048384d4325860c392` (the
last code/docs commit; `fw.py report` commits this file on top of it, which changes no code). All
runs are on macOS, in a fresh clone whose absolute path is written `<clone>` below.

## Verified

### Seat start (required evidence 1)

- `python3 tools/fw.py start --role builder --round 021-placement-record-correction` → exit 0
  ```
  seat ok: builder, round 021-placement-record-correction, branch builder/021-placement-record-correction at 8f7d73b10ae8
    brief: docs/rounds/021-placement-record-correction/brief.md
    finish with: write docs/rounds/021-placement-record-correction/builder.md, then python3 tools/fw.py report --role builder --round 021-placement-record-correction --push
  ```
- `git submodule update --init` → exit 0
  ```
  Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
  Cloning into '<clone>/ocgcore'...
  Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
  ```
- `git submodule status` → exit 0
  ```
   46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
  ```
- Environment, each from a command: `sw_vers` → macOS 27.0 (build 26A428); `uname -m` → arm64;
  `c++ --version` → Apple clang version 21.0.0 (clang-2100.3.34.2); `cmake --version` → 4.4.3;
  `ninja --version` → 1.13.2; `sqlite3 --version` → 3.54.0; `qmake6 -query QT_VERSION` →
  **6.11.1** (CI pins Qt 6.8.3 on Linux, `.github/workflows/edopro-next.yml`; not run here);
  `python3 --version` → 3.9.6 (system); Homebrew `python3.13 --version` → 3.13.15.

### The upstream passages relied on (required evidence 2)

Every passage below was printed from the clone's `gframe/`, at the upstream base
`docs/UPSTREAM.md` records, by line range (not from memory); `ocgcore/ocgapi_constants.h` from
submodule commit `46779fbe`. The line numbers in `docs/architecture/deck-placement.md` still match.

**push_main and push_extra type gates (unforced, not side-decking, section not full)** — `gframe/deck_con.cpp`
```
1577: bool DeckBuilder::push_main(const CardDataC* pointer, int seq, bool forced) {
1578: 	if(pointer->isRitualMonster()) {
1579: 		if(mainGame->is_siding) {
1580: 			if(mainGame->dInfo.HasFieldFlag(DUEL_EXTRA_DECK_RITUAL))
1581: 				return false;
1582: 		} else if(pointer->isRush() && !forced)
1583: 			return false;
1584: 	}
1585: 	if(pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
1586: 		return false;
1587: 	if((pointer->type & (TYPE_LINK | TYPE_SPELL)) == TYPE_LINK)
1588: 		return false;
...
1610: bool DeckBuilder::push_extra(const CardDataC* pointer, int seq, bool forced) {
1611: 	if(pointer->isRitualMonster()) {
1612: 		if(mainGame->is_siding) {
1613: 			if(!mainGame->dInfo.HasFieldFlag(DUEL_EXTRA_DECK_RITUAL))
1614: 				return false;
1615: 		} else if(!pointer->isRush() && !forced)
1616: 			return false;
1617: 	} else if(pointer->type & TYPE_LINK) {
1618: 		if(pointer->type & TYPE_SPELL)
1619: 			return false;
1620: 	} else if((pointer->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) == 0)
1621: 		return false;
```

**the call sites and their orders** — `gframe/deck_con.cpp`
```
624: 		const bool forceInput = gGameConfig->ignoreDeckContents || event.MouseInput.Shift;
...
674: 					push_main(dragging_pointer);
675: 				else if(click_pos == 2)
676: 					push_extra(dragging_pointer);
677: 				else if(click_pos == 3)
678: 					push_side(dragging_pointer);
...
725: 						if (!push_main(pointer, -1, gGameConfig->ignoreDeckContents) && !push_extra(pointer, -1, gGameConfig->ignoreDeckContents))
726: 							push_side(pointer);
...
735: 					if(!push_extra(dragging_pointer))
736: 						push_main(dragging_pointer);
...
758: 			if (hovered_pos == 1) {
759: 				if(!push_main(pointer))
760: 					push_side(pointer);
761: 			} else if (hovered_pos == 2) {
762: 				if(!push_extra(pointer))
763: 					push_side(pointer);
764: 			} else if (hovered_pos == 3) {
765: 				if(!push_side(pointer) && !push_extra(pointer))
766: 					push_main(pointer);
767: 			} else {
768: 				if(!push_extra(pointer) && !push_main(pointer))
769: 					push_side(pointer);
770: 			}
...
875: 					if(hovered_pos == 3)
876: 						push_side(dragging_pointer, hovered_seq + is_lastcard, true);
877: 					else {
878: 						push_main(dragging_pointer, hovered_seq, true) || push_extra(dragging_pointer, hovered_seq + is_lastcard, true);
```

**the lambda and the per-code loop** — `gframe/deck_manager.cpp`
```
335: 	auto is_extra_deck_card = [&](auto* card) {
336: 		if(card->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
337: 			return true;
338: 		if(card->type & (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER))
339: 			return true;
340: 		if(card->isRitualMonster()) {
341: 			if(rituals_in_extra == RITUAL_LOCATION::DEFAULT) {
342: 				return card->isRush();
343: 			} else {
344: 				return rituals_in_extra == RITUAL_LOCATION::EXTRA;
345: 			}
346: 		}
347: 		return false;
348: 	};
...
349: 	for(auto code : mainlist) {
350: 		if(!(cd = gDataManager->GetCardData(code))) {
351: 			cd = gdeckManager->GetDummyOrMappedCardData(code);
352: 			if((!cd || cd->code == 0) && !loadalways) {
353: 				errorcode = code;
354: 				continue;
355: 			}
356: 		}
357: 		if(!cd || cd->type & TYPE_TOKEN)
358: 			continue;
359: 		else if((!extralist || cd->code != 0) && is_extra_deck_card(cd))  {
360: 			deck.extra.push_back(cd);
361: 		} else {
362: 			deck.main.push_back(cd);
363: 		}
364: 	}
```

**CheckDeckContent's two zone callbacks** — `gframe/deck_manager.cpp`
```
219: 	ret = CheckCards(deck.main, lflist, allowedCards, ccount, [&](const CardDataC* cit)->DeckError {
220: 		if ((cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) || (cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
221: 			return { DeckError::EXTRACOUNT };
222: 		if(cit->isRitualMonster() && rituals_in_extra)
223: 			return { DeckError::EXTRACOUNT };
224: 		return { DeckError::NONE };
225: 	});
226: 	if (ret.type) return ret;
227: 	ret = CheckCards(deck.extra, lflist, allowedCards, ccount, [&](const CardDataC* cit)->DeckError {
228: 		if(cit->isRitualMonster()) {
229: 			if(!rituals_in_extra)
230: 				return { DeckError::EXTRACOUNT };
231: 		} else if (!(cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) && !(cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
232: 			return { DeckError::EXTRACOUNT };
233: 		return { DeckError::NONE };
234: 	});
```

**what the server does with a submitted deck** — `gframe/generic_duel.cpp`
```
373: 		DeckError deck_error = DeckManager::CheckDeckSize(dueler.pdeck, host_info.sizes);
374: 		if(deck_error.type == DeckError::NONE && !host_info.no_check_deck_content) {
375: 			if(dueler.deck_error) {
376: 				deck_error.type = DeckError::UNKNOWNCARD;
377: 				deck_error.code = dueler.deck_error;
378: 			} else {
379: 				bool rituals_in_extra = host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32);
380: 				deck_error = DeckManager::CheckDeckContent(dueler.pdeck, gdeckManager->GetLFList(host_info.lflist),
381: 														   static_cast<DuelAllowedCards>(host_info.rule), host_info.forbiddentypes, rituals_in_extra);
...
421: 	bool rituals_in_extra = host_info.duel_flag_high & (DUEL_EXTRA_DECK_RITUAL >> 32);
422: 	if(match_result.empty()) {
423: 		dueler.deck_error = DeckManager::LoadDeckFromBuffer(dueler.pdeck, (uint32_t*)deckbuf, mainc, sidec, rituals_in_extra ? RITUAL_LOCATION::EXTRA : RITUAL_LOCATION::MAIN);
```

**what the client sends** — `gframe/menu_handler.cpp`
```
43: 	BufferIO::Write<uint32_t>(pdeck, static_cast<uint32_t>(deck.main.size() + deck.extra.size()));
44: 	BufferIO::Write<uint32_t>(pdeck, static_cast<uint32_t>(deck.side.size()));
45: 	for(const auto& pcard : deck.main)
46: 		BufferIO::Write<uint32_t>(pdeck, pcard->code);
47: 	for(const auto& pcard : deck.extra)
48: 		BufferIO::Write<uint32_t>(pdeck, pcard->code);
```


Constants: `ocgcore/ocgapi_constants.h:33-58`: `TYPE_MONSTER 0x1`, `TYPE_SPELL 0x2`,
`TYPE_TRAP 0x4`, `TYPE_FUSION 0x40`, `TYPE_RITUAL 0x80`, `TYPE_SYNCHRO 0x2000`,
`TYPE_TOKEN 0x4000`, `TYPE_XYZ 0x800000`, `TYPE_LINK 0x4000000`;
`gframe/data_manager.h:29` `SCOPE_RUSH 0x200`; `:92-98`
`isRitualMonster()` = `(type & (TYPE_MONSTER | TYPE_RITUAL)) == (TYPE_MONSTER | TYPE_RITUAL)`,
`isRush()` = `ot & SCOPE_RUSH`.

### Part 1 and 2: the exhaustive record and its test

- Before writing anything I enumerated all 256 combinations of the eight type bits (Monster,
  Spell, Trap, Fusion, Ritual, Synchro, Xyz, Link) in both Rush scopes with a scratch Python
  transcription of the quoted `push_main`/`push_extra`/lambda (outside the repository). It found
  156 disagreements in six families and, for both call-site orders, no card accepted by both
  pushes. The C++ test in the repository was then written separately and arrives at the same
  total: `./policy/build/test_deck_placement` → exit 0, `14 tests, 0 failed, 0 assertions failed`
  including `ok   deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies`.
- The six families, their counts and their outcomes are in `docs/architecture/deck-placement.md`
  §3.3 and ADR 0011 Decision 2; the test (`policy/tests/test_deck_placement.cpp`, "Part 3")
  requires the differing set to be exactly their union, each with the stated outcomes and count
  (8 + 4 + 2 + 2 + 56 + 84 = 156 of 512), that no differing card is in a family other than its
  own, that no agreeing card is in any, that the two call-site orders agree, and that no card is
  accepted by both pushes. It also requires that, for every differing card, the project's answer
  is the same under all three `RitualPlacement` values (the ADR's argument depends on that).
- Coverage of call sites: §3.2 now lists every place in `deck_con.cpp` that pushes without
  `forced` (`:674-678`, `:725-726`, `:735-736`, `:758-770`, `:701` while side-decking) and the
  order each uses; the earlier text omitted middle-click on a deck card and the right-button
  drop from Side (`:735-736`).

### Acceptance criterion 2: the test fails when any one of the three is changed (required evidence 4)

Each mutation was applied to a clean tree at `6a52b92c`, `policy/build` rebuilt, the test run,
and the file restored with `git checkout -- <file>`; `git status --short` was empty afterwards
and `./policy/build/test_deck_placement` printed `14 tests, 0 failed, 0 assertions failed` again.
Output is the real output with the clone path removed and the list shortened.

**Transcription changed**

1. T1, `policy/tests/test_deck_placement.cpp`, `push_extra`'s Link branch stops refusing Spell:
   ```
   -		if(type & kTypeSpell)
   +		if(type & kTypeTrap)
   ```
   → exit 1
   ```
   FAIL deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       expected: !(builder_push_main_accepts(type, rush) && builder_push_extra_accepts(type, rush))
   FAIL deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x4000004 (non-Rush scope): deck builder -> Side, this project -> Main but family A link-not-monster-not-spell records deck builder -> Extra, this project -> Main
   FAIL deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x4000005 (non-Rush scope): deck builder -> Side, this project -> Extra is in 0 recorded families, expected exactly 1
   ```
2. T2, same file, `push_main`'s Link-without-Spell gate deleted:
   ```
   -	if((type & (kTypeLink | kTypeSpell)) == kTypeLink)
   -		return false;
   ```
   → exit 1
   ```
   FAIL deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x4000000 (non-Rush scope): deck builder -> Main, this project -> Main agree, but 1 recorded family(ies) claim it
   FAIL deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x4000001 (non-Rush scope): deck builder -> Main, this project -> Extra is in 0 recorded families, expected exactly 1
   ```

**Recorded set changed**

3. R1, family D's recorded count:
   ```
   -	 Lands::Side, Lands::Extra, 84},
   +	 Lands::Side, Lands::Extra, 83},
   ```
   → exit 1 (`14 tests, 1 failed, 1 assertions failed`)
   ```
   std::string(family.id) + ": " + std::to_string(family_cards[i]) == std::string(family.id) + ": " + std::to_string(family.cards)
       actual:   D link-spell-fusion-synchro-xyz-not-ritual-monster: 84
       expected: D link-spell-fusion-synchro-xyz-not-ritual-monster: 83
   ```
4. R2, family C1's recorded deck-builder outcome:
   ```
   -	 Lands::Side, Lands::Extra, 2},
   +	 Lands::Extra, Lands::Extra, 2},
   ```
   → exit 1 (`14 tests, 1 failed, 2 assertions failed`)
   ```
   type 0x4000081 (non-Rush scope): deck builder -> Side, this project -> Extra but family C1 ritual-link-not-spell-not-rush records deck builder -> Extra, this project -> Extra
   type 0x4000085 (non-Rush scope): deck builder -> Side, this project -> Extra but family C1 ritual-link-not-spell-not-rush records deck builder -> Extra, this project -> Extra
   ```
5. R3, family B1's predicate widened (drops the Monster requirement):
   ```
   -		 return (t & kTypeLink) && (t & kTypeSpell) && (t & kTypeMonster) && !(t & kTypeRitual) &&
   +		 return (t & kTypeLink) && (t & kTypeSpell) && !(t & kTypeRitual) &&
   ```
   → exit 1 (`14 tests, 1 failed, 5 assertions failed`)
   ```
   type 0x4000002 (non-Rush scope): deck builder -> Main, this project -> Main agree, but 1 recorded family(ies) claim it
   B1 link-spell-monster: 8   (expected 4)
   ```

**This project's rule changed** (`policy/src/deck_placement.cpp`)

6. P1, Link no longer needs the Monster bit:
   ```
   -	if((record.type & kTypeLink) && (record.type & kTypeMonster))
   +	if(record.type & kTypeLink)
   ```
   → exit 1, `14 tests, 4 failed, 218 assertions failed`; the new test is one of the four:
   ```
   bad  linkWithoutMonsterBelongsInMain
   bad  validationZoneChecksAgreeWithTheRuleForEveryTypeCombination
   bad  extraDeckTypeIsTheUnconditionalHalfOnly
   bad  deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x4000000 (non-Rush scope): deck builder -> Extra, this project -> Extra agree, but 1 recorded family(ies) claim it
       type 0x4000002 (non-Rush scope): deck builder -> Main, this project -> Extra is in 0 recorded families, expected exactly 1
   ```
7. P2, Rush ignored under `RushInExtra`:
   ```
   -			return is_rush(record);
   +			return false;
   ```
   → exit 1, `14 tests, 3 failed, 12 assertions failed`; the new test is one of the three:
   ```
   bad  ritualMonsterUnderRushInExtraFollowsRushScope
   bad  tokensAndUnknownCodesAreNeverPlaced
   bad  deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
       type 0x81 (Rush scope): deck builder -> Extra, this project -> Main is in 0 recorded families, expected exactly 1
   ```

### Acceptance criterion 4: every legality message, pinned (required evidence 6, second part)

`ui/tests/test_deckbuilder.cpp`, `everyLegalityMessageSpeaksAboutTheDeckAsArrangedOnly`, calls the
(now public, C++-only) `DeckController::formatLegalityError()` for 18 cases: every `DeckErrorType`
(both branches of MainCount, ExtraCount as count and as placement, SideCount, and None) plus an
out-of-range value, compares the exact text, and requires that none contains "duel entry".
Existing assertions moved to the new text: in `test_deckbuilder.cpp`
`banlistNulloptVsEmptyConcreteLegalityBehaviour` (a comment and a `startsWith`),
`illegalDeckSurfacesSpecificErrors` (the full Main-count text and the placement `contains`),
`noBanlistSelectionDisclosesSkippedChecksForExtraDeckMonsterInMain` (the placement `contains`, and a
new `!contains("belongs in the Main deck")` beside it),
`noBanlistSelectionDisclosesSkippedChecksWhenAlsoIllegal` (`startsWith`) and
`concreteBanlistSelectionCarriesNoSkippedChecksDisclosure` (the exact "passes" text); in
`test_deckbuilder_screen.cpp` `legalityStatusBoxIsRenderedAndUpdatesReactively` (two `startsWith`).
The new test failing when a message is changed (`ui/src/deckbuilder/deck_controller.cpp`, at `6a52b92c`, restored afterwards):

8. U1, a Side-count message put back to the old text:
   ```
   -            return QStringLiteral("Fails as arranged: Side deck has %1 cards, exceeding the maximum of %2.")
   +            return QStringLiteral("Would not be accepted at duel entry: Side deck has %1 cards, exceeding the maximum of %2.")
   ```
   → `test_deckbuilder` exit 1, `Totals: 34 passed, 1 failed`
   ```
   FAIL!  : TestDeckBuilder::everyLegalityMessageSpeaksAboutTheDeckAsArrangedOnly() Compared values are not the same
      Actual   (message)   : "Would not be accepted at duel entry: Side deck has 16 cards, exceeding the maximum of 15."
      Expected (c.expected): "Fails as arranged: Side deck has 16 cards, exceeding the maximum of 15."
   ```
   (`test_deckbuilder_screen` still passes: no screen test reaches a Side-count error, so only the
   new test pins that message.)
9. U2, the placement message put back to the old wording:
   ```
   -            return QStringLiteral("Fails as arranged: %1 is in a section that does not accept it "
   +            return QStringLiteral("Fails as arranged: %1 belongs in the Main deck, not the Extra deck "
   ```
   → `test_deckbuilder` exit 3, `Totals: 32 passed, 3 failed`: `illegalDeckSurfacesSpecificErrors`,
   `noBanlistSelectionDisclosesSkippedChecksForExtraDeckMonsterInMain` (its output: `Fails as
   arranged: 'Fusion Beast' (200) belongs in the Main deck, not the Extra deck (whether a card ...`)
   and `everyLegalityMessageSpeaksAboutTheDeckAsArrangedOnly`. After restoring, `Totals: 35 passed,
   0 failed` and `Totals: 22 passed, 0 failed` again.

### Module cycles (required evidence 3), fresh `rm -rf policy/build ui/build`, at `6a52b92c`

Script output (each command's real exit status; long output shortened to its last lines):

  ```
  6a52b92ceba2a1fdb768c9048384d4325860c392
  $ cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON
  -- Generating done (0.0s)
  -- Build files have been written to: <clone>/policy/build
  -> exit 0

  $ cmake --build policy/build
  [18/19] Building CXX object _data/CMakeFiles/edopro_next_search.dir/src/card_search_index.cpp.o
  [19/19] Linking CXX static library _data/libedopro_next_search.a
  -> exit 0

  policy build warning lines: 0
  $ ctest --test-dir policy/build --output-on-failure
  Test project <clone>/policy/build
      Start 1: lf_list
  1/3 Test #1: lf_list ..........................   Passed    0.14 sec
      Start 2: deck_validation
  2/3 Test #2: deck_validation ..................   Passed    0.20 sec
      Start 3: deck_placement
  3/3 Test #3: deck_placement ...................   Passed    0.16 sec

  100% tests passed out of 3

  Total Test time (real) =   0.50 sec
  -> exit 0

  $ ./policy/build/test_deck_placement
    ok   extraDeckTypeIsTheUnconditionalHalfOnly
    ok   deckBuilderPushCascadeDiffersFromTheRuleInExactlyTheRecordedFamilies
  14 tests, 0 failed, 0 assertions failed
  -> exit 0

  $ cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
  -- Generating done (0.1s)
  -- Build files have been written to: <clone>/ui/build
  -> exit 0

  $ cmake --build ui/build
  [98/99] Building CXX object tests/CMakeFiles/test_deckbuilder_screen.dir/test_deckbuilder_screen_qmltyperegistrations.cpp.o
  [99/99] Linking CXX executable tests/test_deckbuilder_screen
  ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'
  -> exit 0

  ui build:    2 ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'
  $ ctest --test-dir ui/build --output-on-failure
  Test project <clone>/ui/build
      Start 1: deckbuilder
  1/2 Test #1: deckbuilder ......................   Passed    0.41 sec
      Start 2: deckbuilder_screen
  2/2 Test #2: deckbuilder_screen ...............   Passed    1.06 sec

  100% tests passed out of 2

  Total Test time (real) =   1.47 sec
  -> exit 0

  $ ./ui/build/tests/test_deckbuilder
  PASS   : TestDeckBuilder::everyLegalityMessageSpeaksAboutTheDeckAsArrangedOnly()
  PASS   : TestDeckBuilder::cleanupTestCase()
  Totals: 35 passed, 0 failed, 0 skipped, 0 blacklisted, 198ms
  ********* Finished testing of TestDeckBuilder *********
  -> exit 0

  $ ./ui/build/tests/test_deckbuilder_screen
  PASS   : TestDeckBuilderScreen::addButtonsFollowTheControllersPlacement()
  PASS   : TestDeckBuilderScreen::cleanupTestCase()
  Totals: 22 passed, 0 failed, 0 skipped, 0 blacklisted, 974ms
  ********* Finished testing of TestDeckBuilderScreen *********
  -> exit 0
  ```

- Offscreen clean-QML-load check (CI's step, reproduced because macOS has no `timeout(1)`): start
  `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`, wait 20 s, require it to be still
  running, kill it, require empty stderr. Result: **still running after 20 s** (CI's first
  assertion holds); **stderr was not empty**: 148 bytes, one line, a Qt platform notice, not a QML
  diagnostic: `qt.qpa.fonts: Populating font family aliases took 57 ms. Replace uses of missing
  font family "Sans Serif" with one that exists to avoid this cost.` `grep -ciE
  "qml|TypeError|ReferenceError"` on that file → `0`. Rounds 019 and 020 recorded the same
  macOS-only notice; CI's Linux job runs the check verbatim (below).

### Python suite, generators, `fw.py check` (required evidence 5), at `6a52b92c`

- System Python 3.9.6: `python3 -m unittest discover -s tests -v` → **exit 1**, `Ran 133 tests`,
  `FAILED (failures=1, skipped=11)`: `test_readme_status.CommandLineTest.
  test_check_fails_on_a_stale_copy_and_update_repairs_it` (`AssertionError: 1 != 0`). The same
  failure occurs at `8f7d73b1`, the commit this round started from (checked by running
  `python3 -m unittest tests.test_readme_status` there → same one failure), so it is not caused by
  this round; I did not investigate its cause. CI's matrix is Python 3.10 and 3.12
  (`.github/workflows/edopro-next.yml`), and round 020's Verifier also used 3.13, so I did too.
- Homebrew Python 3.13.15: `python3.13 -m unittest discover -s tests -v` → exit 0,
  `Ran 133 tests in 2.112s`, `OK (skipped=11)`. The 11 skips: one Windows-ACL test
  (`test_unreadable_source_tree_fails_closed`: "Windows ACL denial is required for this
  enumeration test") and ten `test_semantic_trace` tests (`TestSemanticGoldens`: 3,
  `TestSemanticQuality`: 7) skipped with "no semantic-trace binary is present and newer than every
  client source file; configure and build client/ ..." — `client/` is not touched by this round.
- `python3.13 tools/generate_messages.py --check` → exit 0, `message table up to date (96 ids)`.
- `python3.13 tools/generate_protocol_constants.py --check` → exit 0, `protocol constants up to
  date (187 values)`.
- `python3.13 tools/generate_readme_status.py --check` → exit 0, `README status block is up to date`.
- `python3.13 tools/check_home_status.py` → exit 0, `home screen status matches docs/ROADMAP.md`.
- `python3 tools/fw.py check` → exit 0, `0 error(s), 0 warning(s)`.
- Not run: golden reproduction (`--update` then `git diff --exit-code -- tests/golden`), because
  the round changes nothing under `tools/`, `tests/` or protocol tables.

### Part 3, 4, 5: what was removed, replaced or changed (required evidence 6)

**Sentences removed from documents, and what replaced them**

- `docs/architecture/deck-placement.md`
  - §1: "The third applies the same rule with `RITUAL_LOCATION::DEFAULT`, except for two unusual
    card shapes (§3.3)." → "The third is a different piece of code: for most type-bit
    combinations it agrees with that rule under `RITUAL_LOCATION::DEFAULT`, and for six families
    of them it does not (§3.3)."
  - §3.3: "That is `is_extra_deck_card` under `RITUAL_LOCATION::DEFAULT`, row for row. It differs
    in only two shapes, both of which the push functions and the lambda treat differently:" and
    the two numbered shapes below it → the ordinary-shapes note, the order-independence
    argument, the six-family table with upstream lines and counts, the statement of what the test
    pins, and "Which is right at duel entry".
  - §3.3: "Neither shape occurs in the card databases of a local Project Ignis install checked
    for this round (no card with the Link bit and neither Monster nor Spell; no Ritual Monster
    with a Fusion, Synchro, Xyz or Link bit). That check was a read-only SQL count over data this
    project does not commit; it is not reproducible in CI and is not a claim about every
    database a user may load." → **removed, not restated**: "Whether any card has these shapes is
    not claimed. This record says nothing about which of the 156 combinations occur in any card
    database; the test covers every combination, which does not depend on the answer." plus a
    parenthesis saying the earlier statement covered two of six families and cannot be reproduced.
  - §3.2: the middle-click bullet, which listed only the search-result branch → all four
    middle-click branches; two bullets and a summary paragraph added (right-button drop from
    Side, `:735-736`; which orders exist).
  - §5.4: "See §6, "Advisory legality", ..." → "See §6, "Legality messages", ..."; added "and
    drops tokens and unknown codes (§5.1)".
  - §6: "The two shapes of §3.3 where they differ follow the lambda: ..." bullet → the six-family
    version with the pointer to ADR 0011 Decision 2.
  - §6: "Advisory legality is unchanged, and §5.4 qualifies it. ... The wording predates this
    round; changing it is outside this round's scope and is raised as an open question in the
    round report rather than changed silently." → "Legality messages describe the deck as
    arranged, not what duel entry would do." (new wording, why, and what is unchanged).
  - §6 Tests: one sentence added about the push-cascade test.
- `docs/adr/0011-extra-deck-classification.md`
  - Context: "disagree with each other on a few card shapes" → "on some card shapes".
  - Decision 2: "For every card shape the cascade and the lambda agree (deck-placement.md §3.3)
    except two, recorded here as divergences:" with its two bullets, and "Neither shape was found
    in the card databases of a local Project Ignis install (a read-only count; not reproducible in
    CI)." → the six-family table, the statement that the test pins it, the removal note, and three
    options with the argument (follow the lambda: chosen; reproduce the push functions: rejected;
    refuse to add the Side families: rejected).
  - Consequences: "Not addressed: the legality banner calls a misplaced card something that
    "would not be accepted at duel entry", although ... left for a decision outside this round."
    → "ADR 0010's wording of the advisory banner ... is superseded by Decision 5".
  - New Decision 5 (legality messages); Status paragraph extended (round 021 note; pinned-by list).
- `docs/adr/0010-...`: a "Superseded in part by ADR 0011, Decision 5" note added after the
  advisory-banner decision; nothing removed.
- `docs/architecture/deck-builder-ui.md` §14.2: `"Deck is legal for duel entry under this ruleset
  and banlist." when legal, "Would not be accepted at duel entry: <reason>" when not.` → the new
  two texts, with a sentence on why and that the old wording lasted until round 021.
- `docs/architecture/deck-builder-legality.md` §4: the example wording `("this deck would not be
  accepted at duel entry: ...")` kept as history, a "Correction (round 021)" note added after it.
- `policy/tests/test_deck_placement.cpp`: `// A Link Spell - the one real shape (deck-placement.md
  §6).` → `// Link and Spell bits, no Monster bit (deck-placement.md §2.1). This is a bit combination
  the rule must handle, not a claim that any card has it.`
- `policy/include/edopro_next/policy/deck_placement.h`: "plus the token/unknown handling that loop
  applies before the lambda is ever asked (:349-358)" → "plus the loop's token skip (:357) and a
  coarser version of its unresolved-code handling (see classify_card() ...)"; a paragraph added to
  `classify_card` listing what of the loop (`:349-364`) it reproduces (tokens: yes; unresolved
  codes: only "no classification", not the error code, not the code-0 placeholder, and not the
  id-mapping lookup `GetDummyOrMappedCardData`; the Extra and Side loops: not modelled); "the two
  cases upstream never places at all ... an unknown code has no type to classify by" → "the cases
  with no placement ... an unresolved code has no type to classify by".

**Every changed message, old and new** (`ui/src/deckbuilder/deck_controller.cpp`)

| Error | Old | New |
|---|---|---|
| MainCount, too few | Would not be accepted at duel entry: Main deck has %1 cards, fewer than the minimum of %2. | Fails as arranged: Main deck has %1 cards, fewer than the minimum of %2. |
| MainCount, too many | Would not be accepted at duel entry: Main deck has %1 cards, exceeding the maximum of %2. | Fails as arranged: Main deck has %1 cards, exceeding the maximum of %2. |
| ExtraCount, a card | Would not be accepted at duel entry: %1 belongs in the Main deck, not the Extra deck. | Fails as arranged: %1 is in a section that does not accept it (whether a card goes in the Main deck or the Extra deck follows from its type). |
| ExtraCount, too many | Would not be accepted at duel entry: Extra deck has %1 cards, exceeding the maximum of %2. | Fails as arranged: Extra deck has %1 cards, exceeding the maximum of %2. |
| ExtraCount, too few | Would not be accepted at duel entry: Extra deck has %1 cards, fewer than the minimum of %2. | Fails as arranged: Extra deck has %1 cards, fewer than the minimum of %2. |
| SideCount, too many | Would not be accepted at duel entry: Side deck has %1 cards, exceeding the maximum of %2. | Fails as arranged: Side deck has %1 cards, exceeding the maximum of %2. |
| SideCount, too few | Would not be accepted at duel entry: Side deck has %1 cards, fewer than the minimum of %2. | Fails as arranged: Side deck has %1 cards, fewer than the minimum of %2. |
| UnknownCard | Would not be accepted at duel entry: Unknown card code %1 (not found in database). | Fails as arranged: Unknown card code %1 (not found in database). |
| ForbiddenType | Would not be accepted at duel entry: Deck contains cards of a forbidden card type. | Fails as arranged: Deck contains cards of a forbidden card type. |
| TooManyLegends | Would not be accepted at duel entry: Deck exceeds the allowed number of Legend cards. | Fails as arranged: Deck exceeds the allowed number of Legend cards. |
| TooManySkills | Would not be accepted at duel entry: Deck exceeds the allowed number of Skill cards. | Fails as arranged: Deck exceeds the allowed number of Skill cards. |
| CardCount | Would not be accepted at duel entry: %1 exceeds the maximum allowed copy limit. | Fails as arranged: %1 exceeds the maximum allowed copy limit. |
| TcgOnly | Would not be accepted at duel entry: %1 is TCG-only, not allowed under this ruleset. | Fails as arranged: %1 is TCG-only, not allowed under this ruleset. |
| OcgOnly | Would not be accepted at duel entry: %1 is OCG-only, not allowed under this ruleset. | Fails as arranged: %1 is OCG-only, not allowed under this ruleset. |
| UnofficialCard | Would not be accepted at duel entry: %1 is an unofficial or custom card. | Fails as arranged: %1 is an unofficial or custom card. |
| Lflist | Would not be accepted at duel entry: %1 exceeds the banlist limitation count. | Fails as arranged: %1 exceeds the banlist limitation count. |
| None (both places it was written) | Deck is legal for duel entry under this ruleset and banlist. | Passes as arranged under this ruleset and banlist. |
| unknown enum value | Would not be accepted at duel entry: Deck is invalid. | Fails as arranged: Deck is invalid. |

Not changed: "Deck meets this ruleset's size and type limits. No banlist is selected: ...", and the
" No banlist is selected: ... not being made either way." suffix. `legalityErrorType`,
`legalityCardCode`, `isLegal` and `policy::validate_deck()` are untouched.

The argument, against §5.4: `generic_duel.cpp:421-423` and `:373-381` (quoted above) show the
server rebuilds the deck from `menu_handler.cpp:43-48`'s concatenated list by type, under the
duel's own `RITUAL_LOCATION`, and `LoadDeck` drops tokens (`deck_manager.cpp:357`) before
`CheckDeckSize`/`CheckDeckContent` run. So (a) a misplaced card is moved, not rejected, so the
placement message's claim was false; and it said "belongs in the Main deck" for an Extra Deck card
sitting in Main, which is false anywhere; (b) a section-size error computed on the editor's
arrangement is not the count the server takes; (c) any message about a deck holding a token speaks
of a card the server removes; (d) the old "legal for duel entry" holds only for a token-free deck.
Full table and options in ADR 0011 Decision 5. The wording states the check's result on the deck
as arranged and claims nothing about duel entry.

### CI (required evidence 7)

- `gh run view 35969040859` (workflow `edopro-next`, triggered by pushing the branch, head
  `6a52b92ceba2a1fdb768c9048384d4325860c392`, status completed, **conclusion success**):
  Card and deck data: success; Semantic client model: success; Qt 6 shell (Linux): success
  (this job runs the offscreen clean-QML-load check verbatim); Regression harness (3.12): success;
  Regression harness (3.10): success; Upstream EDOPro baseline: **skipped** (`edopro-next.yml`
  runs it on master, pull requests, weekly and on demand, not on a push to another branch; the
  round touches neither `gframe/` nor `integration/`). These are the five required checks.
- CI at the report commit that `fw.py report` adds on top (documentation only) is not known when
  this file is written; I checked it after pushing and say so in my final message.

## Not verified

- **Behaviour against upstream's real code.** Upstream was read, never run: nothing here loaded
  `ocgcore`, ran `DeckManager::LoadDeck`/`CheckDeckContent` or the deck builder's push functions.
  The 156-card result is a transcription of the quoted lines, cross-checked by two independent
  transcriptions of my own (scratch Python and the C++ test) written by the same person from the
  same reading; a misreading of upstream would appear in both.
- **The claim in ADR 0011 Decision 5 that a deck passing `validate_deck()` is left unchanged by the
  server's re-split** is derived by hand from the quoted callbacks and the lambda, partly pinned by
  the existing equivalence test in `test_deck_placement.cpp`; it is not run against upstream, and
  not pinned by a test of its own. Likewise "the editor counts a token": I read it from
  `policy/src/deck_validation.cpp` (`deck.main.size()`, `deck.extra.size()`, `deck.side.size()`
  in the size step); no test in this round puts a token in a deck and validates it.
- **That an opened `.ydk` keeps a token** (ADR 0011 Decision 5 relies on ADR 0004 Decision 2
  here): read, not run.
- **Whether any card in any real database has any of the 156 combinations.** Deliberately not
  claimed either way (the earlier statement was removed).
- **Qt 6.8.3** (CI's pinned version) on this machine: not available; only 6.11.1 ran. **Windows
  and Linux builds**: not run here, covered only by CI.
- **Visual rendering** of the legality banner: no screenshot was taken. The screen tests assert the
  banner's text, not its layout, and the longer placement message wraps (the QML `Text` is
  `WordWrap`); its appearance was not looked at.
- The system Python 3.9.6 failure above was not investigated.
- `data/` cycle: not run (`data/` is unchanged by this round). `gframe/` baseline and
  observer-fixture equivalence, `client/` cycle, golden reproduction: not run, since those
  directories are untouched.
- `python tools/check_pr_evidence.py`: no PR exists for this branch yet (Brain opens it), so there
  is no body to check; not run.

## Changed

- `policy/tests/test_deck_placement.cpp`: new test (transcription of `push_main`/`push_extra`,
  the six recorded families, all 512 combinations); one comment corrected.
- `policy/include/edopro_next/policy/deck_placement.h`: comments only (`classify_card` and the file
  header say precisely what of `LoadDeck`'s loop is reproduced). No code, no rule, no
  `validate_deck()` change.
- `ui/src/deckbuilder/deck_controller.cpp`: message text (table above); the two identical "legal"
  branches collapsed into one call to `formatLegalityError()`. `deck_controller.h`:
  `formatLegalityError()` moved from private to public (C++-only, for the tests) with a comment.
- `ui/tests/test_deckbuilder.cpp`, `ui/tests/test_deckbuilder_screen.cpp`: assertions moved to the
  new text; the new every-message test.
- `docs/architecture/deck-placement.md`, `docs/adr/0011-extra-deck-classification.md`,
  `docs/adr/0010-deck-builder-ruleset-and-legality-ui.md`,
  `docs/architecture/deck-builder-ui.md`, `docs/architecture/deck-builder-legality.md`: as listed
  above.
- `docs/rounds/021-placement-record-correction/builder.md`: this report.
- Not touched: `docs/state.md`, `gframe/`, `integration/`, `ocgcore/`, `client/`, `data/`,
  `.github/workflows/`, settings, framework files. No personal path or email address was added
  (checked with a search of the diff for home-directory paths and address patterns → no matches).

Commits on `builder/021-placement-record-correction` after the seat-start commit `8f7d73b1`:
`546de122` (policy test and comments), `0036b9a6` (ui messages and tests), `362143f9` (docs),
`6a52b92c` (RITUAL_LOCATION-independence check in the test), then this report.

## Open questions

1. **The docs' six-family table is not machine-tied to the test's.** The test pins the rule, the
   transcription and the family table inside the test file; the copy of that table in
   `deck-placement.md` and ADR 0011 is prose. Editing only the prose would not fail anything. A
   check that reads the document is possible but was not asked for and would be a new kind of test;
   Brain's call.
2. **I changed the "legal" message too**, although Part 4 speaks of messages "for the error it
   reports": the Goal says every message about duel entry must be true, and "legal for duel
   entry" is false for a deck with a token (Decision 5). If the owner would rather keep the
   positive wording and accept the token case, that one string and its test are the only change
   to undo. I left "Deck meets this ruleset's size and type limits" alone because it does not
   speak about duel entry, though "as arranged" holds for it as well.
3. **The placement message no longer says which section the card belongs in**, because the
   validation error does not say which section failed and for the Ritual hybrid neither accepts it.
   Saying more would need `validate_deck()`'s result to carry the section: a change to the legality
   computation, which this round was told not to make.
4. **`docs/state.md`** (Brain-owned, out of scope): the round 020 Verifier noted it still lists
   automatic Main/Extra classification as open (lines 22 and 89 at that time). I did not re-check
   or touch it.
5. **Behaviour was not changed.** The brief allowed following upstream if the argument concluded
   so; it concluded the opposite (ADR 0011 Decision 2, option 1), so no stop-and-report was needed.
   If Brain or the owner reads the argument differently, that is a new decision.
6. **Ritual hybrids at duel entry**: families B2, C1 and C2 (60 of the 156 combinations) are cards
   `LoadDeck` calls Extra but `CheckDeckContent`'s Extra callback rejects without
   `DUEL_EXTRA_DECK_RITUAL`; the editor's message for such a card is the generic placement one.
   This is upstream disagreeing with itself (already recorded in §4), not something this round
   resolves.

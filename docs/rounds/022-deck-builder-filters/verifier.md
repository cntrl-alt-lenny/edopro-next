<!-- fw-report
round: 022-deck-builder-filters
role: verifier
branch: verifier/022-deck-builder-filters
head: 01ed1eea6821f81b6f40a7dc34b39605d39d17ea
os: macOS 27.0
python: 3.9.6
written: 2026-09-27T17:54:21Z
-->
Reviewed commit: `01ed1eea6821f81b6f40a7dc34b39605d39d17ea` (`origin/builder/022-deck-builder-filters`, the commit `fw.py start` named). Machine: macOS 27.0 (arm64), Apple clang, cmake 4.4.3, ninja 1.13.2, Qt 6.11.1 (CI pins Qt 6.8.3 on Linux; not run here), in a fresh clone of my own.

**Seat start.**
- `python3 tools/fw.py start --role verifier --round 022-deck-builder-filters` → exit 0: `seat ok: verifier, round 022-deck-builder-filters, branch verifier/022-deck-builder-filters at 01ed1eea6821` / `reviewing exactly 01ed1eea6821f81b6f40a7dc34b39605d39d17ea from origin/builder/022-deck-builder-filters`.
- `git submodule update --init` → exit 0: `Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'` / `Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'`.
- `git submodule status` → exit 0: ` 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)`.

I read `builder.md` only after everything under "Commands I ran myself" and "Pass one" below was done.

**Commands I ran myself.**
- `data/`, `policy/`: `cmake -S <m> -B <m>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON` → exit 0; `cmake --build <m>/build` → exit 0; `ctest --test-dir <m>/build --output-on-failure` → exit 0 (both).
- `ui/`: the same with `-DEDOPRO_NEXT_UI_TESTS=ON` → configure 0, build 0, ctest 0 (`deckbuilder`, `deckbuilder_screen`).
- Offscreen clean-QML-load (CI's step, emulated; macOS has no `timeout`): `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell` still running after 20 s; stderr one line, the macOS `qt.qpa.fonts ... "Sans Serif"` notice, no QML diagnostic.
- Upstream comparison harness of my own (scratchpad, not committed; details below) → exit 0, 0 mismatches.
- Mutation runs against the committed tests (details below); `git status --short` empty afterwards.
- `bench_card_search` at the merge base `a1306f93` and at the reviewed commit, same machine, same Debug configure, three alternating runs each.
- Captures with `--capture` at 1280×800 and 960×600 against a synthetic 10-card `.cdb` and three-entry banlist built outside the repository; the same 960×600 capture at the merge base.
- Python 3.13.15: `tools/generate_messages.py --check` → 0; `tools/generate_protocol_constants.py --check` → 0; `tools/generate_readme_status.py --check` → 0; `tools/check_home_status.py` → 0; `-m unittest discover -s tests -v` → 0, `Ran 133 tests`, `OK (skipped=11)` (one Windows-ACL test, ten semantic-trace tests needing a built `client/`). `python3 tools/fw.py check` → 0, `0 error(s), 0 warning(s)`.
- `gh run list --branch builder/022-deck-builder-filters` → `edopro-next` completed/success at `01ed1eea` (run 36334725407) and at `da45ceee` (run 36334551460).

## Findings

### Pass one: upstream re-read, and the acceptance criteria

**What I read, at file and line** (the clone's `gframe/`, the upstream base in `docs/UPSTREAM.md`):
- `deck_con.cpp:21-51` `parse_filter`: `=` or a digit → type 1; `>=` 2; `>` 3; `<=` 4; `<` 5; `?` 6 (returns 0); anything else type 0. `bufferio.h:240-249` `GetVal`: digits accumulate in `uint32_t`, and `if(*pstr == 0) return ret; return 0;`, so trailing text makes the number 0, not "no filter".
- `deck_con.h:115-122`: `filter_atk`/`filter_def` are `int32_t`, `filter_lv`/`filter_scl` are `uint32_t`; `data_manager.h`: `attack`/`defense` `int32_t`, `level`/`lscale` `uint32_t`.
- `deck_con.cpp:1202-1227`: ATK/DEF types 4 and 5 add `|| attack < 0`; type 6 is `!= -2`; DEF adds `|| (type & TYPE_LINK)`; Level and Scale reject everything on type 6 (`|| filter_lvtype == 6`, `|| filter_scltype == 6`); Scale reads `lscale` only and adds `|| !(type & TYPE_PENDULUM)`.
- `data_manager.cpp:137-153`: a Link card's DEF becomes its marker mask and `defense = 0`; `level < 0` stores `-(level & 0xff)` into a `uint32_t`; `lscale = (level >> 24) & 0xff`.
- `deck_con.cpp:1192-1193` (tokens, `SCOPE_HIDDEN`, non-official unless `chkAnime` or whitelist), `:1254-1321` (the limit switch and whitelist drop), `deck_manager.h:23-30` (`GetLimitationIterator`), `game.cpp:661-743` and `:3350-3462` (controls and their item data), `deck_con.cpp:1040-1058` (`StartFilter`), `:338-353`, `:446-616` (event handling), `:1363-1396` (`ClearSearch`/`ClearFilter`), `game_config.inl:73` (`chkAnime`, alias `show_unofficial`, default `false`).

**Each numeric input form, as upstream behaves (my reading) and what this project does.** `n` is `GetVal` of the text after the prefix.

| Form | ATK / DEF (upstream) | Level (upstream) | Scale (upstream) | This project |
|---|---|---|---|---|
| `=n`, `n` | `== n` (n as `int32`) | `== n` (unsigned) | `lscale == n`, Pendulum only | `EqualTo n` |
| `>=n` | `>= n`; a "?" stat (-2) is dropped by ordinary comparison | `>= n` | `>= n`, Pendulum only | `AtLeast n` |
| `>n` | `> n` | `> n` | `> n`, Pendulum only | `AtLeast n+1` (in `int64`, so no overflow at `INT32_MAX`/`UINT32_MAX`) |
| `<=n` | `<= n` **and** `>= 0` | `<= n` | `<= n`, Pendulum only | `AtMost n`, `excludes_negative` on ATK/DEF |
| `<n` | `< n` **and** `>= 0` | `< n` | `< n`, Pendulum only | `AtMost n-1`, `excludes_negative` on ATK/DEF |
| `?` (and `?…`) | `== -2` (not -1) | nothing | nothing | `EqualTo -2`; Level/Scale `EqualTo -1`, which no widened unsigned value equals |
| digits then text (`1500a`), `=`/`>`/`<` alone | the prefix's comparison with 0 | same | same | same |
| other text (empty, space, `-`, letters, non-ASCII digits) | no filter | no filter | no filter | `nullopt` |
| DEF, any form | Link cards always dropped (`:1212`) | | | pre-existing `CardSearchIndex` check (`kTypeLinkBit`) |

**My independent check.** I wrote a C++ harness of my own that builds a synthetic `.cdb` of 5,200 monsters (Normal, Pendulum, Link, Pendulum+Link × ten ATK × ten DEF values including -2, -1, 0, `INT32_MAX`, `INT32_MIN` × thirteen packed levels including negative ones and asymmetric scales). It loads the file through the project's real `CardDatabase` and `CardSearchIndex`, and compares, for each of the four boxes, the cards kept by `parse_numeric_filter` + `search()` with the cards kept by my transcription of `data_manager.cpp:137-153`, `parse_filter`, `GetVal` and `:1202-1227`. The inputs were 4,572: 72 chosen (every prefix, `?5`, `=?`, `-1`, `" 5"`, `1500a`, `=4294967294`, `>4294967295`, `<=4294967041`, `>>5`, `=>5`, Arabic-Indic and full-width digits, …), 4,000 random strings over `0-9 < > = ? - a space`, and 500 random large numbers with random prefixes. **Result: 18,288 field/input checks, 0 mismatches.** I confirmed the harness can fail: dropping `|| a < 0` from my own `<=` model gives 206 mismatches.

**Acceptance criteria.**
1. *Upstream behaviour recorded with citations* — **met.** `card-search.md` §2.2 (every control, the input-form table) and §2.3 (hidden cards, the limit list) state what I read above, line for line; I found no statement there that the source contradicts.
2. *Every numeric form tested or recorded; §2.1 no longer untrue* — **met.** `test_numeric_filter_text.cpp` compares every form on all four boxes against a transcription, and my own harness agrees over a far larger input set. §2.1 now says the old "deliberate simplification" was wrong and why; the `type` row records `type_equals`.
3. *Every control with a `SearchQuery` equivalent is driven by a `ui/` test* — **met, with finding 2.** `searchFiltersDriveEveryUpstreamControl` and the screen test drive card type, sub-type, attribute, race, the four boxes, categories, markers, limit and the switch. The race filter is exact (`e.race != *query.race`, `:1198`); Spell/Trap sub-types are whole-word (`type_equals`, `:1233`, `:1240`); Monster sub-type all-bits (`:1196`); markers all-of, categories any-of (`:1250-1252`).
4. *Parts 4 and 5 implemented as upstream or recorded* — **met.** `policy::deck_search_admits` reproduces `:1192-1193` and `:1254-1321` as I read them, including the whitelist `count = -1`, the exact-`ot` categories versus the bit categories, and `limitation_for` = `GetLimitationIterator` (code first, alias fallback gated by whitelist/artwork range). The cap of 200 is applied after the policy filter (`search_results_model.cpp`), so a hidden card never takes a place. ADR 0012 Decisions 3 and 4 record the "No banlist" handling and the unsaved switch.
5. *Tests can fail* — **met.** My mutations, each on a clean tree, rebuilt and run with `ctest`:
   - comparison: `excludes_negative` ignored → `numeric_filter_text` fails; `AtLeast` made strict → fails; `type_equals` ignored → `card_search` fails (`type_equals_matches_the_whole_type_word_not_a_subset`);
   - parser: strict `>` not shifted → fails; trailing text keeps the digits → fails; `?` on Level/Scale becomes no filter → fails; `?` on ATK becomes -1 → fails; ATK read unsigned → fails;
   - policy: Unlimited admits Limited (`count < 1`) → `deck_search_filter` fails; whitelist drop removed → fails; Anime by bit instead of exact `ot` → fails; tokens shown → fails;
   - ui: Spell sub-type as all-bits → `deckbuilder` fails; DEF kept for the Link sub-type → both ui tests fail; Level box wired into the ATK query → both fail; link markers as any-of → `deckbuilder` fails.
   - Survived: Scale box wired to `right_scale` (finding 2); Level box parsed with the ATK field kind (finding 4).
6. *Records true; README check passes* — **met, with finding 3.** ROADMAP's M3 "still missing" text and `docs/capabilities.md` describe what exists; the M3 box stays unchecked; ADR 0011 and `deck-placement.md` carry supersession notes for the token change; `generate_readme_status.py --check` → 0.

**Invariants.** No matching in `ui/qml/`: the QML diff sets indexes and text and renders labels; the only comparisons in it are `enabled ? … : …`, `checked ? … : …` and the marker grid's `modelData >= 0` for the empty centre cell. `data/` gains no legality, banlist or Qt reference (searched the added lines). `gframe/`, `integration/`, `ocgcore/`, `client/`, `.github/`, `tools/`, `docs/agents/` and `docs/state.md` are untouched. No `.cdb`, `.conf`, image or Lua file is added. No existing test expectation changed: the only removed test lines are the ui fixture helper's `INSERT`, extended to write race, attribute and category. **Benchmark**, merge base vs reviewed commit, ms/query over three alternating runs (Debug configure, so absolute numbers are higher than a Release build's): filtered (type+ATK+level) 8.75/10.12/8.89 vs 9.39/9.53/9.04; exact-name 17.2/15.2/14.2 vs 19.5/15.2/15.3; broad text 16.1/16.6/16.0 vs 17.7/18.0/15.5. No change beyond run-to-run noise, the same conclusion as the builder's Release numbers.

### Findings

1. **[SHOULD FIX]** `ui/qml/screens/DeckBuilderScreen.qml` (the filter grid) — at the 960×600 minimum window size that `deck-builder-ui.md` §13.1 documents, the search column no longer fits. With the filters shown, as they are by default, the search box, the Clear button, the right-hand column of controls (Sub-type, Type/race, DEF, Scale), the Limit list and "Link markers…" run past the column divider into the deck column. Clear touches the "Ruleset" label, and the result list shrinks to about one visible row. At the merge base `a1306f93` the same capture fits inside its column, so this round introduced it. At 1280×800 nothing overlaps. How it fails: on a small window, controls in two columns overlap and the search results are barely usable until the user presses "Hide filters". The builder's report says the 960×600 minimum was not looked at, so this is new information, not a contradiction.
2. **[SHOULD FIX]** `ui/tests/test_deckbuilder.cpp` (fixture around line 1359, Scale checks around lines 1465 and 1601) — the only Pendulum card has equal scales (`(5 << 24) | (5 << 16)`), so the adapter test cannot tell the left scale from the right. I changed `search_filters.cpp` to put the Scale box into `query.right_scale` and every `ui/` test still passed. Upstream's box reads `lscale` only (`deck_con.cpp:1222-1224`), and the code and §2.1 both say so. That is correct today, but nothing in `ui/` would catch it being wrong. One Pendulum fixture with different left and right scales would pin it. (`data/`'s numeric test does cover `left_scale` itself.)
3. **[NOTE]** `docs/adr/0012-deck-builder-search-filters.md`, Decision 6, "When the search runs" — it says upstream runs the search "on the category and marker OK buttons". `BUTTON_CATEGORY_OK` (`deck_con.cpp:346-353`) only sets `filter_effect` and hides the window; it does not call `StartFilter`. Only the marker OK does (`:465`). The same sentence also leaves out that Level and Scale re-run the search on every change (`:498-501`) and the name box once longer than two characters (`:494`). No result depends on it; the ADR's conclusion (the same choices find the same cards) holds.
4. **[NOTE]** `ui/src/deckbuilder/search_filters.cpp` (`buildSearchQuery`) — parsing the Level box with `NumericFilterField::Attack` instead of `Level` survives every test. This is almost an equivalent mutant: the two differ only for numbers of 2³¹ or more, where the signed read goes negative and misses a wrapped negative-level card (upstream stores level -255 as 4294967041). `data/`'s own test pins `parse_numeric_filter` per field, so the risk is only in the wiring.
5. **[NOTE]** `docs/capabilities.md`, "Windows / macOS builds" row, still says `ui/` on macOS "has no recorded evidence, because the machine had no Qt". This round's builder and I both built and tested `ui/` on macOS with Qt 6.11.1. The row predates this round, so it is not this round's defect, but it is now stale.
6. **[NOTE]** `python3 -m unittest discover -s tests` under the system Python 3.9.6 fails one test (`test_readme_status.CommandLineTest.test_check_fails_on_a_stale_copy_and_update_repairs_it`). As found in round 021, `tools/generate_readme_status.py:283` uses `Path.write_text(..., newline=...)`, which needs Python 3.10. The builder reports the failure and says it did not investigate it. Unrelated to this round; 3.13 passes and CI runs 3.10/3.12.

No BLOCKER.

### Pass two: `builder.md`

Agreements, reproduced by me:
- the three findings of Part 2: "at most" drops "?" stats, "?" is -2 only and nothing on Level/Scale, trailing text gives 0;
- the mutation results, including their C1/C2/P1/P2 and policy count mutations failing the tests they name;
- the module cycles passing;
- the offscreen stderr notice;
- the Python totals and skips, the generator checks and `fw.py check`;
- CI success at `da45ceee`, and at `01ed1eea` as well;
- a benchmark with no change beyond noise;
- the 1280×800 capture: filter grid visible, inapplicable controls disabled, nothing overlapping at that size;
- "no personal path or email address added": I searched the round's added lines for home-folder, temp-folder, drive and address patterns and found none.

No UNPROVEN CLAIM: every claim I tried, I reproduced. I did not rerun their exact 14-card capture or their Release-build benchmark, but my own runs of both lead to the same conclusions. Their "Not checked visually" list names the 960×600 minimum, which is where finding 1 is.

## Not verified

- **Upstream was read, not run.** My harness, the committed tests and the implementation are three readings of the same source. A misreading we all share would pass all three. I did not run upstream's deck builder.
- **Irrlicht combo-box behaviour** after upstream clears and refills the limit list: not read by me either (ADR 0012 records this project's behaviour only).
- **Labels**: upstream's wording lives in `strings.conf`, which is not in the repository; not compared.
- **Visual**: I looked at the default filter state at 1280×800 and 960×600 only. I did not see the Effects or Link-marker popups open, any control in a non-default state, the "(active)" label, the anime/whitelist states, keyboard Tab order in a real session, or Qt 6.8.3 rendering. The captures show only the first rows of results, so which cards are hidden (the token, the anime card) was checked by the tests, not by eye.
- **The per-result policy path** after `CardSearchIndex` (a `CardDatabase::find` plus `deck_search_admits` per result) is not in the benchmark; I did not time it.
- **Linux, Windows, Qt 6.8.3**: CI's only. The golden reproduction and the `client/` and `gframe/` baseline were not run (untouched). `tools/check_pr_evidence.py` was not run: there is no PR body yet.

## Verdict

I believe the search semantics in this round are right. Every numeric input form upstream's four number boxes accept gives the same cards here, which I established with my own transcription over 4,572 inputs and 5,200 synthetic cards, through the real loader and index, with zero mismatches. The Spell/Trap sub-type, race, category and marker operators match the source. The banlist-dependent half (hidden cards, the whitelist, the 17 limit filters) sits in `policy/` and matches `:1192-1193` and `:1254-1321`. The QML decides nothing, `data/` gained no legality, and the tests fail for every wrong comparison and parser I tried except two nearly-harmless ui wiring cases. What is wrong is presentation and test coverage, not behaviour: the filter grid overflows its column at the documented 960×600 minimum (finding 1, introduced by this round), and the ui test cannot tell the left scale from the right (finding 2). ADR 0012 misstates one upstream trigger detail (finding 3). None of these changes which cards a search returns. I am highly confident in the matching, moderately confident in the parts I checked only by reading (keyboard reach, popups, Irrlicht combo behaviour), and this review applies to `01ed1eea` only.

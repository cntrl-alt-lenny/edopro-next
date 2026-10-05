# 024-scroll-focus-visibility: keyboard focus stays visible in search filters

Tier: 2
Mode: implementation
Supersedes: 023-filter-layout-correction. Brain rejected its delivery because
the Effects popup lets keyboard focus enter clipped category rows, violating
the brief's requirement that every filter remain usable by keyboard.

## Goal

Keyboard users can see and operate each enabled search-filter control,
including every Effects category, at the shell's minimum and default sizes.
Committed tests detect focus moving into invisible controls. Documentation
distinguishes demonstrated behavior from focus capability and platform limits.

## Context

This brief starts from round 023's complete delivery, including its Verifier
report, at `1a438e7fa79b51ae0e7e63ec91424bfccbf20eaf`. Rounds 022 and 023
remain unmerged; their changes are inherited and would land with this correction.
Keep their reports as historical evidence, rather than rewriting old reviews.

Read round 023's brief and Verifier report, `DeckBuilderScreen.qml`'s filter
grid and both popups, `ui/tests/test_deckbuilder_screen.cpp`, and
`docs/architecture/deck-builder-ui.md` sections 15 and 15.1. The search
matching semantics and the already-corrected numeric wiring are not reopened.

Brain adjudication of round 023, reviewed production commit
`f4b9493626b93b387175418dc31d6f9bb06fbb25`:

- **Blocker reproduced.** The main-grid reveal function walks parents until
  `filterGrid` and returns for non-descendants (`DeckBuilderScreen.qml:546-550`).
  Effects has a separate clipped `ScrollView` at lines 724-750 and no
  equivalent focus-visibility behavior. Brain read the disposable probe,
  compared its production QML byte-for-byte with the reviewed QML
  (`diff -q`, exit 0), rebuilt its screen-test target (exit 0, `ninja: no work
  to do.`), and reran `QT_QPA_PLATFORM=offscreen
  <scratch>/ui/build/tests/test_deckbuilder_screen verifierProbe`. Exit 1:
  actual Tab events reached `categoryCheck31` at scene y=755..795, below the
  popup's y=44..404 bounds. The visibility assertion failed. Brain also
  inspected the generated image: only Categories 1..12 were visible while
  Category 32 held focus. This reproduces the minimum-size failure; Brain
  did not independently repeat the default-size probe or native traversal.
- **Hint wording correction warranted.** Brain inspected the full-shell
  1280x800 capture. ATK/DEF hints are elided, contrary to section 15.1's
  statement that the width fits the whole hint. Editing and column containment
  remain supported; do not turn this wording correction into a layout redesign.
- **Keyboard evidence needs qualification.** Brain read the committed tests:
  `filterControlsReachTheSearchModel` checks `Qt.TabFocus`, while the layout
  test forces focus onto two controls. Neither proves actual Tab traversal.
  The Verifier reports native macOS skipping non-text controls; Brain has not
  reproduced that native observation or established its cause. Record it as
  an attributed, unresolved limitation, not a universal platform conclusion.
- The five deterministic CI jobs passed on the reviewed commit and on the
  Verifier report commit. Green CI does not satisfy the missing usability
  criterion. No merge is accepted or authorized by this brief.

## Scope and non-goals

In scope:

- `ui/qml/` for focus visibility on scrollable search-filter surfaces;
- `ui/tests/` for regression coverage using the real screen;
- `docs/architecture/deck-builder-ui.md` for the correction and evidence limits.

Out of scope:

- search matching, filter choices or operators, deck legality and numeric parsing;
- `data/`, `policy/`, `client/`, `gframe/`, `integration/`, `ocgcore/`;
- general keyboard/controller parity, platform navigation policy changes,
  changing system keyboard preferences, new dependencies or visual redesign;
- workflows, framework files, repository settings and `docs/state.md`;
- rewriting earlier round reports.

## Invariants

- All project invariants in `AGENTS.md` and round 022's brief remain in force.
- The UI renders choices and sends responses; it implements no matching or
  game rule. The same choices must return the same cards.
- Fix the failure class across scrollable filter surfaces, not only Category 32
  (`AGENTS.md`, Evidence). Preserve the working main-grid behavior.
- Existing test expectations must not be silently relaxed. List any changes
  with their before/after and reason.
- No personal paths, email addresses, real card databases, artwork or scripts
  in tracked documents or assets.

## Acceptance criteria

1. At full-shell sizes 960x600 and 1280x800, with synthetic data loaded and
   filters shown, controls stay in their columns and more than one search
   result row remains visible. The scrolled main filters and both open popups
   are usable; inspect captures yourself and distinguish full-shell captures
   from screen-only harness captures.
2. Tests on the actual screen send Tab and Shift+Tab through enabled main
   filters and every Effects category. Each reached filter control is fully
   visible inside the applicable clipping viewport after layout settles.
   Prove all expected controls were reached; a loop that misses a control
   must fail. Initial focus placement may be explicit, but forcing focus onto
   each destination is not traversal evidence. Exercise both requested sizes.
3. Demonstrate keyboard operation of a revealed lower category and the
   popups' close/reopen behavior without hidden active controls. Keep the
   Link-marker popup usable by keyboard as well. Inspect every scrollable
   search-filter surface and state which is covered by which mechanism.
4. The regression fails against round 023's production QML, and fails when
   focus visibility is deliberately disabled. Show literal failing output,
   restore the implementation, and show the restored passing result.
5. Section 15 accurately describes focus capability, key-event coverage,
   visual checks and native platform limitations. Section 15.1 no longer
   says the ATK/DEF hint fits when it is elided. No claim of full keyboard
   parity or unchanged duel behavior is introduced.
6. The required checks pass and CI is green at the delivered commit. A
   platform-specific smoke diagnostic is reported literally, separately from
   the strict Linux CI assertion; it is not relabeled as empty stderr.

## Required evidence

1. Real output and exits for seat start, then `git submodule update --init`
   and `git submodule status`; operating system, compiler, Qt and Python
   versions from commands. Use Python 3.10 or newer for the suite: the known
   system-Python 3.9 `Path.write_text(newline=...)` failure is not this round.
2. UI cycle with output and exits:
   - `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON`
   - `cmake --build ui/build --parallel`
   - `ctest --test-dir ui/build --output-on-failure`
   - the offscreen survival and empty-stderr assertions from
     `.github/workflows/edopro-next.yml`, with any platform adaptation stated.
3. Capture commands, measured image dimensions, what was visually checked
   and what was not. Keep synthetic databases and captures uncommitted.
4. Actual forward/reverse key-event commands and output, reached-control
   accounting, clipping-viewport bounds, popup operation results, and the
   original-QML and disabled-visibility failing checks. Disposable copies
   are allowed for diagnostic mutations; never commit a mutation.
5. With the supported Python executable, report actual commands and exits:
   - `python3 tools/generate_messages.py --check`
   - `python3 tools/generate_protocol_constants.py --check`
   - `python3 tools/generate_readme_status.py --check`
   - `python3 -m unittest discover -s tests -v` (each skip named)
   - `python3 tests/test_replay_trace.py --update`
   - `git diff --exit-code -- tests/golden`
   - `python3 tools/fw.py check`
   - `git diff --check`
6. Every removed documentation sentence and its replacement; every changed
   test expectation. Separate observed native behavior, offscreen key-event
   behavior and source-only inferences. Native parity is not asserted if it
   cannot be established in the seat's environment.
7. CI conclusions at the literal final commit, with report-commit results
   marked unknown until observed. The final Verifier also reruns the standalone
   `data/` and `policy/` WERROR configure/build/CTest cycles from `AGENTS.md`
   against the combined delivery, which inherits changes in both modules.

Finish with the round's seat report and `fw.py report --role <role> --round
024-scroll-focus-visibility --push`, including on an early stop. Neither
Builder nor Verifier accepts or merges this work.

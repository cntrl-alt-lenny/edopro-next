# 026-native-filter-navigation: establish native macOS filter navigation

Tier: 2
Mode: investigation
Supersedes: none

## Goal

Establish whether the reported Cocoa Tab-navigation gap affects the real deck
builder, identify the supported cause or clearly bound what remains unknown,
and recommend the smallest justified follow-up. Distinguish application
behavior from test-harness behavior and platform focus policy. This round
produces evidence and a recommendation, not a shipped navigation-policy fix.

## Context

Starting master: `812f5a38214b377fa09efac9a556334cac54cf3b`.
The owner selected investigation of native macOS keyboard navigation after
round 025's framework adoption. No other round is in flight at briefing.
This is Tier 2 because automated offscreen checks cannot establish native
behavior or the external toolkit/platform facts that explain it.

Read `docs/architecture/deck-builder-ui.md` sections 11, 12 and 15.2;
`ui/src/main.cpp`; `ui/qml/Main.qml`; the main filters and both popups in
`ui/qml/screens/DeckBuilderScreen.qml`; `ui/tests/TestHarness.qml`;
and `filterTabTraversalStaysVisible` with its helpers in
`ui/tests/test_deckbuilder_screen.cpp`. Read `ui/CMakeLists.txt` and
`ui/tests/CMakeLists.txt` only as needed to run/build diagnostic probes.

Historical evidence, not an assumption about this session:

- Round 024's actual-screen offscreen test passed forward/reverse traversal
  at both shell-equivalent sizes, with strict reached-control accounting and
  bounds checks for main filters, Effects categories and Link markers.
- Its Cocoa minimum-forward run reached Search and numeric fields but missed
  non-text controls. The main-control assertion failed before popup testing.
  That does not establish how the native popup behaves independently.
- Round 024's successful Verifier report identifies the native limitation
  but explicitly leaves its cause and physical input parity unverified.

Builder may consult round 024's reports for reproduction details. Verifier
must finish its own first pass before opening round 026's Builder report;
historical reports remain attributed evidence, not independent confirmation.

For toolkit/platform claims, inspect version-matched primary Qt source or
official documentation and targeted system-policy reads. Record exact source
revision, file/line or official URL and a short quotation. Do not infer the
effective runtime policy merely from a focus-capability bit or OS defaults.

## Scope and non-goals

In scope:

- A native macOS reproduction on the unchanged production UI and test.
- Disposable diagnostic probes separating screen harness, full-shell
  integration, effective runtime focus policy and native input delivery.
- Reusable diagnostic/test support under `ui/tests/`, if needed; label it
  diagnostic and preserve the existing offscreen assertions.
- A research record in this round's `attachments/`, and a source-grounded
  update to `docs/architecture/deck-builder-ui.md` describing the findings,
  evidence limits and recommendation.

Out of scope:

- Production navigation-policy changes, product fixes or broad input parity.
- Changing system keyboard preferences, accessibility permissions or any
  global/user setting. Read only settings directly relevant to the question.
- A redesign, controller input, Windows implementation or new CI jobs.
- Search semantics, matching, legality, card data, real databases/artwork,
  `client/`, `data/`, `policy/`, `gframe/`, `integration/` and `ocgcore/`.
- Qt patches, new dependencies, framework changes, repository settings,
  `docs/state.md`, milestone completion claims and unrelated presentation nits.

A diagnostic-only, process-local policy comparison is allowed if supported
by the inspected toolkit API/source. Keep it in a disposable probe or clearly
separate diagnostic mode. It is not the application's shipped default, and
its success cannot be reported as fixing production behavior. If a product
policy decision is necessary, recommend it for a later brief; do not make it
in this investigation.

## Invariants

- Follow `AGENTS.md`: engine authority, presentation-only input behavior,
  no new dependency, owner-controlled merge and independent exact-SHA review.
- Run in the isolated seat checkout, then initialise `ocgcore` immediately
  after seat start. Do not operate in Brain's primary checkout.
- Keep synthetic databases, screenshots and private environment paths out
  of tracked files. Committed text/diagnostic sources must contain no home
  paths, emails, card scripts or card artwork. Redact unrelated desktop data.
- Preserve strict reached-set, visibility and popup-operation expectations.
  A missing native control remains a demonstrated failure, not a passing
  test after changing its expected set, skipping it or forcing focus onto it.
- Explicit initial focus or popup seeding is permitted for isolated surface
  probes, but label it. Subsequent destinations must be reached by key
  events. Seeded popup success does not prove end-to-end reachability.
- Distinguish offscreen key injection, Cocoa QTest events, desktop-delivered
  native events and physical hardware input. Do not call one another.
- When comparing disposable instrumented shell/probe copies, establish which
  production bytes are unchanged and list every instrumentation change.
- A programmatic native test is not a native user-session check merely
  because its QPA plugin is Cocoa. Screenshots alone are not traversal proof.
- Report unavailable capabilities plainly; do not make the owner run a
  technical reproduction or silently substitute offscreen evidence.

## Acceptance criteria

1. Reproduce or accurately contradict the historical observation at the
   stated starting commit, recording native/offscreen platform, Qt version,
   application style, window activation and effective focus-policy state.
   Run forward/reverse at both requested sizes; report each row independently.
2. Establish the actual shell's default behavior at 960x600 and 1280x800
   using synthetic cards and a native window. Record how Tab/Shift+Tab and
   operation keys were delivered, the focus sequence, all expected/reached
   controls and visible focus. Distinguish the harness's 896x600/1064x800
   screen areas from full-shell dimensions.
3. Inspect all three surfaces: main filters, Effects categories plus Done,
   and Link markers plus Done. For a blocked main path, use separately
   labelled initial popup seeds to investigate popup behavior without
   disguising the main failure. Where reachable, operate a lower Effects
   category and close/reopen both popups, checking visible/restored focus.
4. The explanation differentiates OS/toolkit policy, QML focus capability,
   activation/input delivery and harness integration. Support a causal claim
   with a controlled comparison and primary source; a correlation or API
   read alone is not enough. If the cause cannot be isolated without a
   forbidden setting change, state the competing explanations and stop that
   line of work rather than declare a cause.
5. A diagnostic can fail: missing-control accounting and clipping checks
   remain live, and the report shows a literal failing result or a disposable
   fault injection followed by restoration. Demonstrate that the relevant
   probe detects its claimed condition; a list of tried cases is not coverage.
6. The record gives a clear, bounded next-step recommendation, including
   compatibility implications if it would intentionally override platform
   policy. It states what remains unverified and does not claim full native,
   keyboard/controller or duel equivalence. An unavailable native session is
   a reported evidence gap, not successful completion of native criteria.
7. Required checks pass for any committed diagnostic support and documentation;
   no production behavior changes. Verifier independently reproduces the
   load-bearing claim at the delivered SHA before reading Builder's report.
   If its native environment is unavailable, its report leaves native claims
   unproven instead of accepting the Builder's narrative.

## Required evidence

Use a native macOS session for the native investigation. A seat lacking one
may establish source facts and report the precise blocked native gate, but
must not substitute Linux/offscreen success for native evidence. Use Python
3.10 or newer for the suite; identify any alternate supported interpreter
without recording its personal path. Keep commands, relevant literal output
and exits in the seat report, or link committed text attachments relatively.

1. Start with `python3 tools/fw.py start --role <role> --round
   026-native-filter-navigation`; immediately run `git submodule update --init`
   and `git submodule status`. Record `git rev-parse HEAD`, OS, compiler,
   Python and Qt versions from commands. Stop if seat start fails.
2. Configure/build/test the UI, even when production bytes are unchanged:
   - `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON`
   - `cmake --build ui/build --parallel`
   - `ctest --test-dir ui/build --output-on-failure`
3. Run each of the existing four `filterTabTraversalStaysVisible` data rows
   separately with `QT_QPA_PLATFORM=offscreen` and `QT_QPA_PLATFORM=cocoa`:
   `ui/build/tests/test_deckbuilder_screen filterTabTraversalStaysVisible:<row>`.
   Rows: `minimum-forward`, `minimum-reverse`, `default-forward`,
   `default-reverse`. Preserve literal native failures and missing names.
4. Native shell entry point:
   `QT_QPA_PLATFORM=cocoa ui/build/edopro_next_shell --card-db <synthetic.cdb> --start-screen decks`.
   Record synthetic-data generation, native resizing/activation commands,
   input delivery, focus logs, targeted runtime-policy reads and capture
   commands. Inspect captures yourself, record dimensions and state what
   visual checks were and were not made. Keep images/databases uncommitted.
5. Record exact commands/source revisions for diagnostic comparisons, what
   changed between them, and the failure/restoration evidence. Retain
   rerunnable diagnostic sources/instructions without adding a production
   configuration switch or silently modifying the baseline during comparison.
6. Run the offscreen survival and empty-stderr assertions from
   `.github/workflows/edopro-next.yml`. State any macOS timeout adaptation;
   report each assertion separately. Preserve platform font diagnostics
   literally; a collecting wrapper's exit 0 does not make empty stderr pass.
7. Run and record, using supported Python:
   - `python3 tools/generate_messages.py --check`
   - `python3 tools/generate_protocol_constants.py --check`
   - `python3 tools/generate_readme_status.py --check`
   - `python3 -m unittest discover -s tests -v` (name each skip)
   - `python3 tests/test_replay_trace.py --update`
   - `git diff --exit-code -- tests/golden`
   - `python3 tools/fw.py check`
   - `git diff --check` and `git diff --check origin/master...HEAD`
8. List every changed test expectation and documentation sentence. Check CI
   at the literal final delivered commit; mark report-only SHA checks unknown
   until observed. Run `python3 tools/check_pr_evidence.py` before opening or
   updating a PR; its body uses rerunnable commands rather than measured figures.
9. Finish on every exit with this round's seat report and `python3 tools/fw.py
   report --role <role> --round 026-native-filter-navigation --push`. Supporting
   text/sources belong in `attachments/`; neither Builder nor Verifier accepts
   or merges the work.

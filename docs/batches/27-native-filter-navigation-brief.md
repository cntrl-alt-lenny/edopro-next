# EDOPro Next · batch 27-native-filter-navigation · Builder

Normal: UI navigation, then Brain reviews the exact delivery; no automatic
Verifier. Work alongside batch 28; neither its delivery nor the planning PR
is a prerequisite. Read `docs/planning/parallel-workers.md` from the dispatch
branch if unmerged.

Fetch origin; read current AGENTS.md, framework, Worker role and state.
Create `worker/27-native-filter-navigation` from latest `origin/master` in
`.worktrees/worker-27-native-filter-navigation`; initialise `ocgcore` and run
`python3 tools/fw.py status`. Its fictitious "EADME" batch is documented in
`docs/framework-feedback/2026-10-08-legacy-round-status.md`; inspect git.
Never use retired `fw.py start/report`.

Goal: every enabled deck-builder filter is reachable and operable by Tab
and Shift+Tab on native macOS, with visible focus at 960x600 and 1280x800.
Main filters, Effects and Link markers must work through the real shell,
including lower Effects categories, close/reopen and restored focus.
The owner selected application-local all-controls navigation. Leave system
settings untouched. Use supported public APIs compatible with the Qt floor
and CI's pinned Qt; report alternatives to Brain if unavailable.

Own navigation in existing `ui/qml/`, `ui/src/main.cpp` and app-context files
if needed; `ui/tests/test_deckbuilder_screen.cpp`, `TestHarness.qml`,
`docs/architecture/deck-builder-ui.md`, carried round 026 evidence, and your
summary. Reserve ADR 0013 if needed. Batch 28 owns search adapters, dedicated
tests and CMake registration: request necessary build-file changes through
Brain; continue independent work meanwhile.

Carry round 026's relevant investigation and failure evidence from
`origin/verifier/026-native-filter-navigation` without reverting current
rules. Read its actual diff/research. Diagnostic all-controls runs shipped
no fix. Resolve intermittent native popup failures; repeats alone do not
explain them. Preserve the failed macOS clean-stderr assertion.

Keep engine, semantic model, search/legality semantics, dependencies,
repository settings, framework and shared status pages unchanged. No full
native-parity claim. Preserve strict destination, clipping and operation
assertions; prove a relevant regression fails before the fix. Record the
navigation decision and compatibility limits in architecture documentation.
Preserve historical reports.

Evidence: UI Debug/WERROR/UI_TESTS configure/build, CTest, and the workflow's
separate offscreen survival and empty-stderr assertions.
Run every `filterTabTraversalStaysVisible` row separately under offscreen and
Cocoa. Exercise the actual native shell at both sizes using synthetic cards;
record input delivery, focus sequence, popup operation and inspected captures.
Keep databases/local captures untracked, preserve CI screenshots and report
strict smoke failures without suppression. Run generators `--check`, README
status check, Python unittest discovery, golden reproduction and unchanged
diff, `fw.py check`, `git diff --check`.

Commit focused changes and `docs/batches/27-native-filter-navigation.md`
with Done, Checked, Not checked, Failed or blocked (maximum 500 prose words).
Record source SHA, commands, output, exits, skips and failed attempts. Push
on every exit. Check PR evidence before opening/updating a PR; report CI at
delivery SHA. Never merge/self-accept; finish with batch, Builder, outcome
and pushed commit.

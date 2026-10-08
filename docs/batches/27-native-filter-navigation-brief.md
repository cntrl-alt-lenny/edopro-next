# EDOPro Next · batch 27-native-filter-navigation · Builder

Path: Normal, under the current AGENTS.md rules for UI code. Brain reviews
the exact delivery; no automatic Verifier handoff. This continues the
unmerged round 026 investigation toward a product result.

Fetch origin. Read the current AGENTS.md, framework, Worker role and state.
Create `worker/27-native-filter-navigation` from latest `origin/master` in
`.worktrees/worker-27-native-filter-navigation`; initialise `ocgcore` and run
`python3 tools/fw.py status`. Its fictitious "EADME" batch is documented in
`docs/framework-feedback/2026-10-08-legacy-round-status.md`; inspect git for actual work.
Never use the retired `fw.py start/report` commands.

Goal: every enabled deck-builder filter is reachable and operable by Tab
and Shift+Tab on native macOS, with visible focus at 960x600 and 1280x800.
Main filters, Effects and Link markers must work through the real shell,
including lower Effects categories, close/reopen and restored focus.
The owner selected consistent all-controls navigation inside this application;
leave the user's system settings untouched. Use only supported public APIs
compatible with the declared Qt floor and CI's pinned Qt. If that cannot be
established, report the alternatives to Brain before implementing an override.

Carry forward round 026's relevant investigation and failure evidence from
`origin/verifier/026-native-filter-navigation` without reverting current
framework/project rules. Read its real diff and architecture research.
Its diagnostic all-controls runs are not a shipped fix. Address the native
popup reliability gap; successful repeats alone do not explain failures.

Keep engine, semantic model, search/legality semantics, dependencies,
repository settings and copied framework files unchanged. Leave README,
roadmap, capabilities and state to Brain. No controller or full native-parity
claim. Preserve strict destination, clipping and operation assertions; prove
a relevant regression fails before the fix. Record the navigation decision
and compatibility limits in the architecture documentation. Preserve historical
reports, including their failed macOS clean-stderr assertion.

Required evidence: Debug/WERROR/UI_TESTS configure and build of `ui/`, CTest,
and the workflow's separate offscreen survival and empty-stderr assertions.
Run every `filterTabTraversalStaysVisible` row separately under offscreen and
Cocoa. Exercise the actual native shell at both sizes using synthetic cards;
record input-delivery method, focus sequence, popup operation and inspected
captures. Keep synthetic databases and local captures untracked; preserve CI
screen-test screenshots. Report any strict smoke failure without suppression.
Run message/protocol generators with `--check`, README status check, Python
unittest discovery, replay golden reproduction followed by unchanged-golden
diff, `fw.py check` and `git diff --check`.

Commit focused changes and `docs/batches/27-native-filter-navigation.md`
with Done, Checked, Not checked, Failed or blocked; record exact checked SHA,
commands, real output, exits, skips and unsuccessful attempts. Push on every
exit, including a blocker. Report required CI at the exact delivery. Do not
merge or self-accept; end with batch, Builder, outcome and pushed commit.

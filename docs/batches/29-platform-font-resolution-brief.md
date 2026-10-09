# EDOPro Next · batch 29-platform-font-resolution · Builder

Normal: a bounded presentation/startup fix, followed by Brain review. This
fixes a reproduced gate; it is not a task to keep a Worker occupied.

Fetch origin; read current AGENTS.md, framework, Worker role, state, both Brain
reviews and this coordination plan. Create `worker/29-platform-font-resolution`
from latest `origin/master`, in `.worktrees/worker-29-platform-font-resolution`;
initialise `ocgcore` and run `fw.py status`. Batch 28 remains intact for review.
Do not wait for batch 27, batch 28 or the planning PR to merge.

Goal: the real shell survives the workflow's offscreen timeout and independently
produces empty stderr on native macOS and Linux CI, using suitable available
proportional/monospace fonts through supported public Qt APIs. Brain reproduced
the missing `Sans Serif` alias warning before either feature and in both
deliveries. Existing typography also specifies comma-separated family strings;
establish their actual Qt interpretation rather than assuming CSS semantics.

Own `ui/qml/Theme.qml`, font-specific startup in `ui/src/main.cpp`, any necessary
new font helper, a dedicated new font regression test, and narrowly scoped
`ui/CMakeLists.txt`/`ui/tests/CMakeLists.txt` registration. Own
`docs/architecture/platform-fonts.md`, your summary, and ADR 0015 if needed.
Navigation QML, TestHarness and screen-test driver remain batch 27's; search
adapters and existing dedicated search tests remain batch 28's. Brain coordinates
any shared bootstrap/helper call; request it before editing another owner's file.

Fix the resolution class, not one diagnostic. No warning suppression, stderr
filtering, softer smoke assertion, OS font installation, bundled font/dependency,
private API, CI/settings change or gratuitous visual redesign. Preserve theme
sizes, hierarchy and engine/model/data/policy boundaries. Establish compatibility
with the Qt floor; actual platform execution and source compatibility are distinct.

Evidence: demonstrate the real pre-fix warning and a regression that fails;
verify available resolved proportional/monospace families and ordinary UI font
use. UI Debug/WERROR/UI_TESTS configure/build/CTest and literal independent
survival/empty-stderr workflow checks, on macOS plus Linux CI. Inspect the real
shell at 960x600/1280x800 with synthetic cards, including text wrapping, clipping,
focus and popup readability. Keep assets/captures untracked. Preserve required
checks and screenshots. Run generators/README checks, Python discovery, golden
reproduction/unchanged diff, framework/whitespace and PR-evidence checks.

Commit focused changes and `docs/batches/29-platform-font-resolution.md` with
Done, Checked, Not checked, Failed or blocked; exact source SHA, commands, real
output, exits/skips and failed attempts. Push on every exit. Brain will integrate
an accepted fix into the search/navigation deliveries and re-review new heads.
Do not merge/self-accept; finish with batch, Builder, outcome and pushed SHA.

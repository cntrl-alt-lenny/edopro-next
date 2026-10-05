# 025-framework-3-1-0: reliable framework handoffs

Tier: 1
Mode: implementation
Supersedes: none

## Goal

Adopt the released agentic-framework 3.1.0 through its supported adopter,
so seat prompts, review selection and next-action reporting use the released
coordination fixes while this project's rules and product code stay intact.

## Context

Why this slice: after round 024 merged, the installed 3.0.0 tooling still
listed its older blocked Verifier branch as in flight and selected that
branch in an unqualified delivery query. Reliable dispatch is a prerequisite
for the next product round. This is a minor release, not a new milestone.

Brain checked the following on 2026-10-05:

- GitHub reports PR #42 merged at
  `c1a3da98ed53a131298ebb131e3877d97252064e`, which is the starting
  `master` commit. The successful Verifier report is included; it reviewed
  `6b9c661ccc5107e2bdd96124e95ac05f9623a5d0`, and its report commit
  changes only that report. Required deterministic CI checks succeeded.
- `python3 tools/fw.py status`, exit 0, nevertheless printed
  `in flight: 024-scroll-focus-visibility -- verifier on origin/verifier/024-scroll-focus-visibility`.
- `python3 tools/fw.py delivery --round 024-scroll-focus-visibility`,
  exit 0, selected `origin/verifier/024-scroll-focus-visibility` at
  `d396bc5a25e6`, whose report describes the pre-delivery master
  `a1306f93883e`. The later review branch is
  `origin/verifier/024-scroll-focus-visibility-2`, at `91bcc988a3ae`.
- Brain independently configured and rebuilt `ui/` under WERROR with UI
  tests enabled, then ran `ctest --test-dir ui/build --output-on-failure`
  after the build completed. All commands returned 0. This spot-check
  supports the merged offscreen behavior; it does not establish native
  input parity or unchanged duel behavior.
- GitHub's `master` protection query returned `true`. No protection or
  repository setting change is part of this round.

Read the installed framework and Builder card, `AGENTS.md`, the framework
manifest, and the target release's `CHANGELOG.md` adopter section. The target
source is `https://github.com/cntrl-alt-lenny/agentic-framework`, tag
`v3.1.0`, peeled commit `eca1306dc43cb81f0df3ee42f841812da68e8b1e`.
Recheck that identity before using the adopter.

The installed 3.0.0 framework describes all updates as Tier 2. This round
uses Tier 1 because the project's higher-priority `AGENTS.md` requires
release-specific adopter steps, and 3.1.0's adopter section explicitly says
to run this minor update as one Tier 1 round, Worker then Brain. Builder is
this project's Worker seat. No Verifier delivery is due for this round.

Round 024's native Cocoa traversal limitation and stale ATK/DEF source
comment remain separate product follow-ups. Do not fix them in this round.

## Scope and non-goals

In scope:

- Framework-owned files and manifest changes made by the pinned release's
  `tools/adopt.py --update`, preserving the existing adapter/hook options.
- Project-owned reconciliation explicitly required by that release's
  adopter steps: redundant seat files, if any, and a redundant prompt-header
  rule, if any. Establish whether they exist before changing anything.
- `docs/state.md` only to reconcile its framework-version decision with the
  adopted release; list every removed and added sentence in the report.
- The Builder report and supporting evidence in this round's `attachments/`.

Out of scope:

- Product code, semantic or search behavior, QML, game rules and upstream
  engine/client files; README capabilities and milestone completion claims.
- Hand edits to framework-owned implementation or templates, local fixes to
  the released tooling, new dependencies or new framework settings.
- Optional `settings.report_check` configuration: preserve existing settings.
- Branch deletion, worktree cleanup, remotes, protection, required checks,
  OS keyboard preferences, and changes to merge authority.
- Rewriting earlier reports or starting another product round.

## Invariants

- Follow `AGENTS.md`: owner-approves, no seat self-acceptance or merge, no
  personal paths or email addresses in tracked records.
- Run adoption only in the isolated seat checkout; never modify Brain's
  shared primary checkout or another seat's worktree.
- Framework changes come from the verified release's adopter, not a local
  implementation (`AGENTS.md`, framework rule 14).
- Project-owned invariants, adapters, hook options and existing manifest
  settings survive. Inspect every `.framework`, `gone` and `other` result;
  report an unresolved collision instead of silently choosing a file.
- Historical branches/reports are evidence, not live work. Do not delete
  them to make `status` look better. A remaining misleading result is an
  observed limitation, not permission to patch the framework.
- Existing tests may not be weakened or skipped to obtain a green result.
  Python tooling checks do not establish unchanged duel behavior.

## Acceptance criteria

1. The manifest pins 3.1.0 and every installed framework copy matches its
   recorded release fingerprint. Adoption retains existing project options
   and settings; project-owned changes are individually justified.
2. The dry-run output and actual adoption output are recorded. A subsequent
   dry run is idempotent: no replacement or manifest record remains due.
   Missing project-owned files are not recreated accidentally.
3. The released `status` prints seat progress and ends with `next:`.
   Establish its behavior against this repository's retained older review
   branch and superseded rounds, and compare it with the observed 3.0.0
   output above. Distinguish any historical artifact from actual seat work.
4. `prompt` generates a Builder header for this round and a Verifier header
   for a Tier 2 round using the project's exact seat names. Treat historical
   prompts as read-only diagnostics; do not execute them or restart old work.
5. Delivery of this round resolves to the current Builder report at an exact
   SHA once pushed. A disposable fresh clone can fetch the brief and report;
   no private file or chat is required to continue the handoff.
6. The project checks and golden reproduction pass with supported Python.
   Each skip is named and any command failure remains visible. Required CI
   at the final delivered commit is observed or explicitly left unknown.
7. The report identifies every changed file, every project-owned adjustment
   and the remaining limits. It makes no native-input, engine-equivalence or
   merge claim.

## Required evidence

Use Python 3.10 or newer for the suite. If `python3` selects an older system
interpreter, use an available supported executable and describe it without
recording its personal filesystem path. Record commands, outputs, exits,
operating system and Python version at the stated commit.

1. Start with `python3 tools/fw.py start --role builder --round
   025-framework-3-1-0`, then immediately `git submodule update --init` and
   `git submodule status`. Stop if seat start fails and report its output.
2. Record target-framework `git describe --tags --exact-match` and
   `git rev-parse HEAD`. Read its adopter steps from the verified clone.
   Run the release's `python3 tools/adopt.py <seat-checkout> --update
   --dry-run`, then the same command without `--dry-run`; rerun the dry run
   to check idempotence. Do not commit literal personal command paths.
3. Run and record:
   - `python3 tools/fw.py status`
   - `python3 tools/fw.py status --offline`
   - `python3 tools/fw.py prompt --round 025-framework-3-1-0 --role builder`
   - `python3 tools/fw.py prompt --round 024-scroll-focus-visibility --role verifier`
   - `python3 tools/fw.py delivery --round 024-scroll-focus-visibility`
   - `python3 tools/fw.py check`
4. Run and record:
   - `python3 tools/generate_messages.py --check`
   - `python3 tools/generate_protocol_constants.py --check`
   - `python3 tools/generate_readme_status.py --check`
   - `python3 -m unittest discover -s tests -v`
   - `python3 tests/test_replay_trace.py --update`
   - `git diff --exit-code -- tests/golden`
   - `git diff --check`
5. Inspect the diff against the starting master and verify the changed
   framework copies against the release and manifest. Run `python3
   tools/check_pr_evidence.py` before opening or updating any PR; PR bodies
   cite rerunnable commands instead of measured figures. A draft PR is
   allowed; neither acceptance nor merge is authorized.
6. Write `docs/rounds/025-framework-3-1-0/builder.md` and run `python3
   tools/fw.py report --role builder --round 025-framework-3-1-0 --push`,
   including on early exit. Then record delivery from a disposable fresh
   clone using `python3 tools/fw.py delivery --round 025-framework-3-1-0`.
   Append observed delivery/CI evidence and regenerate the report if needed;
   always distinguish checks of the implementation SHA from report-only SHAs.
   End with the completion header printed by the adopted report command.

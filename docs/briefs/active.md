Brief-ID: 010-2026-09-20-framework-adoption

Status: active (delivered and reopened for corrections C1-C5)

## MODE: IMPLEMENTATION

## Goal

Adopt the shared agentic framework in edopro-next so a fresh session can start
from `AGENTS.md` and `docs/agents/roles/brain.md`, reconstruct the live state,
and choose the correct first action without conversation history. Resolve the
known delivered brief queue, install the framework's canonical contracts and
mechanical guards, preserve this project's stronger local evidence and
lifecycle rules, and deliver the result as a reviewed-ready Builder branch.

## Why this is next

This repository already has a three-seat brief loop, but its canonical role
contracts and provider-neutral tooling are absent. Brief 008 is delivered but
not accepted, and brief 009 was delivered without independent review; both
must remain visible as distinct, honest queue records before new work proceeds.
Adoption is the bounded foundation work needed for cold-start continuity and
for the report and checkout mechanisms required by future rounds.

## Base

Cut `meta/framework-adoption` from the current local tip of `master`. Confirm
the exact base SHA with git before the first implementation change. Do not
modify, rebase, force-push, delete, or merge the existing branches
`m3/read-failure-predicate`, `meta/round-5-queue`, or `meta/evidence-freshness`.

## Relevant context

Read `CLAUDE.md`, `AGENTS.md`, `docs/state.md`, `docs/briefs/README.md`, the
archived brief shape, and the shared framework's adoption, constitution,
role-contract, isolation, brief, report, and topology documents. Inspect the
source records on `meta/round-5-queue` for briefs 008 and 009. The engine and
product source tree is out of scope and is not relevant to this documentation
and tooling adoption.

## Reopened round state

The first delivery of this brief reached head `55a8989f` and was delivered for
review but was **not accepted**. This brief is therefore delivered and reopened
in place for the five numbered corrections below; it is not finished. The
reopened work deliberately completes only the corrections and leaves the
neutrality guard deferred rather than claiming complete adoption.

## Scope

- Preserve brief 008's delivered text, including corrections C1-C4 and its
  delivered-and-not-accepted status, in `docs/briefs/delivered/008-...`, fixing
  only its three broken relative links and adding its stable `Brief-ID:`.
- Create a delivered record for brief 009 from the queue branch's active text,
  recording its delivered-and-unadjudicated state, PR #26, branch
  `meta/evidence-freshness`, and the path for re-landing it later.
- Run the shared adoption tool after reading its plan, with Builder and
  Verifier seats, hooks, and the Claude Code adapter. Reconcile every collision
  deliberately; no `.framework` sibling may remain.
- Keep this project's substantive invariants, evidence table, lifecycle, state,
  mutation-tested push guard, and project-specific rationale. Install the
  framework's canonical contracts under `docs/agents/roles/`, retire the old
  `docs/roles/` copies, and keep the executor contract named `worker.md` while
  declaring the project seat as Builder.
- Do not install a repository-local neutrality guard while the framework's
  branch-namespace rule contradicts its adoption guidance. Keep the established
  `m<N>/` and `meta/` convention plainly, and record that provider-shaped lane
  names and branch namespaces are not currently enforced here. Revisit only
  when the framework provides a project-declared namespace mechanism and a
  scanner whose rule agrees with its adoption guidance.
- Update only live normative documents and tests whose paths must move; do not
  rewrite historical archived briefs.
- Demonstrate both restored docs-consistency guards: a dead angle-bracket link
  must fail while a genuine framework placeholder still passes, and the
  adapter's confirmation command set must cover the canonical contract's set.
- Walk every path named by `AGENTS.md` and `docs/agents/roles/brain.md` and
  confirm that each resolves to an existing file and that a fresh Brain's first
  action is possible.

## Non-goals

- Do not change engine behaviour or touch `ocgcore/`, `gframe/`, `client/`,
  `data/`, `policy/`, `ui/` source, build configuration, or workflows.
- Do not fix brief 008 corrections C1-C4 or re-land brief 009's work.
- Do not touch branch protection, repository settings, remotes, or any open
  pull request. Do not merge anything.
- Do not modify the framework repository or its canonical scanner.
- Do not delete or weaken existing tests. Historical archive documents remain
  historical records.

## Protected invariants

- `ocgcore` must have the same recorded submodule commit before and after, and
  no gitlink may appear in this diff.
- The rules engine remains authoritative and separate from presentation; the
  client model remains semantic and free of Qt/Irrlicht types.
- AGPL-3.0-or-later, dynamic Qt linking, upstream ownership boundaries, and the
  prohibition on committing artwork, databases, or CardScripts remain intact.
- Builder and Verifier are executor/reviewer seats, never self-accepting or
  merging seats; Brain remains the routine technical acceptance and merge seat.
- The local lifecycle must retain its `delivered` state, and evidence claims
  must remain honest and reproducible.
- Existing tests, including push-guard, docs-consistency, save-agent-reply,
  CI-check, replay-trace, semantic-trace, and protocol-generation tests, must
  continue to run. Path assertions must be updated to the installed layout,
  never removed or weakened.
- LF normalization must be installed as `* text=auto eol=lf`; renormalization
  must be checked and any unexpected diff reported rather than hidden.

## Required investigation

1. Inspect every adoption collision and compare the project's version with the
   framework version. For `AGENTS.md`, `docs/roles/`, `docs/state.md`, brief
   files, the push hook, gitattributes, Claude adapter files, seat files,
   status command, and launching documentation, record the deliberate outcome
   and any project-specific material moved or dropped.
2. Establish the precise neutrality failure class. Explain why the existing
   `m<N>/` and `meta/` namespaces are retained despite the framework guidance,
   how the repository-local guard still catches a provider-shaped namespace,
   and record that the framework adoption guidance and its branch rule
   contradict each other as an out-of-scope framework finding.
3. Verify the cold-start path from only `AGENTS.md` and the canonical Brain
   contract, including every named path and the first actionable command.

## Acceptance criteria

- The first commit contains this brief at `docs/briefs/active.md` with this
  exact stable identifier.
- Briefs 008 and 009 exist under `docs/briefs/delivered/` with honest statuses,
  stable identifiers, preserved substance, and corrected live links.
- Adoption ran from the shared framework after its plan was read, installed
  the requested canonical files and adapter, and every `.framework` collision
  was deliberately resolved and deleted.
- The repository contains one canonical contract per declared role under
  `docs/agents/roles/`: `brain.md`, `worker.md`, and `verifier.md`, with no
  stale duplicate Builder contract; `docs/roles/` is retired.
- The installed report/checkout tools are present and project tests refer to
  their actual paths. The neutrality scanner, authority scanner, textblock
  helper and their test are explicitly absent pending a framework fix; this
  adoption is partial, not complete.
- The two restored docs-consistency guards demonstrably fail on their targeted
  mutations, pass after the mutations are removed, and the final full suite is
  green.
- Every path named by the two cold-start documents resolves, and the first
  action is a runnable Builder/Brain checkout check as appropriate.
- The branch is pushed as `meta/framework-adoption` and a non-merged PR against
  `master` begins with `DO NOT MERGE — under review` and names base and head
  commits without measured figures.

## Required evidence

- `python3 -m unittest discover -s tests -v`, including installed neutrality,
  checkout, and report tests.
- `python3 tools/generate_messages.py --check` and
  `python3 tools/generate_protocol_constants.py --check`.
- `python3 tools/checkout.py --seat builder` and
  `python3 tools/report.py status`.
- `git submodule status` before and after, with the unchanged `ocgcore` commit.
- `git add --renormalize .`, with the real result and whether it changed
  anything.
- The real failing and passing output for both restored docs-consistency guard
  demonstrations.
- The cold-start path walk and its first-action command.
- The adoption plan and actual adoption output, plus the final absence of
  `.framework` files.
- The final diff, base/head SHAs, push and PR output, and the real output of
  `python3 tools/report.py write --task 010-2026-09-20-framework-adoption`.
- State the machine and platform. Do not build a C++ module: no touched file
  can affect one; say this explicitly. Do not use the replay harness as proof
  of unchanged duel behaviour.

## Reopened corrections C1-C5

1. **C1 — remove the counterexample block.** Delete the `guard:counterexample`
   and `guard:violation` markers from the worktree policy and restore the
   ordinary `m<N>/` and `meta/` branch example.
2. **C2 — withdraw guard-driven wording edits.** Restore the exact
   `deck-builder-ui.md` citation, the “deck-builder legality boundary” wording,
   and the `../edopro-next-builder` example. Keep the corrected canonical role
   paths in `docs/agents/launching.md`.
3. **C3 — defer neutrality.** Remove the locally installed neutrality,
   authority and textblock tools and neutrality test. The project therefore
   does **not** currently enforce provider-shaped lane or branch namespaces;
   this closes only when the framework reconciles its scanner with its adoption
   guidance and supports declared project namespaces.
4. **C4 — restore both docs guards.** Skip only targets that are entirely a
   placeholder, and restore the general confirmation-command superset check
   against `docs/agents/roles/worker.md` while retaining the explicit
   Builder/Worker adapter assertions.
5. **C5 — refresh rehydration state.** Update `docs/state.md` so active work,
   delivered records, closed-but-kept queue branches, rejected PR #24 and the
   honest next slices are current without weakening its spot-check discipline.

## Git expectations

Use focused commits on `meta/framework-adoption`; never push `master`, merge,
force-push, rebase, or delete branches. The Builder delivers a branch and a
report; Brain and the Verifier decide acceptance and merge.

## Completion-report schema

Use the standard Worker report with exact base/head SHAs, changed files,
commands and output, omissions, sources for external claims, and open
questions. Add the collision-by-collision reconciliation decisions, the
project-specific material moved or dropped from `docs/roles/`, the neutrality
exception and remaining catch surface, the framework branch-rule contradiction,
the cold-start path walk, the unchanged submodule evidence, the renormalize
result, the mutation red/green evidence, and the PR URL and report-writer
output.

Brief-ID: 013-2026-09-21-framework-guard-install

Status: accepted

## MODE: IMPLEMENTATION

## Goal

Bring this repository's installed copy of the shared agentic framework up to
one pinned framework revision, consistently, and install the
provider-neutrality guard that the adoption round (Brief 010) deliberately
left out.

**Pinned framework revision:**
`cntrl-alt-lenny/agentic-framework` at
`fed26f360294baddedc74eeaabbaf2e716572260` (the merge of framework PR #11).
Every framework file you install comes from that commit, and no other.

## Why this is next

- Brief 010 installed the framework without its neutrality guard, because the
  guard then contradicted this project's `m<N>/` and `meta/` branch
  convention.
- The framework has since added project-declared branch namespaces (its
  PR #9), cross-clone delivery reporting and a line-ending tool (PR #10), and
  a fix so that command-form and prose-form text is matched the same way
  (PR #11).
- The read-only trial of PR #9 on this repository (recorded in
  `docs/state.md`, *Shared-framework status*) accepted 25 of 26 real branches
  under the declaration `<!-- guard:branch-namespaces prefixes="m<N>,meta" -->`.
- Our installed framework files are older and partly out of step with each
  other. `tools/report.py`, for example, matches framework commit `a70c559d`,
  and no document records which revision was adopted.

## Base and branch

Branch `meta/framework-guard-install`. Its first commit is Brain's close-out
of round 012; it touches only `docs/briefs/`, `docs/state.md` and
`docs/agents/model-notes.md`. Continue on top of it. Do not rebase or
force-push. The close-out commit is part of the reviewed range: report any
error you find in it, and do not rewrite it.

## Scope

- Follow the pinned revision's `framework/adoption.md`, section *Updating an
  adopted framework consistently*, and move the whole versioned surface it
  lists in one change:
  - every `VERBATIM_DOCS` document under `docs/agents/`;
  - the baseline tools and their tests;
  - the four neutrality-guard files;
  - `.gitattributes`, and `.githooks/pre-push` where applicable.

  Derive the exact file list from that revision's `tools/adopt.py`; do not
  copy the list from this brief. Say in the report whether the pinned
  revision treats the installed Claude Code adapter (`.claude/`) as part of
  that surface, and act accordingly.
- Declare this project's branch namespaces in root `AGENTS.md` using the
  pinned revision's declaration syntax.
- Record the pinned framework commit where a future session will find it,
  including in `docs/state.md`'s *Shared-framework status*.
- Run the pinned revision's `tools/line_endings.py check` in every checkout of
  this clone: the primary checkout and each linked worktree. Report the
  output of each run.

## Non-scope

- No change to `.github/workflows/`, branch protection, required checks,
  repository settings or remotes.
- No production code (`client/`, `data/`, `policy/`, `ui/`, `gframe/`,
  `ocgcore/`).
- Do not edit the canonical framework documents or tools to make them fit.
  If one does not fit this project, that is a framework finding: report it
  and do not patch it locally.
- Do not fix the known double-spaced PR-body fixtures (`pr_10.txt`,
  `pr_16.txt`); they are a separate open item.

## Protected invariants

- **`.githooks/pre-push` is project-owned in this repository.** It carries
  project checks and bypass history, pinned by `tests/test_push_guard.py`. If
  the pinned revision's hook differs, the update must not weaken or drop any
  behaviour those tests pin. Explain what you did and why.
- **No exemption that hides a real finding.** Resolve every guard finding in
  one of two ways: fix the project text, or add a declared counterexample
  block around text that genuinely quotes a violation. Never widen a
  declaration just to silence a finding. List every finding and its
  resolution.
- **Project-owned files keep their content.** That means `AGENTS.md` apart
  from the declaration and any genuinely required wording fix, plus
  `docs/agents/model-notes.md`, `launching.md` and `worktree-mechanism.md`.
- **Existing tests survive.** Name any pre-existing assertion you change,
  with its before and after property.

## Required investigation

1. Diff every installed framework file against the pinned revision before
   changing anything. Report which were already current, which were stale,
   and which were locally modified. Treat a local modification as something
   to understand, not something to overwrite silently.
2. Run the guard across the repository and classify each finding: a real
   project defect, a legitimate quotation, or a framework defect.
3. Check the declaration against every branch on `origin`
   (`git ls-remote --heads origin`). The trial expected `modern-ui/bootstrap`
   to be the one non-conforming branch. Say whether the pinned revision
   requires it to be declared (with its tracked witness under
   `docs/branch-namespaces/`), and why.

## Acceptance criteria

- Every installed framework file is byte-identical to the pinned revision,
  except those the adoption guide says the project owns or configures. Show
  SHA-256 for each file on both sides.
- The neutrality test runs and passes. Then it is **shown to fail**: add a
  structurally invalid branch example to a normative document, watch the test
  go red, then remove the example. Real output both ways.
- `python -m unittest discover -s tests -v` and both generator `--check`
  commands are green from Git Bash. `tests/test_push_guard.py` still passes.
- `docs/state.md` records the pinned framework commit and no longer says the
  guard is absent.
- The PR body begins with `DO NOT MERGE — under review`, passes
  `python tools/check_pr_evidence.py`, and is not merged.

## Required evidence

- Checkout check, platform, tool, model and OS (the OS from a command's
  output, not assumed).
- The before-diff inventory from investigation 1.
- Per-file SHA-256 identity against the pinned revision.
- Guard findings and their resolutions.
- The mutation red/green.
- `line_endings.py check` output for every checkout.
- The suite and generator output.
- Check-run conclusions at the final head.
- What you did not run.

## Completion-report schema

The standard report in
[`docs/agents/roles/worker.md`](../agents/roles/worker.md), plus: the file
inventory (current, stale, locally modified), how `.githooks/pre-push` was
handled, the guard findings table, and any framework findings to raise.

---

## Outcome — accepted and merged 2026-09-21

Accepted by Brain and merged as PR #29 (merge `d035c66d`) at head
`503022ae12c8b18c42d46933988d7296e6429692`, base
`8bad984e0e2e4da7b0c3ac51cdb295a25baec4c5`, after one pass. Builder and
Verifier both ran in Antigravity (Gemini 3.8 Flash, High).

**Acceptance conditions, confirmed at the literal head.** The Verifier
reviewed `503022ae` (task `013-verify-framework-guard-install`) and found no
BLOCKER or UNPROVEN CLAIM. Brain independently checked the load-bearing
claims below. All required checks were green (12 success, 1 conditional
matrix entry skipped; merge state CLEAN). The change is inside routine
scope: it adds a check to the existing suite and weakens none.

**Independently established by Brain.**

- Every file in the pinned revision's update surface is byte-identical to
  `fed26f36`: 13 `VERBATIM_DOCS`, six tools, `tests/test_checkout.py`,
  `tests/test_report.py` and `.gitattributes`. Each was hashed against
  `gh api` raw content at that commit. `tests/test_role_neutrality.py`
  differs from the template only in its two placeholder lines.
- The guard passes on this repository. It went red on two mutations neither
  seat had tried: a provider-prefixed lane token in `AGENTS.md`, and a
  counterexample block whose declared violation its text does not contain
  (the inert-block check). It went green again on reverting each.
- `.githooks/pre-push` is unchanged, and `tests/test_push_guard.py` passes.
  `line_endings.py check` is clean in the primary checkout.

**Defects found by Brain, not by the Verifier.**

- **Durable state was deleted.** The Builder's `docs/state.md` rewrite
  dropped the record of two proposals already settled with the framework's
  Brain. Restored in the round-014 close-out commit.
- **Two report misstatements.**
  - `.gitattributes` was listed as stale "bare `text=auto`". The base
    already had `eol=lf`; only its comment header changed.
  - The SHA table showed `tests/test_role_neutrality.py` equal to the pinned
    template. It correctly differs, because it is configured.

  Neither was merged content; the Verifier reported "Conflicts: None".

**Not done, by decision.** `modern-ui/bootstrap` was not declared: nothing
normative names it, and the scanner reads documents, not refs. The
`.claude/` adapter was left as is; the pinned `adoption.md` does not list
adapters in its consistent-update surface.

# Brief 003 — framework hardening: make the guards guard

Status: delivered

Delivered by Builder as PR #17, base `a6956f7d`, head `e4424c3e`.
Carries `DO NOT MERGE — under review`. **Not reviewed, not adjudicated** —
Verifier has not yet returned findings for this head. The outcome section is
appended when it moves to `archive/`.

The text below is the brief exactly as Builder received it in
`docs/briefs/active.md`; only this header has been added. Lifecycle:
[`../README.md`](../README.md).

---

## MODE: IMPLEMENTATION

## Goal

Round 1's verification of the framework returned zero BLOCKERs, seven
SHOULD FIX and five NOTE. The common thread in the most serious ones is
that **two automated guards check a proxy rather than the property they claim
to enforce**, and both were demonstrated to miss a live violation sitting in
the same commit that introduced them.

Close the findings below so the guards actually guard, and so the documented
bypass inventory is true.

## Why this is next

This work comes before the M3 citation audit (superseded brief 002) because
every subsequent round trusts these guards. A test that reports green while
the property it names is violated is worse than no test: it converts an
unchecked claim into an apparently-checked one, which is precisely the
`UNPROVEN CLAIM` failure this project keeps producing.

It is also entirely Python and shell, so it can be fully verified on the
current machine — which has no cmake and no Qt (see `docs/state.md`).

## Base SHA

Branch from `origin/master`. Verify with `git log -1` and record the SHA.

## Relevant context

Read: `tests/test_docs_consistency.py`, `tests/test_push_guard.py`,
`.githooks/pre-push`, `.claude/hooks/save_agent_reply.py`,
`docs/roles/*.md`, `.claude/agents/*.md`,
[`docs/agents/launching.md`](../agents/launching.md), and `AGENTS.md`'s
"Never push to `master`" section.

Do **not** read `docs/architecture/*`, `client/`, `data/`, `policy/` or
`ui/` — unrelated to this brief.

## Scope

Each item below is a confirmed finding, reproduced by both Verifier and
Brain. How to fix each is yours; the acceptance criteria say what must
become true.

1. **The vendor-neutrality guard checks six literal substrings.**
   `test_contracts_do_not_depend_on_one_vendors_mechanics` passes while
   `docs/roles/brain.md:116` says "`/status` runs this sequence" — a
   Claude-Code-only slash command inside a file whose own sibling doc
   excludes slash commands from contracts. Fix the leak *and* make the check
   detect the class rather than a fixed list.

2. **The anti-restatement guard is a 60-line cap.** All three
   `.claude/agents/*.md` adapters open by saying they "deliberately do not
   restate the contract, so the two cannot drift apart", and then restate
   substantive contract rules — never-merge and fetched-text rules in
   `builder.md`, the inbox unknown-means-unknown rule in `brain.md`, the
   commit/push/merge rules in `verifier.md`. One divergence already exists:
   the adapter's worktree-confirmation step lists fewer commands than the
   contract's. Either the adapters stop restating, or the guard detects
   restatement — a line count does neither.

3. **`.githooks/pre-push` has three escape hatches beyond the seven
   documented**, all reproduced live against a throwaway remote:
   `git send-pack` never invokes the hook; a push from a **bare** repository
   makes `git rev-parse --show-toplevel` fail so the script `exit 0`s
   silently before reading stdin; and `git -c core.hooksPath=` disables it
   exactly like `--no-verify` while only `--no-verify` is named anywhere.
   The silent bare-repo exit is a defect in the script — it is *less* safe
   than the missing-python path, which at least warns. The other two are
   inventory errors: the documented "seven fail-open cases" list is now
   demonstrably incomplete, and a reader relying on it would miss three.

4. **`.claude/hooks/save_agent_reply.py` is version-skewed against the
   layout it matches.** It is a tracked file re-executed from whatever commit
   is checked out, and the role mapping changed during PR #14. A Verifier
   worktree detached at a pre-`fa881423` SHA — which the Verifier contract
   explicitly permits, with no recency restriction — silently writes its
   report into `brain-latest.md`, colliding with Brain's own entries, exit 0,
   no error anywhere. Reproduced end-to-end.

5. **`.claude/agents/verifier.md` overclaims its own restriction.** It states
   as fact that the `tools:` frontmatter "restricts you to read-only tools
   plus `Bash`", dropping the contract's hedge. On the documented normal
   launch path for this seat — a plain interactive session in
   `.worktrees/verifier`, not a dispatched subagent — that frontmatter does
   not apply. Round 1's Verifier confirmed first-hand that its actual tool
   list was a strict superset. Say what is true.

6. **A "goes to the owner" boundary that nothing enforces.** `AGENTS.md`
   reserves CI and repository-settings changes for the owner, but every agent
   authenticates as the owner and nothing asserts anything about
   `.github/workflows/` content. A Brain-authored PR could narrow what a
   required check verifies — or drop a matrix leg — and then truthfully
   report "gates green at that SHA", because the same PR weakened the gate.
   The five required protection contexts are matched **by name**; make a
   change to those names or their coverage fail a test.

7. **`HistoricalBypassTest` overclaims.** Its three tests are named after
   distinct historical bypasses but all construct the identical
   already-resolved `refs/heads/master` stdin line, proving nothing about the
   resolution step each is named for. Either make them test what they claim,
   or rename them to say what they actually pin.

8. **The inbox fallback's non-Claude path is thin, and one branch of it is
   undefined.** `docs/roles/README.md` and `docs/agents/launching.md` both
   state the same two steps — the owner pastes the report, or Brain inspects
   repository and PR state — and no mechanism for posting a report anywhere in
   the repository is named (grep for `gh pr comment`/`gh pr review`: no hits).
   "Missing or stale means UNKNOWN" is correctly stated in Brain's numbered
   startup sequence rather than buried in an aside, so a Brain following the
   procedure does hit it — but nothing tells it what to do next, so it cannot
   distinguish "Verifier has not started" from "a non-Claude Verifier finished
   and nobody pasted the report." Close that branch. This matters more as more
   seats run on non-Claude tools, which is the stated direction.

   *(This item replaces the original item 8 — "model-notes.md has no round
   entry" — which PR #16 fixed directly before this brief was dispatched.
   Verifier flagged that it would have cost Builder a wasted investigation.
   The dropped NOTE recorded here is the one finding from the PR #14 round
   that had no disposition anywhere; it now has one.)*

## Non-goals

- **Do not touch `client/`, `data/`, `policy/`, `ui/`, `gframe/` or
  `ocgcore/`.** Nothing here needs them, and they cannot be built on this
  machine.
- **Do not change branch protection or any repository setting.** That is the
  owner's, explicitly. If a finding needs one, report it.
- **Do not change the authority model, the role topology, or the brief
  lifecycle.** Those were reviewed and accepted this round.
- Do not rewrite the role contracts wholesale. Fix the specific leaks.
- Do not weaken a test to make it pass.

## Protected invariants

- **The role contracts stay vendor-neutral.** Any fix that makes a contract
  depend on one tool is wrong, however convenient.
- **`.githooks/pre-push` remains a local convenience, never described as the
  control.** The guarantee is GitHub branch protection. Do not let a fix to
  item 3 drift into implying the hook is sufficient — and do not claim a
  complete bypass inventory unless you have actually enumerated the family.
- **The server enforces the path, not the role.** Nothing you write may imply
  GitHub can distinguish Brain from Builder; every agent authenticates as the
  same account.
- **A missing vendor-specific artifact means UNKNOWN**, never "the task did
  not happen."
- Tests must be able to fail. Every new or changed check must be
  mutation-tested, and the report must state the mutation and the resulting
  failure count.

## Required investigation

1. For items 1 and 2, the interesting question is not "does this instance get
   fixed" but **"what is the class, and can a check detect the class?"** If a
   general check is not practical, say so explicitly and explain what the
   narrower check does and does not cover — do not leave a proxy in place
   while implying it is general.
2. For item 3, enumerate the bypass family properly before writing the
   inventory. Round 1 established that a partial list presented as complete
   is itself the defect.
3. For item 6, work out what a test can actually assert about CI from inside
   CI, with no API credentials, and be honest about the residual gap.

## Acceptance criteria

- Every item above is either fixed, or explicitly declined with a reason.
- The two proxy guards (1, 2) either detect their class or state their limits
  in the test's own docstring — no silent proxies.
- The bare-repo silent `exit 0` (3) no longer passes silently.
- The documented bypass inventory matches what is actually reproducible.
- Every changed or added test is mutation-tested, with the mutation and
  failure count in the report.
- `python -m unittest discover -s tests` passes, and the suite still fails
  when the guards are broken.
- No repository settings changed; no protected invariant weakened.

## Required evidence

- `git diff --stat <base>..<head>`.
- `python -m unittest discover -s tests -v` — real output.
- `python tools/generate_messages.py --check` and
  `python tools/generate_protocol_constants.py --check`.
- For each guard you changed: the mutation you applied, and the resulting
  failure count.
- For item 3: the actual commands you ran against a throwaway remote, and
  their real output. Never against `origin`.
- **Do not run the cmake/ctest cycles** — this machine has neither cmake nor
  Qt, and nothing here could affect them.

## Git expectations

Branch `meta/framework-hardening`, in the Builder worktree
(`.worktrees/builder`). Focused commits — ideally one per item, so a reviewer
can take them separately. Push, open a PR carrying
`DO NOT MERGE — under review`. **Do not merge.**

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../roles/builder.md), plus:

- **Items 1 and 2 first**, with a straight answer to whether the guard now
  detects the class or only a wider set of instances. If the latter, say so
  plainly — an honest narrow check is fine; a narrow check described as
  general is the defect being fixed.
- **The bypass inventory for item 3**, as a list, with how you established it
  is complete — or a statement that you could not.
- **Every mutation test**, with its failure count.
- Anything you declined, and why.

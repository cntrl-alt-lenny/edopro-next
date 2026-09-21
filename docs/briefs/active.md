Brief-ID: 011-2026-09-20-read-failure-class

Status: active — delivered and reopened

## Reopened corrections R1-R4

This brief was delivered in PR #24 and reopened after review at `3b15ad56`.
The loader behavior is accepted as unchanged for this correction round; the
remaining work is to make the contracts, evidence claims, test output, and
CI report truthful.

- **R1 — public contracts and contract test.** The `ydk.h` and `lf_list.h`
  sentences must say that opening or reading failure makes `ok` false, while
  a path inspection/status error alone is deferred and may still produce
  `ok == true` if opening and reading succeed. The contract test must reject
  the old false sentence and assert that documented status-error exception.
  Disclose its property change by name: before, it checked only that the
  words "path inspection", "opening", and "reading" appeared and that
  "exactly when" did not; after, it checks the positive predicate, the
  status-error exception, and absence of the old sentence.
- **R2 — evidence scope.** Brief 008 C1-C3 and the POSIX half of C4 remain
  closed with evidence. On Windows 11/MSVC, the R9 implementation is compiled
  and exercised: both loaders reject canonical free and connected/busy named
  pipes, plus GLOBALROOT, localhost UNC, loopback UNC and extended UNC
  spellings, promptly with `failed to read file`; the old head's new alias
  tests return `failed to open file` instead. The Windows tests do not measure
  a separate freed-mid-load race, unrelated device/symlink cases, or a pipe
  server that denies the native probe with another error. Rescope PR and
  architecture claims to those observed inputs and update the former open item
  in `docs/state.md`.
- **R3 — visible platform skips.** The dangling-symlink and `/dev/zero`
  early returns in both the `data/` and `policy/` test suites must print an
  unambiguous `SKIP` line rather than report an indistinguishable pass.
- **R4 — exact-head CI.** Query and report the check-run conclusions for the
  final head SHA; the prior report omitted this evidence.

- **R5 — Windows named pipes can still reach the ordinary file open.** Observed at 338fe1e8 on Windows 11 / MSVC, independently by the Verifier and by Brain. `std::filesystem::status()` on \\.\pipe\<name> connects to the pipe server as a client and does not classify the pipe as non-regular. When that connection takes the pipe's only free instance, the native `CreateFileW` probe fails with `ERROR_PIPE_BUSY` and the loader falls through to `ifstream`. Every observed load returned `ok=false` promptly, but with "failed to open file" rather than the documented rejection. If an instance frees between the probe and the open, `ifstream` can connect and block on read: the non-terminating-input class this round exists to close. R9 closes the observed class by probing the Windows-resolved object before any filesystem status call: a non-disk handle or `ERROR_PIPE_BUSY` returns `ok=false` before `ifstream`, without enumerating spellings. The final tests cover canonical free/busy and the four named aliases listed in R2. Missing and share-locked regular files still report "failed to open file" (Brief 008 C1); the remaining untested native-probe error mode is named in the architecture document.
- **R6 — Windows evidence now exists, but the documentation still says the Windows code is uncompiled and unverified.** docs/architecture/read-failure-class.md, the open item in docs/state.md, the R2 wording, and the PR body must describe the Windows behaviour actually observed after R5, input by input. They must claim neither more nor less than that evidence.
- **R7 — tests/test_read_failure_contract.py checks independent substrings, so a contract that contradicts the positive predicate or the status-error exception can still pass it.** Make the test fail for such a contradiction, demonstrate one contradictory contract failing, and name the before/after property in your report.
- **R8 — the PR body's scope sentence describes only the latest corrective step, but the whole master..head diff is what merges, and it does change loader behaviour.** Rewrite the body to describe the whole range, including R5's change, and add the check-run conclusions at the final head SHA.

## Reopened corrections R9-R10

R9 — the Windows non-terminating-input class is still open for named-pipe paths spelled outside the two recognised prefixes. Observed by Brain at 797a1d86 on Windows 11 / MSVC. A single-instance server listens at \\.\pipe\<leaf> and re-listens after each client. With it running, load\_ydk("\\.\GLOBALROOT\Device\NamedPipe\<leaf>") never returned: more than 5 seconds, in 3 of 3 runs, with two loader-side client connections. load\_ydk on "\localhost\pipe\<leaf>", "\127.0.0.1\pipe\<leaf>" and "\\?\UNC\localhost\pipe\<leaf>" each connected once and returned "failed to open file", which is the busy fall-through R5 described. Required outcome: R5's outcome, applied to any path Windows resolves to a named pipe however it is spelled, not only to an enumerated list of spellings. Both load\_ydk and load\_lflist must return promptly with ok=false under every instance state. The documentation must explain why spellings you did not test are also covered; if some case genuinely cannot be covered, name it and give the evidence. Add Windows tests that fail at 797a1d86, including the GLOBALROOT case. Every pipe test must fail rather than hang if the loader blocks; the current timeout helpers call future.get() after a timeout and can wait indefinitely. Preserve: missing and share-locked regular files report "failed to open file" (Brief 008 C1); NUL, CON, a directory and a zero-byte file keep their current results; POSIX behaviour is unchanged.

R10 — docs/architecture/read-failure-class.md says the named-pipe guard closes the non-terminating-input class "by construction", which R9 contradicts. Make it, the R2/R5 evidence wording in active.md, docs/state.md and the PR body match what R9 actually establishes, claiming neither more nor less. docs/state.md also still says R1-R8 "are being applied" and that PR #24 "remains open and rejected"; describe the round's lifecycle as it actually stands.

## MODE: IMPLEMENTATION

## Goal

Close Brief 008's four rejected corrections in `data/` and `policy/`, decide
and document a safe predicate for the broader non-terminating-file class, and
answer what validation mechanism should catch platform-dependent runtime
semantics. Deliver the implementation and the evidence as one reviewed-ready
Builder round without changing CI policy.

## Why this is next

Brief 008's delivered code is rejected and its four corrections remain open.
Its directory test can pass with the fix reverted on Linux and Windows, while
`load_ydk()` can never return for inputs such as a POSIX FIFO, a Windows named
pipe, or `/dev/zero`. The project therefore needs both the concrete correction
and an honest class-level decision before another round builds on it.

## Base and branch

Continue on the existing `m3/read-failure-predicate` branch, merge
`origin/master` into it without rebasing or force-pushing, and preserve its
open PR #24 and recorded rejection. The first commit on this branch after the
merge contains this brief and the Brief 010 close-out.

## Scope

- Close Brief 008 corrections C1-C4 by reading their authoritative text from
  `docs/briefs/delivered/008-2026-09-01-read-failure-predicate.md` on
  `master`, implementing only what the corrections require, and adding tests
  that fail before each relevant fix. C1 must assert the actual missing-file
  error message.
- Decide whether the file readers should reject by path type, inspect the
  opened handle, or use another predicate. Exercise the relevant inputs and
  record the cross-platform limits honestly; do not claim portability by
  construction unless the evidence supports it.
- Record the predicate decision and reasoning in `docs/architecture/` and add
  an ADR if the decision is durable rather than merely explanatory.
- Answer the platform-divergence mechanism question in repository
  documentation or the completion report: distinguish compiler, build,
  configure/build, and runtime-semantics classes; recommend one mechanism and
  state what it would not catch. Do not change `.github/workflows/` or required
  checks.
- Move Brief 008 to `docs/briefs/archive/` only if the implementation really
  closes all four corrections, with its outcome stated honestly.

## Non-goals

- Do not re-land Brief 009 from `meta/evidence-freshness`.
- Do not install or patch the neutrality guard or modify the framework
  repository.
- Do not change `.github/workflows/`, branch protection, repository settings,
  remotes, `ocgcore/`, `gframe/`, or `integration/legacy/`.
- Do not merge any pull request or change engine/UI behaviour outside the two
  file-reader sites and their proportionate tests and documentation.

## Protected invariants

- Linux and Windows behaviour must not regress; this is the invariant C1
  explicitly protects and it must be tested rather than assumed.
- `ocgcore`'s recorded gitlink must remain unchanged, and no gitlink may enter
  the diff.
- Every canonical file under `docs/agents/`, `tools/checkout.py`, and
  `tools/report.py` remains byte-identical to
  `~/Dev/agentic-framework`.
- Existing tests survive. Any changed pre-existing test assertion must be
  named in the completion report with its before/after property.
- The neutrality guard remains absent and no local exemption is introduced.

## Required investigation

1. Compare predicates for missing files, directories, permission failures,
   named pipes/FIFOs, symlinks to directories, device files and zero-byte
   regular files across the platforms available. Explain what `ifstream` state
   and filesystem status actually establish, and what cannot be exercised.
2. Reproduce the existing directory-test weakness: it passes when the previous
   fix is reverted on both Linux and Windows. Add a regression test that would
   fail before the correction, especially an assertion on C1's actual error
   message.
3. Classify the six known platform divergences by the cheapest mechanism that
   can catch each. Recommend one mechanism, include its cost and reliability,
   and state which classes it would still miss. A new or changed required CI
   check is the owner's decision, not this round's implementation.

## Acceptance criteria

- C1-C4 are each closed by code and evidence, or the brief remains delivered
  and rejected with the unclosed corrections named; do not archive an open
  correction as accepted.
- `load_ydk()` and `load_lflist()` have a documented, tested decision for the
  non-terminating-file class and no claim stronger than the evidence.
- The predicate decision and platform-divergence recommendation are recorded
  where future readers will find them.
- `data/` and `policy/` configure, build with
  `-DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON`, and pass CTest on this
  machine; report Windows/MSVC separately if it cannot be exercised here.
- The full Python suite, both generator checks, and all required identity and
  submodule checks are green.
- PR #24 remains open, keeps `DO NOT MERGE — under review`, names the base and
  new head, and contains rerunnable commands without measured figures.

## Required evidence

- `python3 tools/checkout.py --seat builder`.
- `python3 -m unittest discover -s tests -v` and both generator `--check`
  commands.
- Before/after demonstrations for each correction, including a C1 assertion
  on the actual missing-file message.
- Configure/build/CTest output for `data/` and `policy/` under the required
  Debug/Werror flags, with real counts. State what was not run.
- `git submodule status` before and after, with no gitlink in the diff.
- SHA-256 identity output for every remaining canonical framework file and
  the two installed tools.
- The predicate/input probe output and the platform-divergence recommendation.
- Machine/platform, exact base/head SHAs, push and PR output, and the real
  report-writer output.
- State explicitly that no replay-harness result is evidence about duel
  behaviour and that Windows/MSVC was not exercised if that remains true.

## Completion-report schema

Use the Worker contract's exact base/head, changed-files, validation,
omissions, source and open-question fields. Add the C1-C4 disposition, the
predicate comparison and decision, the platform-divergence recommendation and
limits, the pre-existing-test disclosure, submodule/hash evidence, platform
gap, PR URL, and report-writer output.

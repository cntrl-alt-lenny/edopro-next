# Brief 008 — the read-failure predicate, and the platform-divergence pattern

Status: delivered

**Adjudicated 2026-09-01: NOT ACCEPTED — corrections required.** The lifecycle
status stays `delivered` because that is what it is: delivered, not archived.
`archive/` means adjudicated *and closed*; this round is adjudicated and
reopened.

Delivered by Builder as PR #24, base `9005f950`, head `ffbb7050`. Verifier
reviewed that exact head on Windows/MSVC 19.44.35228.0 on 2026-09-01: **zero
BLOCKER, four SHOULD FIX, five NOTE**. Brain independently re-derived the
load-bearing findings and **did not accept the round**. PR #24 stays open with
`DO NOT MERGE`; the full adjudication is in its PR comment.

## Corrections required before this round can be accepted

Numbered so a corrective round can cite them.

**C1 - the missing-file diagnostic regressed on Linux and Windows.**
`std::filesystem::is_directory(path, ec)` sets `ec` for a non-existent path, so
a missing file now short-circuits into the new `status_error` branch and never
reaches `"failed to open file"`. Re-derived by Brain under MSVC (`ec=3`) and by
Verifier on Linux (`ec=2`). User-visible via
`ui/src/deckbuilder/deck_controller.cpp:161`. This breaches the brief's own
protected invariant *"Behaviour on Linux and Windows must not regress."*
Fix: test `is_directory` first; treat `status_error` as fatal only when it is
not a not-found error. Add a test asserting the message, since none does.

**C2 - `ydk.h`'s contract sentence is still false as written.**
`"ok is false exactly when the path cannot be opened as a readable file or a
read fails"` is a biconditional, falsified on Windows by `NUL`, which opens and
reads cleanly yet returns `ok=0`. The brief made this sentence an acceptance
criterion. Drop `"exactly when"`; enumerate inspect/open/read failure.

**C3 - a stat failure is now fatal where the open would have worked.**
The `status_error` branch rejects without attempting the open. Demonstrated
with a momentarily-exhausted named pipe (`ec=231`). Narrow it.

**C4 - the fix closes one member of a class, not the class.**
`load_ydk()` **never returns** on a POSIX FIFO, a Windows named pipe, or
`/dev/zero`. An `is_regular_file()` whitelist would close the POSIX cases and
fail closed by construction, but Verifier measured that MSVC's `fs::status`
reports a live named pipe as `file_type::regular` with `ec==0`, so it would not
close the Windows case; only inspecting the opened handle covers both. Decide
the shape and record it. Do not describe the current fix as *portable by
construction* - it is portable by enumeration.

## Recorded findings that outlive the corrections

- **`loading_a_directory_fails_cleanly` cannot fail on any platform this
  project gates on.** Verifier rebuilt the pre-fix code and it passed on both
  Windows and Linux; every CI job is `ubuntu-latest`. The fix ships with no
  executable regression guard. This is direct evidence for this brief's own
  investigation-3 question, and must be weighed when that is finally answered.
- **A new Linux/Windows divergence on the null device** was introduced by a
  change whose purpose was removing a divergence: pre-fix both returned `ok=1`;
  post-fix Windows `NUL` returns `ok=0` while Linux `/dev/null` still returns
  `ok=1`. It arrives incidentally, via `status_error`, not via the directory
  check. Record it or remove it; do not leave it silent.

## Still UNKNOWN, and not counted against Builder

This brief had **two** deliverables. The second - the predicate comparison
table (investigation 1) and the platform-divergence mechanism recommendation
(investigation 3) - lives in Builder's completion report, which exists only in
another host's uncommitted `.git/agent-inbox/` and is not reachable from the
PR, the repository, or this SHA. Neither Verifier nor Brain has seen it.
**That half of this brief is unreviewed, not failed.** It is precisely the
class of loss that brief 011 exists to close.

One brief lives here at a time. On delivery it moves to
[`delivered/`](../delivered/), and only on adjudication to
[`archive/`](../archive/) — see [`README.md`](README.md). Template and field
meanings: [`docs/roles/builder.md`](../../roles/builder.md).

---

## MODE: IMPLEMENTATION

## Goal

Two deliverables, in this order of importance:

1. **Fix a contract violation that is live on `master` today.** Both file
   readers in this project decide "did the read fail?" with `if(file.bad())`.
   On macOS that predicate is false when the path is a directory, so both
   report success on input they cannot have read.
2. **Answer, with evidence, what mechanism should stop this class** — and
   `macOS CI` is a candidate to be argued for or against, not the assumed
   answer. See "Required investigation".

## Why this is next

`master` is currently **red on macOS**: `ctest` in `policy/` fails
`loadLflistDirectoryPathFailsCleanly`. That is not a flaky environment; it is
the test correctly reporting that the implementation is wrong on this
platform.

The same defect is present, untested and silent, in `data/`. It is the sixth
instance of "green on Linux CI, divergent on another supported platform" this
project has hit, and the first where the divergence is a **runtime semantic**
difference rather than a compiler diagnostic or a build-system quirk. That
distinction is the reason the mechanism question is genuinely open.

## Base SHA

Branch from `origin/master`. Record the SHA with `git log -1`.

## The defect

`policy/src/lf_list.cpp:286` and `data/src/ydk.cpp:180` both read via a sized
`file.read()` loop and then test `file.bad()`.

Brain reproduced the platform difference directly, on macOS/arm64, Apple
clang 21:

```
ifstream on a directory:  is_open=1  gcount=0  fail=1  eof=1  bad=0
```

macOS surfaces a directory as an **empty file (EOF)**; Linux surfaces a read
**error**. `bad()` catches only the Linux manifestation. Confirmed
end-to-end against the built library:

```
load_ydk(directory): ok=1 error=""
```

`data/include/edopro_next/data/ydk.h:56-60` states that `ok` is false
"exactly when the file could not be opened/read". That sentence is false on
macOS.

Note the comments at `lf_list.cpp:274` and `ydk.cpp:170` already anticipate
"a directory" as the motivating case. The reasoning recorded there is about a
different failure — a streambuf-level read that leaves `good()` true — and the
predicate chosen for it does not cover this one. Read both comments before
changing either; whatever you do must keep the case they were written for
working, and both comments must end up true.

## Scope

- Both call sites, fixed as one class rather than one at a time.
- Test coverage for the currently-untested `data/` site, of the same shape as
  the `policy/` test that caught it.
- The comments and the `ydk.h` contract sentence made true.
- A written recommendation on the mechanism question (below). Prose in the
  completion report and, if you conclude something durable, a proposed
  paragraph for `docs/architecture/` — do not edit `docs/state.md` or
  `docs/ROADMAP.md`.

## Non-scope

- Do **not** add or modify CI workflows. If your recommendation is a new CI
  leg, that is a proposal for Brain and the owner, not a change to make here.
  Changing `.github/workflows/` is outside routine authority.
- Do not touch `gframe/`, `ocgcore/`, or any repository setting.
- Do not widen this into a general audit of error handling. Two sites.
- Do not weaken or delete the failing test to make `ctest` green.

## Protected invariants

- **`client/`, `data/` and `policy/` build with no Qt, no Irrlicht, no vcpkg
  and no `ocgcore`.** Whatever predicate you choose must not add a dependency.
- The semantic layers stay free of UI types.
- **Failing closed is correct here.** If a read cannot be shown to have
  succeeded, the result is a failure with a non-empty `error`. Do not make a
  case "succeed with empty data" to simplify the predicate.
- Behaviour on Linux and Windows must not regress. This is a portability fix,
  not a Linux rewrite.

## Required investigation

1. **What is the right predicate?** `bad()` is one option among several —
   checking the path's type before opening, checking `!file` after the loop,
   checking `gcount()`/`eof()` combinations, or not using `ifstream` for the
   type check at all. Say what you considered, what each one does on all three
   platforms, and why you chose yours. A predicate that is *portable by
   construction* is worth more than one that enumerates known platforms.
2. **Is a directory the only input that diverges?** Check at least: a
   permission-denied path, a named pipe/FIFO, a symlink to a directory, a
   device file, and a zero-byte regular file. Say which you tested and on
   what. If some are untestable here, say so rather than guessing.
3. **The mechanism question.** Six instances so far, and they are not one
   kind:

   | # | Defect | Platform | Class |
   |---|---|---|---|
   | 1 | narrowing conversions in a test tuple | MSVC | compiler diagnostic |
   | 2 | `bench_card_search` `/RTC1` vs `/O2` | MSVC | build config |
   | 3 | QML cache `mkdir` rejects `..` | Windows | build system |
   | 4 | QML mirror staleness on copy fallback | Windows | build system |
   | 5 | unused `constexpr` under `-Werror` | Apple clang | compiler diagnostic |
   | 6 | this one | macOS libc++ | **runtime semantics** |

   Classes 1 and 5 are caught by *any* build on that toolchain. Classes 2–4
   are caught by a *configure+build*. Class 6 is caught only by *running the
   tests*, and only if a test for it exists — which is exactly why it is
   silent in `data/`.

   Argue what actually follows. Candidate mechanisms include, and are not
   limited to: a macOS CI leg; a Windows CI leg; tightening Linux CI's warning
   set to cover the diagnostic classes at lower cost; a portability rule in
   `AGENTS.md` about platform-dependent standard-library semantics; or
   deciding that some of these classes are acceptable to catch late. Cost and
   reliability are legitimate arguments — `AGENTS.md` already declines to make
   the upstream baseline a required check because it depends on a third
   party's availability, and that reasoning may or may not apply here.

   **Recommend one, and say what it would not catch.** A recommendation that
   claims to close all six classes is almost certainly wrong.

## Acceptance criteria

- `policy/` and `data/` each configure, build and pass `ctest` on macOS under
  `-DEDOPRO_NEXT_WERROR=ON`, including `loadLflistDirectoryPathFailsCleanly`.
- A new `data/` test covering the directory case, which **fails before your
  fix and passes after it** — show both.
- `load_ydk()` and `load_lflist()` agree with each other and with their
  documented contracts on every input in investigation 2.
- `ydk.h`'s contract sentence and both source comments are true as written.
- `client/` still passes 7/7; the Python suite still passes.
- CI green at the head SHA, queried rather than assumed.

## Required evidence

- The before/after for the new `data/` test, as real output.
- `ctest` output for `client/`, `data/` and `policy/` on macOS.
- The platform-behaviour probe for each input in investigation 2, with the
  actual observed stream state — not a description of it.
- `python3 -m unittest discover -s tests`. Note: a stale
  `client/build/edopro_next_semantic_trace` will be silently preferred by
  `tests/test_semantic_trace.py`; either delete it or set
  `EDOPRO_NEXT_SEMANTIC_TRACE`, and say which.
- CI check-run conclusions at the exact head SHA.
- What you did **not** run — in particular, state plainly that Windows/MSVC
  was not exercised, if it was not.

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../../roles/builder.md), plus:

- **The predicate comparison table** from investigation 1, across platforms.
- **The mechanism recommendation** from investigation 3, with what it misses.
- Anything in the existing comments at `lf_list.cpp:274` / `ydk.cpp:170` that
  turned out to be wrong, stated plainly — they were written from empirical
  work and one of them may still be right about the case it describes.

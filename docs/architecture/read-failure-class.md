# File-loader failure classification

`data::load_ydk()` and `policy::load_lflist()` read attacker- or user-selected
paths, so a result must not be successful merely because the stream reached a
state that looks like EOF. A loader either reads a regular file to completion,
or returns `ok == false` with a non-empty diagnostic.

## Decision

Both loaders perform a fail-closed preflight before constructing an
`std::ifstream`:

1. `std::filesystem::status()` rejects directories and every known
   non-regular type, including FIFOs and device files. A missing path or any
   other status error is *not* rejected at this stage; the stream open is
   allowed to provide the authoritative `failed to open file` result.
2. On Windows, a shared native read handle is opened for inspection with
   `CreateFileW()` and `GetFileType()`. This supplements, rather than replaces,
   the filesystem check because MSVC can report a live named pipe as
   `file_type::regular`. A handle that is not `FILE_TYPE_DISK` is rejected
   before the `ifstream` open.
3. Only a path that survives those checks is read with the existing sized
   `ifstream::read()` loop, and `file.bad()` remains the mid-read failure
   predicate.

This deliberately keeps `ifstream` for the actual portable file read and uses
native Windows inspection only where the standard filesystem classification is
known to be insufficient. It is not portable by construction: it is a
platform-aware preflight whose Windows supplement must be maintained with the
Windows API contract. It does, however, fail closed for the non-terminating
classes this round investigated: POSIX FIFOs and devices are rejected by
`status()`, and Windows named pipes are rejected by `GetFileType()` when a
native handle can be opened. A path changing type between preflight and the
subsequent `ifstream` open remains a normal filesystem race and is not claimed
to be eliminated here.

The observable classification is:

| Input | POSIX status | Windows supplement | Result |
| --- | --- | --- | --- |
| Missing path | status error; open attempted | failed probe; open attempted | `failed to open file` |
| Permission-denied path | status may succeed or fail; open decides | same | open diagnostic |
| Directory or symlink to directory | directory | directory | `failed to read file` |
| FIFO | non-regular | not applicable | `failed to read file` before blocking |
| Named pipe | not applicable | non-disk handle | `failed to read file` before `ifstream` |
| Device such as `/dev/zero` | non-regular | non-disk handle | `failed to read file` before blocking |
| Zero-byte regular file | regular | disk handle | successful empty parse |
| Dangling symlink | status error; open attempted | open attempted | `failed to open file` |

`NUL` on Windows is intentionally not described as a successful readable
regular file: its native handle is not `FILE_TYPE_DISK`, so it is rejected by
the same fail-closed rule. The public headers therefore enumerate inspection,
opening, and reading failures rather than making a false biconditional claim.

## Mechanism recommendation

The six observed portability failures span three different detection classes:
compiler diagnostics need a build on the affected toolchain; configuration
and build-system failures need configure plus build; runtime standard-library
semantics need a test that actually runs on the affected platform. A Linux-only
build cannot provide the last two guarantees for macOS or Windows, and a test
that only exercises a directory cannot protect the non-terminating special-file
class.

The recommendation is one cross-platform portability matrix for the
presentation-independent modules, with macOS and Windows jobs running the
`data/` and `policy/` configure/build/CTest suites. Keep those jobs
non-required initially while collecting reliability data; the owner decides
whether to make any new check required. This is a recommendation only: this
round does not edit `.github/workflows/` or branch protection.

The matrix would catch Apple libc++ runtime behavior, MSVC diagnostics and
Windows handle semantics, and the existing Linux leg would remain the cheap
baseline. It would not prove behavior on platforms absent from the matrix,
would not catch a runtime class for which no test exists, and would not replace
the upstream-baseline evidence required when upstream-facing code changes.

The costs are additional hosted runners, dependency setup, and another source
of platform-specific flakiness. A documentation rule in `AGENTS.md` is useful
to prevent unsupported portability claims but cannot observe runtime behavior;
tightening Linux warnings is cheaper but only addresses compiler-diagnostic
classes. Those alternatives are therefore complements, not substitutes for a
small runtime matrix.

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
2. On Windows, documented named-pipe namespaces (`\\.\\pipe\\` and
   `\\?\\pipe\\`, case-insensitive and accepting Windows' slash spelling) are
   rejected before any filesystem API. This ordering is required because
   `std::filesystem::status()` can connect to a named pipe as a client and
   consume its only free instance; a subsequent `CreateFileW()` probe can then
   report `ERROR_PIPE_BUSY` and leave the loader to reach `ifstream`. For other
   paths, a shared native read handle is still opened for inspection with
   `CreateFileW()` and `GetFileType()`; this supplements, rather than replaces,
   the filesystem check because MSVC can report some non-disk objects as
   `file_type::regular`. A handle that is not `FILE_TYPE_DISK` is rejected
   before the `ifstream` open.
3. Only a path that survives those checks is read with the existing sized
   `ifstream::read()` loop, and `file.bad()` remains the mid-read failure
   predicate.

This deliberately keeps `ifstream` for the actual portable file read and uses
platform-aware preflight only where the standard filesystem classification is
known to be insufficient. The named-pipe namespace guard is what closes the
non-terminating-input class: free, busy, inspection-taken, and freed-mid-load
instance states all return before `status()`, `CreateFileW()`, or `ifstream`,
so none can reach a blocking read. The guard also means loading a recognized
named-pipe path has no pipe connection as a side effect. If a caller supplies a
pipe spelling outside the documented namespaces and Windows resolves it as a
pipe, the status/handle inspection APIs may still connect; that is a known
side effect of those native APIs, not a portable guarantee.

At `338fe1e87770142ec7918553eaf43560e0657685` on Windows 11/MSVC, the
pre-guard behavior was observed input by input: a free named pipe could be
connected by `status()`, taking the only instance; a busy pipe and that
inspection-taken pipe made the native probe fail with `ERROR_PIPE_BUSY`; and
the loader reached `ifstream` and returned promptly with `ok == false` and
`failed to open file`. A probe/open timing window could let `ifstream` connect
after an instance freed, which is the blocking-read risk this guard removes.
The current Windows-only tests create a real free pipe and a connected/busy
pipe for both loaders; those tests pass on Windows 11/MSVC at the delivered
head. They do not claim to measure a freed-mid-load race; that state is closed
by the pre-status ordering argument above.

The observable classification is:

| Input | POSIX status | Windows supplement | Result |
| --- | --- | --- | --- |
| Missing path | status error; open attempted | failed probe; open attempted | `failed to open file` |
| Permission-denied path | status may succeed or fail; open decides | same | open diagnostic |
| Directory or symlink to directory | directory | directory | `failed to read file` |
| FIFO | non-regular | not applicable | `failed to read file` before blocking |
| Named pipe, free instance | not applicable | namespace guard returns before status/open; no instance consumed | `failed to read file` promptly; observed by Windows test |
| Named pipe, busy or inspection-taken | not applicable | namespace guard returns before status/open; no `ERROR_PIPE_BUSY` path | `failed to read file` promptly; observed by Windows test |
| Named pipe, freed during a possible probe/open window | not applicable | namespace guard returns before any probe/open | no blocking read by construction; race not separately measured |
| Device such as `/dev/zero` | non-regular | native handle type is inspected when the path is not a named-pipe namespace | `failed to read file` before blocking on POSIX; Windows device result not measured here |
| Zero-byte regular file | regular | disk-handle path | successful empty parse on POSIX; Windows regular-file result not separately measured here |
| Dangling symlink | status error; open attempted | open attempted; Windows result not separately measured here | `failed to open file` on POSIX; Windows not claimed |

The Windows native-handle supplement is compiled and exercised in the
data/policy builds on Windows 11/MSVC. The named-pipe tests establish the
prompt rejection and expected diagnostic for free and busy instances; they do
not establish behavior for unrelated Windows device names or dangling
symlinks. The public headers therefore enumerate inspection, opening, and
reading failures rather than making a false biconditional claim.

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

The matrix would catch Apple libc++ runtime behavior and MSVC diagnostics once
those jobs run. The existing Linux and this machine's Windows evidence are
cheap baselines, but the matrix would make both repeatable in CI. It would not
prove behavior on
platforms absent from it, would not catch a runtime class for which no test
exists, and would not replace the upstream-baseline evidence required when
upstream-facing code changes.

The costs are additional hosted runners, dependency setup, and another source
of platform-specific flakiness. A documentation rule in `AGENTS.md` is useful
to prevent unsupported portability claims but cannot observe runtime behavior;
tightening Linux warnings is cheaper but only addresses compiler-diagnostic
classes. Those alternatives are therefore complements, not substitutes for a
small runtime matrix.

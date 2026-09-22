# File-loader failure classification

`data::load_ydk()` and `policy::load_lflist()` read attacker- or user-selected
paths, so a result must not be successful merely because a stream reached a
state that looks like EOF. A loader either reads a regular file to completion,
or returns `ok == false` with a non-empty diagnostic.

## Decision

POSIX and Windows use different mechanisms because the operating systems expose
different useful predicates:

1. On Windows, each loader first opens the path with `CreateFileW()` for
   `GENERIC_READ` and the normal shared-read/write/delete mode. `GetFileType()`
   classifies that returned handle. A non-disk handle (including a named pipe,
   console, NUL or another device) is rejected before any read. A disk handle
   is read with `ReadFile()` on that same handle; the loader never closes it and
   then constructs an `ifstream` for the path. Thus the object whose type
   permits the load is the object whose bytes are read. `ERROR_PIPE_BUSY` is
   also rejected before another open. When an open fails with
   `ERROR_ACCESS_DENIED` (such as an inbound pipe or an access-restricted
   file), a zero-access probe is used only to reject a non-disk object; a disk
   result remains the ordinary open failure. Directory attributes classify a
   directory when its open is denied.
2. On POSIX, `std::filesystem::status()` rejects directories and every known
   non-regular type, including FIFOs and device files. A missing path or any
   other status error is not rejected at this stage; the same path is then
   opened by `ifstream` so it can provide the authoritative `failed to open
   file` diagnostic.
3. On POSIX, the surviving path is read by the existing sized
   `ifstream::read()` loop, and `file.bad()` remains the mid-read failure
   predicate. A zero-byte regular file is therefore a successful empty input.

The Windows rule is deliberately handle-based rather than a list of path
prefixes. Windows resolves the spelling before returning a handle, so the
classification covers the canonical pipe namespace, `\\?\pipe`, GLOBALROOT,
host aliases and extended UNC spellings without teaching the loaders those
spellings. Crucially, the accepted disk path is read through the classified
handle, closing the classification/read two-open race found at the old head.
For a named pipe, the loader returns before attempting a potentially blocking
read. If a pipe disappears between connection attempts, Windows can instead
report the ordinary `failed to open file`; that is still prompt `ok == false`
and is not treated as a successfully loaded empty file.

At `338fe1e87770142ec7918553eaf43560e0657685` on Windows 11/MSVC, the old
preflight could classify one object and then let `ifstream` open another. A
free pipe could be consumed by `status()`, while a busy pipe made the native
probe return `ERROR_PIPE_BUSY`; the loader then reached `ifstream`. At
`2a3f3e4314f16aa3e77c16446bc841e03c0b2348`, Brain and the Verifier observed
that an immediate-disconnect-and-relisten server could make both loaders
return `ok == true` with an empty result. The corrected tests include that
server state, a holding/busy client, a free single-instance server, and all
six tested spellings for both loaders. Repeated Windows 11/MSVC runs returned
prompt `ok == false`; the observed diagnostic was usually `failed to read
file`, with `failed to open file` when an alias was unavailable at the
classification attempt.

The tests do not claim a particular error string for every possible named-pipe
security or lifetime race. They establish the required invariant for the
tested states: no second open is ever read, no tested spelling reaches a
blocking read, and every result is prompt and unsuccessful.

The observable classification is:

| Input | POSIX predicate | Windows predicate | Result |
| --- | --- | --- | --- |
| Missing path | status error; `ifstream` open attempted | native open fails; open diagnostic retained | `failed to open file` |
| Share-locked regular file | open decides | read-capable native open fails sharing check | `failed to open file` |
| Directory or symlink to directory | directory status or read failure | directory attributes or non-disk handle | `failed to read file` |
| FIFO | non-regular status | not applicable | `failed to read file` before blocking |
| Named pipe, free/holding/busy/immediate-disconnect state | not applicable | same native object is classified; non-disk or `ERROR_PIPE_BUSY` is rejected | prompt `ok == false`; observed read/open diagnostic depends on server timing |
| Windows device (`CON`, `NUL`, and tested peers) | not applicable | non-disk handle or prompt failed open | prompt `ok == false` |
| Zero-byte regular file | regular status, clean EOF | disk handle, `ReadFile()` returns zero bytes | `ok == true`, empty result |
| Dangling symlink | status error; open attempted | not measured here | `failed to open file` on POSIX |

The Windows native-handle implementation is compiled and exercised in the
`data/` and `policy/` builds on Windows 11/MSVC. macOS is unavailable for this
round. Windows tests also skip POSIX-only `/dev/zero` and dangling-symlink
fixtures with visible `SKIP` lines; those POSIX cases remain covered on their
own platform. No claim is made here about an untested restricted-pipe error
mode beyond the required prompt-failure invariant.

## Mechanism recommendation

The six observed portability failures span three detection classes: compiler
diagnostics need a build on the affected toolchain; configuration and
build-system failures need configure plus build; runtime standard-library and
OS semantics need tests that actually run on the affected platform. A
Linux-only build cannot provide the last two guarantees for macOS or Windows,
and a directory-only test cannot protect the non-terminating special-file
class.

The recommendation is one cross-platform portability matrix for the
presentation-independent modules, with macOS and Windows jobs running the
`data/` and `policy/` configure/build/CTest suites. Keep those jobs
non-required initially while collecting reliability data; the owner decides
whether to make any new check required. This round does not edit
`.github/workflows/` or branch protection.

The matrix would catch Apple libc++ runtime behavior and MSVC diagnostics once
those jobs run. It would not prove behavior on platforms absent from it, would
not catch a runtime class for which no test exists, and would not replace the
upstream-baseline evidence required when upstream-facing code changes. The
costs are additional hosted runners, dependency setup, and another source of
platform-specific flakiness. Documentation rules and stricter Linux warnings
are useful complements, but neither observes foreign-platform runtime
semantics.

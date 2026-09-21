# File-loader failure classification

`data::load_ydk()` and `policy::load_lflist()` read attacker- or user-selected
paths, so a result must not be successful merely because the stream reached a
state that looks like EOF. A loader either reads a regular file to completion,
or returns `ok == false` with a non-empty diagnostic.

## Decision

Both loaders perform a fail-closed preflight before constructing an
`std::ifstream`:

1. On Windows, a shared native handle is opened first with `CreateFileW()` and
   classified with `GetFileType()` and `GetFileInformationByHandle()`. This is
   deliberately an object probe, not a path-prefix list: Windows resolves the
   spelling before returning the handle, so a successful non-disk handle is a
   named pipe or other device regardless of whether the input used `\\.\\pipe\\`,
   GLOBALROOT, a host alias, or an extended UNC spelling. `ERROR_PIPE_BUSY` is
   also rejected as a non-regular input. Directories are identified through the
   handle's directory attribute. A failed probe other than `ERROR_PIPE_BUSY`
   falls through to the stream open so missing and share-locked regular files
   retain the authoritative `failed to open file` result.
2. On POSIX, `std::filesystem::status()` rejects directories and every known
   non-regular type, including FIFOs and device files. A missing path or any
   other status error is *not* rejected at this stage; the stream open is
   allowed to provide the authoritative `failed to open file` result.
3. Only a path that survives those checks is read with the existing sized
   `ifstream::read()` loop, and `file.bad()` remains the mid-read failure
   predicate.

This deliberately keeps `ifstream` for the actual portable file read and uses
the Windows object probe before any filesystem status call. The probe may
consume a free named-pipe instance as a side effect, but it closes the handle
after classifying it and never passes that path to `ifstream`; an occupied
instance is rejected from `ERROR_PIPE_BUSY` before the stream open. Therefore
the tested pipe spellings are covered by Windows' object resolution rather
than by an enumerated prefix list. The remaining limit is an untested Windows
error mode in which a pipe server denies the probe with an error other than
`ERROR_PIPE_BUSY`; such a path is not claimed as a separately measured input.

At `338fe1e87770142ec7918553eaf43560e0657685` on Windows 11/MSVC, the
pre-guard behavior was observed input by input: a free named pipe could be
connected by `status()`, taking the only instance; a busy pipe and that
inspection-taken pipe made the native probe fail with `ERROR_PIPE_BUSY`; and
the loader reached `ifstream` and returned promptly with `ok == false` and
`failed to open file`. A probe/open timing window could let `ifstream` connect
after an instance freed, which is the blocking-read risk this guard removes.
The current Windows-only tests create a real single-instance pipe for both
loaders and exercise the GLOBALROOT, local-host, loopback-host, and extended
UNC spellings, as well as the canonical free and connected/busy cases. At the
final head they pass on Windows 11/MSVC with prompt `failed to read file`
results. The tests do not separately measure a freed-mid-load race, unrelated
Windows device names, or dangling symlinks; those remain outside the observed
input set.

The observable classification is:

| Input | POSIX status | Windows supplement | Result |
| --- | --- | --- | --- |
| Missing path | status error; open attempted | failed probe; open attempted | `failed to open file` |
| Permission-denied path | status may succeed or fail; open decides | same | open diagnostic |
| Directory or symlink to directory | directory | directory | `failed to read file` |
| FIFO | non-regular | not applicable | `failed to read file` before blocking |
| Named pipe, free instance | not applicable | native object probe classifies the returned non-disk handle; the probe may consume the instance but no `ifstream` follows | `failed to read file` promptly; observed by Windows test |
| Named pipe, busy or inspection-taken | not applicable | native probe returns `ERROR_PIPE_BUSY` and rejects before status/open | `failed to read file` promptly; observed by Windows test |
| Named pipe, freed during a possible probe/open window | not applicable | no separate race test; either a returned pipe handle or `ERROR_PIPE_BUSY` is rejected before `ifstream` | expected prompt rejection; race not separately measured |
| Device such as `/dev/zero` | non-regular | native handle type is inspected on Windows; POSIX status rejects it | `failed to read file` before blocking; Windows device result not measured here |
| Zero-byte regular file | regular | disk-handle path | successful empty parse on POSIX; Windows regular-file result not separately measured here |
| Dangling symlink | status error; open attempted | open attempted; Windows result not separately measured here | `failed to open file` on POSIX; Windows not claimed |

The Windows native-handle preflight is compiled and exercised in the
data/policy builds on Windows 11/MSVC. The named-pipe tests establish prompt
rejection and the expected diagnostic for the four non-canonical spellings
above, plus free and busy canonical instances. They do not establish behavior
for unrelated Windows device names, dangling symlinks, or a pipe whose server
denies the native probe with an error other than `ERROR_PIPE_BUSY`. The public
headers therefore enumerate inspection, opening, and reading failures rather
than making a false biconditional claim.

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

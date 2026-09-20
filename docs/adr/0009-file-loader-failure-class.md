# ADR 0009 — Fail closed before opening non-regular loader inputs

## Context

`load_ydk()` and `load_lflist()` must distinguish a readable regular file from
paths whose stream operation does not provide a terminating file read. The
previous directory-only correction treated a `filesystem` status error as a
fatal inspection failure, misreported missing files, and still allowed
POSIX FIFOs, `/dev/zero`, and Windows named pipes to block indefinitely.
MSVC's filesystem layer can classify a live named pipe as regular, so a
portable-looking `is_regular_file()` whitelist alone is insufficient.

## Decision

Use a shared policy implemented at both loader call sites: reject directories
and known non-regular filesystem types before opening; defer status errors to
the normal `ifstream` open; and, on Windows, inspect a native shared read
handle with `GetFileType()` so non-disk handles such as named pipes are
rejected. Keep the sized `ifstream::read()` loop and its `bad()` check for the
regular-file read itself.

This is a platform-aware fail-closed preflight, not a claim of portability by
construction. It covers the investigated POSIX and Windows special-file cases
without replacing the ordinary stream reader, while preserving the correct
open diagnostic for missing and inaccessible paths. A type change after the
preflight is a filesystem race outside this decision.

## Alternatives considered

1. **Keep `bad()` alone.** Rejected: libc++ can surface a directory as clean
   EOF, and special files can never reach a terminating read.
2. **Check `!file` after the read loop.** Rejected: it improves the directory
   symptom but cannot prevent a blocking open/read on a FIFO, device, or named
   pipe.
3. **Use only `is_regular_file()`.** Rejected: it closes the POSIX cases but
   the measured MSVC named-pipe classification is `regular`.
4. **Replace `ifstream` with native reads everywhere.** Rejected for this
   round: it would multiply platform-specific I/O code and change the
   established read path unnecessarily. Native inspection is limited to the
   Windows classification gap.

## Status

Accepted for Brief 011. The mechanism recommendation for catching future
platform divergence is recorded in
`docs/architecture/read-failure-class.md`; no CI check or branch protection
setting is changed by this ADR.

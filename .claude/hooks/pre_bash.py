#!/usr/bin/env python3

"""pre_bash.py -- Claude Code PreToolUse hook for Bash.

Intercepts Bash calls before they run, and guards two things about `git push`.

# Guard 1: never push to master

AGENTS.md: every change is a pull request, and no agent merges anything -- the
human owner does, on GitHub. Nothing in the normal loop pushes `master`. Brain
fast-forwards its local `master` from `origin` after the owner merges; it never
pushes it back. So a push whose target is `master` is either a mistake or a
role boundary being crossed, and blocking it costs nothing legitimate.

`git push origin --delete master` is caught by the same rule, deliberately.

# Guard 2: derived tables must not drift

`tools/generate_messages.py` and `tools/generate_protocol_constants.py` emit
files that are derived, not hand-edited, and CI checks that first. Both run in
under a tenth of a second, so checking them before a push catches the drift
while the agent still has the failing output in front of it and can fix it in
the same turn -- instead of finding out from a CI failure several steps later.

Deliberately NOT guarded here: the cmake/ctest cycles. Those take minutes, and
a hook slow enough to be resented is a hook that gets bypassed. AGENTS.md's
per-layer evidence table is where those belong -- as something an agent must
run and paste, not something a hook runs silently on its behalf.

# Bypass

  SKIP_EDOPRO_NEXT_HOOK=1 git push ...
  git push --no-verify ...          # already-explicit hook bypass, respected

Exit codes:
  0 = allow the tool call
  2 = block it (PreToolUse semantics)
"""

from __future__ import annotations

import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PROTECTED = ("master",)

GENERATORS = (
    ("tools/generate_messages.py", "--check"),
    ("tools/generate_protocol_constants.py", "--check"),
)

# Find `git push` even with env prefixes, flags, or shell chaining around it.
# Deliberately does not match a push already carrying --no-verify.
_PUSH_RE = re.compile(r"(?:^|\s|&&|\|\||;)\s*git\s+push\b(?![^\n;&|]*--no-verify)")

# Where a `git push ...` segment ends: at a shell separator or a newline.
_SEGMENT_END_RE = re.compile(r"[\n;]|&&|\|\||(?<!\|)\|(?!\|)")


def _read_event() -> dict:
    try:
        return json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError, OSError):
        return {}


def _push_segments(command: str) -> list[str]:
    """Return each `git push ...` invocation in `command`, as its own string."""
    segments = []
    for match in _PUSH_RE.finditer(command):
        rest = command[match.end():]
        end = _SEGMENT_END_RE.search(rest)
        segments.append(rest[: end.start()] if end else rest)
    return segments


def _current_branch() -> str | None:
    try:
        out = subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "--abbrev-ref", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    return out or None


def _targets_protected_branch(segment: str) -> str | None:
    """Name the protected branch this push would write, or None.

    Two cases: an explicit refspec naming it (`origin master`, `HEAD:master`,
    `--delete master`), or a bare `git push` with no refspec while the current
    branch is itself protected.
    """
    try:
        args = shlex.split(segment)
    except ValueError:
        # Unbalanced quotes -- can't parse it, so fall back to a substring
        # check rather than waving it through.
        args = segment.split()

    positional = [a for a in args if not a.startswith("-")]

    for arg in args:
        if arg.startswith("-"):
            continue
        # A refspec's destination is whatever follows the last colon.
        name = arg.rsplit(":", 1)[-1].removeprefix("refs/heads/")
        if name in PROTECTED:
            return name

    # `git push`, `git push origin`, `git push -u origin`: no refspec, so the
    # current branch is what would be pushed.
    if len(positional) <= 1:
        branch = _current_branch()
        if branch in PROTECTED:
            return branch

    return None


def _generator_drift() -> list[tuple[str, str]]:
    failures = []
    for script, flag in GENERATORS:
        proc = subprocess.run(
            [sys.executable, script, flag],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            failures.append((script, (proc.stdout or "") + (proc.stderr or "")))
    return failures


def main() -> int:
    if os.environ.get("SKIP_EDOPRO_NEXT_HOOK"):
        return 0

    event = _read_event()
    if event.get("tool_name") != "Bash":
        return 0

    command = event.get("tool_input", {}).get("command", "") or ""
    segments = _push_segments(command)
    if not segments:
        return 0

    for segment in segments:
        protected = _targets_protected_branch(segment)
        if protected:
            print(
                f"[edopro-next] `git push` blocked: it targets `{protected}`.\n"
                "\n"
                "AGENTS.md: every change is a pull request, and no agent merges.\n"
                "Push a task branch and open a PR; the owner merges on GitHub.\n"
                "\n"
                "If this really is intended:\n"
                "  SKIP_EDOPRO_NEXT_HOOK=1 git push ...\n",
                file=sys.stderr,
            )
            return 2

    failures = _generator_drift()
    if failures:
        print(
            "[edopro-next] `git push` blocked: derived tables are out of date.\n"
            "These files are generated, not hand-edited, and CI checks them\n"
            "first. Regenerate (drop --check) and commit the result.\n",
            file=sys.stderr,
        )
        for script, output in failures:
            print(f"--- {script} --check ---", file=sys.stderr)
            print(output, file=sys.stderr)
        print(
            "Bypass once:\n"
            "  SKIP_EDOPRO_NEXT_HOOK=1 git push ...\n"
            "  # or: git push --no-verify\n",
            file=sys.stderr,
        )
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())

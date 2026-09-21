"""Reject measured figures in PR bodies before they silently go stale.

PR bodies must name commands a reader can rerun, not record counts that are
silently invalidated when strict branch protection makes the PR's head move.
This is intentionally a shape check rather than a prose/range parser: it
fails closed on a line that combines a number with an evidence metric, and it
does not pretend to prove that a quoted range matches GitHub's current head.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys


_METRIC_RE = re.compile(
    r"\b(?:files?|insertions?|deletions?|tests?|failures?|errors?|"
    r"warnings?|skips?|skipped|passed|seconds?|milliseconds?|ms|"
    r"timings?|duration|lines?)\b",
    re.IGNORECASE,
)
_NUMBER_RE = re.compile(
    r"\b(?:\d[\d,]*(?:\.\d+)?|zero|one|two|three|four|five|six|seven|"
    r"eight|nine|ten|eleven|twelve|thirteen|fourteen|fifteen|sixteen|"
    r"seventeen|eighteen|nineteen|twenty|thirty|forty|fifty|sixty|"
    r"seventy|eighty|ninety|hundred)\b",
    re.IGNORECASE,
)
_COMMAND_RE = re.compile(
    r"(?:^|[`$>\-*]\s*)(?:git|python3?|cmake|ctest|ninja|gh|pytest|"
    r"unittest)\b",
    re.IGNORECASE | re.MULTILINE,
)


def violations(body: str) -> list[tuple[int, str]]:
    """Return line-numbered policy violations in *body*."""
    findings = []
    for line_number, line in enumerate(body.splitlines(), 1):
        if _METRIC_RE.search(line) and _NUMBER_RE.search(line):
            findings.append((
                line_number,
                "measured evidence figure found; name a rerunnable command "
                "instead of quoting counts or timings",
            ))
    if not _COMMAND_RE.search(body):
        findings.append((
            1,
            "no rerunnable command found; evidence must name a command "
            "a reader can run at the current head",
        ))
    return findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check a PR body for stale measured evidence figures.")
    parser.add_argument(
        "--file", type=pathlib.Path,
        help="read the body from this file (default: stdin)",
    )
    args = parser.parse_args(argv)
    try:
        body = (args.file.read_text(encoding="utf-8")
                if args.file else sys.stdin.read())
    except OSError as exc:
        print(f"error: cannot read PR body: {exc}", file=sys.stderr)
        return 2

    findings = violations(body)
    if findings:
        source = str(args.file) if args.file else "stdin"
        for line_number, message in findings:
            print(f"{source}:{line_number}: {message}", file=sys.stderr)
        return 1

    print("PR body uses rerunnable evidence commands and contains no measured figures.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Reject measured figures in PR bodies before they silently go stale.

PR bodies must name commands a reader can rerun, not record counts that are
silently invalidated when strict branch protection makes the PR's head move.
The checker recognises evidence-shaped count and ratio phrases over the whole
body, so a hard-wrapped sentence is still one sentence. It masks identifiers,
dates, commit hashes, line citations and version numbers before matching; it
does not pretend to parse arbitrary prose or prove that a quoted range matches
GitHub's current head.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys


_NUMBER = r"\d[\d,]*(?:\.\d+)?"
_MEASURE = (
    r"files?|insertions?|deletions?|tests?|failures?|errors?|warnings?|"
    r"skips?|skipped|passed|passing|seconds?|milliseconds?|ms|timings?|"
    r"duration"
)
_IDENTIFIER_RE = re.compile(
    r"(?:"
    r"\b0x[0-9a-f]+\b|"
    r"\b[0-9a-f]{7,40}\b|"
    r"\b(?:PR|Brief(?:-ID)?|R|E|C)\s*#?\s*\d+(?:[-–]\s*[A-Z]?\d+)*\b|"
    r"\b\d{4}-\d{2}-\d{2}(?:[Tt][0-9:.+-]+)?\b|"
    r"\b(?:Python|Python3|CMake|Ninja|MSVC|Qt|C\+\+|Visual\s+Studio|"
    r"Windows|macOS|Ubuntu|v)\s*[^0-9\n]{0,8}\d+(?:\.\d+)*\b|"
    r"\b(?:line|lines|L)\s*\d+(?:\s*[-–]\s*\d+)?\b"
    r")",
    re.IGNORECASE,
)
_COMMAND_RE = re.compile(
    r"(?:^|[`$>\-*]\s*)(?:git|python3?|cmake|ctest|ninja|gh|pytest|"
    r"unittest)\b",
    re.IGNORECASE | re.MULTILINE,
)
_COUNT_BEFORE_METRIC_RE = re.compile(
    rf"\b{_NUMBER}\b(?:[ \t]+|\n[ \t]*)\b(?:{_MEASURE})\b",
    re.IGNORECASE,
)
_METRIC_BEFORE_COUNT_RE = re.compile(
    rf"\b(?:{_MEASURE})\b[ \t]*(?:[:=][ \t]*)?\b{_NUMBER}\b",
    re.IGNORECASE,
)
_SUITE_TABLE_RE = re.compile(
    rf"\|\s*(?:unittest|ctest|pytest)\s*\|\s*{_NUMBER}"
    rf"(?:\s*/\s*{_NUMBER})?\s*\|",
    re.IGNORECASE,
)
_SUITE_RATIO_RE = re.compile(
    rf"\b(?:ctest|data|policy|client|ui|ci|unittest|pytest)\b"
    rf"[^\n|]*?\b{_NUMBER}\s*/\s*{_NUMBER}\b",
    re.IGNORECASE,
)
_SUITE_STATUS_RE = re.compile(
    rf"\b(?:data|policy|ci)\b\s+{_NUMBER}\s+"
    r"(?:green|passed|passing|success|successful)\b",
    re.IGNORECASE,
)
_EVIDENCE_RESULT_RE = re.compile(
    rf"\bevidence\s*:\s*{_NUMBER}\b"
    rf"(?:[^\n|]|\n(?!\s*[-*|])){{0,80}}?\btests?\s+"
    r"(?:passed|passing|green|successful)\b",
    re.IGNORECASE | re.DOTALL,
)


def _masked_body(body: str) -> str:
    """Blank non-evidence numerals without changing offsets or line numbers."""
    return _IDENTIFIER_RE.sub(lambda match: " " * len(match.group()), body)


def _measurement_matches(body: str) -> list[re.Match[str]]:
    masked = _masked_body(body)
    matches = []
    for pattern in (
        _COUNT_BEFORE_METRIC_RE,
        _METRIC_BEFORE_COUNT_RE,
        _SUITE_TABLE_RE,
        _SUITE_RATIO_RE,
        _SUITE_STATUS_RE,
        _EVIDENCE_RESULT_RE,
    ):
        matches.extend(pattern.finditer(masked))
    return matches


def violations(body: str) -> list[tuple[int, str]]:
    """Return line-numbered policy violations in *body*.

    Evidence matching is deliberately body-level: PR prose is hard-wrapped,
    and a line-local rule cannot see ``reported 112`` followed by ``tests``.
    """
    findings = []
    measured_message = (
        "measured evidence figure found; name a rerunnable command "
        "instead of quoting counts or timings"
    )
    seen_lines = set()
    for match in _measurement_matches(body):
        line_number = body.count("\n", 0, match.start()) + 1
        if line_number not in seen_lines:
            findings.append((line_number, measured_message))
            seen_lines.add(line_number)
    findings.sort()
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

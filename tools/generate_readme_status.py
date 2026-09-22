#!/usr/bin/env python3
"""Generate the README's "What works" block from docs/ROADMAP.md.

`docs/ROADMAP.md` is the single source of truth for project status. This tool
*derives* the README's status block from it, so the landing page cannot state a
status by hand and drift. It does not keep a second copy of the roadmap: the
milestone list, each milestone's status and each still-open item are read out
of ROADMAP.md every time.

What is read, and how:

  * a `## M<N> - <title>` heading is a milestone. A trailing `done` or
    `in progress` marker is its declared status; no marker means "not started",
    the third word of the roadmap's own status vocabulary.
  * a `- [x]` / `- [ ]` item at column 0 beneath it is one of its items.
  * any other `##` heading ends the milestone list (the roadmap's unnumbered
    trailing sections are not milestones).

The roadmap is also checked against itself, so a heading and its checkboxes
cannot contradict each other and still produce a plausible-looking README: a
`done` milestone must have every item checked, a milestone with no marker must
have none checked, and an `in progress` milestone must have at least one item
still open. Any of these fails loudly rather than being averaged away.

Usage:
    python tools/generate_readme_status.py            # rewrite the README block
    python tools/generate_readme_status.py --check    # fail if it is stale

The check runs in the Python test suite (`tests/test_readme_status.py`), the
same way the other generators' checks are exercised. It changes no workflow.
"""
from __future__ import annotations

import argparse
import dataclasses
import difflib
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
README = REPO / "README.md"
ROADMAP = REPO / "docs" / "ROADMAP.md"

BEGIN = "<!-- BEGIN GENERATED readme-status (tools/generate_readme_status.py; do not edit by hand) -->"
END = "<!-- END GENERATED readme-status -->"

DONE = "done"
IN_PROGRESS = "in progress"
NOT_STARTED = "not started"

# The milestone whose status answers "can I duel with this today?". Found by
# title rather than by number so renumbering the roadmap cannot silently point
# the answer at a different milestone.
DUEL_MILESTONE_TITLE = "Duel field"
UPSTREAM_CLIENT = "https://github.com/edo9300/edopro"

_HEADING_RE = re.compile(r"^##\s+(M\d+)\s+[—–-]\s+(.*?)\s*$")
_MARKER_RE = re.compile(
    r"\s*(?:✅\s*done|\U0001F536\s*in progress)\s*$", re.IGNORECASE
)
_ITEM_RE = re.compile(r"^- \[([ xX])\]\s?(.*)$")
_BOLD_LEAD_RE = re.compile(r"^\*\*(.+?)\*\*")


class RoadmapError(ValueError):
    """The roadmap cannot be turned into a status block honestly."""


@dataclasses.dataclass
class Milestone:
    key: str
    title: str
    declared: str | None
    items: list[tuple[bool, str]]

    @property
    def open_items(self) -> list[str]:
        return [label(text) for done, text in self.items if not done]

    @property
    def status(self) -> str:
        return self.declared or NOT_STARTED


def label(item_text: str) -> str:
    """A one-line name for a roadmap item: its bold lead, else its first line."""
    text = " ".join(item_text.split())
    bold = _BOLD_LEAD_RE.match(text)
    if bold:
        text = bold.group(1)
    else:
        text = item_text.strip().splitlines()[0].strip()
    return text.rstrip(".:; ")


def parse_roadmap(text: str) -> list[Milestone]:
    milestones: list[Milestone] = []
    current: Milestone | None = None
    item_lines: list[str] | None = None
    item_done = False

    def close_item() -> None:
        nonlocal item_lines
        if current is not None and item_lines is not None:
            current.items.append((item_done, "\n".join(item_lines)))
        item_lines = None

    for raw in text.replace("\r\n", "\n").split("\n"):
        heading = re.match(r"^##\s", raw)
        if heading:
            close_item()
            m = _HEADING_RE.match(raw)
            if not m:
                current = None
                continue
            title_and_marker = m.group(2)
            marker = _MARKER_RE.search(title_and_marker)
            declared = None
            if marker:
                declared = IN_PROGRESS if "progress" in marker.group(0).lower() else DONE
                title_and_marker = title_and_marker[: marker.start()]
            current = Milestone(m.group(1), title_and_marker.strip(), declared, [])
            milestones.append(current)
            continue
        if current is None:
            continue
        item = _ITEM_RE.match(raw)
        if item:
            close_item()
            item_done = item.group(1).lower() == "x"
            item_lines = [item.group(2)]
        elif item_lines is not None and raw.startswith((" ", "\t")):
            item_lines.append(raw.strip())
        else:
            close_item()
    close_item()

    validate(milestones)
    return milestones


def validate(milestones: list[Milestone]) -> None:
    if not milestones:
        raise RoadmapError("no `## M<N> - <title>` milestones found in the roadmap")
    seen: set[str] = set()
    for ms in milestones:
        where = f"{ms.key} ({ms.title})"
        if ms.key in seen:
            raise RoadmapError(f"milestone {ms.key} appears twice")
        seen.add(ms.key)
        if not ms.items:
            raise RoadmapError(f"{where} has no checkbox items, so its status cannot be checked")
        checked = sum(1 for done, _ in ms.items if done)
        total = len(ms.items)
        if ms.status == DONE and checked != total:
            raise RoadmapError(f"{where} is marked done but only {checked} of {total} items are checked")
        if ms.status == NOT_STARTED and checked:
            raise RoadmapError(
                f"{where} has no done/in-progress marker (so it is not started) "
                f"but {checked} of {total} items are checked"
            )
        if ms.status == IN_PROGRESS and checked == total:
            raise RoadmapError(f"{where} is marked in progress but every item is checked")
    if not any(ms.title == DUEL_MILESTONE_TITLE for ms in milestones):
        raise RoadmapError(f"no milestone titled {DUEL_MILESTONE_TITLE!r}; cannot say whether a duel is playable")


def _duel_line(milestones: list[Milestone]) -> str:
    ms = next(m for m in milestones if m.title == DUEL_MILESTONE_TITLE)
    if ms.status == NOT_STARTED:
        lead = f"The duel field ({ms.key}) is not started."
    elif ms.status == IN_PROGRESS:
        lead = f"The duel field ({ms.key}) is in progress."
    else:
        lead = (
            f"The duel field ({ms.key}) is marked done in the roadmap; "
            "read its exit criterion before relying on that."
        )
    return f"{lead} To duel today, use [EDOPro]({UPSTREAM_CLIENT})."


def _group(milestones: list[Milestone], status: str) -> list[str]:
    lines = []
    for ms in milestones:
        if ms.status != status:
            continue
        entry = f"- **{ms.key}** {ms.title}"
        if status == IN_PROGRESS:
            entry += " — still open: " + "; ".join(ms.open_items)
        lines.append(entry)
    return lines


def render(milestones: list[Milestone]) -> str:
    """The exact text between the BEGIN and END markers."""
    parts = [_duel_line(milestones), ""]
    for heading, status in (
        ("Done", DONE),
        ("In progress", IN_PROGRESS),
        ("Planned, not started", NOT_STARTED),
    ):
        group = _group(milestones, status)
        if not group:
            continue
        parts += [f"**{heading}**", ""] + group + [""]
    parts.append(
        "<sub>Generated from [`docs/ROADMAP.md`](docs/ROADMAP.md) by "
        "`tools/generate_readme_status.py`; the test suite fails if it drifts. "
        "Detail: [what exists today](docs/capabilities.md).</sub>"
    )
    return "\n".join(parts)


def _normalise(text: str) -> str:
    return text.replace("\r\n", "\n")


def extract_block(readme_text: str) -> str | None:
    """The text between the markers, or None if the block is absent or malformed."""
    text = _normalise(readme_text)
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        return None
    start = text.index(BEGIN) + len(BEGIN)
    stop = text.index(END)
    if stop < start:
        return None
    return text[start:stop].strip("\n")


def apply_block(readme_text: str, rendered: str) -> str:
    text = _normalise(readme_text)
    if extract_block(text) is None:
        raise RoadmapError(
            "README has no single generated block; add the BEGIN/END markers "
            f"first:\n  {BEGIN}\n  {END}"
        )
    start = text.index(BEGIN) + len(BEGIN)
    stop = text.index(END)
    return text[:start] + "\n" + rendered + "\n" + text[stop:]


def check(readme_text: str, roadmap_text: str) -> list[str]:
    """Problems found; an empty list means the README block is current."""
    milestones = parse_roadmap(roadmap_text)
    expected = render(milestones)
    actual = extract_block(readme_text)
    if actual is None:
        return [
            "README has no single generated status block "
            f"(expected exactly one of each marker):\n  {BEGIN}\n  {END}"
        ]
    if actual == expected:
        return []
    diff = difflib.unified_diff(
        actual.splitlines(), expected.splitlines(),
        "README.md (block as committed)", "generated from docs/ROADMAP.md",
        lineterm="",
    )
    return ["README status block does not match docs/ROADMAP.md:\n" + "\n".join(diff)]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="verify the README block is up to date")
    ap.add_argument("--readme", type=pathlib.Path, default=README, help=argparse.SUPPRESS)
    ap.add_argument("--roadmap", type=pathlib.Path, default=ROADMAP, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)

    try:
        roadmap_text = args.roadmap.read_text(encoding="utf-8")
        readme_text = args.readme.read_text(encoding="utf-8")
        if args.check:
            problems = check(readme_text, roadmap_text)
            if problems:
                print("\n".join(problems), file=sys.stderr)
                print("\nrun: python tools/generate_readme_status.py", file=sys.stderr)
                return 1
            print("README status block is up to date")
            return 0
        updated = apply_block(readme_text, render(parse_roadmap(roadmap_text)))
        if updated != _normalise(readme_text):
            args.readme.write_text(updated, encoding="utf-8", newline="\n")
            print(f"wrote {args.readme}")
        else:
            print("README status block already up to date")
        return 0
    except (RoadmapError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())

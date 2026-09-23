#!/usr/bin/env python3
"""Check that the app's home screen states the roadmap's status, not its own.

`ui/qml/screens/HomeScreen.qml` shows one `StatusRow` per roadmap milestone.
The status a row shows is typed into the QML, because the shell is a compiled
binary with no runtime access to `docs/`. That copy is what this tool keeps
honest: it reads the rows out of the QML and the milestones out of
`docs/ROADMAP.md` (through `tools/generate_readme_status.py`, the same parser
the README block uses) and fails when they disagree.

A `StatusRow` is checked for:

  * `milestone`: must be a roadmap milestone key, and every roadmap milestone
    must have exactly one row, so a milestone added to the roadmap cannot be
    left off the home screen;
  * `title`: must equal the roadmap's milestone title;
  * `status`: must equal the roadmap's status word, `done`, `in progress` or
    `not started`;
  * `detail`: must not contain a status word, so that the only status a row
    states is the one checked above.

The screen's own introduction must not claim "not a playable client" once the
roadmap marks the duel field done.

Usage:
    python tools/check_home_status.py            # exit 1 on any disagreement

It runs in the Python test suite (`tests/test_home_status.py`), which CI runs
in the "Regression harness" job. It changes no workflow.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

import tools.generate_readme_status as grs  # noqa: E402

HOME_SCREEN = REPO / "ui" / "qml" / "screens" / "HomeScreen.qml"
ROADMAP = REPO / "docs" / "ROADMAP.md"

# Words a row's free-text detail must not use, since a status stated there
# would be a second, unchecked copy of the one that is checked.
STATUS_WORDS = (
    "done", "in progress", "not started", "planned", "working", "complete",
    "completed", "finished", "shipped", "unfinished",
)

_ROW_RE = re.compile(r"StatusRow\s*\{(?P<body>[^{}]*)\}")
_PROP_RE = re.compile(r'^\s*(?P<key>[a-zA-Z_]\w*)\s*:\s*"(?P<value>(?:[^"\\]|\\.)*)"\s*$', re.MULTILINE)
INTRO_CLAIM = "not a playable client"


def parse_rows(qml_text: str) -> list[dict[str, str]]:
    """The string properties of every `StatusRow { ... }` in the QML."""
    rows = []
    for m in _ROW_RE.finditer(qml_text):
        rows.append({p["key"]: p["value"] for p in _PROP_RE.finditer(m.group("body"))})
    return rows


def problems(qml_text: str, roadmap_text: str) -> list[str]:
    milestones = {ms.key: ms for ms in grs.parse_roadmap(roadmap_text)}
    rows = parse_rows(qml_text)
    found: list[str] = []
    if not rows:
        return ["no StatusRow found in the home screen; the check would otherwise pass vacuously"]

    seen: dict[str, int] = {}
    for i, row in enumerate(rows, 1):
        key = row.get("milestone", "")
        where = f"row {i} ({row.get('title', '?')!r})"
        if not key:
            found.append(f"{where} has no `milestone`; every status must name the roadmap milestone it comes from")
            continue
        seen[key] = seen.get(key, 0) + 1
        ms = milestones.get(key)
        if ms is None:
            found.append(f"{where} names milestone {key}, which is not in docs/ROADMAP.md")
            continue
        if row.get("title") != ms.title:
            found.append(f"{key}: home screen title {row.get('title')!r} != roadmap title {ms.title!r}")
        if row.get("status") != ms.status:
            found.append(
                f"{key} ({ms.title}): home screen says {row.get('status')!r}, "
                f"docs/ROADMAP.md says {ms.status!r}"
            )
        detail = row.get("detail", "").lower()
        for word in STATUS_WORDS:
            if re.search(rf"\b{re.escape(word)}\b", detail):
                found.append(f"{key}: detail states a status word ({word!r}); only the checked `status` may")
    for key in milestones:
        if key not in seen:
            found.append(f"{key} ({milestones[key].title}) is in docs/ROADMAP.md but has no row on the home screen")
    for key, n in seen.items():
        if n > 1:
            found.append(f"{key} has {n} rows on the home screen; expected exactly one")

    duel = next((ms for ms in milestones.values() if ms.title == grs.DUEL_MILESTONE_TITLE), None)
    if duel is not None and duel.status == grs.DONE and INTRO_CLAIM in qml_text.lower():
        found.append(
            f"the home screen still says {INTRO_CLAIM!r} but docs/ROADMAP.md marks "
            f"{duel.key} ({duel.title}) done"
        )
    return found


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--qml", type=pathlib.Path, default=HOME_SCREEN, help=argparse.SUPPRESS)
    ap.add_argument("--roadmap", type=pathlib.Path, default=ROADMAP, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)
    try:
        found = problems(args.qml.read_text(encoding="utf-8"), args.roadmap.read_text(encoding="utf-8"))
    except (grs.RoadmapError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if found:
        print("home screen status does not match docs/ROADMAP.md:", file=sys.stderr)
        for line in found:
            print(f"  - {line}", file=sys.stderr)
        return 1
    print("home screen status matches docs/ROADMAP.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())

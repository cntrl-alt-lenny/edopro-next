"""CLI: emit a deterministic trace for a replay file.

    python -m tools.replaytrace <replay.yrpX> [--name NAME]
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from .reader import ReplayError, parse_file
from .trace import render


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="replaytrace")
    ap.add_argument("replay", type=pathlib.Path)
    ap.add_argument("--name", help="logical fixture name (defaults to the file stem)")
    ap.add_argument("-o", "--output", type=pathlib.Path, help="write to a file instead of stdout")
    args = ap.parse_args(argv)

    try:
        replay = parse_file(args.replay)
    except ReplayError as exc:
        print(f"error: {args.replay}: {exc}", file=sys.stderr)
        return 1

    text = render(replay, source_name=args.name or args.replay.stem)
    if args.output:
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

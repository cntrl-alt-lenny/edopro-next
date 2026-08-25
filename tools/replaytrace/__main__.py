"""CLI: emit a deterministic trace for a replay file.

    python -m tools.replaytrace <replay.yrpX> [--name NAME]
    python -m tools.replaytrace <replay.yrpX> --emit-packets stream.pkts
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from .reader import ReplayError, frame_packets, parse_file
from .trace import render


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="replaytrace")
    ap.add_argument("replay", type=pathlib.Path)
    ap.add_argument("--name", help="logical fixture name (defaults to the file stem)")
    ap.add_argument("-o", "--output", type=pathlib.Path, help="write to a file instead of stdout")
    ap.add_argument("--emit-packets", type=pathlib.Path, metavar="PATH",
                    help="write the framed duel-message stream instead of a trace; "
                         "this is what the C++ semantic decoder consumes")
    args = ap.parse_args(argv)

    try:
        replay = parse_file(args.replay)
    except ReplayError as exc:
        print(f"error: {args.replay}: {exc}", file=sys.stderr)
        return 1

    if args.emit_packets:
        if not replay.header.is_yrpx:
            print(f"error: {args.replay}: only a yrpX carries a message stream", file=sys.stderr)
            return 1
        args.emit_packets.write_bytes(frame_packets(replay.packets))
        return 0

    text = render(replay, source_name=args.name or args.replay.stem)
    if args.output:
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

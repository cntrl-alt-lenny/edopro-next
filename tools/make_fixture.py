#!/usr/bin/env python3
"""Turn a real EDOPro replay into a committable test fixture.

Replays are excellent regression fixtures - they are real recorded duels - but
a raw one carries the player handles of whoever happened to be duelling. Those
are identity, not behaviour, and have no business in a public repository.

This rewrites the player-name fields (including those inside an embedded yrp1)
to neutral placeholders, and re-emits a valid replay. Nothing else is altered:
the duel-message stream, seeds, duel flags and parameters are untouched, so the
fixture still exercises exactly what the original did.

    python tools/make_fixture.py IN.yrpX OUT.yrpX
    python tools/make_fixture.py --extract-yrp1 IN.yrpX OUT.yrp

The second form lifts the YRP1 that modern EDOPro embeds inside a .yrpX out to
a standalone file, so plain-format parsing is exercised by a real recording
rather than a synthesised one. The extracted bytes are the embedded packet's
payload verbatim; sanitisation has already been applied to it in place.

Note that the trace format deliberately excludes names anyway; this is about
the committed binary, not the golden file.
"""
from __future__ import annotations

import argparse
import lzma
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from tools.replaytrace import reader as R  # noqa: E402

NAME_SLOT = 40  # 20 x uint16, UTF-16LE, NUL-padded


def _neutral(index: int) -> bytes:
    name = f"Player{chr(ord('A') + index)}"
    raw = name.encode("utf-16-le")
    if len(raw) > NAME_SLOT:  # pragma: no cover - placeholders are short
        raise ValueError("placeholder name too long")
    return raw + b"\x00" * (NAME_SLOT - len(raw))


def _sanitise_body(body: bytes, header: R.ReplayHeader) -> bytes:
    out = bytearray(body)
    if header.single_mode:
        for i in range(2):
            off = i * NAME_SLOT
            out[off:off + NAME_SLOT] = _neutral(i)
        return bytes(out)

    home = struct.unpack_from("<I", out, 0)[0]
    pos = 4
    idx = 0
    for _ in range(home):
        out[pos:pos + NAME_SLOT] = _neutral(idx)
        pos += NAME_SLOT
        idx += 1
    away = struct.unpack_from("<I", out, pos)[0]
    pos += 4
    for _ in range(away):
        out[pos:pos + NAME_SLOT] = _neutral(idx)
        pos += NAME_SLOT
        idx += 1
    return bytes(out)


def _recompress(body: bytes, header: R.ReplayHeader) -> bytes:
    lclppb = header.props[0]
    dict_size = int.from_bytes(header.props[1:5], "little")
    pb, rem = divmod(lclppb, 45)
    lp, lc = divmod(rem, 9)
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": dict_size, "lc": lc, "lp": lp, "pb": pb}]
    comp = lzma.LZMACompressor(format=lzma.FORMAT_RAW, filters=filters)
    return comp.compress(body) + comp.flush()


def _rebuild(data: bytes) -> bytes:
    """Sanitise one replay (recursively for an embedded yrp1) and re-emit it."""
    header = R.parse_header(data)
    body = data[header.size:]
    if header.compressed:
        body = R._decompress(body, header)

    body = _sanitise_body(body, header)

    # Rewrite an embedded yrp1 in place, adjusting its packet length.
    if header.is_yrpx:
        rebuilt = bytearray(body[:_payload_start(body, header)])
        cur = R._Cursor(body)
        cur.pos = len(rebuilt)
        while cur.remaining >= 5:
            msg = cur.u8()
            length = cur.u32()
            payload = cur.take(length)
            if msg == R.OLD_REPLAY_MODE:
                try:
                    payload = _rebuild(payload)
                except R.ReplayError:
                    pass
            rebuilt += bytes([msg]) + struct.pack("<I", len(payload)) + payload
        body = bytes(rebuilt)

    head = bytearray(data[:header.size])
    struct.pack_into("<I", head, 16, len(body))  # datasize
    payload = _recompress(body, header) if header.compressed else body
    return bytes(head) + payload


def _payload_start(body: bytes, header: R.ReplayHeader) -> int:
    """Offset at which the packet stream begins (i.e. end of names + params)."""
    cur = R._Cursor(body)
    R._read_names(cur, header)
    if header.is_yrp1:
        cur.u32(), cur.u32(), cur.u32()
    cur.u64() if header.flag & R.REPLAY_64BIT_DUELFLAG else cur.u32()
    return cur.pos


def _extract_embedded_yrp1(data: bytes) -> bytes:
    """Return the raw bytes of the YRP1 embedded in a YRPX."""
    header = R.parse_header(data)
    if not header.is_yrpx:
        raise SystemExit("--extract-yrp1 needs a .yrpX source")
    body = data[header.size:]
    if header.compressed:
        body = R._decompress(body, header)
    cur = R._Cursor(body)
    R._read_names(cur, header)
    cur.u64() if header.flag & R.REPLAY_64BIT_DUELFLAG else cur.u32()
    while cur.remaining >= 5:
        msg = cur.u8()
        payload = cur.take(cur.u32())
        if msg == R.OLD_REPLAY_MODE:
            return payload
    raise SystemExit("no embedded YRP1 found in that replay")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--extract-yrp1", action="store_true",
                    help="write the embedded YRP1 as a standalone .yrp instead")
    ap.add_argument("source", type=pathlib.Path)
    ap.add_argument("dest", type=pathlib.Path)
    args = ap.parse_args()

    if args.extract_yrp1:
        raw = _extract_embedded_yrp1(args.source.read_bytes())
        inner = R.parse(raw)
        if inner.trailing_bytes:
            return _fail(f"extracted YRP1 not fully parsed ({inner.trailing_bytes} bytes left)")
        if any(not n.startswith("Player") for n in inner.names):
            return _fail(f"names not sanitised: {inner.names}")
        args.dest.parent.mkdir(parents=True, exist_ok=True)
        args.dest.write_bytes(raw)
        print(f"{args.dest}: {len(raw)} bytes, {len(inner.decks)} decks, "
              f"{len(inner.responses)} responses, names={inner.names}")
        return 0

    out = _rebuild(args.source.read_bytes())

    # The rewritten fixture must still parse, and must still describe the same
    # duel. Verify before writing anything.
    before = R.parse(args.source.read_bytes())
    after = R.parse(out)
    if len(before.packets) != len(after.packets):
        return _fail(f"packet count changed: {len(before.packets)} -> {len(after.packets)}")
    if before.duel_flags != after.duel_flags:
        return _fail("duel_flags changed")
    for a, b in zip(before.packets, after.packets):
        if a.message != b.message or a.payload != b.payload:
            return _fail("packet stream changed")
    if any(not n.startswith("Player") for n in after.names):
        return _fail(f"names not sanitised: {after.names}")

    args.dest.parent.mkdir(parents=True, exist_ok=True)
    args.dest.write_bytes(out)
    print(f"{args.dest}: {len(out)} bytes, {len(after.packets)} packets, names={after.names}")
    return 0


def _fail(msg: str) -> int:
    print(f"error: {msg}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

"""Read EDOPro replay files (.yrp / .yrpX).

Pure standard library. Deliberately independent of the C++ client so the
harness can run headlessly, in CI, without building EDOPro, without ocgcore,
and without card scripts or databases.

Layout is derived from upstream `gframe/replay.h` and `gframe/replay.cpp` at
commit 54ea755a:

    ReplayHeader           id, version, flag, timestamp, datasize, hash, props[8]
    ExtendedReplayHeader   + header_version, seed[4]        (if REPLAY_EXTENDED_HEADER)
    body                   LZMA1-raw compressed             (if REPLAY_COMPRESSED)

The body then contains player names, duel parameters, and either a recorded
packet stream (yrpX) or decks plus player responses (yrp1).
"""
from __future__ import annotations

import dataclasses
import lzma
import struct
from typing import Iterator

# --- header flags (gframe/replay.h) ---
REPLAY_COMPRESSED = 0x1
REPLAY_TAG = 0x2
REPLAY_DECODED = 0x4
REPLAY_SINGLE_MODE = 0x8
REPLAY_LUA64 = 0x10
REPLAY_NEWREPLAY = 0x20
REPLAY_HAND_TEST = 0x40
REPLAY_DIRECT_SEED = 0x80
REPLAY_64BIT_DUELFLAG = 0x100
REPLAY_EXTENDED_HEADER = 0x200

REPLAY_YRP1 = 0x31707279  # 'yrp1'
REPLAY_YRPX = 0x58707279  # 'yrpX'

OLD_REPLAY_MODE = 231  # a yrpX may embed a whole yrp1 in one packet

_BASE_HEADER = struct.Struct("<6I8s")           # 32 bytes
_EXT_TAIL = struct.Struct("<Q4Q")               # header_version + seed[4]
_NAME_BYTES = 40                                # 20 x uint16 (UTF-16LE)


class ReplayError(Exception):
    """Raised when a replay cannot be parsed. Never raised for merely odd data."""


@dataclasses.dataclass(frozen=True)
class ReplayHeader:
    id: int
    version: int
    flag: int
    timestamp: int
    datasize: int
    hash: int
    props: bytes
    header_version: int | None
    seed: tuple[int, ...] | None
    size: int  # bytes consumed by the header

    @property
    def is_yrpx(self) -> bool:
        return self.id == REPLAY_YRPX

    @property
    def is_yrp1(self) -> bool:
        return self.id == REPLAY_YRP1

    @property
    def compressed(self) -> bool:
        return bool(self.flag & REPLAY_COMPRESSED)

    @property
    def single_mode(self) -> bool:
        return bool(self.flag & REPLAY_SINGLE_MODE)


@dataclasses.dataclass(frozen=True)
class Packet:
    """One duel message: an id plus its opaque payload."""

    message: int
    payload: bytes


@dataclasses.dataclass
class Replay:
    header: ReplayHeader
    names: list[str]
    duel_flags: int
    start_lp: int | None
    start_hand: int | None
    draw_count: int | None
    packets: list[Packet]
    embedded_yrp1: "Replay | None" = None
    trailing_bytes: int = 0


def parse_header(data: bytes) -> ReplayHeader:
    if len(data) < _BASE_HEADER.size:
        raise ReplayError(f"file too small for a replay header ({len(data)} bytes)")
    id_, version, flag, timestamp, datasize, hash_, props = _BASE_HEADER.unpack_from(data, 0)
    if id_ not in (REPLAY_YRP1, REPLAY_YRPX):
        raise ReplayError(f"not a replay: leading id 0x{id_:08x}")

    header_version = seed = None
    size = _BASE_HEADER.size
    if flag & REPLAY_EXTENDED_HEADER:
        end = _BASE_HEADER.size + _EXT_TAIL.size
        if len(data) < end:
            raise ReplayError("truncated extended replay header")
        header_version, *seed_vals = _EXT_TAIL.unpack_from(data, _BASE_HEADER.size)
        seed = tuple(seed_vals)
        size = end

    return ReplayHeader(id_, version, flag, timestamp, datasize, hash_, props,
                        header_version, seed, size)


def _decompress(body: bytes, header: ReplayHeader) -> bytes:
    """LZMA1 raw, with the 5-byte props stored in the header.

    Upstream calls LzmaUncompress(..., props, 5), i.e. the classic
    lclppb byte followed by a 32-bit dictionary size.
    """
    lclppb = header.props[0]
    dict_size = int.from_bytes(header.props[1:5], "little")
    pb, rem = divmod(lclppb, 45)
    lp, lc = divmod(rem, 9)
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": dict_size, "lc": lc, "lp": lp, "pb": pb}]
    try:
        out = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters).decompress(
            body, header.datasize)
    except lzma.LZMAError as exc:  # pragma: no cover - corrupt input
        raise ReplayError(f"LZMA decompression failed: {exc}") from exc
    if len(out) != header.datasize:
        raise ReplayError(f"decompressed {len(out)} bytes, header declares {header.datasize}")
    return out


class _Cursor:
    __slots__ = ("buf", "pos")

    def __init__(self, buf: bytes) -> None:
        self.buf = buf
        self.pos = 0

    def take(self, n: int) -> bytes:
        if self.pos + n > len(self.buf):
            raise ReplayError(f"unexpected end of replay body at {self.pos} (+{n})")
        out = self.buf[self.pos:self.pos + n]
        self.pos += n
        return out

    def u8(self) -> int:
        return self.take(1)[0]

    def u16(self) -> int:
        return struct.unpack("<H", self.take(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.take(8))[0]

    def name(self) -> str:
        raw = self.take(_NAME_BYTES).decode("utf-16-le", errors="replace")
        return raw.split("\x00", 1)[0]

    @property
    def remaining(self) -> int:
        return len(self.buf) - self.pos


def _read_packets(cur: _Cursor) -> Iterator[Packet]:
    """Packet framing is: uint8 message, uint32 length, <length> bytes."""
    while cur.remaining >= 5:
        message = cur.u8()
        length = cur.u32()
        if length > cur.remaining:
            raise ReplayError(
                f"packet {message} declares {length} bytes, only {cur.remaining} remain")
        yield Packet(message, cur.take(length))


def parse(data: bytes) -> Replay:
    """Parse a complete replay file."""
    header = parse_header(data)
    body = data[header.size:]
    if header.compressed:
        body = _decompress(body, header)

    cur = _Cursor(body)

    if header.single_mode:
        names = [cur.name(), cur.name()]
    else:
        home = cur.u32()
        names = [cur.name() for _ in range(home)]
        away = cur.u32()
        names += [cur.name() for _ in range(away)]

    start_lp = start_hand = draw_count = None
    if header.is_yrp1:
        start_lp, start_hand, draw_count = cur.u32(), cur.u32(), cur.u32()
    duel_flags = cur.u64() if header.flag & REPLAY_64BIT_DUELFLAG else cur.u32()

    packets: list[Packet] = []
    embedded: Replay | None = None
    if header.is_yrpx:
        for pkt in _read_packets(cur):
            if pkt.message == OLD_REPLAY_MODE:
                # A streamed replay can carry the original yrp1 wholesale.
                if embedded is None:
                    try:
                        embedded = parse(pkt.payload)
                    except ReplayError:
                        embedded = None
                continue
            packets.append(pkt)

    return Replay(header=header, names=names, duel_flags=duel_flags,
                  start_lp=start_lp, start_hand=start_hand, draw_count=draw_count,
                  packets=packets, embedded_yrp1=embedded,
                  trailing_bytes=cur.remaining)


def parse_file(path) -> Replay:
    with open(path, "rb") as fh:
        return parse(fh.read())

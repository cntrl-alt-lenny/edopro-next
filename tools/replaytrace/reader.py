"""Read EDOPro replay files (.yrp / .yrpX).

Pure standard library. Deliberately independent of the C++ client so the
harness can run headlessly, in CI, without building EDOPro, without ocgcore,
and without card scripts or databases.

Layout is derived from upstream `gframe/replay.h` and `gframe/replay.cpp` at
commit 54ea755a:

    ReplayHeader           id, version, flag, timestamp, datasize, hash, props[8]
    ExtendedReplayHeader   + header_version, seed[4]        (if REPLAY_EXTENDED_HEADER)
    body                   LZMA1-raw compressed             (if REPLAY_COMPRESSED)

The body then contains player names and duel parameters, followed by either:

* **YRPX** - a recorded stream of duel messages. This is an *output* recording;
  playing it back requires no engine.
* **YRP1** - decks, optional custom rule cards, and the ordered player
  responses. This is an *input* recording; reproducing the duel means
  re-simulating it through ocgcore using the seed in the header.

A YRPX may embed a whole YRP1 in a single packet (`OLD_REPLAY_MODE`). Both are
parsed by the same code path, so a standalone `.yrp` and an embedded one are
read identically.
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

# LZMA1 property limits (lc <= 8, lp <= 4, pb <= 4), so the packed byte is
# bounded by 9 * 5 * 5. Validated rather than trusted, because a corrupt
# header would otherwise reach Python's lzma API as nonsensical parameters.
_LCLPPB_MAX = 9 * 5 * 5
_DICT_SIZE_MAX = 1 << 30


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

    @property
    def hand_test(self) -> bool:
        return bool(self.flag & REPLAY_HAND_TEST)

    @property
    def new_replay(self) -> bool:
        return bool(self.flag & REPLAY_NEWREPLAY)


@dataclasses.dataclass(frozen=True)
class Packet:
    """One duel message: an id plus its opaque payload."""

    message: int
    payload: bytes


@dataclasses.dataclass(frozen=True)
class ReplayDeck:
    """One player's deck, as card passcodes."""

    main: tuple[int, ...]
    extra: tuple[int, ...]


@dataclasses.dataclass(frozen=True)
class ReplayResponse:
    """One recorded player decision, fed back to the engine verbatim.

    The payload is opaque here: its meaning depends on which message the engine
    was waiting on. Level-2 re-simulation replays these in order.
    """

    data: bytes


@dataclasses.dataclass
class Replay:
    header: ReplayHeader
    names: list[str]
    duel_flags: int
    # YRP1 only.
    start_lp: int | None = None
    start_hand: int | None = None
    draw_count: int | None = None
    script_name: str | None = None
    decks: list[ReplayDeck] = dataclasses.field(default_factory=list)
    rule_cards: tuple[int, ...] = ()
    responses: list[ReplayResponse] = dataclasses.field(default_factory=list)
    # YRPX only.
    packets: list[Packet] = dataclasses.field(default_factory=list)
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

    Upstream calls LzmaUncompress(..., props, 5), i.e. the classic lclppb byte
    followed by a 32-bit dictionary size. Both are validated first: a corrupt
    header would otherwise be handed to Python's lzma API as out-of-range
    filter parameters, producing a confusing error a long way from the cause.
    """
    lclppb = header.props[0]
    if lclppb >= _LCLPPB_MAX:
        raise ReplayError(f"invalid LZMA properties byte {lclppb}")
    dict_size = int.from_bytes(header.props[1:5], "little")
    if not 0 < dict_size <= _DICT_SIZE_MAX:
        raise ReplayError(f"implausible LZMA dictionary size {dict_size}")
    if header.datasize > _DICT_SIZE_MAX:
        raise ReplayError(f"implausible declared body size {header.datasize}")

    pb, rem = divmod(lclppb, 45)
    lp, lc = divmod(rem, 9)
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": dict_size, "lc": lc, "lp": lp, "pb": pb}]
    try:
        out = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters).decompress(
            body, header.datasize)
    except lzma.LZMAError as exc:
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
        if n < 0 or self.pos + n > len(self.buf):
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

    def u32_array(self, count: int, what: str) -> tuple[int, ...]:
        """Read `count` uint32s, rejecting counts the buffer cannot satisfy.

        Checked up front so a corrupt length cannot drive a huge loop or
        allocation before failing on the read that overruns.
        """
        if count * 4 > self.remaining:
            raise ReplayError(
                f"{what} declares {count} entries ({count * 4} bytes), "
                f"only {self.remaining} remain")
        return tuple(self.u32() for _ in range(count))

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


def _read_names(cur: _Cursor, header: ReplayHeader) -> tuple[list[str], int]:
    """Return (names, player_count). Mirrors upstream Replay::ParseNames."""
    if header.single_mode:
        return [cur.name(), cur.name()], 2
    home = cur.u32()
    if home * _NAME_BYTES > cur.remaining:
        raise ReplayError(f"home player count {home} exceeds remaining body")
    names = [cur.name() for _ in range(home)]
    away = cur.u32()
    if away * _NAME_BYTES > cur.remaining:
        raise ReplayError(f"opposing player count {away} exceeds remaining body")
    names += [cur.name() for _ in range(away)]
    return names, home + away


def _read_decks(cur: _Cursor, header: ReplayHeader, players: int) -> tuple[
        list[ReplayDeck], tuple[int, ...]]:
    """Mirrors upstream Replay::ParseDecks, including its skip condition."""
    if header.single_mode and not header.hand_test:
        return [], ()

    decks: list[ReplayDeck] = []
    for _ in range(players):
        main = cur.u32_array(cur.u32(), "main deck")
        extra = cur.u32_array(cur.u32(), "extra deck")
        decks.append(ReplayDeck(main=main, extra=extra))

    rule_cards: tuple[int, ...] = ()
    if header.new_replay and not header.hand_test:
        rule_cards = cur.u32_array(cur.u32(), "custom rule cards")
    return decks, rule_cards


def _read_responses(cur: _Cursor) -> list[ReplayResponse]:
    """Mirrors upstream Replay::ReadNextResponse.

    Framing is uint8 length followed by that many bytes. A zero length
    terminates the list, matching upstream, which treats it as end-of-data.
    """
    out: list[ReplayResponse] = []
    while cur.remaining >= 1:
        length = cur.u8()
        if length == 0:
            break
        out.append(ReplayResponse(cur.take(length)))
    return out


def parse(data: bytes) -> Replay:
    """Parse a complete replay file of either format."""
    header = parse_header(data)
    body = data[header.size:]
    if header.compressed:
        body = _decompress(body, header)

    cur = _Cursor(body)
    names, players = _read_names(cur, header)

    start_lp = start_hand = draw_count = None
    script_name = None
    if header.is_yrp1:
        start_lp, start_hand, draw_count = cur.u32(), cur.u32(), cur.u32()
    duel_flags = cur.u64() if header.flag & REPLAY_64BIT_DUELFLAG else cur.u32()
    if header.is_yrp1 and header.single_mode:
        script_name = cur.take(cur.u16()).decode("utf-8", errors="replace")

    decks: list[ReplayDeck] = []
    rule_cards: tuple[int, ...] = ()
    responses: list[ReplayResponse] = []
    packets: list[Packet] = []
    embedded: Replay | None = None

    if header.is_yrp1:
        decks, rule_cards = _read_decks(cur, header, players)
        responses = _read_responses(cur)
    else:
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
                  script_name=script_name, decks=decks, rule_cards=rule_cards,
                  responses=responses, packets=packets, embedded_yrp1=embedded,
                  trailing_bytes=cur.remaining)


def parse_file(path) -> Replay:
    with open(path, "rb") as fh:
        return parse(fh.read())


def frame_packets(packets) -> bytes:
    """Re-frame parsed packets into upstream's own on-the-wire framing.

    That framing is `uint8 message, uint32 length, payload` - the same one used
    inside a replay body. It is the interchange format between this reader and
    the C++ semantic decoder in `client/`, which consumes a stream of packets
    rather than a replay container: reading .yrpX headers, LZMA bodies and
    embedded YRP1 belongs here (M1) and duplicating it in C++ would buy nothing.

    OLD_REPLAY_MODE packets are absent by construction: `parse` lifts the
    embedded YRP1 out of the stream, and it is not a duel message.
    """
    out = bytearray()
    for pkt in packets:
        out.append(pkt.message)
        out += len(pkt.payload).to_bytes(4, "little")
        out += pkt.payload
    return bytes(out)

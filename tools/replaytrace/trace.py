"""Turn a replay into a deterministic, diffable text trace.

This is the golden-file representation. Its contract:

* **Byte-for-byte reproducible.** The same input always yields the same output,
  on any machine, in any locale, in any Python 3.10+.
* **Nothing environmental.** No pointers, no timestamps, no filesystem paths,
  no absolute sizes of anything outside the replay itself, no dict ordering
  dependence, no wall-clock.
* **Human-readable diffs.** A behavioural change should show up as a small,
  legible hunk, not a wall of hex.

The trace is deliberately *structural* rather than semantic: it records the
message id, name and payload for every packet without interpreting fields.
That is the honest first step, because a semantic decoder does not exist yet
(M2). When it does, a semantic trace can be added alongside this one and the
two compared - which is exactly the property M1 exists to enable.

Excluded from the trace, with reasons:

* ``timestamp``  - wall-clock when the replay was recorded; not behaviour.
* ``hash``       - not populated by current EDOPro (observed 0).
* ``props``      - LZMA tuning; an implementation detail of storage.
* ``datasize``   - a property of compression, not of the duel.
* player names   - identity, not behaviour (and a privacy footgun).
"""
from __future__ import annotations

import hashlib

from .messages import name_for
from .reader import Replay

TRACE_VERSION = 1

# Payloads longer than this are summarised by length + digest rather than
# inlined, so a trace stays readable. UPDATE_DATA payloads can be large.
_INLINE_LIMIT = 32


def _payload_repr(payload: bytes) -> str:
    if not payload:
        return "-"
    if len(payload) <= _INLINE_LIMIT:
        return payload.hex()
    digest = hashlib.sha256(payload).hexdigest()[:16]
    return f"len={len(payload)} sha256:{digest}"


def _flag_names(flag: int) -> str:
    """Render header flags symbolically so a change is legible in a diff."""
    from . import reader as R

    known = [
        (R.REPLAY_COMPRESSED, "COMPRESSED"),
        (R.REPLAY_TAG, "TAG"),
        (R.REPLAY_DECODED, "DECODED"),
        (R.REPLAY_SINGLE_MODE, "SINGLE_MODE"),
        (R.REPLAY_LUA64, "LUA64"),
        (R.REPLAY_NEWREPLAY, "NEWREPLAY"),
        (R.REPLAY_HAND_TEST, "HAND_TEST"),
        (R.REPLAY_DIRECT_SEED, "DIRECT_SEED"),
        (R.REPLAY_64BIT_DUELFLAG, "64BIT_DUELFLAG"),
        (R.REPLAY_EXTENDED_HEADER, "EXTENDED_HEADER"),
    ]
    present = [name for bit, name in known if flag & bit]
    leftover = flag & ~sum(bit for bit, _ in known)
    if leftover:
        present.append(f"0x{leftover:x}")
    return "|".join(present) if present else "-"


def render(replay: Replay, *, source_name: str) -> str:
    """Render a replay as a deterministic trace.

    ``source_name`` is the fixture's logical name, not a path, so traces do not
    embed the machine they were produced on.
    """
    h = replay.header
    out: list[str] = []
    add = out.append

    add(f"# edopro-next replay trace v{TRACE_VERSION}")
    add(f"source: {source_name}")
    add(f"format: {'yrpX' if h.is_yrpx else 'yrp1'}")
    add(f"client_version: 0x{h.version:08x}")
    add(f"flags: {_flag_names(h.flag)}")
    if h.header_version is not None:
        add(f"header_version: {h.header_version}")
    if h.seed is not None:
        add("seed: " + " ".join(f"0x{s:016x}" for s in h.seed))
    add(f"duel_flags: 0x{replay.duel_flags:x}")
    if replay.start_lp is not None:
        add(f"start_lp: {replay.start_lp}")
        add(f"start_hand: {replay.start_hand}")
        add(f"draw_count: {replay.draw_count}")
    add(f"players: {len(replay.names)}")
    add(f"embedded_yrp1: {'yes' if replay.embedded_yrp1 else 'no'}")
    add(f"trailing_bytes: {replay.trailing_bytes}")
    add(f"packets: {len(replay.packets)}")
    add("")

    # Distribution first: a behavioural change usually shows here immediately,
    # before you have to read a thousand individual lines.
    counts: dict[int, int] = {}
    for pkt in replay.packets:
        counts[pkt.message] = counts.get(pkt.message, 0) + 1
    add("## message counts")
    for msg_id in sorted(counts):
        add(f"{msg_id:>3} {name_for(msg_id):<28} {counts[msg_id]}")
    add("")

    add("## packet stream")
    for index, pkt in enumerate(replay.packets):
        add(f"{index:>5} {pkt.message:>3} {name_for(pkt.message):<28} {_payload_repr(pkt.payload)}")

    if replay.embedded_yrp1 is not None:
        inner = replay.embedded_yrp1
        add("")
        add("## embedded yrp1")
        add(f"duel_flags: 0x{inner.duel_flags:x}")
        add(f"start_lp: {inner.start_lp}")
        add(f"start_hand: {inner.start_hand}")
        add(f"draw_count: {inner.draw_count}")
        if inner.header.seed is not None:
            add("seed: " + " ".join(f"0x{s:016x}" for s in inner.header.seed))

    return "\n".join(out) + "\n"

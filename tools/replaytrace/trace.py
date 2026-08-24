"""Turn a replay into a deterministic, diffable text trace.

This is the golden-file representation. Its contract:

* **Byte-for-byte reproducible.** The same input always yields the same output,
  on any machine, in any locale, in any Python 3.10+.
* **Nothing environmental.** No pointers, no timestamps, no filesystem paths,
  no absolute sizes of anything outside the replay itself, no dict ordering
  dependence, no wall-clock.
* **Human-readable diffs.** A change should show up as a small, legible hunk
  naming the message responsible, not a wall of hex.

The trace is deliberately *structural* rather than semantic: it records message
ids and payload digests without interpreting fields. That is the honest first
step, because a semantic decoder does not exist yet (M2).

What a diff here does and does not mean is set out in
`docs/architecture/replay-regression.md`. In short: this detects changes in how
we *read* a recorded protocol, not changes in what a live engine would *emit* -
the fixtures are frozen recordings, so nothing here re-runs ocgcore.

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

TRACE_VERSION = 2

# Payloads longer than this are summarised by length + digest rather than
# inlined, so a trace stays readable. UPDATE_DATA payloads can be large.
_INLINE_LIMIT = 32


def _digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()[:16]


def _payload_repr(payload: bytes) -> str:
    if not payload:
        return "-"
    if len(payload) <= _INLINE_LIMIT:
        return payload.hex()
    return f"len={len(payload)} sha256:{_digest(payload)}"


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


def _yrp1_lines(replay: Replay, indent: str = "") -> list[str]:
    """Render the input-recording half: parameters, decks, responses.

    Card lists are summarised by size plus a digest rather than enumerated.
    That keeps the trace readable while staying fully sensitive to any change,
    and avoids turning a golden file into a card list.
    """
    out: list[str] = []
    add = out.append
    add(f"{indent}start_lp: {replay.start_lp}")
    add(f"{indent}start_hand: {replay.start_hand}")
    add(f"{indent}draw_count: {replay.draw_count}")
    if replay.script_name is not None:
        add(f"{indent}script_name: {replay.script_name}")
    if replay.header.seed is not None:
        add(f"{indent}seed: " + " ".join(f"0x{s:016x}" for s in replay.header.seed))

    add(f"{indent}decks: {len(replay.decks)}")
    for index, deck in enumerate(replay.decks):
        main_bytes = b"".join(c.to_bytes(4, "little") for c in deck.main)
        extra_bytes = b"".join(c.to_bytes(4, "little") for c in deck.extra)
        add(f"{indent}  deck {index}: main={len(deck.main)} sha256:{_digest(main_bytes)} "
            f"extra={len(deck.extra)} sha256:{_digest(extra_bytes)}")

    add(f"{indent}rule_cards: {len(replay.rule_cards)}")
    if replay.rule_cards:
        rule_bytes = b"".join(c.to_bytes(4, "little") for c in replay.rule_cards)
        add(f"{indent}  sha256:{_digest(rule_bytes)}")

    # Responses are the engine's inputs. Their exact bytes decide the duel, so
    # both the count and the content are covered.
    response_bytes = b"".join(bytes([len(r.data)]) + r.data for r in replay.responses)
    add(f"{indent}responses: {len(replay.responses)} sha256:{_digest(response_bytes)}")
    return out


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
    add(f"duel_flags: 0x{replay.duel_flags:x}")
    add(f"players: {len(replay.names)}")
    add(f"trailing_bytes: {replay.trailing_bytes}")

    if h.is_yrp1:
        # An input recording: no message stream, so the duel is described by
        # its seed, decks and responses.
        add("")
        add("## duel setup")
        out.extend(_yrp1_lines(replay))
        return "\n".join(out) + "\n"

    if h.seed is not None:
        add("seed: " + " ".join(f"0x{s:016x}" for s in h.seed))
    add(f"embedded_yrp1: {'yes' if replay.embedded_yrp1 else 'no'}")
    add(f"packets: {len(replay.packets)}")
    add("")

    # Distribution first: a change in the stream usually shows here immediately,
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
        add(f"trailing_bytes: {inner.trailing_bytes}")
        out.extend(_yrp1_lines(inner))

    return "\n".join(out) + "\n"

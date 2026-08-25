#!/usr/bin/env python3
"""Generate the C++ protocol constant header from upstream headers.

Same contract as tools/generate_messages.py, for the same reason: a constant
that is hand-copied is a constant that silently drifts. CI re-runs this with
--check and fails if the committed header differs.

Sources (all upstream-owned, never edited by us):
  gframe/ocgapi_constants.h  -- MSG_*, LOCATION_*, POS_*, PHASE_*, REASON_*,
                                PLAYER_* as defined by ocgcore
  gframe/common.h            -- client-side pseudo-messages (OLD_REPLAY_MODE)

Only literal values and simple bitwise-or compositions of already-defined
names are accepted. Anything else (casts, shifts, arithmetic) is skipped
rather than guessed at, and reported so the omission is visible.

Usage:
    python tools/generate_protocol_constants.py            # write the header
    python tools/generate_protocol_constants.py --check    # verify it is current
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "client" / "include" / "edopro_next" / "client" / "protocol_constants.h"

OCGAPI = REPO / "gframe" / "ocgapi_constants.h"
COMMON = REPO / "gframe" / "common.h"

# Groups are emitted in this order, each into its own section of the header.
# (prefix, source path, C++ type)
GROUPS = [
    ("MSG_", OCGAPI, "std::uint8_t"),
    ("LOCATION_", OCGAPI, "std::uint32_t"),
    ("POS_", OCGAPI, "std::uint8_t"),
    ("PHASE_", OCGAPI, "std::uint32_t"),
    ("REASON_", OCGAPI, "std::uint32_t"),
    ("PLAYER_", OCGAPI, "std::uint8_t"),
]

# Client-side pseudo-messages that share the MSG_ id space but are not
# ocgcore's. Listed explicitly so a stray common.h #define cannot leak in.
EXTRA_MESSAGES = [("OLD_REPLAY_MODE", COMMON)]

_DEFINE = re.compile(r"^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.+?)\s*$")
_HEX = re.compile(r"^0[xX][0-9a-fA-F]+$")
_OCTAL = re.compile(r"^0[0-7]+$")
_DECIMAL = re.compile(r"^(?:0|[1-9]\d*)$")


def _parse_int(token: str) -> int | None:
    """Parse a C integer literal, honouring C's leading-zero octal rule.

    Python's int(x, 0) rejects `0001` outright rather than reading it as octal,
    so the bases are separated here instead of hoping the two languages agree.
    """
    if _HEX.match(token):
        return int(token, 16)
    if _OCTAL.match(token):
        return int(token, 8)
    if _DECIMAL.match(token):
        return int(token, 10)
    return None


class GeneratorError(Exception):
    pass


def _read(path: pathlib.Path) -> list[str]:
    if not path.exists():
        raise GeneratorError(f"missing upstream header: {path}")
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def _evaluate(expr: str, known: dict[str, int]) -> int | None:
    """Resolve a literal, or a `|` composition of literals and known names.

    Returns None for anything else. Deliberately not a C expression parser:
    guessing at upstream arithmetic is exactly the failure mode this script
    exists to prevent.
    """
    expr = expr.split("/*", 1)[0].split("//", 1)[0].strip()
    while expr.startswith("(") and expr.endswith(")"):
        expr = expr[1:-1].strip()
    if not expr:
        return None
    total = 0
    for part in expr.split("|"):
        part = part.strip()
        literal = _parse_int(part)
        if literal is not None:
            total |= literal
        elif part in known:
            total |= known[part]
        else:
            return None
    return total


def collect() -> tuple[dict[str, list[tuple[str, int]]], list[str]]:
    """Return {prefix: [(name, value), ...]} plus a list of skipped defines."""
    files: dict[pathlib.Path, list[str]] = {}
    for _, path, _ in GROUPS:
        files.setdefault(path, _read(path))
    for _, path in EXTRA_MESSAGES:
        files.setdefault(path, _read(path))

    # A single pass per file builds the name table used to resolve
    # compositions, so LOCATION_ONFIELD can refer to LOCATION_MZONE.
    known: dict[str, int] = {}
    per_file: dict[pathlib.Path, list[tuple[str, str]]] = {}
    for path, lines in files.items():
        defines: list[tuple[str, str]] = []
        for line in lines:
            m = _DEFINE.match(line.strip())
            if m:
                defines.append((m.group(1), m.group(2)))
        per_file[path] = defines
        for name, expr in defines:
            value = _evaluate(expr, known)
            if value is not None:
                known[name] = value

    groups: dict[str, list[tuple[str, int]]] = {}
    skipped: list[str] = []
    for prefix, path, _ in GROUPS:
        entries: list[tuple[str, int]] = []
        for name, expr in per_file[path]:
            if not name.startswith(prefix):
                continue
            value = _evaluate(expr, known)
            if value is None:
                skipped.append(f"{name} = {expr}")
                continue
            entries.append((name, value))
        if not entries:
            raise GeneratorError(f"no {prefix}* constants found in {path}")
        groups[prefix] = entries

    for name, path in EXTRA_MESSAGES:
        for defined, expr in per_file[path]:
            if defined != name:
                continue
            value = _evaluate(expr, known)
            if value is None:
                raise GeneratorError(f"{name} in {path} is not a literal")
            groups["MSG_"].append((name, value))
            break
        else:
            raise GeneratorError(f"{name} not found in {path}")

    return groups, skipped


def _check_message_ids(entries: list[tuple[str, int]]) -> list[tuple[str, int]]:
    """Message ids must be unique and fit a byte; ambiguity is fatal."""
    seen: dict[int, str] = {}
    for name, value in entries:
        if not 0 <= value <= 0xFF:
            raise GeneratorError(f"{name} = {value} does not fit in a message id byte")
        if value in seen and seen[value] != name:
            raise GeneratorError(f"id {value} maps to both {seen[value]} and {name}")
        seen[value] = name
    return sorted(entries, key=lambda kv: kv[1])


def render(groups: dict[str, list[tuple[str, int]]]) -> str:
    messages = _check_message_ids(groups["MSG_"])

    out: list[str] = []
    add = out.append
    add("// GENERATED by tools/generate_protocol_constants.py from upstream headers.")
    add("// Do not edit. Re-generate with:")
    add("//     python tools/generate_protocol_constants.py")
    add("//")
    add("// Upstream sources: gframe/ocgapi_constants.h, gframe/common.h")
    add("#ifndef EDOPRO_NEXT_CLIENT_PROTOCOL_CONSTANTS_H")
    add("#define EDOPRO_NEXT_CLIENT_PROTOCOL_CONSTANTS_H")
    add("")
    add("#include <cstdint>")
    add("#include <string_view>")
    add("")
    add("namespace edopro_next::client::protocol {")

    width = max(len(name) for entries in groups.values() for name, _ in entries)

    def emit(title: str, entries: list[tuple[str, int]], ctype: str) -> None:
        add("")
        add(f"// --- {title} ---")
        for name, value in entries:
            add(f"inline constexpr {ctype} {name:<{width}} = 0x{value:x};")

    emit("duel messages", messages, "std::uint8_t")
    for prefix, _, ctype in GROUPS:
        if prefix == "MSG_":
            continue
        title = {
            "LOCATION_": "card locations",
            "POS_": "card positions",
            "PHASE_": "duel phases",
            "REASON_": "event reasons",
            "PLAYER_": "player constants",
        }[prefix]
        emit(title, sorted(groups[prefix], key=lambda kv: (kv[1], kv[0])), ctype)

    add("")
    add("// Canonical name for a duel message id, or an empty view when the id is")
    add("// not one upstream defines. Callers distinguish \"unknown to the protocol\"")
    add("// from \"known but not decoded here\" on the emptiness of this result.")
    add("constexpr std::string_view message_name(std::uint8_t id) noexcept {")
    add("\tswitch(id) {")
    for name, value in messages:
        add(f"\tcase 0x{value:x}: return \"{name}\";")
    add("\tdefault: return {};")
    add("\t}")
    add("}")
    add("")
    add("constexpr bool is_known_message(std::uint8_t id) noexcept {")
    add("\treturn !message_name(id).empty();")
    add("}")
    add("")
    add("} // namespace edopro_next::client::protocol")
    add("")
    add("#endif // EDOPRO_NEXT_CLIENT_PROTOCOL_CONSTANTS_H")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="verify without writing")
    args = ap.parse_args()

    try:
        groups, skipped = collect()
        text = render(groups)
    except GeneratorError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    total = sum(len(v) for v in groups.values())

    if args.check:
        if not OUT.exists():
            print(f"{OUT} does not exist; run without --check", file=sys.stderr)
            return 1
        if OUT.read_text(encoding="utf-8") != text:
            print(f"{OUT} is stale; re-run: python tools/generate_protocol_constants.py",
                  file=sys.stderr)
            return 1
        print(f"protocol constants up to date ({total} values)")
        return 0

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(REPO)} ({total} values)")
    for entry in skipped:
        print(f"  skipped (not a literal or simple or-composition): {entry}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

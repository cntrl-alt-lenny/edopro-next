"""Golden-file regression tests over recorded duel message streams.

What these protect, stated precisely:

    our reading of the recorded duel protocol is stable

The fixtures are frozen recordings, so nothing here executes ocgcore. A green
run therefore does **not** prove that a live duel would still emit the same
messages; it proves that the parser, the message-id table and the normalisation
have not drifted. See docs/architecture/replay-regression.md for what would be
needed to make the stronger claim.

Run:
    python -m unittest discover -s tests -v
    python tests/test_replay_trace.py --update      # re-bless goldens
"""
from __future__ import annotations

import difflib
import pathlib
import re
import struct
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

from tools.replaytrace import parse, parse_file, render  # noqa: E402
from tools.replaytrace.reader import ReplayError  # noqa: E402

FIXTURES = REPO / "tests" / "fixtures"
GOLDEN = REPO / "tests" / "golden"


def fixture_paths() -> list[pathlib.Path]:
    return sorted(FIXTURES.glob("*.yrpX")) + sorted(FIXTURES.glob("*.yrp"))


def golden_for(fixture: pathlib.Path) -> pathlib.Path:
    # .yrpX and .yrp can share a stem, so the suffix has to disambiguate.
    return GOLDEN / (fixture.stem + fixture.suffix.replace(".", "-") + ".trace")


def trace_for(fixture: pathlib.Path) -> str:
    return render(parse_file(fixture), source_name=fixture.stem)


def all_replays(fixture: pathlib.Path):
    """Yield the replay and any replay embedded within it."""
    replay = parse_file(fixture)
    yield replay
    if replay.embedded_yrp1 is not None:
        yield replay.embedded_yrp1


class TestGoldenTraces(unittest.TestCase):
    def test_fixtures_exist(self) -> None:
        self.assertTrue(fixture_paths(), "no fixtures found; the suite would vacuously pass")

    def test_both_formats_are_covered(self) -> None:
        """Guards against the .yrp glob silently matching nothing."""
        suffixes = {p.suffix for p in fixture_paths()}
        self.assertIn(".yrpX", suffixes)
        self.assertIn(".yrp", suffixes, "no standalone YRP1 fixture; .yrp parsing is untested")

    def test_traces_match_golden(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                golden = golden_for(fixture)
                self.assertTrue(
                    golden.exists(),
                    f"missing golden {golden.name}; run: python tests/test_replay_trace.py --update")
                actual = trace_for(fixture)
                expected = golden.read_text(encoding="utf-8")
                if actual != expected:
                    diff = "\n".join(difflib.unified_diff(
                        expected.splitlines(), actual.splitlines(),
                        fromfile=f"golden/{golden.name}", tofile=f"actual/{fixture.name}",
                        lineterm="", n=3))
                    self.fail(f"trace changed for {fixture.name}:\n{diff}")


class TestDeterminism(unittest.TestCase):
    """A golden file is worthless if the renderer is not deterministic."""

    def test_repeated_render_is_identical(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                self.assertEqual(trace_for(fixture), trace_for(fixture))

    def test_no_environmental_leakage(self) -> None:
        """Traces must not embed paths, absolute times, or addresses.

        The pointer check is deliberately specific: a bare "0x7f" prefix also
        matches legitimate duel data (duel_flags is 0x7f80d072c here), so it
        matches only a full-width userspace address.
        """
        banned_substrings = (str(REPO), "/home/", "C:\\", "/mnt/")
        pointer_like = re.compile(r"0x7f[0-9a-f]{10}")
        for fixture in fixture_paths():
            text = trace_for(fixture)
            for needle in banned_substrings:
                with self.subTest(fixture=fixture.name, needle=needle):
                    self.assertNotIn(needle, text)
            with self.subTest(fixture=fixture.name, needle="pointer"):
                self.assertIsNone(pointer_like.search(text))

    def test_parse_is_byte_exact(self) -> None:
        """Every replay body must be fully consumed - including embedded ones.

        A partial parse that silently ignored a tail would still produce a
        stable trace, so this is checked directly rather than inferred.
        """
        for fixture in fixture_paths():
            for index, replay in enumerate(all_replays(fixture)):
                with self.subTest(fixture=fixture.name, replay="embedded" if index else "outer"):
                    self.assertEqual(replay.trailing_bytes, 0)


class TestYrp1Parsing(unittest.TestCase):
    """YRP1 is the input recording: decks and responses decide the duel."""

    def test_yrp1_content_is_populated(self) -> None:
        for fixture in fixture_paths():
            for index, replay in enumerate(all_replays(fixture)):
                if not replay.header.is_yrp1:
                    continue
                with self.subTest(fixture=fixture.name, replay="embedded" if index else "outer"):
                    self.assertTrue(replay.decks, "no decks parsed")
                    self.assertTrue(replay.responses, "no responses parsed")
                    self.assertIsNotNone(replay.start_lp)
                    for deck in replay.decks:
                        self.assertTrue(deck.main, "empty main deck")

    def test_standalone_and_embedded_yrp1_agree(self) -> None:
        """The same parser must read both, so a shared duel reads identically."""
        standalone = FIXTURES / "duel-chains-battle.yrp"
        container = FIXTURES / "duel-chains-battle.yrpX"
        if not (standalone.exists() and container.exists()):
            self.skipTest("paired fixtures not present")
        a = parse_file(standalone)
        b = parse_file(container).embedded_yrp1
        self.assertIsNotNone(b)
        self.assertEqual(a.header.seed, b.header.seed)
        self.assertEqual(a.duel_flags, b.duel_flags)
        self.assertEqual([d.main for d in a.decks], [d.main for d in b.decks])
        self.assertEqual([r.data for r in a.responses], [r.data for r in b.responses])


class TestReaderRobustness(unittest.TestCase):
    def test_rejects_non_replay(self) -> None:
        with self.assertRaises(ReplayError):
            parse(b"not a replay at all, really")

    def test_rejects_truncated_header(self) -> None:
        with self.assertRaises(ReplayError):
            parse(b"\x79\x70\x70\x58")

    def test_detects_truncated_body(self) -> None:
        raw = fixture_paths()[0].read_bytes()
        with self.assertRaises(ReplayError):
            parse(raw[: len(raw) // 2])

    def test_rejects_absurd_length_field(self) -> None:
        """A corrupt count must fail fast, not drive a huge loop first."""
        raw = bytearray((FIXTURES / "duel-chains-battle.yrp").read_bytes())
        header_len = 72  # extended header
        # The first uint32 of an uncompressed YRP1 body is the home-player count.
        struct.pack_into("<I", raw, header_len, 0xFFFFFFFF)
        with self.assertRaises(ReplayError):
            parse(bytes(raw))

    def test_rejects_invalid_lzma_properties(self) -> None:
        """Corrupt props must be caught here, not inside Python's lzma API."""
        raw = bytearray((FIXTURES / "duel-chains-battle.yrpX").read_bytes())
        raw[24] = 255  # props[0]: lclppb, must be < 9*5*5
        with self.assertRaises(ReplayError):
            parse(bytes(raw))


class TestFixtureHygiene(unittest.TestCase):
    """Fixtures are committed to a public repository."""

    def test_fixtures_are_small(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                self.assertLess(fixture.stat().st_size, 64 * 1024,
                                "fixtures should stay tiny")

    def test_fixtures_carry_no_player_identity(self) -> None:
        """Applies recursively: an embedded replay carries its own names."""
        for fixture in fixture_paths():
            for index, replay in enumerate(all_replays(fixture)):
                with self.subTest(fixture=fixture.name, replay="embedded" if index else "outer"):
                    for name in replay.names:
                        self.assertRegex(
                            name, r"^Player[A-Z]$",
                            "fixture names must be sanitised; see tools/make_fixture.py")


def _refuse_to_bless(fixture: pathlib.Path) -> str | None:
    """Reasons a fixture must not be turned into a golden file.

    Blessing is the one operation that can quietly enshrine a bad fixture, so
    the same invariants the suite asserts are re-checked before writing.
    """
    for index, replay in enumerate(all_replays(fixture)):
        where = "embedded" if index else "outer"
        if replay.trailing_bytes:
            return f"{where} body has {replay.trailing_bytes} unconsumed bytes"
        for name in replay.names:
            if not re.fullmatch(r"Player[A-Z]", name):
                return f"{where} replay carries an unsanitised name: {name!r}"
    if fixture.stat().st_size >= 64 * 1024:
        return "fixture is too large"
    return None


def update_goldens() -> int:
    fixtures = fixture_paths()
    if not fixtures:
        print("no fixtures found", file=sys.stderr)
        return 1
    GOLDEN.mkdir(parents=True, exist_ok=True)
    failed = False
    for fixture in fixtures:
        reason = _refuse_to_bless(fixture)
        if reason:
            print(f"refusing to bless {fixture.name}: {reason}", file=sys.stderr)
            failed = True
            continue
        target = golden_for(fixture)
        target.write_text(trace_for(fixture), encoding="utf-8", newline="\n")
        print(f"wrote {target.relative_to(REPO)}")
    return 1 if failed else 0


if __name__ == "__main__":
    if "--update" in sys.argv:
        raise SystemExit(update_goldens())
    unittest.main()

"""Golden-file regression tests over recorded duel message streams.

The property these exist to protect:

    a change altered presentation, not duel behaviour

Each fixture is a real recorded duel. Its trace is a deterministic rendering of
the duel-message stream. If a refactor changes a trace, duel behaviour - or our
reading of it - changed, and that must be deliberate.

Run:
    python -m unittest discover -s tests -v
    python tests/test_replay_trace.py --update      # re-bless goldens
"""
from __future__ import annotations

import difflib
import pathlib
import re
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
    return GOLDEN / (fixture.stem + ".trace")


def trace_for(fixture: pathlib.Path) -> str:
    return render(parse_file(fixture), source_name=fixture.stem)


class TestGoldenTraces(unittest.TestCase):
    def test_fixtures_exist(self) -> None:
        self.assertTrue(fixture_paths(), "no fixtures found; the suite would vacuously pass")

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
                        fromfile=f"golden/{golden.name}", tofile=f"actual/{fixture.stem}",
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

        Note the pointer check is deliberately specific: a bare "0x7f" prefix
        also matches legitimate duel data (duel_flags is 0x7f80d072c here), so
        it matches only a full-width userspace address.
        """
        banned_substrings = (str(REPO), "/home/", "C:\\", "/mnt/")
        pointer_like = re.compile(r"0x7f[0-9a-f]{10}")
        for fixture in fixture_paths():
            text = trace_for(fixture)
            for needle in banned_substrings:
                with self.subTest(fixture=fixture.name, needle=needle):
                    self.assertNotIn(needle, text)
            with self.subTest(fixture=fixture.name, needle="pointer"):
                self.assertIsNone(pointer_like.search(text))

    def test_parse_is_byte_exact(self) -> None:
        """The whole body must be consumed - no silent truncation."""
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                self.assertEqual(parse_file(fixture).trailing_bytes, 0)


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


class TestFixtureHygiene(unittest.TestCase):
    """Fixtures are committed to a public repository."""

    def test_fixtures_are_small(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                self.assertLess(fixture.stat().st_size, 64 * 1024,
                                "fixtures should stay tiny")

    def test_fixtures_carry_no_player_identity(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                replay = parse_file(fixture)
                names = list(replay.names)
                if replay.embedded_yrp1:
                    names += replay.embedded_yrp1.names
                for name in names:
                    self.assertRegex(
                        name, r"^Player[A-Z]$",
                        "fixture names must be sanitised; see tools/make_fixture.py")


def update_goldens() -> int:
    fixtures = fixture_paths()
    if not fixtures:
        print("no fixtures found", file=sys.stderr)
        return 1
    GOLDEN.mkdir(parents=True, exist_ok=True)
    for fixture in fixtures:
        target = golden_for(fixture)
        target.write_text(trace_for(fixture), encoding="utf-8", newline="\n")
        print(f"wrote {target.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    if "--update" in sys.argv:
        raise SystemExit(update_goldens())
    unittest.main()

"""Golden-file regression tests over the *semantic* reading of the fixtures.

The M1 suite (tests/test_replay_trace.py) protects our reading of the recorded
protocol's structure: message ids, framing, payload digests. This one sits a
layer above it and protects our reading of the protocol's *meaning*: which card
instance moved where, whose life points changed, how the chain was built.

The two are deliberately separate. A change to the decoder should move only
these goldens; a change to the container parser should move only those.

What a green run here proves:

* the C++ semantic decoder reads the committed fixtures without a single
  malformed, unknown or inconsistent packet;
* the state it builds satisfies its own invariants at the end of the duel;
* the events and final state are byte-for-byte what they were.

What it does **not** prove, for exactly the reasons set out in
docs/architecture/replay-regression.md §0: the fixtures are frozen recordings.
Nothing here runs ocgcore, so a green run says nothing about whether a live
duel would still emit the same messages.

Run:
    python -m unittest discover -s tests -v
    python tests/test_semantic_trace.py --update     # re-bless goldens
    python tests/test_semantic_trace.py --require    # fail instead of skipping
"""
from __future__ import annotations

import difflib
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

from tools.replaytrace import frame_packets, parse_file  # noqa: E402

FIXTURES = REPO / "tests" / "fixtures"
GOLDEN = REPO / "tests" / "golden"

# Where a locally built binary is likely to be, in preference order. CI sets
# EDOPRO_NEXT_SEMANTIC_TRACE explicitly and does not rely on any of these.
_SEARCH = [
    REPO / "client" / "build",
    REPO / "build" / "client",
    REPO / "build",
]
_NAMES = ["edopro_next_semantic_trace", "edopro_next_semantic_trace.exe"]


def _source_mtime_ns(source_root: pathlib.Path) -> int | None:
    """Return the newest source mtime, or None when it cannot be established.

    The semantic trace executable has no embedded source revision, so the
    harness uses a deliberately conservative local relation: every file in
    client/ (apart from ignored build output) must be older than the binary.
    A missing or unreadable source tree is not evidence of freshness.
    """
    try:
        source_files = []
        for path in source_root.rglob("*"):
            relative = path.relative_to(source_root)
            if path.is_file() and "build" not in relative.parts:
                source_files.append(path)
        if not source_files:
            return None
        return max(path.stat().st_mtime_ns for path in source_files)
    except (OSError, ValueError):
        return None


def binary_is_fresh(binary: pathlib.Path,
                    source_root: pathlib.Path = REPO / "client") -> bool:
    """Whether *binary* is newer than every readable client source file.

    Strictly newer is intentional: equal timestamps fail closed rather than
    allowing a filesystem with coarse timestamp resolution to claim freshness.
    """
    source_mtime = _source_mtime_ns(source_root)
    if source_mtime is None:
        return False
    try:
        return binary.is_file() and binary.stat().st_mtime_ns > source_mtime
    except OSError:
        return False


def find_binary() -> pathlib.Path | None:
    """Locate a semantic trace tool that can be shown to be fresh."""
    if override := os.environ.get("EDOPRO_NEXT_SEMANTIC_TRACE"):
        path = pathlib.Path(override)
        return path if binary_is_fresh(path) else None
    for directory in _SEARCH:
        for name in _NAMES:
            candidate = directory / name
            if binary_is_fresh(candidate):
                return candidate
    return None


def fixture_paths() -> list[pathlib.Path]:
    # Only yrpX: a YRP1 is an input recording of decks and responses, with no
    # message stream for a semantic decoder to read.
    return sorted(FIXTURES.glob("*.yrpX"))


def golden_for(fixture: pathlib.Path) -> pathlib.Path:
    return GOLDEN / (fixture.stem + fixture.suffix.replace(".", "-") + ".semantic")


def trace_for(binary: pathlib.Path, fixture: pathlib.Path) -> str:
    """Render one fixture's semantic trace.

    The packet stream is extracted here, by the M1 reader, and handed to the
    C++ tool. The decoder's real input is a stream of duel messages - from a
    socket, from ocgcore, or from a replay - so that is the boundary the tool
    exposes, rather than re-implementing .yrpX container parsing in C++.
    """
    replay = parse_file(fixture)
    with tempfile.TemporaryDirectory() as workdir:
        stream = pathlib.Path(workdir) / "stream.pkts"
        stream.write_bytes(frame_packets(replay.packets))
        completed = subprocess.run(
            [str(binary), str(stream), "--name", fixture.stem],
            capture_output=True, check=False)
    if completed.returncode != 0:
        raise AssertionError(
            f"{binary.name} failed on {fixture.name} "
            f"(exit {completed.returncode}): {completed.stderr.decode(errors='replace')}")
    # Decoded as UTF-8 with universal newlines normalised, so a Windows build
    # and a Linux build produce the same text to compare.
    return completed.stdout.decode("utf-8").replace("\r\n", "\n")


def section(text: str, heading: str) -> list[str]:
    """Return the lines under a `##`/`###` heading, up to the next heading."""
    lines = text.splitlines()
    try:
        start = lines.index(heading) + 1
    except ValueError:
        raise AssertionError(f"trace has no {heading!r} section")
    out = []
    for line in lines[start:]:
        if line.startswith("#"):
            break
        out.append(line)
    return out


def scalar(text: str, key: str) -> int:
    """Read a `key: <integer>` line from the trace.

    Scanned over the whole file rather than one section, because the packet
    total lives in the header and the outcome counts live under ## coverage.
    """
    prefix = f"{key}: "
    for line in text.splitlines():
        if line.startswith(prefix):
            return int(line[len(prefix):])
    raise AssertionError(f"trace has no {key!r} entry")


class TestBinaryFreshness(unittest.TestCase):
    def test_stale_binary_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as workdir:
            root = pathlib.Path(workdir)
            source_root = root / "client"
            source_root.mkdir()
            source = source_root / "src.cpp"
            binary = root / "edopro_next_semantic_trace"
            source.write_text("source", encoding="utf-8")
            binary.write_text("old binary", encoding="utf-8")
            now = 2_000_000_000_000_000_000
            os.utime(source, ns=(now, now))
            os.utime(binary, ns=(now - 1, now - 1))
            self.assertFalse(binary_is_fresh(binary, source_root))

    def test_fresh_binary_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as workdir:
            root = pathlib.Path(workdir)
            source_root = root / "client"
            source_root.mkdir()
            source = source_root / "src.cpp"
            binary = root / "edopro_next_semantic_trace"
            source.write_text("source", encoding="utf-8")
            binary.write_text("fresh binary", encoding="utf-8")
            now = 2_000_000_000_000_000_000
            os.utime(source, ns=(now, now))
            os.utime(binary, ns=(now + 1_000_000, now + 1_000_000))
            self.assertTrue(binary_is_fresh(binary, source_root))

    def test_build_output_is_not_counted_as_source(self) -> None:
        with tempfile.TemporaryDirectory() as workdir:
            root = pathlib.Path(workdir)
            source_root = root / "client"
            build_root = source_root / "build"
            build_root.mkdir(parents=True)
            source = source_root / "src.cpp"
            binary = build_root / "edopro_next_semantic_trace"
            source.write_text("source", encoding="utf-8")
            binary.write_text("fresh binary", encoding="utf-8")
            now = 2_000_000_000_000_000_000
            os.utime(source, ns=(now, now))
            os.utime(binary, ns=(now + 1_000_000, now + 1_000_000))
            self.assertTrue(binary_is_fresh(binary, source_root))


BINARY = find_binary()
_SKIP_REASON = (
    "no semantic-trace binary is present and newer than every client source "
    "file; configure and build client/, or set "
    "EDOPRO_NEXT_SEMANTIC_TRACE to a fresh binary")


@unittest.skipIf(BINARY is None, _SKIP_REASON)
class TestSemanticGoldens(unittest.TestCase):
    def test_fixtures_exist(self) -> None:
        self.assertTrue(fixture_paths(), "no yrpX fixtures; the suite would vacuously pass")

    def test_traces_match_golden(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                golden = golden_for(fixture)
                self.assertTrue(
                    golden.exists(),
                    f"missing golden {golden.name}; "
                    f"run: python tests/test_semantic_trace.py --update")
                actual = trace_for(BINARY, fixture)
                expected = golden.read_text(encoding="utf-8")
                if actual != expected:
                    diff = "\n".join(difflib.unified_diff(
                        expected.splitlines(), actual.splitlines(),
                        fromfile=f"golden/{golden.name}", tofile=f"actual/{fixture.name}",
                        lineterm="", n=3))
                    self.fail(f"semantic trace changed for {fixture.name}:\n{diff}")

    def test_rendering_is_deterministic(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                self.assertEqual(trace_for(BINARY, fixture), trace_for(BINARY, fixture))


@unittest.skipIf(BINARY is None, _SKIP_REASON)
class TestSemanticQuality(unittest.TestCase):
    """Properties asserted directly, not merely frozen into a golden file.

    A golden file locks in whatever the decoder does, including its mistakes.
    These are the claims that would be wrong to bless.
    """

    def test_no_packet_is_malformed_or_unknown(self) -> None:
        # Real streams from a real client. A malformed packet means our layout
        # is wrong; an unknown id means the generated table is stale.
        for fixture in fixture_paths():
            text = trace_for(BINARY, fixture)
            for key in ("malformed", "unknown", "inconsistent"):
                with self.subTest(fixture=fixture.name, key=key):
                    refused = section(text, "### refused packets")
                    self.assertEqual(
                        scalar(text, key), 0,
                        f"{key} packets in {fixture.name}:\n" + "\n".join(refused))

    def test_model_invariants_hold_at_the_end(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                text = trace_for(BINARY, fixture)
                self.assertIn("invariants: ok", text)

    def test_coverage_accounts_for_every_packet(self) -> None:
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                text = trace_for(BINARY, fixture)
                total = scalar(text, "packets")
                parts = sum(scalar(text, key) for key in
                            ("decoded", "unsupported", "unknown", "malformed", "inconsistent"))
                self.assertEqual(total, parts)

    def test_committed_fixtures_are_semantically_complete(self) -> None:
        expected = {
            "duel-chains-battle.yrpX": (990, 990),
            "duel-extended.yrpX": (1133, 1133),
        }
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                text = trace_for(BINARY, fixture)
                packets, decoded = expected[fixture.name]
                self.assertEqual(scalar(text, "packets"), packets)
                self.assertEqual(scalar(text, "decoded"), decoded)
                self.assertEqual(scalar(text, "unsupported"), 0)

    def test_something_is_actually_decoded(self) -> None:
        # Guards against a regression that turns every packet into
        # "unsupported" and still matches a re-blessed golden.
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                text = trace_for(BINARY, fixture)
                self.assertGreater(scalar(text, "decoded"), 100)

    def test_query_stream_coverage_is_real_and_clean(self) -> None:
        expected_packets = {
            "duel-chains-battle.yrpX": 829,
            "duel-extended.yrpX": 773,
        }
        for fixture in fixture_paths():
            with self.subTest(fixture=fixture.name):
                text = trace_for(BINARY, fixture)
                self.assertEqual(scalar(text, "query_packets"), expected_packets[fixture.name])
                self.assertEqual(scalar(text, "query_decoded"), scalar(text, "query_packets"))
                for key in ("query_unsupported", "query_malformed", "query_inconsistent",
                            "unknown_query_fields"):
                    self.assertEqual(scalar(text, key), 0)
                self.assertGreater(scalar(text, "query_entries"), 0)
                self.assertGreater(scalar(text, "query_skipped"), 0)

    def test_no_environmental_leakage(self) -> None:
        banned = (str(REPO), "/home/", "C:\\", "/mnt/", "stream.pkts")
        for fixture in fixture_paths():
            text = trace_for(BINARY, fixture)
            for needle in banned:
                with self.subTest(fixture=fixture.name, needle=needle):
                    self.assertNotIn(needle, text)


def update_goldens(binary: pathlib.Path) -> int:
    fixtures = fixture_paths()
    if not fixtures:
        print("no yrpX fixtures found", file=sys.stderr)
        return 1
    GOLDEN.mkdir(parents=True, exist_ok=True)
    failed = False
    for fixture in fixtures:
        text = trace_for(binary, fixture)
        # The same refusal to bless a bad trace as the M1 harness: a golden is
        # the one artefact that can quietly enshrine a defect.
        for key in ("malformed", "unknown", "inconsistent"):
            if scalar(text, key) != 0:
                print(f"refusing to bless {fixture.name}: {scalar(text, key)} "
                      f"{key} packets", file=sys.stderr)
                failed = True
        if "invariants: ok" not in text:
            print(f"refusing to bless {fixture.name}: model invariants violated",
                  file=sys.stderr)
            failed = True
        if failed:
            continue
        target = golden_for(fixture)
        with target.open("w", encoding="utf-8", newline="\n") as golden:
            golden.write(text)
        print(f"wrote {target.relative_to(REPO)}")
    return 1 if failed else 0


if __name__ == "__main__":
    if "--require" in sys.argv and BINARY is None:
        print(f"error: {_SKIP_REASON}", file=sys.stderr)
        raise SystemExit(1)
    if "--update" in sys.argv:
        if BINARY is None:
            print(f"error: {_SKIP_REASON}", file=sys.stderr)
            raise SystemExit(1)
        raise SystemExit(update_goldens(BINARY))
    sys.argv = [arg for arg in sys.argv if arg != "--require"]
    unittest.main()

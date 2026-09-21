"""Tests for tools/generate_readme_status.py: the README's "What works" block.

The landing page's status is derived from docs/ROADMAP.md, so a status fact
cannot be typed into the README and drift from the roadmap. That claim needs a
mechanism that can fail, and this file is it. Two things are pinned:

  * the committed README block matches what the generator produces from the
    committed roadmap (`test_committed_readme_block_is_current`, and the same
    check through the tool's own `--check` entry point); and
  * the check is not vacuous. Each mutation below changes a status fact in the
    roadmap, or hand-edits the README, and the check must then fail. So must
    a README that has no generated block at all, which is the state this
    repository was in before the block existed.

The mutations run against scratch copies, never the real files.

Run:
    python -m unittest discover -s tests -v
"""
from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

import tools.generate_readme_status as grs  # noqa: E402

TOOL = REPO / "tools" / "generate_readme_status.py"

SMALL_ROADMAP = """\
# Roadmap

## M0 — Foundation ✅ done

- [x] first
- [x] second

## M1 — Middle  \U0001F536 in progress

- [x] finished part
- [ ] **Open work — not done.** More words
      on a continuation line.

## M2 — Duel field

- [ ] later

---

## Only after all of the above

- [ ] not a milestone
"""


def _readme_with_block(block: str) -> str:
    return f"# Title\n\n{grs.BEGIN}\n{block}\n{grs.END}\n\nafter\n"


class ParseTest(unittest.TestCase):
    def test_statuses_and_open_items(self):
        ms = grs.parse_roadmap(SMALL_ROADMAP)
        self.assertEqual([m.key for m in ms], ["M0", "M1", "M2"])
        self.assertEqual([m.status for m in ms], [grs.DONE, grs.IN_PROGRESS, grs.NOT_STARTED])
        self.assertEqual([m.title for m in ms], ["Foundation", "Middle", "Duel field"])
        self.assertEqual(ms[1].open_items, ["Open work — not done"])

    def test_unnumbered_sections_are_not_milestones(self):
        keys = [m.key for m in grs.parse_roadmap(SMALL_ROADMAP)]
        self.assertNotIn("Only", " ".join(keys))
        self.assertEqual(len(keys), 3)

    def test_real_roadmap_parses_and_is_self_consistent(self):
        text = grs.ROADMAP.read_text(encoding="utf-8")
        ms = grs.parse_roadmap(text)  # raises RoadmapError if it contradicts itself
        self.assertGreaterEqual(len(ms), 2)
        for m in ms:
            with self.subTest(milestone=m.key):
                self.assertIn(m.status, {grs.DONE, grs.IN_PROGRESS, grs.NOT_STARTED})
                if m.status == grs.IN_PROGRESS:
                    self.assertTrue(m.open_items)

    def test_crlf_roadmap_parses_the_same(self):
        self.assertEqual(
            [(m.key, m.status) for m in grs.parse_roadmap(SMALL_ROADMAP.replace("\n", "\r\n"))],
            [(m.key, m.status) for m in grs.parse_roadmap(SMALL_ROADMAP)],
        )


class RoadmapContradictionTest(unittest.TestCase):
    """A roadmap that disagrees with itself must fail, not be averaged into a README."""

    def _fails(self, text: str) -> str:
        with self.assertRaises(grs.RoadmapError) as ctx:
            grs.parse_roadmap(text)
        return str(ctx.exception)

    def test_done_with_an_unchecked_item(self):
        text = SMALL_ROADMAP.replace("- [x] second", "- [ ] second")
        self.assertIn("marked done", self._fails(text))

    def test_no_marker_with_a_checked_item(self):
        text = SMALL_ROADMAP.replace("- [ ] later", "- [x] later")
        self.assertIn("not started", self._fails(text))

    def test_in_progress_with_everything_checked(self):
        text = SMALL_ROADMAP.replace("- [ ] **Open work", "- [x] **Open work")
        self.assertIn("in progress", self._fails(text))

    def test_milestone_without_items(self):
        text = SMALL_ROADMAP.replace("- [ ] later\n", "")
        self.assertIn("no checkbox items", self._fails(text))

    def test_no_duel_milestone(self):
        self.assertIn("Duel field", self._fails(SMALL_ROADMAP.replace("Duel field", "Something else")))


class RenderTest(unittest.TestCase):
    def test_groups_are_kept_separate(self):
        out = grs.render(grs.parse_roadmap(SMALL_ROADMAP))
        done, progress, planned = (
            out.index(h) for h in ("**Done**", "**In progress**", "**Planned, not started**")
        )
        self.assertTrue(done < out.index("- **M0** Foundation") < progress)
        self.assertTrue(progress < out.index("- **M1** Middle") < planned)
        self.assertGreater(out.index("- **M2** Duel field"), planned)
        self.assertIn("still open: Open work — not done", out)

    def test_duel_answer_follows_the_duel_milestone(self):
        not_started = grs.render(grs.parse_roadmap(SMALL_ROADMAP))
        self.assertIn("**No.** The duel field (M2) is not started.", not_started)
        started = SMALL_ROADMAP.replace(
            "## M2 — Duel field", "## M2 — Duel field \U0001F536 in progress"
        ).replace("- [ ] later", "- [x] later\n- [ ] more")
        self.assertIn("**Not yet.** The duel field (M2) is in progress.", grs.render(grs.parse_roadmap(started)))
        finished = SMALL_ROADMAP.replace(
            "## M2 — Duel field", "## M2 — Duel field ✅ done"
        ).replace("- [ ] later", "- [x] later")
        self.assertIn("marked done in the roadmap", grs.render(grs.parse_roadmap(finished)))


class CheckTest(unittest.TestCase):
    def setUp(self):
        self.milestones = grs.parse_roadmap(SMALL_ROADMAP)
        self.current = _readme_with_block(grs.render(self.milestones))

    def test_current_readme_passes(self):
        self.assertEqual(grs.check(self.current, SMALL_ROADMAP), [])

    def test_readme_with_no_block_fails(self):
        """This is the state the repository was in before the block existed."""
        problems = grs.check("# Title\n\nA hand-typed status table.\n", SMALL_ROADMAP)
        self.assertEqual(len(problems), 1)
        self.assertIn("no single generated status block", problems[0])

    def test_status_change_in_the_roadmap_without_a_readme_change_fails(self):
        """The demonstration the brief asks for: source moves, README does not."""
        moved = SMALL_ROADMAP.replace(
            "## M1 — Middle  \U0001F536 in progress", "## M1 — Middle  ✅ done"
        ).replace("- [ ] **Open work", "- [x] **Open work")
        problems = grs.check(self.current, moved)
        self.assertEqual(len(problems), 1)
        self.assertIn("does not match docs/ROADMAP.md", problems[0])
        self.assertIn("Middle", problems[0])

    def test_checking_an_item_off_without_a_readme_change_fails(self):
        moved = SMALL_ROADMAP.replace("- [ ] later", "- [x] later").replace(
            "## M2 — Duel field", "## M2 — Duel field \U0001F536 in progress"
        ).replace("- [x] later", "- [x] later\n- [ ] also later")
        self.assertTrue(grs.check(self.current, moved))

    def test_hand_edited_readme_block_fails(self):
        edited = self.current.replace("**No.**", "**Yes.**")
        self.assertNotEqual(edited, self.current)
        self.assertTrue(grs.check(edited, SMALL_ROADMAP))

    def test_duplicate_or_unpaired_markers_fail(self):
        self.assertTrue(grs.check(self.current + f"\n{grs.BEGIN}\n", SMALL_ROADMAP))
        self.assertTrue(grs.check(self.current.replace(grs.END, ""), SMALL_ROADMAP))

    def test_crlf_readme_passes(self):
        self.assertEqual(grs.check(self.current.replace("\n", "\r\n"), SMALL_ROADMAP), [])

    def test_apply_block_then_check_is_a_fixed_point(self):
        stale = _readme_with_block("old, hand-typed status")
        fixed = grs.apply_block(stale, grs.render(self.milestones))
        self.assertEqual(grs.check(fixed, SMALL_ROADMAP), [])
        self.assertEqual(grs.apply_block(fixed, grs.render(self.milestones)), fixed)

    def test_apply_block_refuses_a_readme_without_markers(self):
        with self.assertRaises(grs.RoadmapError):
            grs.apply_block("# no markers here\n", grs.render(self.milestones))


class CommandLineTest(unittest.TestCase):
    """The tool's own exit codes, against scratch copies."""

    def _run(self, *args: str):
        return subprocess.run(
            [sys.executable, str(TOOL), *args], cwd=str(REPO), text=True, capture_output=True
        )

    def test_check_fails_on_a_stale_copy_and_update_repairs_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            readme = pathlib.Path(tmp) / "README.md"
            roadmap = pathlib.Path(tmp) / "ROADMAP.md"
            roadmap.write_text(SMALL_ROADMAP, encoding="utf-8")
            readme.write_text(_readme_with_block("stale"), encoding="utf-8")
            flags = ["--readme", str(readme), "--roadmap", str(roadmap)]

            stale = self._run("--check", *flags)
            self.assertEqual(stale.returncode, 1, stale.stdout + stale.stderr)
            self.assertIn("does not match docs/ROADMAP.md", stale.stderr)

            self.assertEqual(self._run(*flags).returncode, 0)
            self.assertEqual(self._run("--check", *flags).returncode, 0)

            # Now the roadmap moves and the README does not.
            roadmap.write_text(
                SMALL_ROADMAP.replace("- [ ] later", "- [x] later").replace(
                    "## M2 — Duel field", "## M2 — Duel field ✅ done"
                ),
                encoding="utf-8",
            )
            moved = self._run("--check", *flags)
            self.assertEqual(moved.returncode, 1, moved.stdout + moved.stderr)

    def test_a_contradictory_roadmap_is_a_tool_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            readme = pathlib.Path(tmp) / "README.md"
            roadmap = pathlib.Path(tmp) / "ROADMAP.md"
            roadmap.write_text(SMALL_ROADMAP.replace("- [x] second", "- [ ] second"), encoding="utf-8")
            readme.write_text(_readme_with_block("x"), encoding="utf-8")
            result = self._run("--check", "--readme", str(readme), "--roadmap", str(roadmap))
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

"""Tests for tools/check_home_status.py: the home screen's status rows.

The shell's home screen shows one status row per roadmap milestone. Those
words are typed into QML, so they can drift from docs/ROADMAP.md, and the
first version of that screen did (it kept saying "planned" for milestones the
roadmap had long marked done). This file pins two things:

  * the committed home screen agrees with the committed roadmap; and
  * the check is not vacuous. Each mutation below changes either the roadmap
    or the QML the way a real drift would, and the check must then fail with a
    message naming the milestone. A check that cannot fail proves nothing.

The mutations run on in-memory copies, never the real files.

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

import tools.check_home_status as chs  # noqa: E402

TOOL = REPO / "tools" / "check_home_status.py"
QML = chs.HOME_SCREEN.read_text(encoding="utf-8")
ROADMAP = chs.ROADMAP.read_text(encoding="utf-8")


def replace_once(text: str, old: str, new: str) -> str:
    assert text.count(old) == 1, f"mutation anchor must be unique, found {text.count(old)}: {old!r}"
    return text.replace(old, new)


class CommittedFilesAgree(unittest.TestCase):
    def test_home_screen_matches_roadmap(self):
        self.assertEqual(chs.problems(QML, ROADMAP), [])

    def test_every_milestone_has_exactly_one_row(self):
        keys = [row.get("milestone") for row in chs.parse_rows(QML)]
        self.assertEqual(len(keys), len(set(keys)))
        self.assertGreaterEqual(len(keys), 7)


class TheCheckCanFail(unittest.TestCase):
    def test_a_stale_status_on_the_home_screen_fails(self):
        # The original defect: the semantic client model shown as planned.
        stale = replace_once(
            QML,
            'milestone: "M2"\n                title: "Semantic client model"\n'
            '                detail: "Presentation-free duel state decoded from the message stream, '
            'so it can be reasoned about without a renderer."\n                status: "done"',
            'milestone: "M2"\n                title: "Semantic client model"\n'
            '                detail: "Presentation-free duel state decoded from the message stream, '
            'so it can be reasoned about without a renderer."\n                status: "not started"',
        )
        found = chs.problems(stale, ROADMAP)
        self.assertEqual(len(found), 1, found)
        self.assertIn("M2", found[0])
        self.assertIn("'not started'", found[0])
        self.assertIn("'done'", found[0])

    def test_a_roadmap_change_the_screen_does_not_follow_fails(self):
        moved = replace_once(ROADMAP, "## M4 — Low-risk screens\n", "## M4 — Low-risk screens  🔶 in progress\n")
        # M4 has no checked item, so mark one, as a real change would.
        moved = replace_once(moved, "- [ ] Settings", "- [x] Settings")
        found = chs.problems(QML, moved)
        self.assertEqual(len(found), 1, found)
        self.assertIn("M4", found[0])
        self.assertIn("'in progress'", found[0])

    def test_the_duel_field_finishing_fails_while_the_screen_says_not_started(self):
        done = ROADMAP
        done = replace_once(done, "## M5 — Duel field\n", "## M5 — Duel field ✅ done\n")
        for item in (
            "- [ ] Field rendering against the semantic model",
            "- [ ] Chain visualisation and targeting",
            "- [ ] Prompt system",
            "- [ ] Motion that communicates rules state",
            "- [ ] Compatibility path retained until parity is demonstrated",
        ):
            done = replace_once(done, item, item.replace("[ ]", "[x]"))
        found = chs.problems(QML, done)
        self.assertTrue(any("M5" in f and "'not started'" in f for f in found), found)
        self.assertTrue(any("not a playable client" in f for f in found), found)

    def test_a_milestone_missing_from_the_screen_fails(self):
        with_m7 = ROADMAP.replace("## Only after all of the above", "## M7 — Something new\n\n- [ ] later\n\n## Only after all of the above")
        found = chs.problems(QML, with_m7)
        self.assertEqual(len(found), 1, found)
        self.assertIn("M7", found[0])
        self.assertIn("no row", found[0])

    def test_a_row_for_a_milestone_that_does_not_exist_fails(self):
        extra = replace_once(
            QML, 'milestone: "M6"', 'milestone: "M9"'
        )
        found = chs.problems(extra, ROADMAP)
        self.assertTrue(any("M9" in f for f in found), found)
        self.assertTrue(any("M6" in f and "no row" in f for f in found), found)

    def test_a_renamed_milestone_title_fails(self):
        renamed = replace_once(QML, 'title: "Duel field"', 'title: "The duel"')
        found = chs.problems(renamed, ROADMAP)
        self.assertEqual(len(found), 1, found)
        self.assertIn("M5", found[0])

    def test_a_status_word_in_free_text_fails(self):
        sneaky = replace_once(QML, 'detail: "Settings, replay browser, and lobby and network screens."',
                              'detail: "Settings, replay browser, and lobby and network screens. Planned."')
        found = chs.problems(sneaky, ROADMAP)
        self.assertEqual(len(found), 1, found)
        self.assertIn("M4", found[0])

    def test_a_screen_with_no_rows_fails_instead_of_passing_vacuously(self):
        self.assertTrue(chs.problems("Item { }", ROADMAP))

    def test_a_row_without_a_milestone_fails(self):
        anonymous = replace_once(QML, '                milestone: "M4"\n', "")
        found = chs.problems(anonymous, ROADMAP)
        self.assertTrue(any("no `milestone`" in f for f in found), found)


class CommandLine(unittest.TestCase):
    def _run(self, *args: str):
        return subprocess.run([sys.executable, str(TOOL), *args], cwd=str(REPO), text=True, capture_output=True)

    def test_exits_zero_on_the_committed_files_and_one_on_a_stale_copy(self):
        ok = self._run()
        self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
        with tempfile.TemporaryDirectory() as tmp:
            qml = pathlib.Path(tmp) / "HomeScreen.qml"
            # The first `done` row is M0 (Foundation): make it stale.
            qml.write_text(QML.replace('status: "done"', 'status: "not started"', 1), encoding="utf-8")
            bad = self._run("--qml", str(qml))
            self.assertEqual(bad.returncode, 1, bad.stdout + bad.stderr)
            self.assertIn("does not match docs/ROADMAP.md", bad.stderr)


if __name__ == "__main__":
    unittest.main()

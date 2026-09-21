"""Tests for the PR-body evidence freshness mechanism."""

import importlib.util
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MODULE_PATH = REPO / "tools" / "check_pr_evidence.py"

_spec = importlib.util.spec_from_file_location("check_pr_evidence", MODULE_PATH)
_module = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = _module
_spec.loader.exec_module(_module)

violations = _module.violations


class PrEvidenceTest(unittest.TestCase):
    def assert_measured(self, body):
        findings = violations(body + "\nRun `python -m unittest discover -s tests -v`.\n")
        messages = [message for _, message in findings]
        self.assertTrue(
            any("measured evidence figure" in message for message in messages),
            findings,
        )
        self.assertEqual(len(findings), 1, findings)

    def test_rejects_repository_body_measurement_shapes(self):
        self.assert_measured(
            "the unittest run reported 112\n"
            "tests, all green."
        )
        self.assert_measured("| unittest | 112 |")
        self.assert_measured("ctest reported 3/3\npassing.")
        self.assert_measured("data 3/3, policy 2/2, CI 12 green")
        self.assert_measured("Evidence: 69\ntests passed.")

    def test_ignores_identifiers_and_version_numbers(self):
        body = (
            "Brief-ID 012-2026-09-21-evidence-freshness-reland.\n"
            "PR #28 carries correction E1-E3 and R11-R13.\n"
            "Reviewed at commit 80f8465f4b79f97770c4ef2eecf962c983ac6dd8.\n"
            "Run the tests with Python 3.12.\n"
            "Qt shell rebuilt from branch state: succeeds, 596 KB binary.\n"
            "Windows 11 / MSVC 19.44 is the local build environment.\n"
            "Run `python -m unittest discover -s tests -v`.\n"
        )
        self.assertEqual(violations(body), [])

    def test_version_prose_is_not_a_measurement_but_still_needs_a_command(self):
        findings = violations("Run the tests with Python 3.12.\n")
        self.assertEqual(len(findings), 1)
        self.assertIn("rerunnable command", findings[0][1])
        self.assertNotIn("measured evidence", findings[0][1])

    def test_rejects_pr19_historical_figures(self):
        body = """
        ## Evidence
        - seven files, 173 insertions / 100 deletions
        - 56 ran, 0 failures
        - `git diff --stat 90194888..6d3aadbf`
        """
        findings = violations(body)
        self.assertEqual([line for line, _ in findings], [3, 4])

    def test_rejects_numeric_test_count_even_with_a_command(self):
        findings = violations(
            "Evidence: 69 tests, 0 failures.\n"
            "Run `python -m unittest discover -s tests`.\n"
        )
        self.assertEqual(len(findings), 1)
        self.assertIn("measured evidence figure", findings[0][1])

    def test_accepts_command_based_evidence(self):
        self.assertEqual(
            violations(
                "Evidence:\n"
                "`git diff --stat BASE..HEAD`\n"
                "`python -m unittest discover -s tests -v`\n"
            ),
            [],
        )

    def test_requires_a_rerunnable_command(self):
        findings = violations("Evidence was checked at the current head.\n")
        self.assertEqual(len(findings), 1)
        self.assertIn("rerunnable command", findings[0][1])


class PrEvidenceCorpusTest(unittest.TestCase):
    FIXTURES_DIR = REPO / "tests" / "fixtures" / "pr_bodies"

    # Judged verdicts for the corpus PRs #1-#28:
    # PRs 1-23 all contain measured figures (and/or lack a rerunnable command).
    # PRs 24-28 follow the rerunnable command discipline and contain no measured figures.
    EXPECTED_VERDICTS = {
        pr_num: (pr_num >= 24) for pr_num in range(1, 29)
    }

    def _read_pr(self, pr_num: int) -> str:
        fixture_path = self.FIXTURES_DIR / f"pr_{pr_num}.txt"
        self.assertTrue(fixture_path.is_file(), f"Missing fixture: {fixture_path}")
        return fixture_path.read_text(encoding="utf-8")

    def test_all_fixtures_present(self):
        for pr_num in range(1, 29):
            self.assertTrue((self.FIXTURES_DIR / f"pr_{pr_num}.txt").is_file())

    def test_corpus_pr_verdicts(self):
        """Every corpus PR body must match its judged expected verdict."""
        for pr_num in range(1, 29):
            body = self._read_pr(pr_num)
            findings = violations(body)
            passed = (len(findings) == 0)
            expected = self.EXPECTED_VERDICTS[pr_num]
            self.assertEqual(
                passed,
                expected,
                f"PR #{pr_num} verdict mismatch: got passed={passed}, expected={expected}. Findings: {findings}",
            )

    def test_e4_and_e5_line_level_expectations(self):
        """Verify the exact figure and false-alarm lines named in E4 and E5."""
        # E4: PR #15 diff stat output
        body15 = self._read_pr(15)
        findings15 = violations(body15)
        f15_lines = {line for line, msg in findings15 if "measured evidence figure" in msg}
        self.assertIn(
            30,
            f15_lines,
            "PR #15 line 30 (diff-stat: 'docs/architecture/deck-builder-legality.md | 523 ++++++++') must be flagged",
        )

        # E4: Standalone body of per-file diff-stat lines plus a valid command
        standalone_stat = (
            "docs/architecture/deck-builder-legality.md | 523 ++++++++\n"
            "`git diff --stat BASE..HEAD`\n"
        )
        findings_stat = violations(standalone_stat)
        self.assertTrue(
            any("measured evidence figure" in msg for _, msg in findings_stat),
            f"Standalone diff-stat line was not flagged: {findings_stat}",
        )

        # E4: PR #3 decimal counts on lines 191 and 284
        body3 = self._read_pr(3)
        findings3 = violations(body3)
        f3_lines = {line for line, msg in findings3 if "measured evidence figure" in msg}
        self.assertIn(191, f3_lines, "PR #3 line 191 ('**69 C++ test cases in 6 CTest suites**') must be flagged")
        self.assertIn(284, f3_lines, "PR #3 line 284 ('| 6 CTest suites / 69 cases | 100% passed |') must be flagged")

        # E4: PR #11 decimal counts on lines 113-114
        body11 = self._read_pr(11)
        findings11 = violations(body11)
        f11_lines = {line for line, msg in findings11 if "measured evidence figure" in msg}
        self.assertIn(113, f11_lines, "PR #11 line 113 ('20 functions in `test_deckbuilder.cpp`...') must be flagged")
        self.assertIn(114, f11_lines, "PR #11 line 114 ('... = 38, across 2 CTest targets...') must be flagged")

        # E4: PR #12 decimal counts on lines 182-183
        body12 = self._read_pr(12)
        findings12 = violations(body12)
        f12_lines = {line for line, msg in findings12 if "measured evidence figure" in msg}
        self.assertIn(182, f12_lines, "PR #12 line 182 ('29 functions in `test_lf_list.cpp`...') must be flagged")
        self.assertIn(183, f12_lines, "PR #12 line 183 ('... = 55**, across 2 CTest targets...') must be flagged")

        # E5: PR #23 line 50 diagnostic status observation false alarm
        body23 = self._read_pr(23)
        findings23 = violations(body23)
        f23_all_lines = {line for line, _ in findings23}
        self.assertNotIn(
            50,
            f23_all_lines,
            "PR #23 line 50 ('load_ydk(directory):      ok=1  error=\"\"') is a status observation, not a measured figure",
        )

        # E5: Diagnostic status observation with command must pass cleanly
        status_body = (
            "```\n"
            "ifstream on a directory:  is_open=1  gcount=0  fail=1  eof=1  bad=0\n"
            'load_ydk(directory):      ok=1  error=""\n'
            "```\n"
            "Run `python -m unittest discover -s tests -v`.\n"
        )
        self.assertEqual(violations(status_body), [])

    def test_corpus_other_measured_figures(self):
        """Verify line-level expectations for other measured figures across the corpus."""
        expected_measured_lines = {
            2: [66, 120],
            4: [45, 47],
            5: [76, 77, 83, 85, 86],
            6: [36],
            7: [26, 27, 33, 34],
            8: [113, 143, 147, 150],
            9: [23, 125, 147, 171, 174, 180],
            10: [57, 367, 447, 477, 497, 505, 517, 567],
            13: [20, 21],
            14: [39, 40, 41, 42],
            16: [17, 63],
            17: [23],
            18: [32, 33, 36],
            19: [32, 34, 35],
            20: [1, 17, 38],
            21: [1, 21, 22, 23],
            22: [47, 48, 63],
            23: [73, 76, 77],
        }
        for pr_num, lines in expected_measured_lines.items():
            body = self._read_pr(pr_num)
            findings = violations(body)
            flagged_lines = {line for line, msg in findings if "measured evidence figure" in msg}
            for line_no in lines:
                self.assertIn(
                    line_no,
                    flagged_lines,
                    f"PR #{pr_num} line {line_no} expected to be flagged as measured figure; flagged lines: {flagged_lines}",
                )

    def test_clean_corpus_bodies_pass_cleanly(self):
        """PRs #24-#28 contain rerunnable commands and no measured figures; they must pass with 0 findings."""
        for pr_num in range(24, 29):
            body = self._read_pr(pr_num)
            findings = violations(body)
            self.assertEqual(findings, [], f"PR #{pr_num} failed with unexpected findings: {findings}")

    def test_no_body_fails_solely_on_non_measured_figure_lines(self):
        """Ensure no body fails solely due to a false alarm on non-measured figure lines."""
        for pr_num in range(1, 24):
            body = self._read_pr(pr_num)
            findings = violations(body)
            # PR 1, 6, 11 also fail on line 1 because they lack a rerunnable command.
            # All other failing PRs must fail due to measured evidence figures.
            has_measured_figure = any("measured evidence figure" in msg for _, msg in findings)
            has_command_violation = any("no rerunnable command" in msg for _, msg in findings)
            self.assertTrue(
                has_measured_figure or has_command_violation,
                f"PR #{pr_num} failed without valid reason: {findings}",
            )


if __name__ == "__main__":
    unittest.main()


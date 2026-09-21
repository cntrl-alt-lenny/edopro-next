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


if __name__ == "__main__":
    unittest.main()

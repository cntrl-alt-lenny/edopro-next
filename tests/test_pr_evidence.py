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

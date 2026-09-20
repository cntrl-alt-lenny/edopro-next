"""Regression tests for the Claude adapter's report-writing convenience.

The framework hook deliberately delegates inbox location, role derivation,
atomic writes and provenance to ``tools/report.py``. These tests pin the
adapter-specific work that remains: extracting the final assistant text,
preserving a current self-written report, and tagging a fallback mirror with
the Claude session rather than inventing a brief identifier.
"""

import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

REPO = Path(__file__).resolve().parent.parent
MODULE_PATH = REPO / ".claude" / "hooks" / "save_agent_reply.py"

_spec = importlib.util.spec_from_file_location("save_agent_reply", MODULE_PATH)
_module = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = _module
_spec.loader.exec_module(_module)


class FakeReport:
    ReportError = RuntimeError

    def __init__(self, provenance=None):
        self.provenance = provenance
        self.writes = []

    def head_sha(self):
        return "head-sha"

    def latest_report_provenance(self):
        return self.provenance

    def write_report(self, text, *, task, source):
        self.writes.append((text, task, source))


class SaveAgentReplyTest(unittest.TestCase):
    def test_last_assistant_text_uses_final_text_block(self):
        with tempfile.TemporaryDirectory() as tmp:
            transcript = Path(tmp) / "session.jsonl"
            transcript.write_text(
                "\n".join(
                    [
                        json.dumps({"role": "user", "content": "start"}),
                        json.dumps({"role": "assistant", "content": "first"}),
                        json.dumps({
                            "message": {
                                "role": "assistant",
                                "content": [
                                    {"type": "text", "text": "final one"},
                                    {"type": "tool_use", "id": "ignored"},
                                    {"type": "text", "text": "final two"},
                                ],
                            }
                        }),
                    ]
                ),
                encoding="utf-8",
            )
            self.assertEqual(
                _module._last_assistant_text(transcript), "final one\nfinal two"
            )

    def test_current_self_written_report_is_preserved(self):
        fake = FakeReport(SimpleNamespace(head="head-sha", source="cli"))
        with patch.object(_module, "_report", fake):
            self.assertTrue(_module._agent_already_reported_this_work())

    def test_stale_or_hook_report_does_not_block_fallback(self):
        for provenance in (
            SimpleNamespace(head="old-sha", source="cli"),
            SimpleNamespace(head="head-sha", source="claude-code-stop-hook"),
            None,
        ):
            with self.subTest(provenance=provenance):
                fake = FakeReport(provenance)
                with patch.object(_module, "_report", fake):
                    self.assertFalse(_module._agent_already_reported_this_work())

    def test_main_writes_session_tagged_fallback_through_report_writer(self):
        fake = FakeReport()
        with tempfile.TemporaryDirectory() as tmp:
            transcript = Path(tmp) / "session.jsonl"
            transcript.write_text(
                json.dumps({"role": "assistant", "content": "completion"}) + "\n",
                encoding="utf-8",
            )
            event = {"transcript_path": str(transcript), "session_id": "abc123"}
            with patch.object(_module, "_report", fake), patch.object(
                sys, "stdin", io.StringIO(json.dumps(event))
            ):
                self.assertEqual(_module.main(), 0)
        self.assertEqual(
            fake.writes, [("completion", "claude-code-session:abc123", "claude-code-stop-hook")]
        )


if __name__ == "__main__":
    unittest.main()

"""Consistency checks for the coordination docs.

# Why this file exists

`docs/state.md`, `docs/briefs/` and `docs/agents/model-notes.md` exist to be
read cold by a session with no memory of how things got this way. A stale
statement in them is therefore worse than ordinary documentation drift: it is
read as current fact and acted on.

They drifted within two rounds of being written. External review found two
contradictions and a third turned up while fixing them:

  - docs/state.md said "No Builder round has run under it yet" while the same
    file, further down, described PR #15 as the first Builder round.
  - model-notes.md said brief 001 was "still in docs/briefs/active.md" after
    it had been moved out.
  - docs/briefs/archive/README.md said "Empty so far" while holding a brief.

All three share one cause: a prose sentence asserting something a command
could have answered. The durable fix is to stop writing those, and to make a
machine check the few structural invariants that remain.

# What this covers, and what it cannot

Covered: the brief lifecycle's structural rules, and that intra-repo links in
the coordination docs resolve.

NOT covered: whether prose is *true*. No test can read `docs/state.md` and
tell you the milestone summary is current. That stays a Brain
responsibility at every session start, which is why `/status` re-derives live
state rather than trusting the file. This narrows the drift surface; it does
not close it.
"""

import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BRIEFS = REPO / "docs" / "briefs"

# Which statuses may appear in which directory. This is the lifecycle in
# docs/briefs/README.md, expressed so it can fail.
ALLOWED = {
    BRIEFS: {"queued", "active"},
    BRIEFS / "delivered": {"delivered"},
    BRIEFS / "archive": {"accepted", "rejected"},
}

VALID_STATUSES = {s for group in ALLOWED.values() for s in group}

_STATUS_RE = re.compile(r"^Status:\s*\**\s*([a-z]+)", re.MULTILINE)

# Markdown links to repo-relative paths. Skips URLs and pure anchors.
_LINK_RE = re.compile(r"\[[^\]]*\]\(([^)#]+)(?:#[^)]*)?\)")

ROLES = ("brain", "builder", "verifier")

COORDINATION_DOCS = [
    REPO / "AGENTS.md",
    REPO / "CLAUDE.md",
    REPO / "README.md",
    REPO / "docs" / "state.md",
    REPO / "docs" / "briefs" / "README.md",
    REPO / "docs" / "briefs" / "active.md",
    REPO / "docs" / "briefs" / "archive" / "README.md",
    REPO / "docs" / "agents" / "model-notes.md",
    REPO / "docs" / "agents" / "worktree-mechanism.md",
    REPO / "docs" / "agents" / "launching.md",
    *sorted((REPO / "docs" / "roles").glob("*.md")),
    *sorted((REPO / ".claude" / "agents").glob("*.md")),
    *sorted((REPO / ".claude" / "commands").glob("*.md")),
]


def _brief_files(directory: Path):
    """Brief files in `directory`, excluding its README."""
    if not directory.is_dir():
        return []
    return sorted(
        p for p in directory.glob("*.md")
        if p.name.lower() != "readme.md"
    )


def _status_of(path: Path):
    match = _STATUS_RE.search(path.read_text(encoding="utf-8"))
    return match.group(1) if match else None


class BriefLifecycleTest(unittest.TestCase):
    def test_active_brief_exists(self):
        self.assertTrue((BRIEFS / "active.md").is_file(),
                        "docs/briefs/active.md must always exist, even as a placeholder")

    def test_every_brief_declares_a_valid_status(self):
        for directory in ALLOWED:
            for brief in _brief_files(directory):
                with self.subTest(brief=str(brief.relative_to(REPO))):
                    status = _status_of(brief)
                    self.assertIsNotNone(status, "no `Status:` line")
                    self.assertIn(status, VALID_STATUSES)

    def test_status_matches_directory(self):
        """The lifecycle's core rule: archive/ means adjudicated.

        This is the check that would have caught brief 001 being parked in
        archive/ while it was still awaiting review.
        """
        for directory, allowed in ALLOWED.items():
            for brief in _brief_files(directory):
                with self.subTest(brief=str(brief.relative_to(REPO))):
                    self.assertIn(
                        _status_of(brief), allowed,
                        f"a brief in {directory.name or 'briefs'}/ may only be {sorted(allowed)}",
                    )

    def test_at_most_one_active_brief(self):
        self.assertEqual(
            len(_brief_files(BRIEFS)), 1,
            "exactly one brief lives at docs/briefs/ top level: active.md",
        )

    def test_brief_filenames_are_sortable(self):
        pattern = re.compile(r"^\d{3}-\d{4}-\d{2}-\d{2}-[a-z0-9-]+\.md$")
        for directory in (BRIEFS / "delivered", BRIEFS / "archive"):
            for brief in _brief_files(directory):
                with self.subTest(brief=brief.name):
                    self.assertRegex(brief.name, pattern)

    def test_brief_numbers_are_unique_across_states(self):
        """A brief keeps its number as it moves between directories."""
        seen = {}
        for directory in (BRIEFS / "delivered", BRIEFS / "archive"):
            for brief in _brief_files(directory):
                number = brief.name[:3]
                self.assertNotIn(
                    number, seen,
                    f"brief {number} appears twice: {seen.get(number)} and {brief.name}",
                )
                seen[number] = brief.name


class CoordinationLinkTest(unittest.TestCase):
    def test_intra_repo_links_resolve(self):
        """A dead link in a rehydration doc sends a cold session nowhere."""
        for doc in COORDINATION_DOCS:
            if not doc.is_file():
                continue
            for target in _LINK_RE.findall(doc.read_text(encoding="utf-8")):
                target = target.strip()
                if not target or "://" in target or target.startswith("mailto:"):
                    continue
                with self.subTest(doc=str(doc.relative_to(REPO)), link=target):
                    self.assertTrue(
                        (doc.parent / target).resolve().exists(),
                        f"{doc.relative_to(REPO)} links to a path that does not exist",
                    )


class RoleContractTest(unittest.TestCase):
    """Contracts are vendor-neutral; adapters are thin and point at them.

    The separation exists because the owner runs Anthropic, OpenAI and Google
    models and any seat may be any of them. It is easy to erode by accident --
    someone adds a Claude-specific instruction to a contract, or copies
    contract text into an adapter where it then drifts.
    """

    def test_every_role_has_a_canonical_contract(self):
        for role in ROLES:
            with self.subTest(role=role):
                self.assertTrue((REPO / "docs" / "roles" / f"{role}.md").is_file())

    def test_adapters_point_at_the_contract_and_do_not_restate_it(self):
        for role in ROLES:
            adapter = REPO / ".claude" / "agents" / f"{role}.md"
            if not adapter.is_file():
                continue
            with self.subTest(role=role):
                text = adapter.read_text(encoding="utf-8")
                self.assertIn(
                    f"docs/roles/{role}.md", text,
                    "an adapter must point at its canonical contract",
                )
                # A thin adapter. If one grows past this it is probably
                # restating the contract, which is how the two drift apart.
                self.assertLess(
                    len(text.splitlines()), 60,
                    "adapter looks like it is restating the contract rather than pointing at it",
                )

    def test_contracts_do_not_depend_on_one_vendors_mechanics(self):
        """Naming a vendor as an example is fine; requiring one is not."""
        forbidden = (
            "subagent_type:",
            "allowed-tools:",
            "settings.local.json",
            "effortLevel",
            "ultracode",
            "slash command",
        )
        for role in ROLES:
            contract = (REPO / "docs" / "roles" / f"{role}.md").read_text(encoding="utf-8")
            for token in forbidden:
                with self.subTest(role=role, token=token):
                    self.assertNotIn(
                        token, contract,
                        f"vendor-specific mechanic in a role contract; it belongs in "
                        f"docs/agents/launching.md",
                    )

    def test_contracts_carry_no_tool_frontmatter(self):
        for role in ROLES:
            with self.subTest(role=role):
                first = (REPO / "docs" / "roles" / f"{role}.md").read_text(
                    encoding="utf-8").lstrip().splitlines()[0]
                self.assertNotEqual(first.strip(), "---",
                                    "a contract must not carry one tool's frontmatter")


if __name__ == "__main__":
    unittest.main()

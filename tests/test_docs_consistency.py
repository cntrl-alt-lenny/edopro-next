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
import subprocess
import tempfile
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

# An inline-code span whose content is a leading slash followed by a bare
# word -- the shared spelling of "a chat-tool command" across vendors
# (`/status`, `/code-review`, ...), not one tool's specific vocabulary.
_SLASH_COMMAND_RE = re.compile(r"`/[a-zA-Z][a-zA-Z0-9_-]*`")

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


def _tracked_paths(repo: Path = REPO):
    """Every path in the commit at HEAD, as repo-relative posix strings.

    Resolved against the committed tree rather than the working tree on
    purpose. Git does not track empty directories, so a directory that exists
    locally can be absent from a fresh checkout -- which is exactly what
    happened when docs/briefs/delivered/ emptied: two documents linked into
    it, the local run passed because the directory was still on disk, and
    only CI caught it. Reading the tree at HEAD (`git ls-tree -r`), not the
    index (`git ls-files`), is what actually makes a local run agree with a
    fresh clone: `ls-files` reflects whatever is staged, so a file that has
    been `git add`ed but never committed reads as tracked here while being
    genuinely absent from the pushed commit CI checks out. That gap was live
    in this exact function -- verified by staging a new file and observing it
    appear in `git ls-files` but not in `git ls-tree -r HEAD`. See
    `TrackedPathsTest` below, which pins this against a disposable repo.

    `repo` is injectable so tests can point this at a throwaway repo instead
    of mutating this checkout's own index.
    """
    out = subprocess.run(
        ["git", "ls-tree", "-r", "--name-only", "HEAD"],
        cwd=str(repo), text=True, capture_output=True,
    )
    return {line.strip() for line in out.stdout.splitlines() if line.strip()}


class TrackedPathsTest(unittest.TestCase):
    """Regression test for the ls-files-vs-ls-tree bug (brief 003, item (a)).

    Runs against a disposable repo built in a temp directory, never against
    this checkout -- so the test can freely stage a file without touching the
    real index.
    """

    def _git(self, repo: Path, args: list):
        result = subprocess.run(
            ["git", *args], cwd=str(repo), text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_staged_but_uncommitted_file_is_not_tracked(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            self._git(repo, ["init", "-q", "-b", "main"])
            self._git(repo, ["config", "user.email", "test@example.invalid"])
            self._git(repo, ["config", "user.name", "test"])

            (repo / "committed.md").write_text("committed", encoding="utf-8")
            self._git(repo, ["add", "committed.md"])
            self._git(repo, ["commit", "-q", "-m", "initial"])

            (repo / "staged_only.md").write_text("staged", encoding="utf-8")
            self._git(repo, ["add", "staged_only.md"])

            tracked = _tracked_paths(repo)

            self.assertIn("committed.md", tracked)
            self.assertNotIn(
                "staged_only.md", tracked,
                "a file staged but never committed must not read as tracked "
                "-- a fresh clone of the pushed commit would not have it",
            )


def _link_resolves(path: Path, tracked: set, repo: Path = REPO) -> bool:
    """Whether a coordination-doc link target is present in a fresh checkout.

    Module-level so `LinkResolutionOutOfRepoTest` can exercise the
    out-of-repo branch directly, not just via whatever links happen to exist
    today.
    """
    try:
        rel = path.resolve().relative_to(repo).as_posix()
    except ValueError:
        # The link resolves outside the repo root. A coordination doc meant
        # to be read cold in any clone has no business pointing off-repo, so
        # this is a failure, not a case to fall back on. An earlier version
        # fell back to a working-tree existence check here -- exactly the
        # local-disk-vs-committed-tree mechanism that caused the
        # docs/briefs/delivered/ bug this test exists to catch. No current
        # link takes this path in this repository (dormant in practice), but
        # it is now inert by construction rather than a live copy of the
        # original bug -- see LinkResolutionOutOfRepoTest.
        return False
    if rel in tracked:
        return True
    # A directory is present in a checkout only if it holds a tracked file.
    prefix = rel.rstrip("/") + "/"
    return any(entry.startswith(prefix) for entry in tracked)


class CoordinationLinkTest(unittest.TestCase):
    def test_intra_repo_links_resolve(self):
        """A dead link in a rehydration doc sends a cold session nowhere."""
        tracked = _tracked_paths()
        self.assertTrue(tracked, "git ls-tree returned nothing; cannot verify links")

        for doc in COORDINATION_DOCS:
            if not doc.is_file():
                continue
            for target in _LINK_RE.findall(doc.read_text(encoding="utf-8")):
                target = target.strip()
                if not target or "://" in target or target.startswith("mailto:"):
                    continue
                with self.subTest(doc=str(doc.relative_to(REPO)), link=target):
                    self.assertTrue(
                        _link_resolves(doc.parent / target, tracked),
                        f"{doc.relative_to(REPO)} links to a path git does not track, "
                        f"so it will be missing from a fresh checkout",
                    )


class LinkResolutionOutOfRepoTest(unittest.TestCase):
    """Regression test for the ValueError fallback (brief 003, item (b)).

    A link that resolves outside the repo root must fail, not fall back to
    a working-tree existence check -- that fallback is exactly the
    local-disk-vs-committed-tree mechanism responsible for the original
    docs/briefs/delivered/ bug this whole test file exists to catch.
    """

    def test_link_outside_repo_root_does_not_resolve(self):
        with tempfile.TemporaryDirectory() as tmp:
            outside = Path(tmp) / "exists_on_disk_but_outside_the_repo.md"
            outside.write_text("present on disk", encoding="utf-8")
            self.assertTrue(outside.exists())  # sanity: the old fallback would pass this
            self.assertFalse(_link_resolves(outside, tracked=set()))


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
        """Naming a vendor as an example is fine; requiring one is not.

        Two different shapes of leak, checked two different ways -- this is a
        deliberately partial generalisation, not a claim that the whole class
        is now covered:

        - A **slash-command reference** (`/status`, `/code-review`, ...) has
          a shared syntactic marker across every chat-driven coding tool that
          has one: an inline-code span whose content is a leading slash
          followed by a bare word, e.g. `` `/status` ``. That is a structural
          pattern, not one vendor's vocabulary, so `_SLASH_COMMAND_RE` below
          detects the *class*: any future slash-command reference, in any
          contract, on any tool's convention, not just this one. This closes
          the actual leak Round 1 found (`docs/roles/brain.md:116`, "`/status`
          runs this sequence") -- the old `forbidden` list below only had the
          literal phrase "slash command", which that sentence never
          contained, which is exactly how it slipped through.
        - The rest of `forbidden` -- a frontmatter key, a config filename, a
          settings flag name -- shares no common syntax across vendors, so
          there is no structural pattern to regex on. Each is still checked
          only as the literal string it is. **This half remains an
          enumerated list of known instances, not a class detector**: a
          vendor-specific artifact this list has not seen would still slip
          past it. Stated plainly rather than left implicit.
        """
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
            with self.subTest(role=role, token="<slash-command reference>"):
                match = _SLASH_COMMAND_RE.search(contract)
                self.assertIsNone(
                    match,
                    f"{match.group(0) if match else ''!r} in {role}.md looks like a "
                    f"chat-tool slash-command reference; that mechanic belongs in "
                    f"docs/agents/launching.md, not a vendor-neutral contract",
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

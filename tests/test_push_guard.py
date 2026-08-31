"""Regression tests for .githooks/pre-push.

# Why this file exists

An earlier version of this guard was a Claude Code PreToolUse hook that
inferred intent by parsing the Bash command string. It was validated once, by
hand, against nine cases -- and external review then found two bypasses that
were not among them. A wider sweep found seven fail-open cases in total (any
quote before `git`, so `sh -c '...'`, `bash -c "..."`, `(...)`, `$(...)`; the
`+branch` force shorthand; `+refs/heads/branch`; and flags before the
subcommand, `git -C . push`) plus one fail-closed case (a heredoc that merely
documented a push).

The lesson was not "test more cases". It was that hand-validation produces a
claim, not a mechanism -- exactly the distinction docs/state.md draws between
what is proven and what is merely intended. So the guard moved to a layer
where intent is unambiguous (git resolves the refspec and tells the hook what
will actually be written), and this file makes it a thing CI can fail on.

# What is covered, and what is not

Covered: the hook's own decision, driven by the same stdin protocol git uses.
Every historical bypass appears below as a named case, so a regression that
reintroduces one fails here by name.

NOT covered: whether git actually invokes the hook. That depends on
`core.hooksPath` being set in the clone (see README.md), which is per-machine
configuration this test deliberately does not assert -- a fresh clone legitimately
has no guard until it is set up. It is also why the hook is documented as a
local convenience rather than a control: the non-bypassable guarantee is GitHub
branch protection on `master`, which no local test can stand in for.

Note also that git does not invoke pre-push at all when a push turns out to be
a no-op ("Everything up-to-date"), because there are no ref updates to offer
it. That is a property of git, not a gap in the guard, but it does mean a
manual end-to-end test against an already-up-to-date remote proves nothing --
which is how the first attempt at verifying this hook produced a false pass.
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HOOK = REPO / ".githooks" / "pre-push"

ZERO = "0" * 40
SOME = "1111111111111111111111111111111111111111"

_SH = shutil.which("sh") or shutil.which("bash")
_GIT = shutil.which("git")


def _git(args, cwd):
    result = subprocess.run(
        ["git", *args], cwd=str(cwd), text=True, capture_output=True,
    )
    assert result.returncode == 0, f"git {args} failed: {result.stderr}"
    return result


class _ThrowawayRepos:
    """A disposable local bare 'remote' plus a working repo with
    core.hooksPath pointed at the real .githooks/pre-push, for tests that
    need git to actually invoke the hook rather than being fed synthetic
    stdin. Everything lives under the caller's temp directory. Never touches
    `origin` or any real remote -- `git remote add origin` here points at a
    throwaway local bare repo of the same name, not this project's GitHub
    remote.
    """

    def __init__(self, tmp: Path):
        self.remote = tmp / "remote.git"
        self.work = tmp / "work"
        _git(["init", "-q", "--bare", str(self.remote)], cwd=tmp)
        _git(["init", "-q", "-b", "master", str(self.work)], cwd=tmp)
        _git(["config", "user.email", "test@example.invalid"], cwd=self.work)
        _git(["config", "user.name", "test"], cwd=self.work)
        _git(["config", "core.hooksPath", str(HOOK.parent)], cwd=self.work)
        (self.work / "file.txt").write_text("x", encoding="utf-8")
        # Guard 2 (the derived-table check) runs `$repo_root/tools/*.py
        # --check` unconditionally once it has a working tree. This repo has
        # no real tools/, so without a stub every push here would be
        # rejected by Guard 2 regardless of Guard 1's verdict -- confirmed
        # empirically: a push of a non-master branch was rejected here with
        # "derived tables are out of date" before this stub was added,
        # which would have made every test using this fixture pass for the
        # wrong reason. The stub exits 0 unconditionally so only Guard 1 is
        # under test.
        tools_dir = self.work / "tools"
        tools_dir.mkdir()
        for name in ("generate_messages.py", "generate_protocol_constants.py"):
            (tools_dir / name).write_text(
                "import sys\nsys.exit(0)\n", encoding="utf-8",
            )
        _git(["add", "file.txt", "tools"], cwd=self.work)
        _git(["commit", "-q", "-m", "initial"], cwd=self.work)
        _git(["remote", "add", "origin", str(self.remote)], cwd=self.work)

    def push(self, *refspec):
        return subprocess.run(
            ["git", "push", "origin", *refspec],
            cwd=str(self.work), text=True, capture_output=True,
        )


def _run(stdin: str):
    """Invoke the hook the way git does: argv is (remote name, remote URL)."""
    return subprocess.run(
        [_SH, str(HOOK), "origin", "https://example.invalid/repo.git"],
        input=stdin,
        text=True,
        capture_output=True,
        cwd=str(REPO),
    )


def _line(remote_ref: str, local_sha: str = SOME, remote_sha: str = ZERO) -> str:
    """One line of git's pre-push stdin protocol."""
    return f"refs/heads/local {local_sha} {remote_ref} {remote_sha}\n"


@unittest.skipIf(_SH is None, "no sh/bash on PATH to run the hook with")
class PushGuardTest(unittest.TestCase):
    def test_hook_exists_and_is_executable(self):
        self.assertTrue(HOOK.is_file(), f"{HOOK} is missing")
        # The exec bit is tracked in git; without it git silently runs nothing.
        mode = subprocess.run(
            ["git", "ls-files", "--stage", ".githooks/pre-push"],
            cwd=str(REPO), text=True, capture_output=True,
        ).stdout.split()
        if mode:  # empty if the file is not yet staged/committed
            self.assertEqual(mode[0], "100755", "pre-push must be committed executable")

    def test_rejects_push_to_master(self):
        self.assertNotEqual(_run(_line("refs/heads/master")).returncode, 0)

    def test_rejects_deleting_master(self):
        # A deletion arrives with an all-zero LOCAL sha.
        self.assertNotEqual(
            _run(_line("refs/heads/master", local_sha=ZERO, remote_sha=SOME)).returncode, 0
        )

    def test_allows_task_branch(self):
        result = _run(_line("refs/heads/m3/some-scope"))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_allows_meta_branch(self):
        result = _run(_line("refs/heads/meta/some-scope"))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_master_among_several_refs(self):
        # `git push --all`, or any multi-ref push, offers one line per ref.
        # Master must be caught even when it is not the first or only line.
        stdin = (
            _line("refs/heads/m3/one")
            + _line("refs/heads/master")
            + _line("refs/heads/m3/two")
        )
        self.assertNotEqual(_run(stdin).returncode, 0)

    def test_allows_empty_input(self):
        # Git offers no lines when there is nothing to push. Rejecting here
        # would block harmless pushes.
        self.assertEqual(_run("").returncode, 0)

    def test_message_names_the_branch(self):
        # The rejection has to be actionable, not just non-zero.
        stderr = _run(_line("refs/heads/master")).stderr
        self.assertIn("master", stderr)
        self.assertIn("pull request", stderr.lower())

    def test_branch_merely_containing_master_is_allowed(self):
        # Substring matching is what broke the previous guard. `master` must
        # match the whole ref, not appear anywhere in it.
        for ref in (
            "refs/heads/master-notes",
            "refs/heads/not-master",
            "refs/heads/m3/master-plan",
            "refs/tags/master",
        ):
            with self.subTest(ref=ref):
                self.assertEqual(_run(_line(ref)).returncode, 0)


@unittest.skipIf(_GIT is None, "no git on PATH")
class HistoricalBypassTest(unittest.TestCase):
    """Each case here defeated the previous, command-string-parsing guard.

    Brief 003, item 7: the previous version of this class was named after
    three distinct historical bypasses but constructed the identical
    already-resolved `refs/heads/master` stdin line for all three -- which
    PushGuardTest.test_rejects_push_to_master already covers, and which
    proves nothing about the *resolution* step each test claims to pin
    (`+master`, `+refs/heads/master` and `HEAD:master` all resolving to the
    same ref update before the hook ever sees them).

    This version exercises real `git push` with each exact refspec spelling
    against a throwaway local bare remote, so it is git's own refspec
    resolution feeding the hook, not a hand-rolled stdin line standing in
    for it. That is the actual claim this class makes, and now it is what
    gets tested. Nothing here touches `origin` or any real remote --
    `_ThrowawayRepos` only ever points at a bare repo in a temp directory.
    """

    def test_force_shorthand_resolves_to_the_same_ref(self):
        # `git push origin +master` -- the `+` never reaches the hook.
        with tempfile.TemporaryDirectory() as tmp:
            repos = _ThrowawayRepos(Path(tmp))
            result = repos.push("+master")
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("REJECTED", result.stderr)

    def test_fully_qualified_force_ref(self):
        # `git push origin +refs/heads/master`
        with tempfile.TemporaryDirectory() as tmp:
            repos = _ThrowawayRepos(Path(tmp))
            result = repos.push("+refs/heads/master")
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("REJECTED", result.stderr)

    def test_colon_refspec(self):
        # `git push origin HEAD:master`
        with tempfile.TemporaryDirectory() as tmp:
            repos = _ThrowawayRepos(Path(tmp))
            result = repos.push("HEAD:master")
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("REJECTED", result.stderr)


@unittest.skipIf(_GIT is None, "no git on PATH")
class BareRepositoryTest(unittest.TestCase):
    """Regression test for the bare-repo silent-exit defect (brief 003, item 3).

    Reproduces the actual defect end-to-end: pushes to master FROM a bare
    repository (no working tree at all), which is where
    `git rev-parse --show-toplevel` has nothing to report. The pre-fix hook
    resolved `repo_root` first and exited 0 before the stdin loop ran at
    all when that lookup failed, so Guard 1 -- the actual protected-ref
    rejection -- never ran. `_run()`'s hand-rolled stdin can't reproduce
    this; it always runs with a working tree as cwd. Only an actual bare
    repository as the push source does. Never touches origin; everything
    here is a throwaway repo in a temp directory.
    """

    def test_push_to_master_from_a_bare_repository_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            repos = _ThrowawayRepos(tmp_path)

            pusher_bare = tmp_path / "pusher.git"
            _git(["clone", "-q", "--bare", str(repos.work), str(pusher_bare)], cwd=tmp_path)
            _git(["config", "core.hooksPath", str(HOOK.parent)], cwd=pusher_bare)

            target = tmp_path / "target.git"
            _git(["init", "-q", "--bare", str(target)], cwd=tmp_path)
            _git(["remote", "set-url", "origin", str(target)], cwd=pusher_bare)

            result = subprocess.run(
                ["git", "--git-dir", str(pusher_bare), "push", "origin", "master"],
                cwd=str(tmp_path), text=True, capture_output=True,
            )
            self.assertNotEqual(
                result.returncode, 0,
                f"push from a bare repository was silently allowed: "
                f"{result.stdout}{result.stderr}",
            )
            self.assertIn("REJECTED", result.stderr)


if __name__ == "__main__":
    unittest.main()

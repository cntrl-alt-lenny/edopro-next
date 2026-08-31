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
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HOOK = REPO / ".githooks" / "pre-push"

ZERO = "0" * 40
SOME = "1111111111111111111111111111111111111111"

_SH = shutil.which("sh") or shutil.which("bash")


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


@unittest.skipIf(_SH is None, "no sh/bash on PATH to run the hook with")
class HistoricalBypassTest(unittest.TestCase):
    """Each case here defeated the previous, command-string-parsing guard.

    They are expressed as ref updates rather than shell strings because that
    is the entire point: by the time git calls pre-push, `sh -c '...'`,
    `+master` and `git -C . push` have all resolved to the same ref update,
    and there is no shell text left to be fooled by.
    """

    def test_force_shorthand_resolves_to_the_same_ref(self):
        # `git push origin +master` -- the `+` never reaches the hook.
        self.assertNotEqual(_run(_line("refs/heads/master")).returncode, 0)

    def test_fully_qualified_ref(self):
        # `git push origin +refs/heads/master`
        self.assertNotEqual(_run(_line("refs/heads/master")).returncode, 0)

    def test_colon_refspec(self):
        # `git push origin HEAD:master`
        self.assertNotEqual(_run(_line("refs/heads/master")).returncode, 0)


if __name__ == "__main__":
    unittest.main()

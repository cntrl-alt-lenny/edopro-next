"""Regression tests for .claude/hooks/save_agent_reply.py's role mapping.

# Why this file exists

Brief 003, item 4. `_role_from_worktree` used to return "brain" for *any*
worktree name it did not recognize as builder/verifier -- conflating "this is
genuinely the primary checkout" with "this name is unrecognized". A Verifier
worktree detached at a commit before the bare-name match landed (fa881423)
ran a version of this function that only matched a `-verifier` suffix, so
`.worktrees/verifier` itself fell into that unconditional "brain" branch and
silently overwrote Brain's own inbox entry.

This does not retroactively fix a worktree already detached at a pre-fix
commit -- that worktree runs whatever this file was at that commit, not this
one. What it tests is the failure mode going forward: "brain" is returned
only when the worktree is confirmed to be the primary checkout, and every
other unrecognized name lands in a distinctly-named "unknown-*" bucket that
cannot collide with brain-latest.md.
"""

import importlib.util
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MODULE_PATH = REPO / ".claude" / "hooks" / "save_agent_reply.py"

_spec = importlib.util.spec_from_file_location("save_agent_reply", MODULE_PATH)
_module = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = _module
_spec.loader.exec_module(_module)

_role_from_worktree = _module._role_from_worktree

PRIMARY = Path("/repo/edopro-next")


class RoleFromWorktreeTest(unittest.TestCase):
    def test_builder_worktree_bare_name(self):
        self.assertEqual(
            _role_from_worktree("/repo/edopro-next/.worktrees/builder", PRIMARY),
            "builder",
        )

    def test_verifier_worktree_bare_name(self):
        self.assertEqual(
            _role_from_worktree("/repo/edopro-next/.worktrees/verifier", PRIMARY),
            "verifier",
        )

    def test_sibling_directory_suffix_layout(self):
        self.assertEqual(
            _role_from_worktree("/repo/edopro-next-builder", PRIMARY), "builder",
        )
        self.assertEqual(
            _role_from_worktree("/repo/edopro-next-verifier", PRIMARY), "verifier",
        )

    def test_confirmed_primary_checkout_is_brain(self):
        self.assertEqual(_role_from_worktree(str(PRIMARY), PRIMARY), "brain")

    def test_unrecognized_name_that_is_not_the_primary_checkout_is_not_brain(self):
        """The regression this file exists to pin.

        A worktree that is neither builder nor verifier by name, and is
        provably *not* the primary checkout, must never fall back to
        "brain" -- that is exactly the collision the historical defect
        produced.
        """
        role = _role_from_worktree("/repo/edopro-next/.worktrees/scratch", PRIMARY)
        self.assertNotEqual(role, "brain")
        self.assertEqual(role, "unknown-scratch")

    def test_unrecognized_name_with_no_primary_root_available_is_not_brain(self):
        # primary_root is None when git-common-dir couldn't be resolved --
        # must not default to "brain" just because there is nothing to
        # compare against.
        role = _role_from_worktree("/repo/edopro-next/.worktrees/scratch", None)
        self.assertNotEqual(role, "brain")

    def test_missing_worktree_root_is_not_brain(self):
        role = _role_from_worktree(None, PRIMARY)
        self.assertNotEqual(role, "brain")


if __name__ == "__main__":
    unittest.main()

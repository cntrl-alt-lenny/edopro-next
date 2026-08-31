"""Regression test: the five required-check names must still exist.

# Why this file exists

Brief 003, item 6. AGENTS.md reserves changing CI for the owner, but every
agent authenticates as the owner's account and nothing in this repository
asserted anything about `.github/workflows/edopro-next.yml`'s content. A
PR could rename a required job, drop a matrix leg, or delete a job outright,
and truthfully report "gates green at that SHA" -- because the same PR
weakened what the gate covers.

# What this can actually assert, and what it cannot

This runs inside CI, with no GitHub API credentials, so it cannot ask
branch protection what is currently required (that lives in repository
settings, which this project explicitly keeps out of agent hands -- see
CLAUDE.md and AGENTS.md's "Never push to master"). What it *can* do is
parse this repository's own workflow file and confirm the five known
required-check names are still names the workflow would actually produce.

Required-check contexts (`gh api repos/.../branches/master/protection --jq
'.required_status_checks.contexts'`, run against live branch protection
2026-08-31) are pinned below as REQUIRED_CHECK_CONTEXTS. A GitHub Actions
matrix job's context name is `<job name> (<matrix value>)`; a non-matrix
job's context name is just its `name:`. This file re-derives those names
from the workflow YAML with a small line-based scanner (deliberately not a
full YAML parser -- this test suite is stdlib-only by design, and pulling
in PyYAML as a new dependency for one test needs the ADR CLAUDE.md's
"no new dependencies without justification" calls for, which this brief's
scope does not cover) tailored to this file's current shape: job ids at
2-space indent directly under `jobs:`, a `name:` field at 4-space indent,
and a `python: [...]` matrix line. A workflow restructuring this scanner
cannot follow (a differently-nested matrix, a computed job name) makes it
fail to find the pinned names -- which fails this test, not silently pass
it, because it asserts the pinned names are a *subset* of what it found,
not that its inventory is complete.

# The residual gap, stated plainly

This proves the workflow file *would* still produce checks under these
exact names. It does not prove those checks are still configured as
*required* in GitHub branch protection -- that is repository-settings
state this test suite cannot see and must not touch. A single PR that
renamed a job here **and** repointed branch protection at the new name
consistently would pass both branch protection and this test while still
being the kind of change AGENTS.md reserves for the owner; catching that
needs the owner's own review of the settings change, not a test running
inside the PR being reviewed. This test's job is narrower: catch the far
more likely accident of a rename or dropped matrix leg on only one side.
"""

import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WORKFLOW = REPO / ".github" / "workflows" / "edopro-next.yml"

# gh api repos/cntrl-alt-lenny/edopro-next/branches/master/protection \
#   --jq '.required_status_checks.contexts'
# Run live 2026-08-31; see AGENTS.md's "Never push to master" section.
REQUIRED_CHECK_CONTEXTS = frozenset({
    "Regression harness (3.10)",
    "Regression harness (3.12)",
    "Semantic client model",
    "Card and deck data",
    "Qt 6 shell (Linux)",
})

_TOP_JOB_RE = re.compile(r"^  ([a-zA-Z0-9_-]+):\s*$")
_NAME_RE = re.compile(r"^    name:\s*(.+?)\s*$")
_MATRIX_PY_RE = re.compile(r"^\s+python:\s*\[(.*)\]\s*$")


def _derive_workflow_contexts(workflow_text: str) -> set:
    """Every check-context name this workflow's `jobs:` block would produce.

    Deliberately includes non-required jobs too (e.g. upstream-baseline) --
    callers check the pinned names are a subset of this, not that this
    equals the pinned set, so an extra job here is not itself a failure.
    """
    jobs = {}
    current = None
    in_jobs = False
    for line in workflow_text.splitlines():
        if not in_jobs:
            if line == "jobs:":
                in_jobs = True
            continue
        top = _TOP_JOB_RE.match(line)
        if top:
            current = top.group(1)
            jobs[current] = {"name": None, "python": None}
            continue
        if current is None:
            continue
        name_match = _NAME_RE.match(line)
        if name_match and jobs[current]["name"] is None:
            jobs[current]["name"] = name_match.group(1)
            continue
        matrix_match = _MATRIX_PY_RE.match(line)
        if matrix_match:
            jobs[current]["python"] = [
                v.strip().strip('"').strip("'")
                for v in matrix_match.group(1).split(",")
            ]

    contexts = set()
    for job_id, info in jobs.items():
        name = info["name"] or job_id
        if info["python"]:
            contexts.update(f"{name} ({v})" for v in info["python"])
        else:
            contexts.add(name)
    return contexts


class RequiredCheckContextsTest(unittest.TestCase):
    def test_workflow_still_produces_the_required_check_names(self):
        self.assertTrue(WORKFLOW.is_file(), f"{WORKFLOW} is missing")
        workflow_contexts = _derive_workflow_contexts(
            WORKFLOW.read_text(encoding="utf-8")
        )
        self.assertTrue(
            workflow_contexts,
            "the scanner found no jobs at all -- it likely can no longer "
            "follow this workflow file's structure; see this file's module "
            "docstring for what it assumes",
        )
        missing = REQUIRED_CHECK_CONTEXTS - workflow_contexts
        self.assertFalse(
            missing,
            f"the workflow no longer produces these required-check "
            f"contexts by name: {sorted(missing)} -- a rename, a dropped "
            f"matrix leg, or a removed job. If this is deliberate, branch "
            f"protection's required contexts (owner-controlled; see "
            f"AGENTS.md) must be updated to match, and "
            f"REQUIRED_CHECK_CONTEXTS here re-pinned to the new live "
            f"values.",
        )


if __name__ == "__main__":
    unittest.main()

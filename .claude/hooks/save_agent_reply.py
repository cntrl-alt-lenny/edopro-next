#!/usr/bin/env python3

"""save_agent_reply.py -- Claude Code Stop hook.

Captures the final assistant turn of a session and writes it to a shared inbox,
so Brain can read what Builder or Verifier said without the human copy-pasting
it. Adapted from a pattern used in a sibling project.

# Why this exists

The three roles run in separate worktrees under .worktrees/ (see
docs/agents/worktree-mechanism.md). When a Builder or Verifier round ends
without the human relaying the report, Brain can re-derive the *facts* from the
diff -- but it loses what the agent said it was unsure of, what it deliberately
left out, and what it could not check. Those are exactly the parts Brain most
needs, so this hook preserves them:

    <git-common-dir>/agent-inbox/<role>-latest.md   most recent reply
    <git-common-dir>/agent-inbox/<role>-log.md      append-only history

# IMPORTANT CAVEAT -- Claude Code sessions only

This is Claude Code's own Stop-hook protocol. It does NOT fire for a round run
in a different vendor's tool -- and this project explicitly *prefers* a
non-Claude Verifier (AGENTS.md, "Verifier is deliberately model-diverse"). For
those rounds the human relaying the report is still how Brain finds out.

So: check the timestamp, and never read a missing or stale inbox file as
"nothing happened". It means "unknown".

# Path and role choices

- `git rev-parse --git-common-dir` gives the repo's shared `.git/` -- the same
  value from every worktree, wherever the clone lives on disk.
- `<git-common-dir>/agent-inbox/` sits inside `.git/`, which git never
  version-controls. No .gitignore entry needed; it survives `git clean -fdx`
  and disappears with the clone.
- Role comes from the worktree directory's basename, matching the layout in
  docs/agents/worktree-mechanism.md: `.worktrees/builder` and
  `.worktrees/verifier`, with the primary checkout as `brain`. A `-builder` /
  `-verifier` suffix is also accepted, so a sibling-directory layout (the
  earlier convention, and still the natural one if someone clones the repo
  twice instead of using worktrees) keeps working. `brain` is used only when
  the worktree's toplevel is confirmed to be the primary checkout (compared
  against `<git-common-dir>/..`, stable across every worktree); anything
  else unrecognized writes to an `unknown-<name>-latest.md` bucket instead of
  guessing `brain` -- see `_role_from_worktree`'s own docstring for why that
  distinction matters.

Stop hooks must never block a session from ending, so every failure path here
returns 0 silently.
"""

from __future__ import annotations

import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]

_ROLES = ("builder", "verifier")


def _git(args: list[str]) -> str | None:
    try:
        return subprocess.check_output(
            ["git", "-C", str(_REPO), *args],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def _last_assistant_text(transcript_path: Path) -> str | None:
    try:
        lines = transcript_path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError):
        return None

    last = None
    for line in lines:
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            continue
        role = entry.get("role") or entry.get("message", {}).get("role")
        if role == "assistant":
            last = entry

    if last is None:
        return None

    content = last.get("content") or last.get("message", {}).get("content")
    if isinstance(content, str):
        return content.strip() or None
    if not isinstance(content, list):
        return None

    parts: list[str] = []
    for block in content:
        if isinstance(block, str):
            parts.append(block)
        elif isinstance(block, dict) and block.get("type") == "text":
            text = block.get("text")
            if isinstance(text, str):
                parts.append(text)
    return "\n".join(parts).strip() or None


def _role_from_worktree(worktree_root: str | None, primary_root: Path | None) -> str:
    """Map a worktree path to a role -- "brain" only when confirmed.

    An earlier version returned "brain" for *any* name that did not match a
    builder/verifier pattern, conflating "this genuinely is the primary
    checkout" with "this name is unrecognized". That let an unrecognized
    worktree silently write into brain-latest.md, colliding with Brain's own
    entries -- reproduced with a Verifier worktree whose checked-out commit
    predates the `.worktrees/verifier` bare-name match added in fa881423: at
    that commit this function only recognized a `-verifier` suffix, so
    `.worktrees/verifier` itself fell through to the "brain" branch.

    This does not retroactively fix that: a worktree detached at a commit
    before this fix runs whatever version of this file was checked out
    then, not this one -- a tracked, per-worktree hook cannot be patched
    after the fact for a commit that predates the patch. What this closes
    is the failure *mode* going forward: from this commit on, "brain" is
    returned only when the worktree's toplevel is confirmed to equal the
    primary checkout's root (computed by the caller from
    `git rev-parse --git-common-dir`, which is stable across all worktrees).
    Any other unrecognized name -- including this same function running
    stale in some future relayout it does not know about -- now falls into
    a distinctly named "unknown-<name>" bucket instead, which cannot
    collide with brain-latest.md.
    """
    if not worktree_root:
        return "unknown-no-worktree-root"
    path = Path(worktree_root)
    name = path.name.lower()
    for role in _ROLES:
        # `.worktrees/builder` (current layout) or `edopro-next-builder`
        # (sibling-directory layout, still supported).
        if name == role or name.endswith(f"-{role}"):
            return role
    if primary_root is not None:
        try:
            if path.resolve() == primary_root.resolve():
                return "brain"
        except OSError:
            pass
    return f"unknown-{name or 'worktree'}"


def _seed_readme(inbox: Path) -> None:
    readme = inbox / "README.md"
    if readme.exists():
        return
    readme.write_text(
        "# .git/agent-inbox/\n\n"
        "Auto-populated by `.claude/hooks/save_agent_reply.py` (a Stop hook --\n"
        "Claude Code sessions only; see that script's docstring).\n\n"
        "`<role>-latest.md` holds the final assistant turn of the most recent\n"
        "Claude Code session in the matching worktree (`brain`, `builder` or\n"
        "`verifier`); `<role>-log.md` is the append-only history.\n\n"
        "Read these to see what another role said without shuttling text by\n"
        "hand -- but CHECK THE TIMESTAMP. A round run in a non-Claude tool\n"
        "never writes here, and this project deliberately prefers a\n"
        "non-Claude Verifier. Missing or stale means unknown, not nothing.\n\n"
        "Not under version control (lives inside `.git/`).\n",
        encoding="utf-8",
    )


def main() -> int:
    try:
        raw = sys.stdin.read()
    except (OSError, KeyboardInterrupt):
        return 0
    if not raw.strip():
        return 0
    try:
        event = json.loads(raw)
    except json.JSONDecodeError:
        return 0

    transcript = event.get("transcript_path")
    if not transcript:
        return 0
    transcript_path = Path(transcript)
    if not transcript_path.exists():
        return 0

    text = _last_assistant_text(transcript_path)
    if not text:
        return 0

    common_dir = _git(["rev-parse", "--git-common-dir"])
    if not common_dir:
        return 0
    common = Path(common_dir)
    if not common.is_absolute():
        common = (_REPO / common).resolve()
    inbox = common / "agent-inbox"
    try:
        inbox.mkdir(parents=True, exist_ok=True)
    except OSError:
        return 0
    _seed_readme(inbox)

    # `common` is the shared .git dir's path -- identical from every
    # worktree -- so its parent is the primary checkout's root regardless of
    # which worktree this hook is running in.
    role = _role_from_worktree(_git(["rev-parse", "--show-toplevel"]), common.parent)
    session_id = event.get("session_id", "")
    stamp = datetime.now().isoformat(timespec="seconds")
    header = (
        f"<!-- captured {stamp} from worktree role={role}"
        f"{f' session={session_id}' if session_id else ''} -->\n\n"
    )

    try:
        (inbox / f"{role}-latest.md").write_text(header + text + "\n", encoding="utf-8")
    except OSError:
        return 0

    try:
        with (inbox / f"{role}-log.md").open("a", encoding="utf-8") as handle:
            handle.write(f"\n\n---\n\n{header}{text}\n")
    except OSError:
        pass

    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Structural provider-neutrality scanner.

Enforces that a project's *lane identity* — its roles, branch namespaces, task
queues and dispatch topology — is derived from ROLES and never from whichever
provider, model or tool happens to be running a seat this round.

DESIGN NOTE, and it is the entire point of this module.

A guard built from a list of known provider names is worthless against the next
provider. Every rule here is instead expressed as a POSITIVE invariant over the
caller's declared role set, and FAILS CLOSED on anything it does not recognise.
A provider that has never existed is therefore rejected by default, with no edit
to this file.

`GRAMMAR_QUALIFIERS` is the one allowance, and it is a closed set of English
function words — not a vendor list. A test asserts it contains no proper nouns,
so it cannot quietly become one.

HISTORICAL TEXT IS OUT OF SCOPE. Case studies, round logs, archived briefs and
failure catalogues record which tool actually ran. That is a record of events,
never a lane definition. Callers pass only their *normative* surface.

COUNTEREXAMPLE BLOCKS. A normative document sometimes needs to quote a banned
form in order to prohibit it. Wrap it and declare the structural violation it
demonstrates:

    <!-- guard:counterexample -->
    <!-- guard:violation compound-lane roles=builder text="Acme Builder" -->
    ... text that SHOULD be rejected ...
    <!-- /guard:counterexample -->

The declaration names the exact offending text and the roles with which it is
invalid. The probe runs that text through the real scanner with those declared
roles, not the adopting project's roles. A missing, absent or unflagged
declaration is inert. Findings inside such a block are suppressed by the outer
scan but still reported separately, so `ScanResult.inert_counterexamples()` can
make a useless exemption fail.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from typing import Iterable, Sequence

from textblocks import counterexample_blocks, logical_lines, negated

__all__ = [
    "Finding",
    "Counterexample",
    "ScanResult",
    "GRAMMAR_QUALIFIERS",
    "scan",
    "scan_counterexample",
    "scan_adapter_blocks",
    "adapter_policy_hits",
]

@dataclass(frozen=True)
class Finding:
    """One structural violation."""

    source: str
    line: int
    rule: str
    message: str
    matched: str = ""

    def __str__(self) -> str:  # pragma: no cover - formatting only
        return f"{self.source}:{self.line} [{self.rule}] {self.message}"


@dataclass(frozen=True)
class Counterexample:
    """A suppressed region, and whatever the scanner found inside it."""

    source: str
    line: int
    text: str
    findings: tuple[Finding, ...]


@dataclass
class ScanResult:
    findings: list[Finding] = field(default_factory=list)
    counterexamples: list[Counterexample] = field(default_factory=list)
    lines_scanned: int = 0

    def __bool__(self) -> bool:
        return bool(self.findings)

    def inert_counterexamples(self) -> list[Counterexample]:
        """Blocks that suppress nothing — i.e. exemptions protecting nothing.

        A caller should treat a non-empty result as a failure: either the
        example is not actually a violation (so the document is teaching the
        wrong thing), or the rule that used to catch it has regressed.
        """
        return [c for c in self.counterexamples if not c.findings]

    def report(self) -> str:
        return "\n".join(str(f) for f in self.findings)


#: English function words and ordinary adjectives that may legitimately precede
#: a capitalised role word. Deliberately NOT a vendor list: an unrecognised
#: qualifier fails closed, which is exactly what makes a novel provider name get
#: rejected without ever appearing here.
GRAMMAR_QUALIFIERS = frozenset(
    {
        "a", "an", "the", "this", "that", "these", "those", "each", "every",
        "both", "either", "neither", "one", "two", "three", "any", "no",
        "and", "or", "per", "for", "to", "as", "is", "are", "was", "were",
        "be", "by", "with", "from", "of", "in", "on", "at", "into", "via",
        "your", "our", "its", "their", "my",
        "standing", "active", "primary", "current", "assigned", "temporary",
        "permanent", "independent", "optional", "required", "additional",
        "second", "third", "fresh", "new", "single", "same", "other",
        "role", "roles", "lane", "lanes", "seat", "seats", "specialist",
        "worker", "workers", "executor", "executors", "reviewer", "reviewers",
        "not", "never", "only", "also", "still", "then", "when", "where",
        "while", "if", "so", "but", "because", "than", "before", "after",
        "what", "which", "who", "whether", "how",
        "next", "previous", "another", "each's",
    }
)

_WORD_TO_INT = {
    "one": 1, "two": 2, "three": 3, "four": 4, "five": 5,
    "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10,
}


def _role_alt(roles: Sequence[str]) -> str:
    return "|".join(re.escape(r) for r in sorted(roles, key=len, reverse=True))


#: One qualifier word. A dot is part of the word only when it is immediately
#: followed by another word character -- a version- or domain-shaped token
#: such as "Acme.io" or "Gpt5.6" -- never when it is followed by whitespace or
#: end of word. That keeps a genuine sentence-ending period ("Verifier. That
#: Builder" -- two ordinary role mentions, not a provider-shaped lane) from
#: being absorbed into the preceding word, while still letting a dotted
#: provider-shaped qualifier match as one token. Hyphens are deliberately
#: EXCLUDED: an ordinary capitalised English compound adjective ("Self-
#: contained Builder", "Follow-up Builder") must not be forced into one
#: qualifier "word" that then fails the grammar check just because it isn't a
#: dictionary word -- a hyphen here breaks tokenisation the same way one did
#: on main, so the compound never becomes a candidate at all.
_QUALIFIER_WORD = r"[A-Z][\w+]*(?:\.\w[\w+]*)*"


def _compound_lane_re(roles: Sequence[str]) -> re.Pattern[str]:
    """`<Proper Noun> <Role>` — binding a proper noun to a role makes a lane
    out of the qualifier. Anything that is not plain English grammar is treated
    as a proper noun, i.e. a provider, and rejected."""
    caps = "|".join(re.escape(r.capitalize()) for r in sorted(roles, key=len, reverse=True))
    return re.compile(
        r"(?<![\w-])((?:" + _QUALIFIER_WORD + r"\s+){1,3})(" + caps + r")\b"
    )


def _prefixed_lane_re(roles: Sequence[str]) -> re.Pattern[str]:
    """`<something>-<role>` / `<something>_<role>` — a lane token built by
    prefixing a role. No prefix is ever legitimate."""
    return re.compile(r"(?<![\w])([a-z0-9][\w.+]*)[-_](" + _role_alt(roles) + r")\b")


#: A branch PRESCRIPTION. Deliberately narrow: a branch name is only claimed
#: when a git command creates it, or when it is written as a backticked token on
#: a line that is actually about branches. A detector that mistakes prose for a
#: branch cries wolf forever and then gets disabled.
BRANCH_COMMAND = re.compile(
    r"git\s+(?:switch\s+-c|checkout\s+-b)\s+(?:origin/)?([\w.+-]+)/[\w.<>-]+"
    r"|git\s+worktree\s+add\s+(?:--\S+\s+)*\S+\s+-b\s+(?:origin/)?([\w.+-]+)/[\w.<>-]+"
)
BRANCH_BACKTICK = re.compile(r"`(?:origin/)?([a-z][\w.+-]*)/([\w.<>-]+)`")
BRANCH_LINE = re.compile(r"\bbranch(?:es|ed|ing)?\b|\bnamespace\b", re.IGNORECASE)
FILE_SUFFIX = re.compile(r"\.[a-z0-9]{1,5}$", re.IGNORECASE)

#: A count that DIRECTLY quantifies standing lanes. "lane" is an overloaded
#: word, so a role-ish qualifier is required — that keeps build concurrency
#: ("four parallel jobs") out and catches "three standing lanes".
LANE_COUNT = re.compile(
    r"\b(one|two|three|four|five|six|seven|eight|nine|ten|\d+)\s+"
    r"(?:active\s+|parallel\s+|concurrent\s+|standing\s+)*"
    r"(?:standing|worker|executor|role|permanent)\s+(?:lanes?|roles?|seats?|sessions?)\b",
    re.IGNORECASE,
)

#: Things an OPTIONAL provider-adapter block may never contain, because each
#: would redefine role, queue, branch, authority or gate.
ADAPTER_FORBIDDEN = re.compile(
    r"(git\s+switch\s+-c|git\s+checkout\s+-b|git\s+merge\b|gh\s+pr\s+merge"
    r"|\bmay\s+merge\b|\bmerge\s+authority\b|\bqueue\b|\bbranch\s+namespace\b)",
    re.IGNORECASE,
)
ADAPTER_MARKER = re.compile(r"OPTIONAL\s*[-—–]+\s*(.+?)\s+only", re.IGNORECASE)


def scan(
    text: str,
    roles: Iterable[str],
    *,
    source: str = "<text>",
    coordinator: str = "brain",
    queue_pattern: str | None = None,
    max_lanes: int | None = None,
) -> ScanResult:
    """Scan normative text for provider-shaped lane identity.

    ``roles``       the project's declared executor roles. The single source of
                    truth for lane identity; everything below derives from it.
    ``coordinator`` the coordinating role, which may also own branches.
    ``queue_pattern`` optional regex with one capture group yielding the stem of
                    a canonical (non-archived) queue path. Enabled only for
                    projects that keep such files.
    ``max_lanes``   optional; when set, prose claiming more standing lanes than
                    this is a violation. Leave ``None`` for documents that
                    legitimately discuss several topologies.
    """
    roles = tuple(roles)
    if not roles:
        raise ValueError("at least one role must be declared; an empty role set "
                         "would make every rule vacuous")

    compound_re = _compound_lane_re(roles)
    prefixed_re = _prefixed_lane_re(roles)
    queue_re = re.compile(queue_pattern) if queue_pattern else None
    role_set = set(roles)
    branch_prefixes = role_set | {coordinator}
    role_branch_re = re.compile(
        r"(?<![\w-])((?:[a-z0-9][\w.+]*[-_])?(?:" + _role_alt(roles) + r"))/"
        r"[\w.<>-]+"
    )

    result = ScanResult()
    blocks, suppressed = counterexample_blocks(text)
    lines = text.splitlines()
    result.lines_scanned = len(lines)

    def emit(n: int, rule: str, message: str, *, matched: str = "") -> None:
        result.findings.append(Finding(source, n, rule, message, matched))

    # Token-adjacency rules run over LOGICAL lines, so a compound split across a
    # soft wrap -- "the SomeProvider" ending one line and "Worker" starting the
    # next -- is still caught.
    for n, line in logical_lines(text):
        if n in suppressed:
            continue
        for match in compound_re.finditer(line):
            qualifier, role = match.groups()
            words = [w for w in qualifier.split() if w]
            bad = [
                w for w in words
                if w.lower().strip(".,;:*`\"'()[]") not in GRAMMAR_QUALIFIERS
            ]
            if bad:
                emit(
                    n, "compound-lane",
                    f"'{qualifier.strip()} {role}' binds a proper noun to a role; "
                    f"lanes are the bare roles {roles}",
                    matched=match.group(0).strip(),
                )

        for match in prefixed_re.finditer(line):
            prefix, role = match.groups()
            emit(
                n, "prefixed-lane",
                f"'{prefix}-{role}' prefixes a role to make a lane token; "
                f"the lane is '{role}'",
                matched=match.group(0),
            )

    # Positional rules stay on PHYSICAL lines: joining a paragraph would let the
    # word "branch" anywhere in it enable backtick-branch detection for the
    # whole thing, which is how this kind of detector starts crying wolf.
    for n, line in enumerate(lines, 1):
        if n in suppressed:
            continue

        candidates: list[tuple[str, str]] = []
        for match in BRANCH_COMMAND.finditer(line):
            candidates.extend(
                (group, match.group(0))
                for group in match.groups() if group
            )
        if BRANCH_LINE.search(line):
            for match in BRANCH_BACKTICK.finditer(line):
                prefix, rest = match.groups()
                if prefix == "origin":
                    if "/" not in rest:
                        continue
                    prefix, rest = rest.split("/", 1)
                if FILE_SUFFIX.search(rest):
                    continue  # a file path, not a branch
                candidates.append((prefix, match.group(0)))
        for prefix, matched in candidates:
            if prefix in branch_prefixes:
                continue
            emit(
                n, "branch-namespace",
                f"branch prefix '{prefix}/' is not a role; new branches are "
                f"<role>/<scope> for {sorted(branch_prefixes)}",
                matched=matched.strip("`"),
            )

        # A branch token whose namespace itself prefixes a declared role is
        # unambiguously provider-shaped even when prose omits the word
        # "branch" (for example, ``claude-decomper/fix-123``). Keep the
        # ordinary context requirement above for generic paths such as
        # ``acme/some-scope``.
        if not BRANCH_LINE.search(line):
            for match in role_branch_re.finditer(line):
                if match.group(1) in branch_prefixes:
                    continue
                emit(
                    n, "branch-namespace",
                    f"branch prefix '{match.group(1)}/' is not a role; new "
                    f"branches are <role>/<scope> for {sorted(branch_prefixes)}",
                    matched=match.group(0).strip("`"),
                )

        if queue_re is not None:
            for match in queue_re.finditer(line):
                stem = match.group(1)
                if stem not in role_set:
                    emit(
                        n, "queue-identity",
                        f"canonical queue '{stem}' is not a role queue; live "
                        f"queues are {roles}",
                        matched=match.group(0),
                    )

        if max_lanes is not None:
            for match in LANE_COUNT.finditer(line):
                tok = match.group(1)
                value = _WORD_TO_INT.get(tok.lower()) or (
                    int(tok) if tok.isdigit() else None
                )
                if value is not None and value > max_lanes:
                    emit(
                        n, "lane-count",
                        f"topology says '{tok}' standing lanes where there are "
                        f"{max_lanes}; a provider never adds a lane",
                        matched=match.group(0),
                    )

    for start, body in blocks:
        inner = scan_counterexample(
            body, roles,
            source=f"{source}#counterexample@{start}",
            coordinator=coordinator,
            queue_pattern=queue_pattern,
            max_lanes=max_lanes,
        )
        result.counterexamples.append(
            Counterexample(source, start, body, tuple(inner))
        )

    return result


_COUNTEREXAMPLE_DECLARATION = re.compile(
    r'^\s*<!--\s*guard:violation\s+'
    r'(?P<rule>[a-z][a-z0-9-]*)\s+'
    r'roles=(?P<roles>[a-z0-9][a-z0-9_-]*(?:\s*,\s*[a-z0-9][a-z0-9_-]*)*)\s+'
    r'text="(?P<text>[^"]+)"\s*-->\s*$'
)


def _counterexample_declarations(text: str) -> list[tuple[str, tuple[str, ...], str]]:
    """Parse exact declarations without inferring anything from nearby prose."""
    declarations: list[tuple[str, tuple[str, ...], str]] = []
    for line in text.splitlines():
        match = _COUNTEREXAMPLE_DECLARATION.match(line)
        if match:
            declared_roles = tuple(
                role.strip() for role in match.group("roles").split(",")
            )
            declarations.append(
                (match.group("rule"), declared_roles, match.group("text"))
            )
    return declarations


def scan_counterexample(
    text: str,
    roles: Iterable[str],
    *,
    source: str = "<text>",
    coordinator: str = "brain",
    queue_pattern: str | None = None,
    max_lanes: int | None = None,
) -> list[Finding]:
    """Check declarations against the real scanner, independently of ``roles``.

    The old implementation guessed role-shaped tokens from surrounding words.
    That cannot distinguish harmless prose from a provider-shaped lane. A block
    is non-inert only when every declaration is present in the body and its
    declared rule is emitted by ``scan`` with the declaration's roles.
    """
    del roles
    declarations = _counterexample_declarations(text)
    if any("guard:violation" in line for line in text.splitlines()) and not all(
        _COUNTEREXAMPLE_DECLARATION.match(line)
        for line in text.splitlines()
        if "guard:violation" in line
    ):
        return []
    if not declarations:
        return []

    declaration_lines = {
        line for line in text.splitlines()
        if _COUNTEREXAMPLE_DECLARATION.match(line)
    }
    body = "\n".join(
        line for line in text.splitlines() if line not in declaration_lines
    )
    findings: list[Finding] = []
    for rule, declared_roles, offending_text in declarations:
        if not offending_text or offending_text not in body:
            return []
        try:
            result = scan(
                body, declared_roles, source=source,
                coordinator=coordinator, queue_pattern=queue_pattern,
                max_lanes=max_lanes,
            )
        except ValueError:
            return []
        matching = [
            finding
            for finding in result.findings
            if finding.rule == rule and (
                offending_text in finding.matched
                or finding.matched in offending_text
            )
        ]
        if not matching:
            return []
        findings.extend(matching)
    return findings


def scan_adapter_blocks(text: str, *, source: str = "<text>") -> list[Finding]:
    """An OPTIONAL provider block may add launch mechanics and nothing else."""
    problems: list[Finding] = []
    lines = text.splitlines()
    _, suppressed = counterexample_blocks(text)
    for n, line in enumerate(lines):
        if (n + 1) in suppressed:
            continue
        marker = ADAPTER_MARKER.search(line)
        if not marker:
            continue
        # The block is the marker's own paragraph: marker line through the next
        # blank line. Scanning to end-of-file would attribute the whole rest of
        # the document to the adapter, which is how this first cried wolf.
        for offset, body in enumerate(lines[n:], start=n + 1):
            if offset > n + 1 and not body.strip():
                break
            if offset in suppressed:
                continue
            hit = ADAPTER_FORBIDDEN.search(body)
            # "no queue, no gate" prohibits; it does not define. Same negation
            # rule the authority guard uses, for the same reason.
            if hit and not negated(body, hit.start()):
                problems.append(
                    Finding(
                        source, offset, "adapter-overreach",
                        f"adapter block for '{marker.group(1)}' contains "
                        f"'{hit.group(1)}' — an adapter may never touch the "
                        f"role, queue, branch, authority or gate",
                    )
                )
    return problems


def adapter_policy_hits(text: str) -> list[str]:
    """Policy vocabulary an adapter may not use, ignoring prohibitions of it."""
    return [
        m.group(1) for line in text.splitlines()
        for m in ADAPTER_FORBIDDEN.finditer(line)
        if not negated(line, m.start())
    ]


# --- Command line -----------------------------------------------------------
#
# See the note in `authority.py`: the path walk is duplicated there rather than
# shared, because both files are copied standalone into other repositories and
# a scanner that needs a private helper module breaks when only it is copied.


def _iter_files(paths: Sequence[str]) -> list["Path"]:
    from pathlib import Path
    out: list[Path] = []
    for raw in paths:
        p = Path(raw)
        if not p.exists():
            raise FileNotFoundError(raw)
        if p.is_dir():
            out += sorted(q for q in p.rglob("*.md") if q.is_file())
        elif p.is_file():
            out.append(p)
        else:
            raise OSError(f"not a regular file or directory: {raw}")
    return out


def main(argv: Sequence[str] | None = None) -> int:
    """Scan files or directories for provider-shaped lane identity.

    `--roles` is required and has no default. That is the same fail-closed
    decision `scan` makes by raising on an empty role set: every rule here is
    expressed over the caller's declared roles, so guessing them would make the
    scan quietly meaningless rather than loudly wrong.

    Exit 0 clean, 1 findings, 2 nothing scanned.
    """
    import argparse

    ap = argparse.ArgumentParser(
        prog="neutrality.py",
        description=(
            "Detect provider-shaped lane identity -- roles, branch namespaces, "
            "queues and topology that derive from a vendor rather than a role."
        ),
        epilog=(
            "Scans exactly what it is pointed at. Selecting the normative "
            "surface is the caller's job -- a historical document that names "
            "the tool that actually ran will report findings, correctly. "
            "Exit status: 0 clean, 1 findings found, 2 nothing was scanned."
        ),
    )
    ap.add_argument(
        "paths", nargs="+",
        help="files, or directories to scan recursively for *.md",
    )
    ap.add_argument(
        "--roles", required=True,
        help="comma-separated executor roles this project declares, e.g. "
             "'worker,verifier' -- the single source of truth for lane identity",
    )
    ap.add_argument(
        "--coordinator", default="brain",
        help="the coordinating role, which may also own branches "
             "(default: brain)",
    )
    ap.add_argument(
        "--queue-pattern", default=None,
        help="optional regex with one capture group yielding a queue stem; "
             "only for projects that keep such files",
    )
    ap.add_argument(
        "--max-lanes", type=int, default=None,
        help="optional; prose claiming more standing lanes than this is a "
             "violation. Leave unset for documents discussing several topologies",
    )
    ap.add_argument(
        "--quiet", action="store_true",
        help="print nothing; use the exit status only",
    )
    args = ap.parse_args(argv)

    roles = tuple(r.strip() for r in args.roles.split(",") if r.strip())
    if not roles:
        print("neutrality: --roles must name at least one role; an empty set "
              "would make every rule vacuous", file=sys.stderr)
        return 2

    try:
        files = _iter_files(args.paths)
    except OSError as exc:
        print(f"neutrality: cannot scan requested path: {exc}", file=sys.stderr)
        return 2
    if not files:
        print("neutrality: no files matched; refusing to report success",
              file=sys.stderr)
        return 2

    findings: list[Finding] = []
    inert: list[str] = []
    for path in files:
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            print(f"neutrality: cannot read {path}: {exc}", file=sys.stderr)
            return 2
        result = scan(
            text, roles,
            source=str(path),
            coordinator=args.coordinator,
            queue_pattern=args.queue_pattern,
            max_lanes=args.max_lanes,
        )
        findings += result.findings
        findings += scan_adapter_blocks(text, source=str(path))
        # An exemption that protects nothing is a silent widening of the guard.
        inert += [
            f"{path}:{c.line} counterexample block suppresses nothing"
            for c in result.inert_counterexamples()
        ]

    if not args.quiet:
        for finding in findings:
            print(finding)
        for line in inert:
            print(line)
        print(
            f"neutrality: {len(findings) + len(inert)} finding(s) in "
            f"{len(files)} file(s)",
            file=sys.stderr,
        )
    return 1 if (findings or inert) else 0


if __name__ == "__main__":
    sys.exit(main())

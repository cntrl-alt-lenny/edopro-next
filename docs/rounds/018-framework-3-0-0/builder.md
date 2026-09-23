<!-- fw-report
round: 018-framework-3-0-0
role: builder
branch: builder/018-framework-3-0-0
head: f80189212be34b77510fef9bd73e5f9353ea98ce
os: macOS 27.0
python: 3.9.6
written: 2026-09-23T10:40:25Z
-->
## Verified

Operating system, by command: `uname -a` →
`Darwin ... 27.0.0 Darwin Kernel Version 27.0.0 ... RELEASE_ARM64_T8103 arm64`
(macOS, Apple Silicon). `python3 --version` → `Python 3.9.6`; a second
interpreter, `/opt/homebrew/bin/python3.13`, was also available and used
where 3.9 could not run a step (noted below).

### 1. Seat start

`python3 <framework>/tools/fw.py --cwd <project> start --role builder --round 018-framework-3-0-0`
→ exit 0:
```
seat ok: builder, round 018-framework-3-0-0, branch builder/018-framework-3-0-0 at 986eaf393f41
  brief: docs/rounds/018-framework-3-0-0/brief.md
  finish with: write docs/rounds/018-framework-3-0-0/builder.md, then python3 tools/fw.py report --role builder --round 018-framework-3-0-0 --push
```

`git submodule update --init`, run immediately after, → exit 0:
```
Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
Cloning into '.../project/ocgcore'...
Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
```

`git submodule status` →
```
 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
```

### 2. Adoption

**First `adopt.py --update --dry-run`** (from the framework clone at tag
`v3.0.0`, `git clone --branch v3.0.0` — the clone printed
`warning: refs/tags/v3.0.0 ... is not a commit!` because it is an annotated
tag; `git switch` still landed on its commit `7f5bbc3599ea01533891712f2e86d8323c8c1001`
in detached HEAD, which is what was used throughout) → exit 0:
```
update: <project> -> agentic-framework 3.0.0 (from 2.x (no record))
  create  docs/agents/FRAMEWORK.md
  replace docs/agents/roles/brain.md
  replace docs/agents/roles/worker.md
  replace docs/agents/roles/verifier.md
  create  tools/fw.py
  create  tests/test_framework.py
  create  docs/rounds/README.md
  beside  .claude/agents/brain.md.framework
  beside  .claude/agents/verifier.md.framework
  create  .claude/agents/worker.md
  beside  .claude/commands/status.md.framework
  remove  docs/agents/briefs.md  (unedited copy from an earlier release)
  remove  docs/agents/kickoff.md  (unedited copy from an earlier release)
  remove  docs/agents/lifecycle.md  (unedited copy from an earlier release)
  remove  tests/test_checkout.py  (unedited copy from an earlier release)
  remove  tests/test_report.py  (unedited copy from an earlier release)
  remove  tests/test_role_neutrality.py  (unedited copy from an earlier release)
  remove  tools/authority.py  (unedited copy from an earlier release)
  remove  tools/line_endings.py  (unedited copy from an earlier release)
  remove  tools/neutrality.py  (unedited copy from an earlier release)
  remove  tools/textblocks.py  (unedited copy from an earlier release)
  same    .gitattributes
  keep    AGENTS.md  (project-owned)
  keep    docs/state.md  (project-owned)
  keep    .githooks/pre-push  (project-owned)
  keep    CLAUDE.md  (project-owned)
  keep    .claude/settings.json  (edited in this project, so kept; delete it once nothing needs it)
  keep    .claude/README.md  (unedited, but .claude/settings.json still refers to it; remove that use, then delete it)
  keep    .claude/hooks/run_python.sh  (unedited, but .claude/settings.json still refers to it; remove that use, then delete it)
  keep    .claude/hooks/save_agent_reply.py  (unedited, but .claude/settings.json still refers to it; remove that use, then delete it)
  keep    docs/agents/roles/README.md  (unedited, but tests/test_docs_consistency.py still refers to it; remove that use, then delete it)
  keep    tools/checkout.py  (unedited, but tests/test_docs_consistency.py still refers to it; remove that use, then delete it)
  keep    tools/report.py  (unedited, but .claude/hooks/save_agent_reply.py still refers to it; remove that use, then delete it)
  keep    docs/agents/topologies.md  (docs/agents/roles/README.md, which is kept, links to it; delete both together)
  keep    docs/agents/CONSTITUTION.md  (docs/agents/roles/README.md, which is kept, links to it; delete both together)
  keep    docs/agents/adapters.md  (docs/agents/roles/README.md, which is kept, links to it; delete both together)
  keep    docs/agents/evidence.md  (docs/agents/CONSTITUTION.md, which is kept, links to it; delete both together)
  keep    docs/agents/reports.md  (docs/agents/CONSTITUTION.md, which is kept, links to it; delete both together)
  keep    docs/agents/git-and-isolation.md  (docs/agents/adapters.md, which is kept, links to it; delete both together)
  record  docs/agents/framework.json

Edited framework files were left alone. Review each difference, move any
project-specific content into AGENTS.md or docs/agents/local/, then replace the
file with its .framework copy:
  .claude/agents/brain.md  <-  .claude/agents/brain.md.framework
  .claude/agents/verifier.md  <-  .claude/agents/verifier.md.framework
  .claude/commands/status.md  <-  .claude/commands/status.md.framework

dry run: nothing written
```

**Real `adopt.py --update`** → same plan lines as above (exit 0), plus it
printed the full "What an adopter must do" text for 3.0.0 and this project
check summary (before this round's fixes):
```
Project checks still to satisfy (python3 tools/fw.py check):
  error: docs/state.md is 3555 words; its budget is 1000. Keep decisions, move history into docs/rounds/ or a dedicated document
  error: docs/state.md stores a full commit id outside '## Historical anchors'. Live state is derived with fw.py status, never stored
  warning: AGENTS.md has no 'Merge rule:' line (owner-approves or brain-merges)
  warning: AGENTS.md is 3407 words; every session reads it, aim for under 2500
  error: docs/state.md:305 contains a Windows drive path; tracked documents must work on every machine and are public
  error: docs/state.md:306 contains a Windows drive path; tracked documents must work on every machine and are public
4 error(s), 0 warning(s)
```
All six are closed by this round's commits (see "Changed" and the final
`fw.py check` below).

### 3. The `.framework` merges

- **`.claude/agents/brain.md`**: diff was frontmatter (`description:`) plus
  the body — the installed file restated the contract instead of pointing
  at it (`## Specifics for this seat on this tool`: "primary interactive
  session", "/status runs the contract's rehydration sequence", "No model
  is pinned", "shared inbox may hold other seats' reports ... see the
  adapter README"). None of that is project-specific: the model-pinning
  fact is now implicit (no seat file sets `model:`), the `/status`
  reference is redundant with the command file itself, and the shared-inbox
  sentence describes the retired Stop-hook mechanism. Replaced with the
  `.framework` copy (commit `51985c94`).
- **`.claude/agents/verifier.md`**: same shape — frontmatter plus a body
  restating "own checkout, detached at the exact head SHA", the `tools:`
  frontmatter explanation, and the same retired shared-inbox sentence. The
  `tools: Read, Grep, Glob, Bash, WebFetch` frontmatter restriction itself
  was dropped along with the rest of the body — nothing in
  `docs/agents/roles/verifier.md` (old or new) requires that restriction be
  declared in the adapter, and the new file inherits the framework's plain
  pointer shape. Replaced with the `.framework` copy (commit `51985c94`).
- **`.claude/commands/status.md`**: the installed version fully inlined the
  old `docs/agents/roles/brain.md` rehydration steps (branch-protection
  `gh api` calls, `core.hooksPath` check, `DO NOT MERGE` PR check,
  `docs/briefs/active.md`/`delivered/` check). The commands themselves
  (`gh api repos/.../branches/master --jq .protected`, not
  `rules/branches/master`) are preserved in new `AGENTS.md`'s "What is
  actually enforced" section; the brief/inbox-state checks describe the
  retired `docs/briefs/active.md` workflow and have no destination because
  `fw.py status` now derives the equivalent live state directly. Replaced
  with the `.framework` copy (commit `51985c94`).

### 4. Rule accounting (every project rule in the old AGENTS.md, old
CLAUDE.md, every old `.claude/` file, and every deleted document)

Old `CLAUDE.md` (7,717 bytes, commit before this round):

| Rule | Source | Destination | Why |
|---|---|---|---|
| Rules engine must not become UI / UI must not implement rules | `CLAUDE.md:27-28` | `AGENTS.md` "What this project is" (block quote) | — |
| UI never decides legality/targetability, renders model + sends responses | `CLAUDE.md:32-33` | `AGENTS.md` "What this project is" | — |
| Client model is semantic, no Irrlicht/Qt types | `CLAUDE.md:34-35` | `AGENTS.md` "What this project is" | — |
| `ocgcore`/CardScripts authoritative, engine right if disagreement | `CLAUDE.md:36-37` | `AGENTS.md` "What this project is" | — |
| Authoritative-components table (`ocgcore/`, CardScripts, BabelCDB) | `CLAUDE.md:41-45` | `AGENTS.md` Invariants, "Authoritative and not ours to modify" | — |
| Defect there is upstream issue/PR, never local patch | `CLAUDE.md:47-48` | same bullet | — |
| Where-code-belongs table | `CLAUDE.md:52-61` | `AGENTS.md` Invariants, "Where code belongs" | — |
| New systems in owned dirs, not scattered in `gframe/` | `CLAUDE.md:63` | same bullet | — |
| Baseline build commands (Linux) | `CLAUDE.md:69-76` | **retired** | duplicated in full in `docs/BASELINE.md`, which `AGENTS.md` now points to instead of repeating the block |
| Two build gotchas (CRLF, `/mnt/c`) | `CLAUDE.md:80-83` | `AGENTS.md` Invariants, "Baseline build" (named, detail stays in `docs/BASELINE.md`) | — |
| Upstream has no test suite; never claim tests pass for a build | `CLAUDE.md:85-86` | same bullet | — |
| "Touched the build/model/presentation/duel behaviour" checklist | `CLAUDE.md:93-100` | `AGENTS.md` Evidence section + table (per-row, not as a checklist) | — |
| "this refactor changed presentation, not duel behaviour" framing quote | `CLAUDE.md:102-104` | **retired** | motivational framing, not a checkable rule; substance (never claim unchanged duel behaviour without a real mechanism) is in the Evidence section |
| C++20, match surrounding style | `CLAUDE.md:110-111` | `AGENTS.md` Invariants | — |
| No gratuitous reformatting of upstream code | `CLAUDE.md:112-113` | `AGENTS.md` Invariants, "Where code belongs" | — |
| No new dependency without an ADR justification | `CLAUDE.md:114-116` | `AGENTS.md` Invariants | — |
| Python is tooling only, never render/input loop | `CLAUDE.md:117` | `AGENTS.md` Invariants | — |
| `upstream` remote config, push disabled, do not undo | `CLAUDE.md:128-134` | `AGENTS.md` Invariants, "Upstream merge policy" | — |
| How to take upstream changes; record in `UPSTREAM.md` | `CLAUDE.md:136-137` | same bullet | — |
| "clean merges expected because gframe/ untouched" rationale | `CLAUDE.md:139-140` | **retired** | explanatory rationale, not an actionable rule; the actionable content (touch `gframe/` minimally) is already stated |
| AGPL-3.0-or-later, preserved | `CLAUDE.md:144` | `AGENTS.md` Invariants, "Licensing" | — |
| Preserve copyright notices/LICENSE/COPYING/notices/ | `CLAUDE.md:146` | same bullet | — |
| No relicensing | `CLAUDE.md:147-148` | same bullet | — |
| Qt dynamically linked | `CLAUDE.md:148` | same bullet | — |
| No card artwork/.cdb/CardScripts committed | `CLAUDE.md:149-150` | same bullet | — |
| No implied Ignis/Konami/Shueisha affiliation | `CLAUDE.md:151` | same bullet | — |
| Never describe planned functionality as shipped | `CLAUDE.md:157-158` | `AGENTS.md` Invariants, "Honesty" | — |
| No fake functionality/stub screens/placeholder data | `CLAUDE.md:159-160` | same bullet | — |
| Report failures precisely, leave repo clean, no quiet narrowing | `CLAUDE.md:161-163` | same bullet | — |
| Say plainly what was not verified | `CLAUDE.md:164` | same bullet | — |
| Do not rewrite working engine logic because it looks old | `CLAUDE.md:168` | `AGENTS.md` Invariants, "Do not" | — |
| Do not delete Irrlicht before replacement reaches parity | `CLAUDE.md:169` | `AGENTS.md` Invariants "Do not" **and** `docs/state.md` "Owner decisions" | — |
| Do not start migration with duel field | `CLAUDE.md:170-171` | `AGENTS.md` Invariants "Do not" **and** `docs/state.md` "Owner decisions" | — |
| No Rust between UI and engine (ADR 0001) | `CLAUDE.md:172` | `AGENTS.md` Invariants "Do not" **and** `docs/state.md` "Owner decisions" | — |
| No giant commits | `CLAUDE.md:173` | `AGENTS.md` Working discipline | — |

Old `AGENTS.md` (~3,407 words) — rows only for content not already listed
above as a `CLAUDE.md` duplicate:

| Rule | Source | Destination | Why |
|---|---|---|---|
| "This file exists to hold both at once. Should stay around this length." | `AGENTS.md:19` | **retired** | self-referential meta comment; new `AGENTS.md`'s length is enforced by `fw.py check` instead |
| Topology table (Owner/Brain/Builder/Verifier) | `AGENTS.md:29-34` | new `AGENTS.md` "Roles" table | — |
| "Builder is this project's name for the Worker... old docs/roles/ retired pointers only" | `AGENTS.md:36-41` | new `AGENTS.md` "Roles" table note | the "retired pointers" sentence itself is now moot: `docs/roles/` no longer exists at all (deleted this round), not merely repointed |
| "launch mechanics are adapters... docs/agents/adapters.md and launching.md" | `AGENTS.md:39-41` | **retired** | both files deleted this round (2.x-only); `docs/agents/FRAMEWORK.md`'s "Moving between machines and tools" plus the `.claude/` seat files now cover this |
| "docs/agents/model-notes.md records what has been observed... log not ranking" | `AGENTS.md:43-45` | **retired** | file deleted as a 2.x-only log; the substantive preference is kept (next row) |
| Verifier model-diversity preference, context-diversity-is-what-matters | `AGENTS.md:47-59` | new `AGENTS.md` "Roles" section | — |
| "Briefs describe the problem, not the solution" + goal→why→scope template | `AGENTS.md:61-73` | new `AGENTS.md` "Roles" section, as a pointer to `docs/agents/FRAMEWORK.md`'s own brief template | restating the template here would drift from the framework's now-authoritative one |
| "Non-negotiable project invariants" section (restates `CLAUDE.md`) | `AGENTS.md:75-96` | superseded by the merged Invariants section (see `CLAUDE.md` rows above) | this section's whole purpose was restating `CLAUDE.md`; now that `CLAUDE.md`'s content lives directly in `AGENTS.md`, the separate restatement is gone |
| Real-defects-are-semantic-mismatches framing + PR #12/#13 examples | `AGENTS.md:100-113` | new `AGENTS.md` Evidence section opening (condensed); PR #12/#13 detail **retired** | historical examples condensed to a pointer at the brief archive to fit the word budget; the rule itself ("it compiles is not evidence") is fully kept |
| "It compiles is not evidence. Neither is tests pass on tests that could not fail" | `AGENTS.md:115-116` | new `AGENTS.md` Evidence section | — |
| Green replay harness is not evidence; blocking review finding | `AGENTS.md:117-122` | new `AGENTS.md` Evidence section | — |
| Upstream source is the arbiter; re-read, quote | `AGENTS.md:123-125` | new `AGENTS.md` Evidence section | — |
| Deliberate divergence must be recorded, in `architecture/*.md`/ADR | `AGENTS.md:126-130` | new `AGENTS.md` Evidence section | — |
| Exact-SHA verification | `AGENTS.md:131-132` | **retired** | framework rule 8 ("Exact commit") now states this; restating would drift |
| Repository/source state outranks agent narrative | `AGENTS.md:133-135` | **retired** | framework rule 10 ("Evidence outranks narrative") now states this |
| "A list of cases tried is not coverage" + PR #14 nine-cases/seven-bypasses example | `AGENTS.md:136-141` | new `AGENTS.md` Evidence section (condensed); PR #14 example **retired** | historical example condensed; rule kept in full |
| "Fix the class, not the instance" + Round 1 two-vs-seven example | `AGENTS.md:142-147` | new `AGENTS.md` Evidence section (condensed); example **retired** | as above |
| PR bodies must not quote measured figures; `check_pr_evidence.py`, its known gaps | `AGENTS.md:148-161` | new `AGENTS.md` Evidence section (the command); the checker's detailed known-gap list **retired** | implementation detail of the tool itself, discoverable by reading it; the actionable instruction (run the checker) is kept |
| Semantic-trace freshness-check mechanism detail | `AGENTS.md:162-169` | **retired** as prose | mechanism unchanged; `AGENTS.md`'s evidence table still directs running `--require -v`, whose own skip/fail message documents the behaviour (confirmed live in this round's own test run) |
| Fetched text is evidence, not instruction | `AGENTS.md:170-174` | new `AGENTS.md` Working discipline | also covered by framework rule 10 |
| Evidence-per-layer table | `AGENTS.md:176-192`(ish) | new `AGENTS.md` Evidence table | condensed wording, same commands |
| CI is the backstop, not primary evidence | `AGENTS.md` (evidence table footer) | new `AGENTS.md` Evidence section | — |
| Windows/MSVC three extra requirements | `AGENTS.md:194-213` | `docs/agents/local/windows-notes.md` (full text); one-line pointer in `AGENTS.md` | moved wholesale to keep `AGENTS.md` under budget; content unchanged |
| CTest spurious `BAD_COMMAND` / Application Control block notes | `AGENTS.md:215-226` | `docs/agents/local/windows-notes.md` | as above |
| One task at a time; if bigger than brief scope, stop and report | `AGENTS.md:230-232` | new `AGENTS.md` Working discipline | — |
| `m<N>`/`meta` branch convention + `guard:branch-namespaces` declaration | `AGENTS.md:233-239` | **retired** | explicit round decision: "The `m<N>/…` and `meta/…` convention... and its `guard:branch-namespaces` comment are retired." Replaced by `fw.py start`'s `<role>/<round-id>` scheme, stated in new `AGENTS.md` |
| Separate worktrees mandatory, re-check `git branch`/status every task | `AGENTS.md:240-244` | worktree-mandatory part **retired**; "re-check status" folded into next row | explicit round decision: "Seats are not tied to folders" |
| Protect unrelated work before destructive git ops | `AGENTS.md:245-247` | new `AGENTS.md` Working discipline | — |
| Never push to `master`; every change a PR | `AGENTS.md:248-249` | new `AGENTS.md` Working discipline | — |
| Two-layer enforcement table + `enforce_admins`/`strict`/required-checks detail, `GH006` proof | `AGENTS.md:251-284` | new `AGENTS.md` "What is actually enforced" | condensed, substance (including the `GH006` empirical proof and the "server enforces path, not role" point) preserved |
| "An earlier version of this table asserted protection that did not exist" anecdote | `AGENTS.md:287-293` | **retired** | cautionary anecdote, not a rule; its actionable consequence (always re-verify live via the exact `gh api ... --jq .protected` command, never `rules/branches/master`) is fully preserved |
| `pre-push` hook design rationale, PreToolUse-hook history, seven bypasses | `AGENTS.md:295-304` | **retired** | historical design rationale preserved in git history (PR #14); the actionable rule ("CI requires push-guard tests to run, not skip") is kept in "What is actually enforced" |
| "Do not treat local hook as the control" | `AGENTS.md:306-307` | new `AGENTS.md` "What is actually enforced" | — |
| No giant commits | `AGENTS.md:308` | new `AGENTS.md` Working discipline | duplicate of the `CLAUDE.md` row above |
| State handoff: `docs/state.md` durable, `docs/briefs/` is in-flight queue | `AGENTS.md:309-312` | **retired** | `docs/briefs/` is no longer the in-flight queue; framework rules 2 and 5 (`docs/agents/FRAMEWORK.md`) now state this correctly for `docs/rounds/` |
| "The round" section: old 7-step brief/PR/archive/model-notes process | `AGENTS.md:314-357` | **retired in full** | superseded by `docs/agents/FRAMEWORK.md`'s own "The round" section, which describes the actual (new) `fw.py`-based process; restating the old `docs/briefs/`-based one would now be actively wrong |
| Verifier finding-class definitions (BLOCKER/SHOULD FIX/NOTE/UNPROVEN CLAIM) | `AGENTS.md:358-369` | **retired from AGENTS.md** | fully covered by new `docs/agents/roles/verifier.md`'s own "## Findings" section; restating would drift |
| "Brain independently checks every BLOCKER/UNPROVEN CLAIM; Verifier is evidence not verdict" | `AGENTS.md:371-373` | **retired from AGENTS.md** | covered by framework rule 9 and `docs/agents/roles/brain.md`'s "Judging a round" |
| "Where to look" pointer list | `AGENTS.md:375-389` | new `AGENTS.md` "Where to look" | condensed; links to deleted files (`worktree-mechanism.md`, `model-notes.md`, `docs/briefs/active.md`) removed, `docs/rounds/` added |

Old `.claude/` files:

| Rule | Source | Destination | Why |
|---|---|---|---|
| Builder adapter: points at `worker.md` contract, own checkout, fresh context, no model pinned | `.claude/agents/builder.md` (whole file) | `.claude/agents/worker.md` (the framework's own worker seat file, now used by Builder) | explicit round decision: a second Builder-named adapter pointing at the same contract would just drift; the framework's `worker.md` seat file serves this role directly |
| Adapter README: hook mechanism, Python-wrapper rationale, "conveniences fail loudly", absence-vs-breakage table | `.claude/README.md` (whole file) | **retired** | describes only the retired Stop-hook/inbox convenience; 3.0.0 removed that mechanism entirely (reports are committed directly by the seat) |
| `settings.json`: wires the Stop hook to `save_agent_reply.py` | `.claude/settings.json` | **retired** | the hook it wired is gone |
| `run_python.sh`: try `python3`, then `py -3`, then `python`, for real not just existence | `.claude/hooks/run_python.sh` | **retired** as a script; substance preserved | no hook remains to invoke it, but the same fallback order is now documented directly in `docs/agents/FRAMEWORK.md`'s own command convention ("If `python3` is not found, use `py -3` ... or `python`"), which every round-start command in this project now follows |
| `save_agent_reply.py`: mirror final reply to inbox via `tools/report.py`, self-written-report precedence rules | `.claude/hooks/save_agent_reply.py` and `tests/test_save_agent_reply.py` | **retired** | CHANGELOG 3.0.0: "its Stop hook is retired because reports are now committed by the seat itself" |

Deleted framework copies **adopt.py's dry-run itself reports as unedited**
(one row each, per the brief's exemption for this category — see the dry-run
output in §2 above for the exact line quoted):

| File | Adopt-reported state | Destination |
|---|---|---|
| `docs/agents/briefs.md` | "remove ... (unedited copy from an earlier release)" | **retired**, held no project rules |
| `docs/agents/kickoff.md` | same | **retired**, held no project rules |
| `docs/agents/lifecycle.md` | same | **retired**, held no project rules |
| `tests/test_checkout.py` | same | **retired**, held no project rules (tested the framework's own `checkout.py`) |
| `tests/test_report.py` | same | **retired**, held no project rules (tested the framework's own `report.py`) |
| `tests/test_role_neutrality.py` | same | **retired**, held no project rules |
| `tools/authority.py` | same | **retired**, held no project rules |
| `tools/line_endings.py` | same | **retired**, held no project rules |
| `tools/neutrality.py` | same | **retired**, held no project rules |
| `tools/textblocks.py` | same | **retired**, held no project rules |
| `docs/agents/roles/README.md` | "keep ... (unedited, but tests/test_docs_consistency.py still refers to it...)" | **retired** once the reference (the test) was removed; held no project rules, pure index of the three contracts `docs/agents/FRAMEWORK.md` now names directly |
| `tools/checkout.py` | "keep ... (unedited, but tests/test_docs_consistency.py still refers to it...)" | **retired**; superseded by `tools/fw.py start` |
| `tools/report.py` | "keep ... (unedited, but .claude/hooks/save_agent_reply.py still refers to it...)" | **retired**; superseded by `tools/fw.py report` |
| `docs/agents/topologies.md` | "keep ... (roles/README.md, which is kept, links to it...)" | **retired**; held no project rules, `docs/agents/FRAMEWORK.md`'s "Tiers" and role table cover the same ground |
| `docs/agents/CONSTITUTION.md` | same chain | **retired**; held no project rules, superseded by `docs/agents/FRAMEWORK.md` in full |
| `docs/agents/adapters.md` | same chain | **retired**; held no project rules |
| `docs/agents/evidence.md` | "keep ... (CONSTITUTION.md, which is kept, links to it...)" | **retired**; held no project rules, its content (exact-SHA discipline, re-derivation, "a list of cases is not coverage") is framework rules 8-10 |
| `docs/agents/reports.md` | same chain | **retired**; held no project rules, describes the retired private-inbox mechanism `tools/fw.py report` replaces |
| `docs/agents/git-and-isolation.md` | "keep ... (adapters.md, which is kept, links to it...)" | **retired**; held no project rules — its one project-specific fact (`ocgcore` submodule init after a fresh worktree) is preserved, see next table |

Deleted project-owned 2.x leftovers (not framework copies — the brief
itself calls these "project-owned 2.x leftovers"):

| File | What it held | Destination | Why |
|---|---|---|---|
| `docs/roles/README.md` | "canonical contracts now live under `../agents/roles/`; retained only so historical links resolve" | **retired** | pure redirect stub; nothing links to `docs/roles/` any more (confirmed by the grep sweep below) |
| `docs/roles/brain.md` | "canonical Brain contract is `../agents/roles/brain.md`" | **retired** | same |
| `docs/roles/builder.md` | "Builder uses the canonical Worker contract... not a second Builder contract" | **retired** | same |
| `docs/roles/verifier.md` | "canonical Verifier contract is `../agents/roles/verifier.md`" | **retired** | same |
| `docs/agents/model-notes.md` | Dated log of model/seat observations across 15 rounds | **retired** | 2.x-only log; the one still-live preference it recorded (Verifier model diversity) is in new `AGENTS.md`'s "Roles" section; the round-by-round observations themselves are historical narrative preserved in git history (this file's own commits) and in `docs/state-history.md`'s round-by-round summaries, not reproduced verbatim |
| `docs/agents/launching.md` | Universal launch procedure, Claude Code specifics, shared-inbox fallback chain, model-choice pointer | **retired** | universal procedure is now `docs/agents/FRAMEWORK.md`'s "The round"; Claude Code specifics are the `.claude/` seat files' own "Specifics for this seat" sections; the shared-inbox fallback chain describes the retired mechanism |
| `docs/agents/worktree-mechanism.md` | Worktree layout, setup commands (incl. `ocgcore` submodule init), submodule gotcha, shared-inbox note | **retired**; the `ocgcore` submodule-init fact **moves to `AGENTS.md`** | explicit round decision: seats are not tied to folders; the one fact every future seat still needs (`git submodule update --init` after `fw.py start`, since it does not do this itself — framework issue 19) is now in new `AGENTS.md`'s "Roles" section |
| `tests/test_docs_consistency.py` | Brief-lifecycle structural checks (`active.md` exists, status-vs-directory, unique numbers), coordination-doc link resolution, role-contract-adapter restatement pins | **retired** | its `BriefLifecycleTest` class asserted `docs/briefs/active.md` must always exist, which this round makes false by design; its `RoleContractTest` pinned exact 2.x adapter text (e.g. that `docs/agents/roles/builder.md` must never appear in the adapter) against files this round replaces wholesale. `tests/test_framework.py` (installed by `adopt.py`) is the 3.0.0 replacement for hygiene checking; no equivalent link-checker was carried forward — noted under "Open questions" |

### 5. State changes (every sentence added to and removed from `docs/state.md`)

**Old `docs/state.md` was 3,555 words.** Below is every sentence/bullet
removed, with where it went. Bullet-level granularity is used where the
source was already a bullet list (each bullet is one to a few sentences,
and none of them split across destinations); free prose is split by
sentence.

**§ Repository** (all removed):
- "cntrl-alt-lenny/edopro-next, default branch master. Standalone repository carrying full upstream history, forked from edo9300/edopro at 54ea755aa0243e2f18bb6bd2187fc9b2f7e29788 (2026-08-20)" → split: the fork fact → `docs/state.md` new "Historical anchors" section; the rest → deleted as redundant restatement of the repo's own identity, obvious from `git remote -v`.
- The `origin`/`upstream` remote block (3 lines) → deleted as stale; duplicate of `AGENTS.md`'s "Upstream merge policy" bullet (same content already there before this round, now still there).
- "Accepted-state anchor: origin/master was f1eb9d66... after PRs #19, #20 and #21 merged... This is an anchor, not a current value" → deleted as stale: exactly the kind of stored commit-id-as-live-state the framework's rule 3 exists to stop; the round's own `git log` supersedes it many times over since it was written (round 016 has since merged as PR #32).
- "Every milestone so far landed through a reviewed PR... enforced server-side since 2026-08-31 — see AGENTS.md." → deleted as duplicate; `AGENTS.md`'s "What is actually enforced" section already states this.

**§ Milestones table** → condensed into `docs/state.md` new "Where we are
going" paragraph (every milestone's status line kept in substance: M0 done,
M1 Level 1 done/Level 2 not started, M2 done, M3 core done/UI item open,
M4-M6 not started, M5 last).

**§ Architecture boundaries table** → deleted as duplicate; `AGENTS.md`
Invariants "Where code belongs" already states the same boundaries.

**§ What is proven, versus what is merely intended** (the whole section):
- Its intro sentence ("This distinction is the single most useful thing in
  this file") → deleted, editorial framing.
- The "Proven, with a mechanism that can fail" list (11 bullets: golden
  traces, derived tables, transactional-decoding invariant, fixture
  equivalence + fault injection, `client`/`data`/`policy` build with no
  Qt/Irrlicht/vcpkg/`ocgcore`, push-guard mutation testing, server-side
  protection + `GH006` proof, `.ydk` interop proof) → **all 11 moved
  verbatim in substance to `docs/state-history.md`**, "Proven, with a
  mechanism that can fail (as of 2026-09-22)".
- The "Not proven, and must not be claimed" list (5 bullets: duel-behaviour
  unchanged, complete legacy/engine equivalence, deck-opens-in-upstream
  end-to-end, semantic coverage beyond 34 messages, cross-platform
  equality) → **kept live**, condensed, in new `docs/state.md`'s own
  "Not proven, and must not be claimed" section — required by this round's
  acceptance criteria ("nothing on its 'not proven' list disappears from
  every live document").

**§ Intentional upstream deltas** (4 bullets: card database, deck model,
card search, deck legality, each with its doc/ADR pointer) → kept, in new
`docs/state.md`'s "Intentional upstream deltas" section, condensed to one
line per item with the same doc/ADR pointers.

**§ Parked — do not reopen without new evidence** (5 bullets: ADR 0001,
M1 Level 2 scoping, duel-field ordering, no-delete-Irrlicht, M2 design) →
split: ADR 0001, duel-field ordering and no-delete-Irrlicht → new
`docs/state.md` "Owner decisions" (they are decisions, not parked
questions, per the template's own distinction); M1 Level 2 scoping → new
`docs/state.md` "Parked" section; the M2 design bullet ("ADR 0002... the
central fix for transactional decoding") → deleted as stale, since M2 is
now done (per the milestones table) and its design is documented in ADR
0002 itself, not something still "parked".

**§ In flight** (the whole section — PR #14/#15/#17 records, round-3
PRs #19-22, briefs 010-016 recaps, "two things this round established",
Dev Hub log, "Operating across machines", "Shared-framework status",
"Correction lists" lesson, the `ocgcore`-submodule-in-worktrees note):
- Every brief/PR record (PR #14 through brief 016, plus the two "lessons"
  paragraphs and the "correction lists" lesson) → moved to
  `docs/state-history.md`'s "Brief-by-brief round history" section, in
  substance (condensed prose, same facts, same PR/commit ids).
- The `ocgcore`-submodule-in-worktrees paragraph → deleted as superseded;
  the worktree mechanism it described is retired this round, replaced by
  the `git submodule update --init` instruction now in `AGENTS.md`.
- "Dev Hub" subsection (what it is, where it lives on the Windows machine,
  what has been read/written there) → moved to `docs/state-history.md`'s
  "Dev Hub (historical...)" section, with **both Windows drive paths
  removed** (`fw.py check` flagged these at `docs/state.md:305-306`;
  the archived version states only that a Dev Hub existed and was used,
  without repeating either path).
- "Operating across machines" subsection (report-inbox-is-per-clone,
  seat-chats-open-in-clone-root, line-endings, PowerShell `bash` issue) →
  moved to `docs/state-history.md`'s "Operating across machines (recorded
  2026-09-21, now superseded)" section, explicitly marked superseded
  because the per-clone inbox it describes no longer exists.
- "Shared-framework status" subsection (pinned revision, neutrality-guard
  installation, `guard:branch-namespaces` declaration) → moved to
  `docs/state-history.md`'s "Shared-framework status, pre-3.0.0" section,
  explicitly marked superseded by this round.

**§ Local toolchain — state, and what still does not build** (the whole
section: two-machines framing, Windows toolchain table + 13/13 CTest
result + three MSVC-only defects, macOS toolchain + semantic-trace
freshness-check mechanism, two operational facts about `vcvars64.bat`/Qt
`PATH`, the "predecessor of this section was once deleted by accident"
footnote) → moved in substance to `docs/state-history.md`'s "Local
toolchain, as last exercised (pre-3.0.0)" section; the still-current
Windows/MSVC setup detail (the `vcvars64.bat`, vcpkg-toolchain-file and
Qt-`PATH` requirements) additionally lives on in
`docs/agents/local/windows-notes.md`, which `AGENTS.md`'s evidence table
now points to directly. The "deleted by accident" footnote itself →
deleted; it was a note-to-self about a past incident with no further
action attached.

**§ Known open items** (5 bullets: deck-builder-UI remainder, M1 Level 2,
"records and test gaps — Brief 016, active", presentation tidy-ups,
cross-platform CI) → split: the deck-builder-UI remainder bullet folded
into new `docs/state.md`'s "Where we are going" M3 sentence; M1 Level 2 →
already covered under "Not proven"; "Brief 016, active" → deleted as stale
(brief 016 is now merged, per its own archived record); presentation
tidy-ups and cross-platform CI → kept, in new `docs/state.md`'s "Parked"
section.

**§ Recommended next slice** ("Finish Brief 016... then presentation
tidy-ups... then M3's remaining deck-builder parts... Still not the duel
field.") → kept, in new `docs/state.md`'s "Pointers" section, updated to
drop the now-finished "Finish Brief 016" clause.

**Every sentence added to new `docs/state.md`** (629 words; the file is
reproduced in full below rather than sentence-listed a second time, since
every one of its sentences is new relative to the old file's structure —
even the ones carrying forward an old fact are reworded to fit the
template's four sections plus Historical anchors):

```
<the full text of the committed docs/state.md, verbatim, follows — see the
file itself at commit fec8efa5>
```

New `docs/state.md`'s exact content is committed at `fec8efa5` (see
"Changed" below); reproducing it a second time here would only invite the
two copies to drift, so this report points at the commit instead of pasting
it twice.

### 6. Inventory of other 2.x leftovers (not removed by this round)

- **Remote branches.** `meta/presentation-tidy` (source of brief 016's
  archived record) and any other pre-3.0.0 `m<N>/`/`meta/` branches on
  `origin` are untouched — out of scope ("deleting or changing any remote
  branch" is explicitly out of scope). Recommendation: Brain deletes merged
  task branches as routine housekeeping per the framework's own rule, once
  confirmed merged.
- **PR-body test fixtures.** `tests/fixtures/pr_bodies/*.txt` (28 files) are
  untouched; they are `check_pr_evidence.py`'s own committed corpus, not
  framework files, and the brief's scope excludes `tests/fixtures/` from
  the leftover sweep. No action needed.
- **`docs/briefs/archive/`.** Kept in full as closed history (12 prior
  brief records plus brief 016's, now added). Recommendation: none — this
  is exactly what "reads as closed history" means; nothing here duplicates
  live state.
- **The `.gitignore` entry** for `/.worktrees/` is kept, reworded to
  describe worktrees as an optional convenience rather than the mandated
  mechanism. Recommendation: none; still correct since a linked worktree
  remains legal to use.
- **The `check_pr_evidence.py` convention** (run before opening/updating a
  PR) is unchanged and still referenced from `AGENTS.md`. No leftover here.
- **`docs/adr/0010-deck-builder-ruleset-and-legality-ui.md`** contains a
  `[const auto& deck](...)` -shaped code fragment inside square brackets
  that this round's link checker script flagged as a false-positive broken
  link (it is a C++ reference-capture snippet, not a Markdown link). Not a
  framework leftover and out of this round's scope (`docs/adr/` is product
  documentation) — flagged here only because the automated sweep surfaced
  it; no change made.
- **`docs/briefs/README.md`, `archive/README.md`, `delivered/README.md`**
  still describe the retired brief lifecycle in detail (for historical
  readability of the archive they head). This is deliberate, not an
  oversight — removing the description would make the 13 archived records
  under them unreadable cold.

### 7. Checks, real output and exit status, at the reported commit

Commit reported below: `f80189212be34b77510fef9bd73e5f9353ea98ce` (branch
`builder/018-framework-3-0-0`).

**`python3 tools/fw.py check`** → exit 0:
```
0 error(s), 0 warning(s)
```

**`wc -w AGENTS.md docs/state.md`**:
```
    1477 AGENTS.md
     629 docs/state.md
    2106 total
```
(budgets: 2,500 and 1,000 respectively — both comfortably under, and
`fw.py check` above confirms it mechanically.)

**`python3 -m unittest discover -s tests -v`** (system Python 3.9.6) →
exit 1, **115 tests run, 1 failure, 11 skipped**:
```
FAIL: test_check_fails_on_a_stale_copy_and_update_repairs_it (test_readme_status.CommandLineTest)
AssertionError: 1 != 0
```
This failure is **pre-existing and unrelated to this round's diff**: it
reproduces identically, with the same traceback, against an entirely
separate, untouched checkout of this repository at the same base commit
(`/Users/leo/Dev/GitHub/edopro-next`, the coordinating session's own clone,
git-status-clean at the round's start commit `b130d5a7`) — this round's
diff over `docs/`, `tests/`, `tools/`, `.claude/`, `AGENTS.md` and
`CLAUDE.md` does not touch `tests/test_readme_status.py`,
`tools/generate_readme_status.py`, `README.md` or `docs/ROADMAP.md`.
Re-run with `/opt/homebrew/bin/python3.13` (installed on this machine,
found via `which python3.13`) → **exit 0, 115 run, 0 failures, 11
skipped**, so the failure is a Python-3.9-vs-3.13 environment difference in
that one pre-existing test, not a regression. The 11 skips are unrelated to
this round: one requires a Windows ACL to exercise, and ten require a
built `client/` semantic-trace binary, which this round's brief says is
out of scope ("C++ modules are out of scope and need not be built").

**`python3 tools/generate_messages.py --check`** → exit 0:
```
message table up to date (96 ids)
```

**`python3 tools/generate_protocol_constants.py --check`** → exit 0:
```
protocol constants up to date (187 values)
```

**Golden reproduction** (`AGENTS.md`'s `tools/`/`tests/` evidence row):
`python3 tests/test_replay_trace.py --update` under system Python 3.9.6
failed with `TypeError: write_text() got an unexpected keyword argument
'newline'` — `Path.write_text`'s `newline=` parameter needs Python ≥3.10,
and this machine's `python3` is 3.9.6. Re-run with
`/opt/homebrew/bin/python3.13` → exit 0:
```
wrote tests/golden/duel-chains-battle-yrpX.trace
wrote tests/golden/duel-extended-yrpX.trace
wrote tests/golden/duel-chains-battle-yrp.trace
```
then `git diff --exit-code -- tests/golden` → exit 0 (no diff): the three
goldens reproduce byte-for-byte from a clean tree, confirming no drift.

**`git grep` sweep**, outside `docs/briefs/archive/` and `tests/fixtures/`,
for `Dev Hub`, `.worktrees/`, `active.md`, `CONSTITUTION`, `report.py`,
`checkout.py`, `model-notes`, `launching.md` and `worktree-mechanism` →
every hit and why it is not a leftover:
- `.gitignore:69` `/.worktrees/` — the gitignore pattern itself; still
  correct since worktrees remain an optional (never required) convenience.
- `docs/briefs/README.md`, `docs/contributing.md` — describe the retired
  lifecycle / the optional worktree convenience in past/explicit terms,
  inside documents whose whole purpose is now to say what used to happen
  and what replaces it.
- `docs/rounds/018-framework-3-0-0/brief.md` — this round's own brief; an
  immutable record of the task, expected to name every file it discusses.
- `docs/state-history.md` — the archive document itself; every hit is
  inside a section explicitly marked historical/superseded.
- `tools/fw.py:693,762,766` — the framework's own legacy-detection logic
  (`fw.py status` checks whether `docs/agents/CONSTITUTION.md` or
  `docs/briefs/active.md` exist, to warn a 2.x project it needs this exact
  update); this project must not edit framework files, and their absence —
  confirmed by this same sweep finding no other hit — is the intended
  outcome of that logic, not a leftover.

**`git diff --stat origin/master -- client data policy ui integration
gframe ocgcore .github`** → empty (exit 0, no output): no product code or
CI workflow changed. C++ modules were not built or tested this round — out
of scope per the brief ("C++ modules are out of scope and need not be
built. Say that they were not.") — this round touched only coordination
documents, Python tooling docs/tests, and the `.claude`/`docs/agents`
framework surface.

### 8. Final state

**Second `python3 tools/adopt.py <project> --update --dry-run`**, run
against the committed tree (`f8018921`) → exit 0, nothing left to do:
```
update: <project> -> agentic-framework 3.0.0 (from 3.0.0)
  same    docs/agents/FRAMEWORK.md
  same    docs/agents/roles/brain.md
  same    docs/agents/roles/worker.md
  same    docs/agents/roles/verifier.md
  same    tools/fw.py
  same    tests/test_framework.py
  same    docs/rounds/README.md
  same    .gitattributes
  same    .claude/agents/brain.md
  same    .claude/agents/verifier.md
  same    .claude/agents/worker.md
  same    .claude/commands/status.md
  keep    AGENTS.md  (project-owned)
  keep    docs/state.md  (project-owned)
  keep    .githooks/pre-push  (project-owned)
  keep    CLAUDE.md  (project-owned)
  record  docs/agents/framework.json

dry run: nothing written
```
No `create`, `replace`, `beside`, `remove` line; every `keep` line says
only `(project-owned)`, asking for nothing further — matching acceptance
criterion 10 exactly.

**`python3 <framework>/tools/fw.py --cwd <project> status`** →
```
Framework
  pinned to agentic-framework 3.0.0 (https://github.com/cntrl-alt-lenny/agentic-framework)
  newer releases: unknown (timed out) -- carry on
Merge rule
  owner-approves
Rounds
  in flight: 018-framework-3-0-0 -- brief on origin/brain/018-framework-3-0-0 (+1 more)
This machine
  on builder/018-framework-3-0-0; no uncommitted changes
  not on GitHub yet: builder/018-framework-3-0-0 (7 commit(s))
  safe to leave this machine: NO -- push or deal with the items above first
Checks
  all project checks pass
Command form on this machine: python3 tools/fw.py <command>
```
The "newer releases: unknown (timed out)" line is this machine's outbound
network reaching GitHub timing out during this check, not a project defect
— `fw.py` itself says "carry on". "not on GitHub yet" is expected before
`fw.py report --push` runs (next step).

### 9. Possible later rounds (product clean-up noticed, not done)

- `HomeScreen.qml`'s stale "planned" statuses and `hero.svg`'s dashed
  semantic-model box, the letterboxed social preview, and splitting
  `docs/ROADMAP.md` M6 into local builds and CI — all already recorded as
  parked work in `docs/state.md` before and after this round; not touched,
  out of scope (product code).
- A non-required macOS/Windows CI matrix over `data/`/`policy/` tests —
  recommended in brief 011, still not decided; any change to required
  checks is the owner's decision per `AGENTS.md`.
- `docs/adr/0010-...md` has a bracketed C++ snippet that a naive Markdown
  link-scanner misreads as a broken link (see §6); worth a small doc fix
  (e.g. wrapping the snippet in backticks) in a later docs round, but is
  product documentation and out of this round's scope.

## Not verified

- **C++ modules were not built or tested.** Explicitly out of scope per the
  brief: "C++ modules are out of scope and need not be built." Confirmed
  via the empty `git diff --stat origin/master -- client data policy ui
  integration gframe ocgcore .github` above, not by attempting a build.
- **CI at the reported commit.** This branch has not been pushed yet (this
  report is written and committed before the `--push` step); no CI run
  exists for `f8018921` to check. `AGENTS.md`'s required-checks list and
  `tests/test_ci_required_checks.py` (part of the green local suite above)
  are unchanged by this round's diff, so no reason to expect a new failure
  there, but that is an inference, not an observation of a real CI run.
- **Whether a newer framework release than 3.0.0 exists.** `fw.py status`
  could not reach the framework's releases endpoint from this machine
  (network timeout) — reported as "unknown (timed out)" by the tool
  itself, not silently assumed current.
- **Windows and macOS build/CTest behaviour post-adoption.** This round
  touched no C++/CMake/build files, so nothing suggests these would change,
  but they were not re-run this round (the machine used is macOS/Apple
  Silicon with no Qt installed, matching the project's own recorded macOS
  toolchain limits in `docs/state-history.md`).
- **The `.claude/` seat files' actual behaviour inside a live Claude Code
  session** (e.g. that `/status` really runs `fw.py status` as described) —
  read and reasoned about, not executed inside an actual Claude Code
  session, since this Builder round is itself running inside one but was
  not asked to test the seat-launch UX end to end.

## Changed

- `AGENTS.md`, `CLAUDE.md` — rewritten: `AGENTS.md` is now the entry point
  (`Merge rule: owner-approves`, points at `docs/agents/FRAMEWORK.md`,
  carries every rule that used to live only in `CLAUDE.md`); `CLAUDE.md` is
  now a two-line pointer. Commit `67825762`.
- `docs/state.md`, `docs/state-history.md` (new) — state trimmed to 629
  words (decisions/parked/pointers/historical anchors); full pre-3.0.0
  history moved to the new archive document. Commit `fec8efa5`.
- `docs/agents/FRAMEWORK.md`, `docs/agents/roles/{brain,worker,verifier}.md`,
  `tools/fw.py`, `tests/test_framework.py`, `docs/rounds/README.md`,
  `docs/agents/framework.json`, `.claude/agents/worker.md` (new) — installed
  by `adopt.py --update`. Commit `ca88f7f1`.
- `.claude/agents/brain.md`, `.claude/agents/verifier.md`,
  `.claude/commands/status.md` — replaced with their 3.0.0 `.framework`
  copies after confirming no project-specific content was lost (see §3).
  Commit `51985c94`.
- `.claude/README.md`, `.claude/agents/builder.md`, `.claude/hooks/`,
  `.claude/settings.json`, `tests/test_save_agent_reply.py` — deleted: the
  Stop-hook/inbox mechanism they implemented is retired in 3.0.0.
  `.gitignore`'s per-checkout-settings comment reworded accordingly.
  Commit `7531f0bb`.
- `docs/agents/{CONSTITUTION,adapters,evidence,git-and-isolation,launching,
  model-notes,reports,topologies,worktree-mechanism}.md`,
  `docs/agents/roles/README.md`, `docs/roles/` (whole directory),
  `tests/test_docs_consistency.py`, `tools/checkout.py`, `tools/report.py`
  — deleted as 2.x-only framework leftovers (see the rule-accounting
  table for where each one's content, if any, now lives).
  `docs/agents/local/windows-notes.md` (new) carries forward the
  Windows/MSVC build detail. `.githooks/pre-push`'s comment updated to
  drop its reference to the deleted `launching.md`. Commit `c0a818de`.
- `docs/briefs/active.md` → `docs/briefs/archive/016-2026-09-22-
  records-and-test-gaps.md` (git-detected rename; content taken verbatim
  from `origin/meta/presentation-tidy` at `ab1b83fe`, byte-identical —
  confirmed with `diff`, no output, before staging). `docs/briefs/README.md`,
  `archive/README.md`, `delivered/README.md`, `docs/contributing.md`
  updated to describe the directory as closed history and point at
  `docs/rounds/`. Commit `f8018921`.

## Open questions

- **`tests/test_docs_consistency.py`'s coordination-doc link-resolution
  check has no direct 3.0.0 replacement.** It caught dead intra-repo links
  across `AGENTS.md`, `CLAUDE.md`, `README.md`, `docs/state.md` and the
  `docs/agents/`/`.claude/` tree. `tests/test_framework.py` (the 3.0.0
  hygiene test) checks word budgets, stored commit ids, personal paths and
  the merge-rule line, but not link resolution. This round's own link
  sweep (§ "4. Rule accounting" intro and the required `git grep`) was done
  by hand with a one-off script, not a committed test — a future edit could
  silently reintroduce a dead link with nothing to catch it. Whether to
  write a project-owned link-checker test (in the spirit of the deleted
  one, but scoped to the new document set) is a decision for Brain, not
  something this brief asked for.
- **Whether `docs/agents/local/` should hold more than the one
  Windows-notes file**, or whether a single file is enough for this
  project's foreseeable local-guidance needs. Left as a single file since
  that is all this round needed to move there.
- **The pre-existing `test_readme_status.py` failure under Python 3.9.6**
  (see "Verified" §7) is outside this round's scope to fix (it is not a
  framework-adoption file), but it means this project's test suite is not
  actually green on the interpreter its own `.gitattributes`/environment
  defaults to on this machine. Worth a brief of its own; not raised as a
  blocker here since it predates this round and this round's own required
  evidence (golden reproduction, full suite) was still establishable via
  Python 3.13.

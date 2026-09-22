Brief-ID: 014-2026-09-21-readme-standard

Status: accepted

## MODE: IMPLEMENTATION

## Goal

Make `README.md` a landing page that meets the owner's house standard, and
make its "What works" status impossible to leave stale by hand. Everything
cut from the landing page moves into `docs/`, or into collapsible blocks.
Nothing is deleted.

The standard is `standards/readme.md` in `cntrl-alt-lenny/agentic-framework`
at `fed26f360294baddedc74eeaabbaf2e716572260`. Read it there.

## Why this is next

The owner asked for it. It is also an honesty fix. The current README, about
1,940 words against the standard's 250–500 visible, states things that are no
longer true. Among them, found by Brain on 2026-09-21:

- It says Windows and macOS builds were "not attempted, Linux only". All four
  C++ modules have since been built and tested under MSVC (Brief 005), and
  `client/`, `data/` and `policy/` under Apple clang (Brief 007; `ui/`
  could not be configured on the Mac, which has no Qt).
- Its architecture list omits `data/` and `policy/`.
- The screenshot caption says four of five subsystems are "planned".

`CLAUDE.md`'s honesty rules make a false README a defect in its own right.

## Owner decisions (2026-09-21) and rulings to apply

The framework's Brain accepted these rulings in its Dev Hub reply of
2026-09-21; the standard's own text will be corrected later. Where they
differ from the standard, they win.

- **"What works" is generated.** It comes from a single source of truth for
  project status, with a check that fails when the README and that source
  disagree. The check must fail at this branch's base. You decide what the
  source of truth is. It must be real project data, and it must not become a
  second copy of the roadmap that can drift from `docs/ROADMAP.md`. It must
  keep what exists, what is in progress and what is planned separate, as
  `CLAUDE.md` requires. Run the check in the existing Python test suite,
  following the pattern of `tools/generate_messages.py --check`. Make no
  workflow change.
- **No development-stage badge**, and no milestone badge: both are
  changeable facts. Nothing on the landing page may state a changeable fact
  by hand, unless a check covers it.
- **The license badge is static:** `AGPL-3.0-or-later`, linking to
  `LICENSE`. `LICENSE` must stay upstream's verbatim (`CLAUDE.md`), so
  GitHub cannot recognise it and a live license badge would say "not
  specified".
- **Badges** come from shields.io in `flat` style, in the standard's order,
  four or five at most, each linking to its evidence. The existing live CI
  badge stays.
- **The hero banner stays.** Also produce a raster 1280×640 export for
  GitHub's social preview, committed under `docs/assets/`. The owner uploads
  it as a repository setting; do not change any setting yourself. Committing
  a rasterising dependency is out of scope. Say how you produced the image.

## Scope

- `README.md`.
- New or extended pages under `docs/` for the material that moves out.
- `docs/assets/`.
- The generator and its check, under `tools/` and `tests/`.
- Any link elsewhere in the repository that the move would break.

## Non-scope

- `LICENSE`, `COPYING` and `notices/`: untouched.
- `.github/workflows/`, repository settings (including the social preview),
  branch protection.
- Production code.
- Framework-owned files under `docs/agents/` and the installed framework
  tools. The neutrality guard must still pass.
- No card artwork, card databases or scripts in any image or page.
- No news-style activity panels (the owner's decision in the standard).

## Protected invariants

- **Nothing is deleted.** Every paragraph, table, diagram and code block
  removed from `README.md` appears elsewhere in the repository: moved,
  possibly re-headed, with facts corrected where they were false. Deliver a
  mapping from each removed README section to where it now lives.
- **Honesty.** Planned is never described as shipped. Correct every false
  statement you move, and list each correction with its evidence.
- **The credits and trademark notice** (Project Ignis, edo9300, Konami,
  Shueisha, no affiliation or endorsement), and the statement that no card
  scripts, databases or artwork are included, stay on the landing page.

## Acceptance criteria

- **Word count.** Visible landing-page words, outside `<details>`, are within
  250–500. State the counting method and the count, and re-run it at your
  final head.
- **Badges.** Every badge image URL returns HTTP 200, and every badge link
  resolves. Show the output of the command you used.
- **Generated status.** The "What works" check fails at base
  `d035c66d05ec06419a41415162c6dca5f186e3d0` and passes at your head. Show
  it failing when a status fact changes in the source of truth but not in
  the README.
- **Social preview.** The PNG exists and is exactly 1280×640.
- **Mapping.** The section mapping above is complete.
- **Suite.** `python -m unittest discover -s tests -v` and both generator
  `--check`s are green from Git Bash, including the neutrality test.
- **Links.** Every relative link in `README.md` and in the pages you touched
  resolves.
- **The PR body.** It begins `DO NOT MERGE — under review` and passes
  `python tools/check_pr_evidence.py`.

## Required evidence

- Checkout check, and the tool, model and OS (the OS from `cmd /c ver`).
- The word count, badge probe output, generator fail/pass demonstration,
  PNG dimensions, section mapping, list of corrected facts, and link check.
- The suite and generator output.
- Check-run conclusions at the final head.
- **Presentation evidence** (`AGENTS.md`): say what you verified visually and
  what you did not. For example, render `README.md` with
  `gh api /markdown` and inspect the result, and say whether you inspected it
  as an image or only as HTML.
- What you did not run.
- Any sentence you removed from `docs/state.md` or another durable document,
  listed explicitly.

## Completion-report schema

The standard report in
[`docs/agents/roles/worker.md`](../agents/roles/worker.md), plus: the source
of truth chosen for "What works" and why, the section mapping, the corrected
facts, and the visual-verification statement.

## Reopened corrections R1-R3

R1 — README.md's "What is this?" opens "edopro-next is a modern Qt 6 / QML client for EDOPro." That describes as existing a client that is not yet usable: the duel field is not started, as the generated status says. CLAUDE.md's honesty rules forbid describing planned functionality as shipped. Required outcome: the landing page describes what the project is today, without implying a usable client exists, and stays consistent with the hero banner ("A modern client architecture for EDOPro") and the generated status.

R2 — the generated status block opens with "**No.** The duel field (M5) is not started." under the heading "What works". The "No." answers a question the page never asks, and read on its own it suggests nothing works. Required outcome: the generated first sentence reads correctly on its own under that heading. It must still be computed from the Duel field milestone's status in docs/ROADMAP.md, never typed by hand, and the tests must cover the new wording.

R3 — "Quick start" lists "a C++20 compiler, CMake and Ninja" as what is needed, but its last step runs Python. Required outcome: the section states everything a reader needs to complete every step it lists. That includes Python's minimum version, and a pointer for Windows readers, since cmake -G Ninja needs the MSVC environment there; linking docs/building.md is enough.

---

## Outcome — accepted and merged 2026-09-22

Accepted by Brain and merged as PR #30 (merge `113e2c7f`) at head
`065f4eb01226cd4541254d1f7d660eebe7b15731`, base
`d035c66d05ec06419a41415162c6dca5f186e3d0`, after two passes. The first pass
(head `83a269fd`) ran in Claude Code (Sonnet 5) for both seats. The corrective
pass ran in Antigravity (Gemini 3.8 Flash, High) for both.

**First pass, `83a269fd`.** The Verifier found no BLOCKER, and its
section-by-section walk of the old README found nothing deleted. Brain
reopened the round on three landing-page sentences:

- **R1.** "edopro-next is a modern Qt 6 / QML client" described a usable
  client that does not exist.
- **R2.** The generated status opened with an unprompted "**No.**".
- **R3.** Quick start omitted Python.

Brain overruled the Verifier's SHOULD FIX asking for checks on the C++20 and
Qt 6 badges. The standard allows static language and platform badges, and
both facts change only by a recorded architecture decision.

**Corrective pass, `065f4eb0`.** All three corrections were made as
specified; the Verifier found nothing further. Brain read the diff and the
new landing page, and confirmed the generated line is still computed from
`docs/ROADMAP.md`.

**Acceptance conditions, confirmed at the literal head.** The Verifier
reviewed `065f4eb0` (task `014-verify-readme-standard-2`). Brain checked the
merged content itself. All required checks were green (12 success, 1
conditional matrix entry skipped; merge state CLEAN). The change is inside
routine scope.

**Owner actions.** The owner uploaded `docs/assets/social-preview.png` as
the repository's social preview on 2026-09-22.

**Left open, recorded.**

- The social preview draws the 1200×320 banner letterboxed on a 1280×640
  canvas, so its text is small when a link is shared. Cosmetic.
- `ui/qml/screens/HomeScreen.qml` still hard-codes stale "planned" statuses,
  and `hero.svg` draws the semantic-model box dashed.
- `docs/capabilities.md` is a hand-written snapshot. It says so, and defers
  to the roadmap.
- The roadmap's M6 joins local Windows/macOS builds (evidenced) with CI
  (not).

# 019-presentation-tidy: make what the project shows about itself true

Tier: 2
Mode: implementation
Supersedes: 017-2026-09-22-presentation-tidy, a 2.x brief that was queued on
branch `meta/presentation-tidy` and never started. This round reissues it
under 3.0.0 with its facts re-checked on today's `master`.

## Goal

Everything the project shows about itself says what is true today:

- the app's home screen;
- the shell screenshot;
- the hero banner;
- the link-preview image;
- the roadmap.

The home screen's status can no longer go stale without a check failing.

## Context

Why this round is next: rounds 014 and 015 recorded these items and deferred
them. Each item is small, but each one misstates the project to a reader, and
`AGENTS.md`'s honesty invariant applies to presentation as much as to
documents.

It is Tier 2 because whether an image or a screen says something true is
exactly what the test suite cannot catch.

Worth reading:

- `AGENTS.md`: its invariants and its evidence table (the `ui/` and
  "Presentation only" rows).
- `docs/capabilities.md` §"The shell". It already records the stale home
  screen.
- `tools/generate_readme_status.py`: its docstring gives the roadmap
  conventions it validates.
- `ui/src/main.cpp`: the `--capture`, width and height options.
- `ui/qml/Theme.qml`: the palette.
- The archived rounds `docs/briefs/archive/014-…` and `015-…`, section
  "Left open", for where these items came from.

Not worth reading: anything under `client/`, `data/`, `policy/`, `gframe/`
or `ocgcore/`.

## The items (each is a problem, with its source)

1. **The home screen states status by hand, and it is stale.**
   `ui/qml/screens/HomeScreen.qml` hard-codes five status rows:

   | Row | Shows |
   |---|---|
   | Upstream baseline builds | working |
   | Qt 6 / QML shell | working |
   | Semantic client model | planned |
   | Deck builder | planned |
   | Duel field | planned |

   The file has had one commit, the shell's first. The roadmap says M2, the
   semantic client model, is done, and that a deck-builder core with advisory
   legality exists.

   Required outcome: what the home screen shows as status is true, and cannot
   drift from the project's real status without a check failing. You choose
   the mechanism and argue for it. Two known options:

   - derive the status from `docs/ROADMAP.md`, the source the README's status
     block already comes from;
   - stop stating status on the home screen at all.
2. **The shell screenshot records the stale screen.**
   `docs/assets/shell.png` is the image in `docs/capabilities.md`. Its alt
   text says "honest project status", but it shows the rows above. The
   caption and the paragraph under it explain that it is out of date.

   Required outcome: the image, its alt text and the surrounding text agree
   with each other and with what the shell shows now. The image is a real
   `--capture` of the built shell; say which build.
3. **The hero banner implies the semantic model is unbuilt.**
   `docs/assets/hero.svg` draws its "semantic model" box with a dashed
   outline and dimmed text, which reads as "not built", but M2 is done.

   Required outcome: the banner implies nothing false about what exists. Keep
   its design and its palette (`ui/qml/Theme.qml`).
4. **The link-preview image is hard to read when shared.**
   `docs/assets/social-preview.png` is exactly 1280×640. Its source,
   `docs/assets/social-preview.svg`, centres the 1200×320 banner on that
   canvas, so the text is small in a link preview.

   Required outcome:
   - a 1280×640 image that reads well at the sizes GitHub and chat apps
     display it;
   - made from the corrected banner, with its SVG source kept in step;
   - say how the PNG was produced;
   - commit no rasterising dependency.

   The owner uploads the image to GitHub; change no repository setting.
5. **The roadmap's M6 joins two different facts.** The first M6 item in
   `docs/ROADMAP.md` reads "Windows and macOS builds and CI".

   - Local builds are evidenced in the archived rounds 005 and 007 and in
     `docs/capabilities.md`: Windows/MSVC for all four modules, macOS/Apple
     clang for `client/`, `data/` and `policy/`.
   - CI on those platforms is not.

   Required outcome: the roadmap states each fact truthfully, for exactly
   the platforms and modules the evidence covers. Any change in milestone
   status must follow the conventions `tools/generate_readme_status.py`
   validates. Regenerate the README block. If this moves M6's status on the
   README landing page, say so plainly; the owner will see it there.

## Scope and non-goals

In scope:

- `ui/qml/`, and `ui/src/` or `ui/CMakeLists.txt` only if item 1's
  mechanism needs them;
- `ui/tests/`, and new tests for item 1;
- `docs/assets/`, `docs/ROADMAP.md` and the regenerated README status block;
- `docs/capabilities.md` and `docs/architecture/deck-builder-ui.md`, where
  they describe the home screen, the screenshot or the banner.

Out of scope:

- any deck-builder or legality behaviour;
- `client/`, `data/`, `policy/`, `integration/`, `gframe/`, `ocgcore/`;
- `.github/workflows/` and repository settings;
- framework files (`docs/agents/`, `tools/fw.py`, `tests/test_framework.py`);
- M3's remaining deck-builder work, which is a later round.

## Invariants

- The UI implements no game rule, and nothing in `client/`, `data/` or
  `policy/` gains a Qt dependency (`AGENTS.md`, invariants).
- Never describe planned functionality as shipped (`AGENTS.md`, honesty).
  Every status you show is backed by the roadmap or by evidence you cite.
- The existing `ui/` tests survive. Name any changed assertion with its
  before and after.
- `python3 tools/generate_readme_status.py --check` passes. The README
  block is regenerated from the roadmap, never edited by hand.
- No card artwork, and nothing derived from it, in any image (`AGENTS.md`,
  licensing).
- No new dependency without an ADR (`AGENTS.md`). A tool you use once to
  rasterise the PNG is not committed and is not a dependency; name it in the
  report.
- Nothing personal in any tracked document or image metadata: no home-folder
  paths, no email addresses.

## Acceptance criteria

1. Item 1: a check fails when the home screen's status and its source
   disagree, and runs in CI. Alternatively, the screen no longer states
   status at all, and the report argues why that is the better choice.
2. Items 2 to 5 each meet the required outcome above.
3. `ui/` configures and builds with `-DEDOPRO_NEXT_UI_TESTS=ON` against Qt 6,
   and `ctest` passes. The offscreen clean-QML-load check from
   `.github/workflows/edopro-next.yml` is clean. CI's "Qt 6 shell (Linux)"
   job, which pins the Qt version, is green at the final commit.
4. The Python suite, the three generator `--check` commands and
   `python3 tools/fw.py check` are green.
5. Every image the round produces or changes has been looked at by you. The
   report says what you checked visually and what you did not.

## Required evidence

1. Seat start:
   - the real output of `python3 tools/fw.py start --role builder --round
     019-presentation-tidy`;
   - the output of `git submodule update --init`, run right after it;
   - the output of `git submodule status`;
   - the operating system and Qt version, each from a command, not from
     memory.
2. The `ui/` cycle from `AGENTS.md`'s evidence table, with real output and
   exit status: configure, build, `ctest`, and the offscreen QML load. Name
   the platform and the Qt version. The Qt version may differ from CI's
   pinned one, so say so if it does.
3. **Item 1:** the literal change you made to break the agreement, and the
   literal failing output of the check against it. If you chose to stop
   showing status, argue for it instead.
4. The `--capture` command line you used for the screenshot, and the pixel
   size of every PNG produced, from a command.
5. `python3 -m unittest discover -s tests -v` (totals, and each skip named),
   `python3 tools/generate_messages.py --check`,
   `python3 tools/generate_protocol_constants.py --check`,
   `python3 tools/generate_readme_status.py --check` and
   `python3 tools/fw.py check`.
6. Every sentence removed from `docs/ROADMAP.md`, `docs/capabilities.md` or
   `docs/architecture/deck-builder-ui.md`, listed explicitly, with what
   replaced it.
7. CI check conclusions at your final commit.
8. What you did not run or did not look at.

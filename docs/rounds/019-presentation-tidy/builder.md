<!-- fw-report
round: 019-presentation-tidy
role: builder
branch: builder/019-presentation-tidy
head: e05761f8e16fea2a84f5588c5cceca87809db9db
os: macOS 27.0
python: 3.13.15
written: 2026-09-23T13:58:13Z
-->
# Builder report: 019-presentation-tidy

Branch `builder/019-presentation-tidy`. Work commits, oldest first:
`86bc16d9` (ROADMAP + README block), `6e74e5fc` (home screen, checker, its
test), `51b6b85a` (banner, social preview, asset test), `9823ace0`
(screenshot, capabilities.md). This report is committed on top of them.

## Verified

Seat start (real output, run in a fresh clone under the session scratchpad,
not the owner's checkout):

- `python3 tools/fw.py start --role builder --round 019-presentation-tidy` → exit 0
  ```
  seat ok: builder, round 019-presentation-tidy, branch builder/019-presentation-tidy at 91e01312753d
    brief: docs/rounds/019-presentation-tidy/brief.md
    finish with: write docs/rounds/019-presentation-tidy/builder.md, then python3 tools/fw.py report --role builder --round 019-presentation-tidy --push
  ```
- `git submodule update --init` (run right after) →
  ```
  Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
  Cloning into '<clone>/ocgcore'...
  Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
  ```
- `git submodule status` →
  ```
   46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
  ```
- Platform, each from a command: `sw_vers -productVersion` → `27.0` (macOS,
  `ProductName: macOS`, build 26A428), `uname -m` → `arm64`,
  `qmake6 -query QT_VERSION` → `6.11.1`, `cmake --version` → 4.4.3,
  `ninja --version` → 1.13.2. **Qt 6.11.1 differs from CI's pinned 6.8.3.**
  Python: the system `python3` is 3.9.6, below CI's 3.10 floor; every Python
  command below was run with Homebrew `python3.13` (see Open questions).

`ui/` cycle (AGENTS.md evidence row), macOS 27.0 arm64 / Apple clang / Qt
6.11.1, on a clean `ui/build`, at the commit whose tree the code and tests are
(`9823ace0`; only docs/tests/assets were added after the last `ui/` change,
`6e74e5fc`). CI's job configures Release, so this did too:

- `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DEDOPRO_NEXT_UI_TESTS=ON -DCMAKE_PREFIX_PATH=<Qt prefix>` → exit 0
  (`-- Build files have been written to: .../ui/build`; only CMake
  author-policy warnings from Qt's macros and `data/CMakeLists.txt:40`, none
  from this round's files)
- `cmake --build ui/build --parallel` → exit 0
  (`[98/98] Linking CXX executable tests/test_deckbuilder_screen`; the only
  compiler-side warning is `ld: warning: ignoring duplicate libraries:
  'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'`, pre-existing)
- `ctest --test-dir ui/build --output-on-failure` → exit 0
  ```
  1/2 Test #1: deckbuilder ......................   Passed    0.31 sec
  2/2 Test #2: deckbuilder_screen ...............   Passed    1.06 sec
  100% tests passed out of 2
  ```
- Offscreen load. macOS has no `timeout(1)`, so the CI step's two assertions
  were reproduced by hand: `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`
  was still alive after 20 s (`kill -0` succeeded), then killed. **stderr was
  not empty, so CI's second assertion (empty stderr) would not pass verbatim
  here**: it held exactly one line, a Qt platform notice, not a QML diagnostic:
  `qt.qpa.fonts: Populating font family aliases took 55 ms. Replace uses of
  missing font family "Sans Serif" with one that exists to avoid this cost.`
  It comes from `Theme.fontFamily` naming fonts (Inter, Segoe UI, ...) that a
  Mac lacks, is emitted by `--capture` too, and I did not touch `Theme.qml`.
  No line contained a QML, TypeError or ReferenceError diagnostic. Whether the
  CI Linux job is clean is decided by CI, not by this run.
- `ui/`'s existing tests survive unchanged: no assertion in `ui/tests/` was
  edited (`git diff origin/master --stat` lists no file under `ui/tests/`).

Item 1 (home screen status). Mechanism chosen: the home screen keeps
showing status, one row per roadmap milestone (M0 to M6), and
`tools/check_home_status.py` fails when it disagrees with `docs/ROADMAP.md`.
It runs in CI through `tests/test_home_status.py`, which the "Regression
harness" job's `python -m unittest discover -s tests -v` picks up, so no
workflow was edited. It uses `tools/generate_readme_status.py`'s own parser,
so the home screen and the README block read the roadmap identically.

The literal change made to break the agreement (in the real
`ui/qml/screens/HomeScreen.qml`, then reverted): the M2 row's
`status: "done"` became `status: "not started"`:

```
                milestone: "M2"
                title: "Semantic client model"
                detail: "Presentation-free duel state decoded from the message stream, so it can be reasoned about without a renderer."
                status: "not started"
```

and the literal output of `python3.13 tools/check_home_status.py` against it:

```
home screen status does not match docs/ROADMAP.md:
  - M2 (Semantic client model): home screen says 'not started', docs/ROADMAP.md says 'done'
exit=1
```

After reverting, the same command → `home screen status matches
docs/ROADMAP.md`, exit 0. The unit test the CI job runs fails on the same
mutation too (`test_home_screen_matches_roadmap`: `First list contains 1
additional elements ... "M2 (Semantic client model): home screen says 'not
started', docs/ROADMAP.md says 'done'"`). `tests/test_home_status.py` also
mutates, in memory, and requires failure for: a stale status; a roadmap
change the screen does not follow (M4 marked in progress); the duel field
marked done while the screen says not started and "not a playable client";
a roadmap milestone with no row (M7); a row for a nonexistent milestone; a
renamed title; a status word in a row's free text; a screen with no rows (so
the check cannot pass vacuously); a row with no `milestone`; and the CLI
exiting 1.

What that mechanism does not catch: it checks status, milestone and title
strings, not the prose of the intro sentence beyond the one "not a playable
client" claim, and it cannot check the committed screenshot (the PNG).

Items 2 to 5, what was checked:

- Item 2: `docs/assets/shell.png` is a real capture, command line (from the
  clone root, binary built from commit `6e74e5fc`, whose `Shell version` row
  shows `6e74e5fc`): `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell
  --capture docs/assets/shell.png --capture-width 1280 --capture-height 960`
  → exit 0. Build: macOS 27.0 arm64, Qt 6.11.1, Release, Apple clang. The
  window is 960 tall rather than the 800 default because at 800 the Build
  rows and licence notice fell below the fold. Alt text, caption and
  paragraph in `docs/capabilities.md` agree with each other and the image.
- Item 3: `docs/assets/hero.svg`'s semantic-model chip now uses the same
  accented fill/stroke/text as the Qt 6 / QML chip; the palette is
  `ui/qml/Theme.qml`'s (`accentSubtle #3A3218`, `accent #C9A227`), and no
  other element changed.
- Item 4: `docs/assets/social-preview.svg` is now a standalone 1280x640
  re-layout of the banner's own elements (field geometry at 1.5x, wordmark at
  96 px versus the banner's 42 px, stack chips at 1.8x, tagline 30 px, closing line 26 px),
  with no `<image>` reference. `docs/assets/social-preview.png` was rendered
  from it with **headless Google Chrome** (`--headless=new --disable-gpu
  --hide-scrollbars --force-device-scale-factor=1 --window-size=1280,640
  --screenshot=... file://<path>/social-preview.svg`), a tool already
  installed on this machine, used once, not committed, not a dependency.
  Downscaled to 400x200 with `sips` for a legibility check (not committed).
  Font: Chrome resolved the SVG's font stack to a system sans-serif (Inter is
  not installed here), so glyph shapes differ from an Inter rendering.
- Item 5: `docs/ROADMAP.md` M6 split into a done local-builds item and a
  not-started CI item; the heading gained the `in progress` marker the
  generator's conventions require. **This moves M6 on the README landing
  page from "Planned, not started" to "In progress"** (README block
  regenerated by `python3.13 tools/generate_readme_status.py`, never edited
  by hand); the owner will see it there.

Pixel sizes of every PNG produced, from `file`:

```
docs/assets/shell.png:          PNG image data, 1280 x 960, 8-bit/color RGB, non-interlaced
docs/assets/social-preview.png: PNG image data, 1280 x 640, 8-bit/color RGB, non-interlaced
```

PNG chunk walk of both (and of a scratch render of the banner): `IHDR`, `IDAT`,
`IEND`, plus `pHYs` on shell.png; no `tEXt`/`iTXt`/`eXIf` chunk, so no path
or address is embedded. The previous shell.png was 1280 x 800; the change to
960 tall is deliberate and is stated in the caption.

Python and generators (`python3.13`, exit codes from `$?`):

- `python3.13 -m unittest discover -s tests -v` → exit 0: `Ran 133 tests in
  1.998s`, `OK (skipped=11)`. Skips, each named: 10 in
  `test_semantic_trace` skip with "no semantic-trace binary is present and
  newer than every client source file; configure and build client/ ..."
  (`TestSemanticGoldens` x3: `test_fixtures_exist`,
  `test_rendering_is_deterministic`, `test_traces_match_golden`;
  `TestSemanticQuality` x7: `test_committed_fixtures_are_semantically_complete`,
  `test_coverage_accounts_for_every_packet`,
  `test_model_invariants_hold_at_the_end`, `test_no_environmental_leakage`,
  `test_no_packet_is_malformed_or_unknown`,
  `test_query_stream_coverage_is_real_and_clean`,
  `test_something_is_actually_decoded`), because `client/` was not built and
  this round did not touch `client/`; and 1,
  `TestBinaryFreshness.test_unreadable_source_tree_fails_closed`, "Windows ACL
  denial is required for this enumeration test". 133 is 115 from `master` plus
  this round's 18 (12 + 6).
- `python3.13 tools/generate_messages.py --check` → exit 0, `message table up to date (96 ids)`
- `python3.13 tools/generate_protocol_constants.py --check` → exit 0, `protocol constants up to date (187 values)`
- `python3.13 tools/generate_readme_status.py --check` → exit 0, `README status block is up to date`
- `python3.13 tools/fw.py check` → exit 0, `0 error(s), 0 warning(s)`
- Golden reproduction: `python3.13 tests/test_replay_trace.py --update` wrote
  the three traces, then `git diff --exit-code -- tests/golden` → exit 0.

Personal data: `git ls-files | xargs grep -l` for a home-folder path or an
email pattern lists files none of which this round touched (existing archive
and fixture files and upstream `gframe/` sources); none of this round's files
or images contain one. I did not open those to judge whether they are real.

Sentences removed, and what replaced them:

`docs/ROADMAP.md`:
- Heading `## M6 — Platform and input` → `## M6 — Platform and input  🔶 in progress`.
- Item `- [ ] Windows and macOS builds and CI` → two items: a checked `**Windows and macOS local builds**`
  (Windows 11 / MSVC all four modules, brief 005, noting the recorded
  no-re-run after brief 007; macOS / Apple clang `client/`, `data/`, `policy/`
  only, brief 007 and `state-history.md`; `ui/` on macOS has no recorded
  evidence) and an unchecked `**Windows and macOS CI.**` ("Nothing runs on
  either platform in the active CI, which is Linux-only.").

`docs/capabilities.md`:
- Heading "The shell, as first captured" → "The shell".
- Alt text "The edopro-next Qt/QML shell: navigation rail, home screen with
  honest project status, and live build metadata" → "The edopro-next Qt/QML
  shell's home screen as captured at the build named below: navigation rail,
  one status row per roadmap milestone, and live build metadata".
- Caption "Captured from the running binary via --capture (build 94b15108,
  2026-08-24). Two of the five status rows say working and three say
  planned." → the new caption naming build `6e74e5fc`, the platform, Qt
  version, the exact `--capture` command line, and why the window is 960 tall.
- Whole paragraph "Two caveats. The screenshot's caption on the old landing
  page said "four of five subsystems say planned"; the image itself shows
  three. And its status rows are hard-coded in HomeScreen.qml, which has not
  changed since the shell was first committed (git log ... lists one commit).
  Its "Semantic client model: planned" and "Deck builder: planned" rows
  therefore no longer match the roadmap, where the semantic client model (M2)
  is done and a deck-builder core exists. Treat the image as a record of the
  first shell, not of current status." → two paragraphs: what the home screen
  shows and which check covers it (`tools/check_home_status.py`), and that
  the check covers the QML and not the image, so the roadmap wins if the image
  is stale. The removed caveats are resolved, not lost: the stale screen they
  describe no longer exists.
- Table row "Windows / macOS builds", sentences "macOS / Apple clang:
  `client/`, `data/` and `policy/` build and pass; `ui/` could not be
  configured there because the machine has no Qt ... [`state.md`] ("Local
  toolchain"). Neither is in the active CI (Linux-only), and the roadmap
  still lists platform builds and CI under M6 as not started" → "... build and
  their CTest suites pass; `ui/` on macOS has no recorded evidence, because
  the machine had no Qt ... [`state-history.md`] ("Local toolchain, as last
  exercised"). Neither platform is in the active CI, which is Linux-only; the
  roadmap records the local builds as done and Windows/macOS CI as not
  started, which makes M6 in progress". The old `state.md` "Local toolchain"
  pointer was dead (that section now lives in `state-history.md`).

`docs/architecture/deck-builder-ui.md`: unchanged. Its only Home mention (the
`--start-screen` paragraph) is still true.

`docs/state.md`: unchanged (not in scope; see Open questions).

`ui/qml/screens/HomeScreen.qml` (not a document, listed for completeness):
the five hard-coded rows (Upstream baseline builds working; Qt 6 / QML shell
working; Semantic client model planned "... Not started."; Deck builder
planned; Duel field planned) and the sentence "This shell is an architectural
proof, not a playable client. Nothing below is dressed up as finished." were
replaced by the seven roadmap-checked rows and "This shell is not a playable
client. To duel today, use upstream EDOPro. Each status below is the state of
the matching milestone in docs/ROADMAP.md, and tools/check_home_status.py fails
the test suite if they disagree." `StatusRow.qml` now takes `milestone` and
displays the roadmap vocabulary (`done`, `in progress`, `not started`)
instead of `working`/`progress`/`planned`; nothing else used `StatusRow`.

CI check conclusions at the final commit: filled in below after the push.

## Not verified

- **Visual verification, precisely.** Looked at (opened and inspected as
  images): the final `shell.png` (both the 1280x800 probe, which showed the
  clipped Build section and a misaligned milestone column, and the final
  1280x960 image after the alignment fix), the final `social-preview.png` at
  1280x640 and, downscaled, at 400x200, and a Chrome render of the corrected
  `hero.svg` at 1200x320. Not looked at: the QML at any window size other than
  1280x960 and 1280x800 (no 960x600 minimum-size check, no narrow-window
  wrap check of the seven rows or the long intro sentence), any other screen,
  light mode (there is none), and the images as GitHub or a chat app actually
  displays them. `hero.svg` and `social-preview.svg` were rendered in Chrome
  only; not opened in another SVG renderer. I never saw the hero on the
  README page itself.
- The PNG cannot be checked against its SVG by a test (no committed
  rasteriser, by design), so `social-preview.png` matching `social-preview.svg`
  rests on my having rendered it and looked at it. The SVG chips are
  test-pinned to the banner; the PNG is not.
- CI on Linux/Qt 6.8.3 (the "Qt 6 shell (Linux)" job): this machine cannot run
  it. See the CI section for what came back.
- Windows/MSVC and any Linux run of `ui/`: not run.
- Whether the Linux CI offscreen-load step is clean: not run on Linux; on
  macOS stderr held the one Qt font notice described above.
- `client/`, `data/`, `policy/`, the `gframe/` upstream baseline and the
  observer fixture-equivalence were not built or run: this round changed none
  of them, and the evidence table's rows for them do not apply. Nothing here
  says anything about duel behaviour, and no replay-harness result is offered
  as evidence of it.
- `python3 tools/check_pr_evidence.py`: the AGENTS.md step before opening a
  PR. It reads a PR body on stdin, and there is no PR body yet (Builder does
  not open the PR); run bare it exits 1 with "no rerunnable command found".
  Brain should run it on the PR body.
- Python 3.10 (CI floor) was not run; I used 3.13. Under the system 3.9.6 one
  existing test fails (see Open questions).

## Changed

- `docs/ROADMAP.md`, `README.md` (generated block only): item 5.
- `ui/qml/screens/HomeScreen.qml`, `ui/qml/components/StatusRow.qml`: item 1.
- `tools/check_home_status.py` (new), `tests/test_home_status.py` (new): item
  1's check and its mutation tests.
- `docs/assets/hero.svg`: item 3.
- `docs/assets/social-preview.svg`, `docs/assets/social-preview.png`,
  `tests/test_presentation_assets.py` (new): item 4 and item 3's guard.
- `docs/assets/shell.png`, `docs/capabilities.md`: item 2 and the M6 row.
- `docs/rounds/019-presentation-tidy/builder.md`: this report.
- Not changed: `docs/state.md`, `docs/architecture/deck-builder-ui.md`,
  `.github/workflows/`, `ui/CMakeLists.txt`, `ui/src/`, `ui/tests/`, any
  framework file, repository settings, `client/`, `data/`, `policy/`,
  `integration/`, `gframe/`, `ocgcore/`.

## Open questions

1. **`docs/state.md` is now stale on two items** (out of scope, so untouched):
   the "Parked" bullet listing `HomeScreen.qml`'s stale statuses, `hero.svg`'s
   dashed box, the letterboxed social preview and the M6 split, all four of
   which this round resolves, and the "Recommended next slice" bullet that
   names them. Brain owns `state.md`.
2. **The owner uploads the social preview.** `docs/assets/social-preview.png`
   is the file to upload in the repository settings; I changed no setting.
3. **`docs/capabilities.md` still has two rows the roadmap has moved past**,
   also out of this brief's scope and left alone: "Deck legality / LFList
   policy" says "nothing in `ui/` calls it, so the QML deck builder still
   shows no legality", and "Deck builder UI" says "no legality". The roadmap's
   M3 says legality is connected through ADR 0010. They deserve a separate
   fix.
4. **The macOS `ui/` evidence now exists but is not recorded in the roadmap.**
   This round configured, built and ran `ui/`'s two CTest suites and the
   offscreen shell on macOS 27.0 arm64 / Qt 6.11.1 (above). The roadmap's M6
   item says `ui/` on macOS has no recorded evidence, because that is what the
   archived rounds show; recording this round's run there needs the round
   accepted first. It is a candidate for a follow-up edit.
5. **The system Python 3.9.6 fails one existing test**
   (`tests.test_readme_status.CommandLineTest.test_check_fails_on_a_stale_copy_and_update_repairs_it`,
   also on unmodified `master`). Cause, reproduced:
   `tools/generate_readme_status.py` calls `Path.write_text(..., newline="\n")`,
   and `python3 -c "import pathlib; pathlib.Path('/tmp/x').write_text('a',newline='\n')"`
   raises `TypeError: write_text() got an unexpected keyword argument 'newline'`
   on 3.9. `FRAMEWORK.md` says Python 3.9 or newer; CI's floor is 3.10. Not
   fixed here, since it is neither in scope nor a framework file I may edit.
   The new tools and tests in this round pass under 3.9.6 and 3.13.
6. **The Linux CI offscreen-load check requires empty stderr.** On macOS the
   shell prints a Qt font-alias notice caused by `Theme.fontFamily`'s
   font list. Not a QML diagnostic and not touched here, but a macOS CI leg
   (M6's open item) would trip that check as written.
7. **The screenshot's caption names build `6e74e5fc`.** It is on this branch;
   it stays reachable only if the round is merged with its commits kept (the
   repository's merge commits do), not squashed. If Brain squashes, the SHA in
   the caption and the `Shell version` row in the image will name a commit that
   is not on `master`; the caption's platform and command line remain true.
8. **Design choice for Brain's judgement:** in the banner and the social
   preview I accented both the semantic model chip and the Qt 6 / QML chip
   ("layers this project builds"). The alternative, drawing the semantic model
   in the neutral "preserved" style, would misdescribe it as upstream's.

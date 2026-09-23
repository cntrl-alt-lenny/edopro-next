<!-- fw-report
round: 019-presentation-tidy
role: verifier
branch: verifier/019-presentation-tidy
head: e4c9d5a955e81dbc22f9ad08ca42a69111ff1934
os: macOS 27.0
python: 3.9.6
written: 2026-09-23T16:34:50Z
-->
# Verifier report: 019-presentation-tidy

Reviewed commit: e4c9d5a955e81dbc22f9ad08ca42a69111ff1934 (full id), branch
`origin/builder/019-presentation-tidy`. Platform: macOS 27.0 arm64, Apple clang, Qt 6.11.1
(`qmake6 -query QT_VERSION`), system Python 3.9.6. Fresh clone in the session scratchpad.

Seat start, `python3 tools/fw.py start --role verifier --round 019-presentation-tidy` (exit 0):

```
seat ok: verifier, round 019-presentation-tidy, branch verifier/019-presentation-tidy at e4c9d5a955e8
  reviewing exactly e4c9d5a955e81dbc22f9ad08ca42a69111ff1934 from origin/builder/019-presentation-tidy
```

`git submodule update --init` (first attempt, no retry needed):

```
Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
Cloning into '<clone>/ocgcore'...
Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
```

`git submodule status`:

```
 46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
```

Commands I ran myself (exit codes):
- `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_UI_TESTS=ON -DEDOPRO_NEXT_WERROR=ON -DCMAKE_PREFIX_PATH=<Qt prefix>` 0; `cmake --build ui/build --parallel` 0 (98/98); `ctest --test-dir ui/build --output-on-failure` 0, 2/2 passed (`deckbuilder`, `deckbuilder_screen`).
- Offscreen load (`QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`, 20 s alarm): still running at the alarm (SIGALRM, status 142), so no crash. stderr held one line: a `qt.qpa.fonts` "Sans Serif" alias notice, not a QML diagnostic (see NOTE 3).
- `--capture ../my-shell.png --capture-width 1280 --capture-height 960` on my own build: 1280x960; pixel difference from the committed `docs/assets/shell.png` is confined to one 63x11 px box, the "Shell version" build-id text, so the committed screenshot is a real capture of this shell.
- `generate_messages.py --check` 0; `generate_protocol_constants.py --check` 0; `generate_readme_status.py --check` 0; `tools/check_home_status.py` 0.
- `python3 -m unittest discover -s tests`: 133 tests, 1 failure, 11 skips (see NOTE 1; failure is pre-existing on Python 3.9).
- Mutation tests of `tools/check_home_status.py` (each reverted with `git checkout`, tree clean afterwards), see Item 1.
- `gh run list --branch builder/019-presentation-tidy`: run on e4c9d5a9 `success` (also f33e67e3 `success`). The delta between the two is the builder report only.

## Per-item outcome

1. Home screen status: MET. See mutation evidence below.
2. Shell screenshot: MET. The image (seen: 1280x960, dark shell, rail at left with Home/Decks/Duel/Replays/Settings, STATUS block with seven rows M0 to M6 in the roadmap's vocabulary, BUILD block reading Qt 6.11.1 and macOS 27.0 arm64, licence notice in frame) agrees with its alt text, caption and the paragraph in `docs/capabilities.md`. Build named (`6e74e5fc`) is the last commit that touched `ui/`; I reproduced the capture.
3. Hero banner: MET. The diff turns the semantic-model chip from dashed/dimmed into the accent fill and stroke used by the Qt 6 / QML chip, same palette as `Theme.qml` (`#3A3218`, `#C9A227`).
4. Social preview: MET. `social-preview.png` is 1280x640 RGB. I looked at it: the "edopro-next" wordmark is very large, the tagline and the four layer chips are large, semantic model and Qt 6 / QML are accented, no card artwork. The SVG source is standalone (no `<image>`), kept in step by `tests/test_presentation_assets.py`. PNG chunk walk of both PNGs: only IHDR, IDAT, IEND (and pHYs on shell.png); no text chunks, no personal data. No rasteriser was committed.
5. Roadmap M6: MET. Split into a checked "local builds" item (Windows/MSVC four modules; macOS/Apple clang three modules) and an unchecked "CI" item; heading gained the `in progress` marker. I checked the cited evidence: `state-history.md` records Windows 13/13 CTest, macOS `client/` 7/7, `data/` 3/3, `policy/` 2/2 (after brief 011), no Qt on macOS. `generate_readme_status.py --check` passes. The README block moved M6 from "Planned" to "In progress", which the builder says plainly.

## Item 1 mutation evidence

Each mutation made in my scratch clone, the check run, then reverted:
- M2 row `status: "done"` -> `"not started"` in `HomeScreen.qml`: exit 1, `M2 (Semantic client model): home screen says 'not started', docs/ROADMAP.md says 'done'`. `tests.test_home_status` also failed (7 failures) on that mutation.
- Roadmap M4 heading marked `in progress` without touching the QML: exit 1, `M4 ... home screen says 'not started', docs/ROADMAP.md says 'in progress'`.
- M5 row deleted: exit 1, `M5 (Duel field) is in docs/ROADMAP.md but has no row on the home screen`.
- M4 detail changed to include the word "done": exit 1, `detail states a status word ('done')`.
- M4 status changed to a QML expression (`"in" + " progress"`): exit 1 (parsed as no status), so it fails closed rather than passing.
The check runs in CI because `python -m unittest discover -s tests -v` (Regression harness job, Python 3.10 and 3.12) picks up `tests/test_home_status.py`; CI is green on e4c9d5a9. No other status wording exists in `ui/qml`/`ui/src` (grep).

## Findings

- [NOTE 1] `tests/test_readme_status.py:234`, `tools/generate_readme_status.py:283` — on Python 3.9, `Path.write_text(..., newline=)` raises `TypeError`, failing `test_check_fails_on_a_stale_copy_and_update_repairs_it`. Pre-existing (neither file is in this diff; builder reports the same on master). CI's floor is 3.10 and passes; the framework says 3.9+. Not caused by this round.
- [SHOULD FIX, for Brain at merge, outside builder's scope] `docs/state.md:23` says "M4-M6 not started", which now contradicts the roadmap (M6 in progress). `docs/state.md:79-81` and the "Recommended next slice" bullet still list the presentation tidy-ups (all five now done) as parked/next. The builder flagged both as out of scope; they need updating in the merge or a follow-up, else the merged tree carries a stale status claim.
- [NOTE 2] `docs/ROADMAP.md` M6: cites brief 007 for the macOS `policy/` pass, but brief 007 records `policy/` ctest failing (`lf_list`); the pass came later (brief 011), and the roadmap's other cited source (`state-history.md`) does record `policy/` 2/2. Citation is slightly loose, claim is true. Also, `ui/` on macOS now has evidence (this round's and my own build and 2/2 ctest on macOS 27 / Qt 6.11.1) that the roadmap says does not exist; builder flagged it as a follow-up.
- [NOTE 3] The offscreen load on macOS prints a Qt font-alias notice to stderr; the CI's empty-stderr assertion would fail on a macOS leg. Linux CI is green. Not a defect of this round.
- [NOTE 4] `docs/capabilities.md` caption names build `6e74e5fc`; that SHA exists only if merged without squash (builder open question 7). The mechanism `check_home_status.py` cannot check the PNG; the doc says so honestly.
- [NOTE 5] The home screen's M6 detail line reads "Windows and macOS builds and CI, ..." next to "in progress". It describes the milestone scope, and the status column is the checked claim; acceptable, but a reader could skim it as "builds and CI exist". Judgment call, not a defect.
- No BLOCKERs. No UNPROVEN CLAIMs: every builder claim I tested reproduced (ui cycle, capture, check failure text, CI conclusions, sizes).

## Not verified

- Linux / Qt 6.8.3 (CI's pinned version) and Windows: not run by me; relied on CI success for Linux. My Qt is 6.11.1.
- `hero.svg` as a whole: I rendered it with Quick Look, which cropped it to a square thumbnail, so I saw the left part only (field geometry, wordmark, first two chips), not the semantic-model chip. Its correctness rests on the SVG diff and on the same chip drawn in `social-preview.png`, which I did see.
- `social-preview.png` matches its SVG only by my eye; no test binds them (by design). Not viewed at chat-app sizes, nor on GitHub.
- Python 3.10 and 3.12 locally: I only had 3.9.6; CI covers those.
- Screen at other window sizes; other screens; `client/`, `data/`, `policy/`, `gframe/` and duel behaviour (untouched; no claim made).
- `tools/check_pr_evidence.py`: needs a PR body; not applicable yet.

## Verdict

Every one of the five items meets its required outcome, and the home screen's status can no longer drift without a red test: I broke it five ways and each failed with a specific message, and the check runs in CI with the existing suite. The images I saw are true to the roadmap and carry no personal metadata, and the M6 split is supported by the recorded evidence. The only open matter is that `docs/state.md` (outside this round's scope) now contradicts the roadmap on M6 and still lists these items as pending; Brain should update it at merge. Confidence high on items 1 to 5 as I could test them, moderate on the hero's rendered appearance.

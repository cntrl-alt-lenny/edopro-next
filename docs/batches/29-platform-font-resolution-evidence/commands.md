# Batch 29 evidence commands

Source: `771f25462d28dab888e6292d350b689b6345b0d8`.
Native macOS arm64, Qt 6.11.1, Apple Clang; separate worktree from
`1068ee315006c5d3599e5c2100435aa8b9441967`. ocgcore initialized at its pinned
commit, unmodified. Log paths are sanitized; captures and synthetic databases
are untracked.

| Command | Real result / exit |
| --- | --- |
| `git fetch origin`; `git submodule update --init`; `python3 tools/fw.py status` | 0; framework current, parallel batches retained |
| `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON` | 0; exact-configure.txt retains pre-existing QTP0004/Vulkan messages |
| `cmake --build ui/build --parallel` | 0; exact-build.txt |
| `mkdir -p "$EDOPRO_FOCUS_CAPTURES"`; `ctest --test-dir ui/build --output-on-failure` with capture environment | 0; exact-ctest.txt, all three targets passed |
| `QT_QPA_PLATFORM=offscreen ui/build/tests/fonts/test_platform_fonts` | Included in CTest: real Main captures at both sizes, font membership/pitch, ordinary controls/delegates and literal QML family semantics |
| `python3 tools/generate_messages.py --check` | 0; message table up to date (96 ids) |
| `python3 tools/generate_protocol_constants.py --check` | 0; protocol constants up to date (187 values) |
| `python3 tools/generate_readme_status.py --check`; `python3 tools/check_home_status.py` | 0 each; README/home status current |
| `python3.13 -m unittest discover -s tests -v` | 0; 133 tests, 11 skips (10 absent semantic binary, one Windows ACL test), python-supported.txt |
| `python3 tests/test_replay_trace.py --update`; `git diff --exit-code -- tests/golden` | 0 each; golden.txt; unchanged goldens |
| `python3 tools/fw.py check`; `git diff --check` | 0 each; 0 framework errors/warnings, no whitespace errors |
| `python3 tools/check_pr_evidence.py --file <draft-body>` | 0; rerunnable evidence commands and no measured figures |

## Independent macOS smoke

No GNU timeout is installed on this Mac. Python subprocess timeout supplies
its process-lifetime equivalent; the workflow's survival/status and nonempty
stderr predicates remain independent and unchanged. Run from repository root:

```sh
QT_QPA_PLATFORM=offscreen python3 -c '
import subprocess, sys
try:
    sys.exit(subprocess.run(["./ui/build/edopro_next_shell"], timeout=20).returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
' > qml-stdout.log 2> qml-stderr.log && status=0 || status=$?
if [ "$status" -ne 124 ]; then
    cat qml-stderr.log
    exit 1
fi
if [ -s qml-stderr.log ]; then
    cat qml-stderr.log
    exit 1
fi
```

Exact-source result: survival assertion exit 0 (status 124), empty-stderr
assertion exit 0, wrapper exit 0. `exact-qml-stderr.txt` is zero bytes.
Before implementation at baseline production source: survival 0, stderr 1,
missing Sans Serif diagnostic (`pre-smoke-stderr.txt`). Dedicated Theme
regression at `fcbf00d1` exited 1 (`negative.txt`).

## Preserved failed attempts

- New test module in the screen CMake directory: initial output collision;
  distinct output directory configured but replaced screen registration,
  causing missing TestHarness failures. A separate directory scope fixes
  the class; existing screen assertions are unchanged.
- New helper initially lacked Theme's explicit EdoproNext import in the
  production subdirectory: smoke survived but stderr failed with undefined
  PlatformFonts. Added the import, then reran the actual shell.
- Popup delegate enumeration initially searched QObject ownership rather
  than visual children: test failed. Traversing popup content visual items
  establishes actual delegate fonts.
- First separate-directory configure needed its own absolute resource alias;
  retained configure-scoped.txt. Registered the real files, without copying.
- First CTest capture run failed because the destination directory was absent;
  created it, as CI already does. Kept ctest-final.txt and reran unchanged
  assertions. No retry-based native reliability claim.
- Python 3.9.6 discovery failed an existing README scratch-write test because
  pathlib.Path.write_text does not support newline there. Supported 3.13
  discovery passed; no tool/test code changed (python.txt).
- Initial scratch synthetic fixture had a wrong INSERT arity (12 values for
  11 columns); corrected fixture creation. Neither database is tracked.
- PR checker without a body rejected empty stdin (exit 1); supplied the
  actual draft body with rerunnable commands, exit 0.

## Linux CI at the source SHA

[Run 37937649254](https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37937649254)
completed successfully. Qt 6.8.3, existing Release/UI_TESTS configuration,
build and all three CTest targets passed; the literal unchanged workflow
`timeout 20` smoke reached its success message after both assertions.
`linux-ci.txt` retains configure/build/test/smoke output. All five required
jobs succeeded; upstream-baseline was skipped on this ordinary push, as
configured. The `ui-screenshots` artifact exists and was not expired when
checked. CI configuration/check names/assertions were not edited. This is
not Linux Debug/WERROR evidence.

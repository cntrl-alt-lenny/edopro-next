# Batch 28 — card-code-search

## Done

Known decimal codes select one filtered result through the existing search
box. Trimmed ASCII-only, checked nonzero uint32 conversion accepts leading
zeroes; unknown/invalid input keeps ordinary text search. Active structured,
unofficial and banlist visibility filters remain effective. Clearing and
catalogue/filter/banlist changes refresh correctly. ADR 0005 is unchanged.
Dedicated synthetic tests and scoped CMake registration added; upstream
quotations and deliberate differences recorded in architecture/ADR 0014.

Brain's suggested shared-page update after acceptance: decimal code lookup
exists; full sigil search parity remains planned. No QML/startup/screen-test
or other Builder-owned files changed. Normal delivery for Brain review;
strict local smoke gate remains red, so acceptance is outstanding.

## Checked

Source SHA: `80c4e694f214729facd81b5d33bd0788e3da8146`, based on
`origin/master` at `1068ee31`. macOS arm64, Apple LLVM 21, Qt 6.11.1.
`ocgcore` initialised at `46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57`.
The following are real local outputs, not CI claims.

| Command | Exit | Output |
| --- | --- | --- |
| `cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` | 0 | Generating done |
| `cmake --build ui/build` | 0 | Shell and three test executables linked |
| `ctest --test-dir ui/build --output-on-failure` | 0 | deckbuilder, deckbuilder_screen, card_code_search passed; 100% tests passed out of 3 |
| `python3.13 tools/generate_messages.py --check` | 0 | message table up to date (96 ids) |
| `python3.13 tools/generate_protocol_constants.py --check` | 0 | protocol constants up to date (187 values) |
| `python3.13 tools/generate_readme_status.py --check` | 0 | README status block is up to date |
| `python3.13 -m unittest discover -s tests -v` | 0 | Ran 133 tests; OK (skipped=11) |
| `python3.13 tests/test_replay_trace.py --update` | 0 | wrote all three golden traces |
| `git diff --exit-code -- tests/golden` | 0 | empty diff |
| `python3.13 tools/fw.py check` | 0 | 0 error(s), 0 warning(s) |
| `git diff --check` | 0 | empty output |

CI at the pushed summary SHA must be checked live, using
`gh run list --branch worker/28-card-code-search --commit <delivery-SHA>`.
No PR opened or merge performed.

## Not checked

Native interaction, real card databases, Windows/Linux runtime and complete
legacy parity. No duel-behaviour equivalence claimed. Python skipped ten
semantic-binary tests (client not built in this UI worktree) and one Windows
ACL test. No visual inspection; existing screen assertions ran offscreen.

## Failed or blocked

Pre-feature corrected fixtures: CTest exit 8, 14 Qt assertions passed and
three failed (known lookup, filters, reload); proves feature regressions fail.
Initial fixture names accidentally contained numbers, giving two additional
text matches; fixed before the red-test commit. First implementation test
run: 16 passed, one failed; test assumed banlist replacement but store appends.
Corrected test selects the newly appended list. Model tester emits Qt's
`Trying to construct an instance of an invalid type, type id: 4097`; retained.

Python 3.9 discovery: exit 1, one failure, 11 skips. Golden update: exit 1,
`TypeError: write_text() got an unexpected keyword argument 'newline'`.
Existing README writer uses that argument too. Python 3.13 passes both.
CMake reports existing SQLite target deprecation; linker reports duplicate
static libraries. No suppression or ownership expansion.

Strict workflow-equivalent smoke on macOS: survived 20 seconds (status 124),
but empty-stderr assertion exit 1. No timeout binary installed; use:

```python
import os, subprocess
from pathlib import Path
with open('qml-stdout.log', 'w') as out, open('qml-stderr.log', 'w') as err:
    p = subprocess.Popen(['./ui/build/edopro_next_shell'],
        env={**os.environ, 'QT_QPA_PLATFORM': 'offscreen'}, stdout=out, stderr=err)
    try:
        status = p.wait(timeout=20)
    except subprocess.TimeoutExpired:
        status = 124
        p.terminate()
        p.wait(timeout=5)
assert status == 124, status
assert not Path('qml-stderr.log').read_bytes()
```

Actual stderr (148 bytes):

```text
qt.qpa.fonts: Populating font family aliases took 65 ms. Replace uses of missing font family "Sans Serif" with one that exists to avoid this cost.
```

Startup/font resolution belongs to batch 27/Brain; this batch does not change
it or waive the strict gate. Framework status's legacy `EADME` mislabel was
checked against git; no framework update needed.

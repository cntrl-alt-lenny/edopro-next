# Building

How to build and test each part of the repository. These are the commands the landing
page's quick start points to. The full per-layer evidence commands, including the
warnings-as-errors flag CI uses, are in [`AGENTS.md`](../AGENTS.md) ("What counts as
evidence, per layer"), and CI runs them from
[`.github/workflows/edopro-next.yml`](../.github/workflows/edopro-next.yml).

## Semantic client model

C++20 and nothing else. No Qt, no Irrlicht, no `ocgcore`. Needs CMake 3.21+ and Ninja.

```bash
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build client/build
ctest --test-dir client/build --output-on-failure
```

## Card and deck data, and deck legality

`data/` (card database facade, `.ydk` codec, card search) and `policy/` (deck legality)
are separate, Qt-free projects. Both need the SQLite3 development files (for example
`libsqlite3-dev` on Debian and Ubuntu) and are built and tested the same way:

```bash
cmake -S data -B data/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build data/build
ctest --test-dir data/build --output-on-failure

cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build policy/build
ctest --test-dir policy/build --output-on-failure
```

## Qt shell

Needs Qt 6.5+ (developed against 6.8.3 LTS) and, because the deck builder is linked into
the shell, the same SQLite3 development files as `data/`. The CI job that builds it
installs `libsqlite3-dev` alongside the Qt GUI dependencies.

```bash
cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ui/build
./ui/build/edopro_next_shell
```

## Regression harness

Python 3.10+ (CI tests 3.10 and 3.12), no dependencies. The semantic golden tests need the
client model above to have been built; without it they skip.

```bash
python -m unittest discover -s tests -v
```

Two derived files have their own checks, and CI runs both:

```bash
python tools/generate_messages.py --check
python tools/generate_protocol_constants.py --check
```

The README's "What works" block is derived from [`ROADMAP.md`](ROADMAP.md). After editing
the roadmap, refresh it, then check it (the check also runs in the test suite above):

```bash
python tools/generate_readme_status.py
python tools/generate_readme_status.py --check
```

## Windows and macOS

Neither is in the active CI, which is Linux-only. Both have been built by hand:

- **Windows 11 / MSVC**: all four modules configure, build and pass CTest
  ([brief 005](briefs/archive/005-2026-08-31-windows-msvc-build.md)). `cmake -G Ninja`
  needs to run inside the MSVC environment (`vcvars64.bat`); `data/`, `policy/` and `ui/`
  need vcpkg's SQLite3 (`-DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake
  -DVCPKG_TARGET_TRIPLET=x64-windows`); `ui/` also needs `-DCMAKE_PREFIX_PATH=<Qt install>/msvc2022_64`,
  and Qt's `bin/` on `PATH` to run its tests. The details are in `AGENTS.md`, under "On
  Windows/MSVC".
- **macOS / Apple clang**: `client/`, `data/` and `policy/` build and pass. `ui/` has not
  been built there, because that machine has no Qt
  ([`state.md`](state.md), "Local toolchain").

## Upstream client

See [`BASELINE.md`](BASELINE.md) for the full record, environment and the gotchas that cost
real time.

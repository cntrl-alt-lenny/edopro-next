# Baseline build record

The purpose of this document is to prove that **untouched upstream EDOPro builds and runs**
before this project changes anything, and to make that result reproducible.

If a future change breaks the build, this is the known-good point to bisect against.

## Result

| | |
|---|---|
| Upstream commit | `54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` (2026-08-20, "make Duel constructor private") |
| Status | **Success** |
| Artifact | `bin/x64/release/ygoprodll` — 117 MB, ELF 64-bit PIE, not stripped |
| Runtime self-report | `EDOPro version 41.0.2` / `Irrlicht Engine version 1.9.0` |
| Build date | 2026-08-24 |

The binary starts, initialises Irrlicht, and brings up an OpenGL 4.5 context. It was not
driven further than startup — no duel was played, and no card data was present.

## Environment

| | |
|---|---|
| Host | Windows 11 Pro 25H2 (build 26200), AMD Ryzen 7 5800X, 16 GB RAM |
| Build environment | WSL2, Ubuntu 26.04 LTS |
| Compiler | gcc / g++ **15.2.0** |
| CPUs available to WSL | 4 (capped via `.wslconfig`) |
| RAM available to WSL | 5.8 GB (capped via `.wslconfig`) |
| premake5 | 5.0.0-beta2 (downloaded by `travis/install-premake5.sh`) |
| Dependencies | Upstream's prebuilt vcpkg cache, `installed_x64-linux.7z` (37 MB) |

Note the compiler divergence: upstream CI builds Linux with **gcc-10 / gcc-11**. This
baseline used **gcc 15.2**. It compiled successfully, with warnings only (see below).

## Reproduction

No `sudo` was required at any point. Everything needed was either already present or
downloadable to the user's home directory.

```bash
# 1. Copy the tree onto WSL's native ext4. Do not build on /mnt/c —
#    the 9p filesystem makes large C++ builds punishingly slow.
DST=$HOME/edopro-baseline
mkdir -p "$DST"
tar -C /mnt/c/Users/leona/Dev/edopro-next --exclude=.git -cf - . | tar -C "$DST" -xf -
cd "$DST"

# 2. Normalise line endings (see Gotcha 1).
find . -name '*.sh' -type f -exec sed -i 's/\r$//' {} +

# 3. Environment expected by the travis/ scripts.
export TRAVIS_OS_NAME=linux TARGET_OS=linux BUILD_CONFIG=release ARCH=x64
export VCPKG_ROOT="$DST/vcpkg"

# 4. Build tooling and bundled assets.
./travis/install-premake5.sh linux
./travis/install-local-dependencies.sh linux   # NotoSansJP font + UPX

# 5. Dependencies: upstream publishes a prebuilt vcpkg tree, so nothing
#    is compiled from source here.
mkdir -p "$VCPKG_ROOT" && cd "$VCPKG_ROOT"
curl -L -o installed.7z \
  https://github.com/edo9300/edopro-vcpkg-cache/releases/latest/download/installed_x64-linux.7z
python3 -c "import py7zr; py7zr.SevenZipFile('installed.7z').extractall()"   # see Gotcha 2
cd "$DST"

# 6. Build.
./travis/build.sh

# 7. Verify.
./bin/x64/release/ygoprodll   # prints version banner, opens a window
```

## Gotchas discovered

These cost real time and are the reason this document exists.

**1. CRLF line endings break the build scripts.**
Git for Windows ships `core.autocrlf=true` at system level, and upstream's
`.gitattributes` is only `* text=auto`. A Windows clone therefore gives `travis/*.sh`
CRLF endings, and the shebang becomes `#!/usr/bin/env bash\r`. The failure is
`env: 'bash\r': No such file or directory` with exit code 127, which does not name the
real cause. Normalising `*.sh` to LF fixes it.

**2. `7z` is not present in a default Ubuntu 26.04 WSL image**, and installing
`p7zip-full` needs `sudo` (this machine's WSL requires a password). The vcpkg cache is a
`.7z`. Rather than requiring elevation, extraction was done with Python's `py7zr`,
installed into a venv in the user's home. `bsdtar` was also absent and would otherwise
have worked.

**3. Do not build on `/mnt/c`.** WSL's 9p filesystem is drastically slower for builds that
touch thousands of small files. Copying to `$HOME` first is the difference between minutes
and a long wait.

**4. gcc 15 emits warnings that gcc 10 did not.** None were fatal. The notable ones:

- `-Wno-unused-lambda-capture`, `-Wno-implicit-const-int-float-conversion` and
  `-Wno-unknown-warning-option` are Clang-oriented flags that gcc does not recognise; gcc
  reports them as unrecognised at top level.
- One `-Wstringop-overflow=` warning inside bundled Lua (`ocgcore/lua/src/lstrlib.c`,
  via `luaL_addlstring`) with an implausible bound. This is in vendored upstream Lua, not
  in EDOPro code.

Neither is a defect introduced by this project, and neither was worked around.

## What this does *not* prove

Stated plainly so it is not over-claimed:

- It does **not** prove a Windows or macOS build. Only Linux/gcc was exercised.
- It does **not** prove the client works end to end. It was started, not played. No card
  database, card scripts or images were installed, so no duel was run.
- It does **not** exercise upstream's own CI matrix, which uses older compilers and
  additionally builds for Android, iOS and Windows.
- There is no upstream test suite to run. Upstream has no unit tests; CI builds and
  deploys. This is precisely the gap that `docs/ROADMAP.md` intends to close on our side
  of the boundary.

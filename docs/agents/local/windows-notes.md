# Windows/MSVC build notes

Longer project guidance, pointed at from `AGENTS.md`'s evidence table per
`CLAUDE.md`/framework rule 14 (`docs/agents/local/` is for project detail
that does not need to be in every session's read). Confirmed working (brief
005): all four modules (`client/`, `data/`, `policy/`, `ui/`) configure,
build under `-DEDOPRO_NEXT_WERROR=ON`, and pass `ctest` this way.

- `cmake -G Ninja` finds no compiler unless it runs *inside* the MSVC
  environment. Run everything from a shell where
  `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`
  has been invoked first (e.g. `cmd /c "call vcvars64.bat && <command>"`, or
  a Developer Command Prompt / Developer PowerShell).
- `data/`, `policy/` and `ui/` additionally need
  `-DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake
  -DVCPKG_TARGET_TRIPLET=x64-windows` (SQLite3 comes from vcpkg on this
  platform), and `ui/` also needs
  `-DCMAKE_PREFIX_PATH=<Qt install>/msvc2022_64` (e.g.
  `C:/Qt/6.8.3/msvc2022_64`).
- Qt-linked test executables (`ui/`) do not launch unless Qt's `bin/`
  directory is on `PATH` at runtime, including for `ctest` itself. Without
  it, CTest reports `BAD_COMMAND` — which reads as a build failure and is
  not one; the build already succeeded.

Also seen, and not a defect: CTest occasionally reports one spurious
`BAD_COMMAND` for a test immediately after a parallel link (observed on
`client/`'s `duel_state`, brief 005) — a Windows file-locking race right
after the linker closes the executable, not a real failure. It passes on an
immediate re-run. Do not chase it; re-run once and move on. Separately, a
**freshly linked** `.exe` has occasionally been blocked once on its first
launch by a local Application Control policy (seen on
`test_protocol_decoder.exe` and `edopro_next_shell.exe`, brief 005),
reporting `BAD_COMMAND`/"An Application Control policy has blocked this
file" even though the binary is fine; deleting the stale `.exe` and
relinking has cleared it every time it has been seen. Neither of these is
project code to fix.

# Brief 005 — make the C++ layers build on Windows/MSVC

Status: queued

One brief lives here at a time. On delivery it moves to
[`delivered/`](delivered/), and only on adjudication to
[`archive/`](archive/) — see [`README.md`](README.md). Template and field
meanings: [`docs/roles/builder.md`](../roles/builder.md).

---

## MODE: IMPLEMENTATION

## Goal

`client/`, `data/`, `policy/` and `ui/` must configure, build with
`-DEDOPRO_NEXT_WERROR=ON`, and pass their CTest suites on Windows with MSVC —
without weakening a single warning setting, and without changing what any of
this code does.

## Why this is next

The primary development machine now has MSVC 19.44, CMake 3.31.6, Qt 6.8.3
(`msvc2022_64`) and vcpkg-built SQLite3. Brain installed them and then tried
to run `AGENTS.md`'s per-layer evidence table for real.

**Every test suite passed — 13 of 13, on a compiler this code has never been
compiled with before.** That is meaningful evidence in its own right and it is
not what this brief is about.

What this brief is about is that **three things fail to build**, and CI cannot
see any of them because CI builds only Linux/GCC. Until they are fixed, four
of the six rows in the evidence table cannot be satisfied on this machine, and
every future round touching `ui/` is back to "CI is the only proof" — which is
the exact problem installing the toolchain was meant to solve.

The next round after this one is the deck-builder legality UI, which touches
`ui/` and is **blocked by defect 3 below**.

## Base SHA

**Branch from `origin/meta/round-2-close`** — this brief lives on that branch
(PR #20) and is not on `master` yet. If PR #20 has merged by the time you
start, branch from `origin/master` instead. Verify with `git log -1`, record
the actual SHA, and say which you used.

## The three defects

Brain established each of these by running the real commands on this machine.
**Reproduce each one yourself before fixing it** — a previous agent's finding
is evidence, not fact — but you are not expected to rediscover them.

1. **`client/tests/test_protocol_decoder.cpp` — narrowing conversions.**
   Under MSVC `/W4 /WX` the build fails with `C4244` inside
   `std::tuple<uint32_t, uint8_t, uint8_t, uint32_t>` construction. The call
   sites pass `std::uint32_t` protocol constants (for example
   `proto::LOCATION_MZONE`, declared `inline constexpr std::uint32_t` in
   `client/include/edopro_next/client/protocol_constants.h:115`) into the
   `std::uint8_t` tuple slots — see `confirm_cards_packet` around
   `test_protocol_decoder.cpp:112` and its call site near `:934`. GCC's
   `-Wall -Wextra` is silent on this; it is `-Wconversion` territory, which
   the project does not enable.

2. **`data/CMakeLists.txt` — incompatible optimisation flags.**
   `bench_card_search` sets `/O2` unconditionally for MSVC. A Debug configure
   adds `/RTC1`, and MSVC rejects the combination outright:
   `cl : Command line error D8016 : '/RTC1' and '/O2' command-line options are
   incompatible`. This is a hard error, not a warning, so `/WX` is irrelevant
   to it. Note the `/O2` is deliberate — that target is a performance
   measurement and `docs/architecture/card-search.md#performance` explains why
   it is not a CTest case. Preserve the intent.

3. **`ui/tests/CMakeLists.txt` — QML cache path with a `..` segment.**
   Building the `ui/` test targets fails at:
   `ninja: error: mkdir(tests/.rcc/qmlcache/test_deckbuilder_screen_../qml):
   No such file or directory`. The generated cache directory carries a literal
   `..` in the middle of the path, which Windows cannot create. The
   `edopro_next_shell` target itself builds and links fine, and
   `test_deckbuilder` builds and passes; only `test_deckbuilder_screen` is
   affected.

## Two operational facts that are true and written down nowhere

Not defects — but a future agent will lose an hour to each. Put them somewhere
they will actually be found.

4. On Windows, `cmake -G Ninja` finds no compiler unless it runs inside the
   MSVC environment (`vcvars64.bat`). `AGENTS.md`'s evidence-table commands do
   not mention this and, taken literally, do not work here.

5. Qt-linked test executables do not launch unless Qt's `bin` directory is on
   `PATH` at runtime. Without it CTest reports `BAD_COMMAND`, which reads as a
   build failure and is not one.

Also worth knowing, and not a bug in anything: CTest reported one spurious
`BAD_COMMAND` for `duel_state` immediately after a parallel link, and passed
on re-run. Windows file locking, not a defect — do not chase it.

## Non-goals

- **Do not change what any of this code does.** This is a portability round.
  If a fix would alter behaviour, stop and report rather than proceeding.
- **Do not touch `gframe/`, `ocgcore/`, or the upstream baseline build.**
- **Do not change `.github/workflows/`.** CI is owner-reserved, and this round
  must not "fix" anything by making CI check less.
- Do not add a dependency. If you believe one is genuinely required, that
  needs an ADR and it is out of scope here — report it instead.
- Do not start M6. Windows as a *supported platform* is a milestone this
  project has not begun; this round makes the existing layers buildable on the
  development machine so their evidence can be produced, and nothing in
  `README.md` or `docs/ROADMAP.md` may start claiming otherwise.

## Protected invariants

- **Never weaken a warning to make a build pass.** `/W4` and `/permissive-`
  stay; `EDOPRO_NEXT_WERROR=ON` must still mean warnings are errors. The fix
  for defect 1 belongs in the code, not the flags. If you conclude some
  warning genuinely must be suppressed, name it, scope it as narrowly as the
  language allows, and justify it in the report — a blanket `/wd4244` is a
  rejection.
- **Linux/GCC must keep working.** CI is the proof, and it is not optional
  for this round: a fix that trades one platform for the other is worthless.
- **The module separation holds.** `client/`, `data/` and `policy/` gain no
  Qt, no Irrlicht and no `ocgcore`. `data/` stays Qt-free.
- **`edopro_next_deck` must still link neither SQLite nor `edopro_next_data`** —
  `data/CMakeLists.txt` says that separation is proven by the link graph
  rather than by a comment, and this round touches that file.

## Required investigation

1. **Is defect 1 confined to test code?** Brain observed it only in
   `test_protocol_decoder.cpp`, but only reached that file — the production
   library linked before the tests were compiled. Build **all four modules**
   with `/W4 /WX` and find out whether production code has the same pattern.
   That answer matters more than the fix: a narrowing conversion in a decoder
   that parses untrusted `.cdb` and network bytes is a different conversation
   from one in a test fixture.
2. For defect 1, is the right fix an explicit cast at each call site, a
   narrower constant type, or a differently-typed tuple? Say why you chose
   what you chose. An explicit cast that silences a genuine truncation is a
   defect, not a fix.
3. For defect 3, establish whether the `..` in the generated path comes from
   our `CMakeLists.txt` or from Qt's own tooling, and fix it at the layer that
   actually owns it.
4. Are there **further** Windows/MSVC failures beyond these three? Brain
   stopped at the first failure in each module. Enumerate what you find; a
   partial list presented as complete is this project's recurring defect.

## Acceptance criteria

- All four modules configure, build with `-DEDOPRO_NEXT_WERROR=ON`, and pass
  `ctest` on Windows/MSVC. Real output for each.
- `ui/`'s **`test_deckbuilder_screen` builds and passes**, since the next
  round depends on it.
- CI is green at your head SHA — that is the Linux/GCC half of the proof, and
  the round is not complete without it.
- Items 4 and 5 are documented where an agent following the evidence table
  will encounter them, not in a file nobody opens.
- No warning suppressed rather than fixed, or a named, narrowly-scoped
  exception with its justification.
- The answer to required investigation 1, stated plainly, whichever way it
  comes out.

## Required evidence

- `git diff --stat <base>..<head>`.
- **Real output of all four cycles**, configure through `ctest`, on Windows.
  Not a summary — the actual text.
- `python -m unittest discover -s tests -v`.
- `python tools/generate_messages.py --check` and
  `python tools/generate_protocol_constants.py --check`.
- CI status at the exact head SHA, checked rather than assumed.
- **State your exact toolchain versions** — MSVC, CMake, Qt, and how you
  invoked the MSVC environment. The next person to reproduce this needs them.

## Git expectations

Branch `meta/windows-msvc-build`, in the Builder worktree
(`.worktrees/builder`).

The `meta/` prefix is deliberate and worth understanding: this round exists to
make the project's **evidence apparatus** work on the development machine, not
to ship a Windows build of the product. That is M6, it has not started, and
this brief must not imply otherwise.

Focused commits — ideally one per defect, so a reviewer can take them
separately. Push, open a PR carrying `DO NOT MERGE — under review`.
**Do not merge.**

## Completion-report schema

The standard report in [`docs/roles/builder.md`](../roles/builder.md), plus:

- **The answer to required investigation 1 first** — whether production code
  carries the same narrowing pattern as the test code, and if so where.
- **The full list of Windows/MSVC failures you found**, including any beyond
  the three above, and how you established the list is complete — or a plain
  statement that you could not.
- **Every warning you suppressed rather than fixed**, if any, with scope and
  justification. If none, say so.
- **Your four cycles' real output**, and the toolchain versions.

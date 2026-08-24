<h1 align="center">edopro-next</h1>

<p align="center">
  A modern client architecture for <a href="https://github.com/edo9300/edopro">EDOPro</a> —
  preserving Project Ignis's duel engine and card scripts, and replacing the
  presentation layer it inherited from old YGOPro.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-early%20scaffold-C08A3E" alt="Status: early scaffold">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-blue" alt="AGPL-3.0-or-later">
  <img src="https://img.shields.io/badge/UI-Qt%206%20%2F%20QML-41CD52" alt="Qt 6 / QML">
</p>

---

> **This is not a playable client, and is not close to being one.**
> It is a repository foundation, an architecture, and one compiled UI shell. If you want
> to play Yu-Gi-Oh today, use [EDOPro](https://github.com/edo9300/edopro). This project
> would not exist without it.

## Why

EDOPro's duel engine is remarkable. It automates one of the most rules-dense card games
ever made, across the entire card pool, maintained by volunteers over many years. That is
the hard part, and it already works.

The client around it is visibly descended from YGOPro: fixed-coordinate widgets on a
patched Irrlicht 1.8, without a layout system, DPI awareness, an accessibility layer, or
a declarative focus model. The goal here is a client that feels like it was released
today, without touching the engine that makes it worth using.

The rule the whole project is organised around:

> **The rules engine must not become the UI. The UI must not implement game rules.**

## Status

Honest, and deliberately so. Nothing below is rounded up.

| | Status |
|---|---|
| Upstream baseline builds and runs | **Working** — `EDOPro version 41.0.2`, see [docs/BASELINE.md](docs/BASELINE.md) |
| Architecture survey of upstream | **Working** — [docs/architecture/current-edopro.md](docs/architecture/current-edopro.md) |
| UI stack decision | **Decided** — Qt 6 / QML, [ADR 0001](docs/adr/0001-ui-runtime-stack.md) |
| Qt 6 / QML shell | **Working** — compiles and runs, zero QML errors |
| Design token system | **Working** — [ui/qml/Theme.qml](ui/qml/Theme.qml) |
| Semantic client model | **Not started** |
| Deck builder | **Not started** |
| Duel field | **Not started** — deliberately last |
| Automated tests | **None yet** |
| CI | **None yet** |

The shell renders a navigation rail, a home screen reporting real build metadata from
C++, and honest "not implemented" states for every screen that does not exist. There are
no placeholder decks, no fake cards and no mock duel.

## Architecture

```
                      ocgcore  (upstream, authoritative)
                         |
                  byte message stream over a C ABI
                         |
                  duel state adapter          <-- not built yet
                         |
              semantic client model           <-- not built yet
                         |
        +----------------+----------------+
        |                                 |
     Qt 6 / QML                       tools / AI
     (this repo's ui/)                (future)
```

The load-bearing discovery from the survey is that **the engine boundary is already
clean**: the client loads `ocgcore` as a shared library and talks to it over a byte
protocol. A new client needs no engine changes at all.

The problem is on our side of that boundary. `gframe/duelclient.cpp` handles 97 `MSG_*`
cases in one function that simultaneously decodes the protocol, mutates state, schedules
animation, drives widgets and gates input — and `ClientCard` interleaves Irrlicht
transforms with card semantics, so game state cannot be reasoned about without a
renderer. Separating those is the actual work.

Migration order, cheapest and safest first: deck and card data, settings, replay browser,
lobby, then duel state, then the duel field last. Reasoning in
[the survey](docs/architecture/current-edopro.md).

## Building

**Upstream client** (verified on Linux; see [docs/BASELINE.md](docs/BASELINE.md) for the
full record, environment and gotchas):

```bash
export TRAVIS_OS_NAME=linux TARGET_OS=linux BUILD_CONFIG=release ARCH=x64
export VCPKG_ROOT="$PWD/vcpkg"
./travis/install-premake5.sh linux
./travis/install-local-dependencies.sh linux
./travis/build.sh          # -> bin/x64/release/ygoprodll
```

**The Qt shell** (independent of the above):

```bash
cd ui
qt-cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/edopro_next_shell
```

Requires Qt 6.5+. Developed against 6.8.3 LTS.

## Relationship to upstream

This is a fork of [edo9300/edopro](https://github.com/edo9300/edopro), currently based on
`54ea755a`. It tracks upstream and intends to keep doing so — new code is confined to its
own directories specifically so that merges stay tractable. See
[docs/UPSTREAM.md](docs/UPSTREAM.md).

We are **not** affiliated with or endorsed by Project Ignis, Konami or Shueisha.

## Licence

EDOPro is free software under the **GNU AGPL v3 or later**, and so is this fork. Upstream
copyright notices, `LICENSE`, `COPYING` and `notices/` are preserved unchanged. Inherited
code is not relicensed.

Card scripts, card databases and card artwork are **not** in this repository. They belong
to Project Ignis and other rights holders, and are fetched at runtime by the client.

## Credit

Everything that makes this project worth attempting was built by
[Project Ignis](https://github.com/ProjectIgnis) and
[edo9300](https://github.com/edo9300) — the duel engine, the card scripts, the databases,
and the years of correctness work behind them. This project modernises a presentation
layer. It does not reinvent the game.

Upstream's own README is preserved at [docs/upstream-README.md](docs/upstream-README.md).

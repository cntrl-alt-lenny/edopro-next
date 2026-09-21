<div align="center">

<img src="docs/assets/hero.svg" alt="edopro-next — a modern client architecture for EDOPro" width="100%">

# edopro-next

**Preserve the engine. Expose clean semantics. Modernise the client.**

[![CI](https://github.com/cntrl-alt-lenny/edopro-next/actions/workflows/edopro-next.yml/badge.svg)](https://github.com/cntrl-alt-lenny/edopro-next/actions/workflows/edopro-next.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat)](client/CMakeLists.txt)
[![Qt 6](https://img.shields.io/badge/Qt-6%20%2F%20QML-41CD52?style=flat)](docs/adr/0001-ui-runtime-stack.md)
[![license](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue?style=flat)](LICENSE)

</div>

## What is this?

edopro-next is a modern Qt 6 / QML client for [EDOPro](https://github.com/edo9300/edopro). It sits on top of EDOPro's existing duel engine and Project Ignis's card scripts, which stay authoritative and untouched. The new client reads a presentation-free model of the duel and never decides a rule itself.

[What works](#what-works) says exactly where it stands. [The overview](docs/overview.md) explains the reasoning.

## Quick start

Build from source and run the tests. The semantic client model needs only a C++20 compiler, CMake and Ninja:

```bash
git clone https://github.com/cntrl-alt-lenny/edopro-next.git
cd edopro-next
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build client/build
ctest --test-dir client/build --output-on-failure
python -m unittest discover -s tests -v
```

The Qt shell, the `data/` and `policy/` modules, and per-platform notes are in [docs/building.md](docs/building.md).

## What works

<!-- BEGIN GENERATED readme-status (tools/generate_readme_status.py; do not edit by hand) -->
**No.** The duel field (M5) is not started. To duel today, use [EDOPro](https://github.com/edo9300/edopro).

**Done**

- **M0** Foundation
- **M2** Semantic client model

**In progress**

- **M1** Make change provable — still open: Level 2 — not started, and required for the strong claim
- **M3** Deck and card data — still open: Deck builder UI in QML: filters, legality, preview, keyboard parity

**Planned, not started**

- **M4** Low-risk screens
- **M5** Duel field
- **M6** Platform and input

<sub>Generated from [`docs/ROADMAP.md`](docs/ROADMAP.md) by `tools/generate_readme_status.py`; the test suite fails if it drifts. Detail: [what exists today](docs/capabilities.md).</sub>
<!-- END GENERATED readme-status -->

## Documentation

- [Overview](docs/overview.md): what this changes, what it does not, and why
- [Roadmap](docs/ROADMAP.md) and [what exists today](docs/capabilities.md)
- [Building](docs/building.md) and [contributing](docs/contributing.md)
- [Making change provable](docs/making-change-provable.md): the regression baseline and its limits
- [Architecture survey of upstream](docs/architecture/current-edopro.md), [decisions](docs/adr/) and [design system](docs/DESIGN_SYSTEM.md)
- [Upstream policy](docs/UPSTREAM.md) and [upstream's own README](docs/upstream-README.md)

## Credits and license

Everything that makes this project worth attempting was built by **[Project Ignis](https://github.com/ProjectIgnis)** and **[edo9300](https://github.com/edo9300)**: the duel engine, the card scripts, the databases, and the years of correctness work behind them. This project modernises a presentation layer. It does not reinvent the game.

EDOPro is free software under the **GNU AGPL v3 or later**, and so is this fork. Upstream copyright notices, [`LICENSE`](LICENSE), [`COPYING`](COPYING) and `notices/` are preserved unchanged, and inherited code is not relicensed.

Card scripts, card databases and card artwork are **not** in this repository. They belong to Project Ignis and other rights holders, and are fetched at runtime by the client.

**Yu-Gi-Oh! is a trademark of Shueisha and Konami.** This project is not affiliated with or endorsed by Project Ignis, Konami or Shueisha.

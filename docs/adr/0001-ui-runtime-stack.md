# ADR 0001 — UI and runtime stack

- **Status:** Accepted
- **Date:** 2026-08-24
- **Context commit:** upstream `54ea755a`

## Context

EDOPro's client is C++ built on a **patched fork of Irrlicht 1.8/1.9**
(`edo9300/irrlicht1-8-4`, branch `1.9-custom`). Irrlicht is a fixed-function-era 3D
engine whose GUI layer is immediate and pixel-positioned. Upstream does handle high-DPI,
but by hand: a `dpi_scale` factor and `Game::Scale()` helpers applied at each call site
(`gframe/game.h`, used throughout `game.cpp` and `image_manager.cpp`). That works, and it
is the reason the client is usable on modern displays - but scaling fixed coordinates is
not the same as a layout system, and it is applied per widget rather than derived. The
presentation problems this project exists to fix are largely structural consequences of
that choice.

Any replacement must satisfy constraints established in
`docs/architecture/current-edopro.md`:

1. The rules engine (`ocgcore`) and Project Ignis's Lua CardScripts remain authoritative
   and untouched.
2. The engine boundary is a **C ABI over a byte message stream** — not UI callbacks.
   Whatever we choose must interoperate with C/C++ cheaply.
3. Migration must be incremental. Legacy Irrlicht screens and new screens must coexist
   for a period, so upstream merges keep working.
4. Targets: Windows, macOS, Linux desktop now; Steam Deck and handheld later; mobile is
   an upstream concern we must not gratuitously break.

## Options considered

### A. C++20 + Qt 6 / QML  *(chosen)*

Keep the entire codebase in C++, replace only presentation with Qt Quick.

- **Migration cost — low.** No FFI. New code links directly against existing
  `deck_manager`, `data_manager`, `replay` and the ocgcore shim. A `QObject` facade can
  expose existing state to QML without copying it.
- **FFI complexity — none.** This is the decisive advantage. Every other option puts a
  language boundary between the UI and a byte-stream protocol that is already C.
- **Rendering and animation.** Qt Quick is a retained-mode scene graph with GPU batching,
  a real animation framework, and shader effects. Card movement, chain visualisation and
  reveal animations are first-class rather than hand-rolled tweens as in `drawing.cpp`.
- **Responsive layout.** QML's layout system and property bindings directly address the
  fixed-coordinate problem. High-DPI is handled by the framework.
- **Controller navigation.** Qt Quick has a built-in focus system
  (`FocusScope`, `KeyNavigation`, `Keys`) that is declarative and therefore testable.
- **Accessibility.** Qt exposes native accessibility on all three desktop platforms.
- **Packaging.** Mature, if heavy: `windeployqt`, `macdeployqt`, Linux AppImage.
- **CI.** `install-qt-action` is well established on GitHub Actions.
- **Upstream mergeability — good.** Changes stay in C++ and can be confined to new
  directories, so upstream merges touch only files we deliberately modify.
- **Costs, stated honestly.** Qt is a large dependency (hundreds of MB). Licensing is
  LGPLv3/GPL for open source — compatible with AGPL-3.0-or-later, but it must be
  dynamically linked, and that constrains packaging. QML has its own learning curve and it
  is easy to write slow QML.

### B. Rust frontend and backend bridge, Rust-native UI toolkit

Replace the client with Rust, driving `ocgcore` over FFI, using a Rust UI toolkit
(Slint, egui, Iced, Dioxus).

- **FFI complexity — high, and in the worst possible place.** The engine boundary is a
  byte protocol full of variable-length, version-sensitive records. Reimplementing that
  decode in Rust means maintaining a second protocol implementation in lockstep with an
  upstream that has no protocol stability guarantee. That is a permanent tax.
- **Migration cost — very high.** There is no incremental path. Rust screens cannot
  coexist inside the existing Irrlicht event loop, so this is effectively a rewrite —
  which the project brief explicitly rules out.
- **Upstream mergeability — poor.** Divergence becomes immediate and total.
- **Ecosystem maturity.** Slint is the strongest candidate and is genuinely good, but its
  accessibility, controller-navigation and mature-desktop-packaging stories are behind
  Qt's. egui is immediate-mode and would reproduce the very problem being escaped.
- **Verdict:** rejected. Not because Rust is unsuitable, but because it maximises FFI
  surface exactly where the protocol is most volatile, and forecloses incrementalism.

### C. Qt/QML UI with isolated Rust subsystems

Option A, plus Rust for a bounded subsystem behind an explicit interface.

- Architecturally sound *later*, for components with a narrow interface and no place in
  the render or input path: card search and indexing, replay analysis, semantic card
  analysis, or offline data pipelines.
- Introducing it **now** would add a toolchain, a build-system integration and a CI matrix
  dimension before there is any component that needs them.
- **Verdict:** not rejected, deferred. Revisit when a specific subsystem justifies it,
  via a new ADR. The rule is that Rust may own a subsystem, never sit between the UI and
  the engine.

### D. Keep and modernise Irrlicht

Rework the existing presentation layer in place.

- **Cheapest short term**, and it preserves all current behaviour by definition.
- But the constraints are inherent to the toolkit: Irrlicht's GUI is pixel-positioned,
  scaled manually for DPI rather than laid out, with no accessibility layer, no
  declarative focus model and no animation framework. Every goal in the brief —
  responsive layout, crisp typography, restrained motion, controller navigation,
  accessibility — would have to be built from scratch inside a fork of an engine that is
  effectively unmaintained upstream.
- We would end up maintaining a bespoke GUI toolkit as well as a card game.
- **Verdict:** rejected as a destination. However, Irrlicht **is** retained as the
  transitional runtime; see the decision below.

## Decision

**Adopt option A: C++20 with Qt 6 / QML for all new presentation.**

The deciding argument is not that Qt is the most beautiful or most modern option in the
abstract. It is that the engine boundary is a C byte protocol, and Qt is the only
candidate that lets a modern, animated, accessible, controller-navigable UI sit directly
on top of that protocol **with no FFI and no second protocol implementation** — while
allowing legacy screens to keep running during migration.

Consequences accepted:

- Qt becomes a required build dependency, and packaging grows.
- Qt must be dynamically linked to satisfy LGPL alongside AGPL-3.0-or-later.
- New UI code lives in its own directories so upstream merges stay tractable.
- Irrlicht is **not** removed. It remains the runtime for un-migrated screens until
  feature parity is demonstrated, per the migration order in the architecture survey.

Python remains tooling only — repository scripts, code and data generation, schema
validation, tests and migration helpers. It does not enter the render or input path.

## Revisit this decision if

- Qt's licensing terms change in a way incompatible with AGPL-3.0-or-later distribution.
- Upstream substantially changes the `ocgcore` boundary such that the C-ABI advantage
  disappears.
- Qt Quick proves unable to hit frame targets for the duel field on low-end hardware
  including the Steam Deck. This is the most plausible failure mode and should be measured
  during the first duel-field prototype, not assumed either way.

<div align="center">

<img src="docs/assets/hero.svg" alt="edopro-next — a modern client architecture for EDOPro" width="100%">

<br>

**Preserve the engine. Expose clean semantics. Modernise the client.**

<br>

[![CI](https://github.com/cntrl-alt-lenny/edopro-next/actions/workflows/edopro-next.yml/badge.svg)](https://github.com/cntrl-alt-lenny/edopro-next/actions/workflows/edopro-next.yml)
[![Status](https://img.shields.io/badge/status-early%20development-C08A3E)](docs/ROADMAP.md)
[![Milestone](https://img.shields.io/badge/milestone-M2%20semantic%20client%20model-C9A227)](docs/ROADMAP.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](docs/adr/0001-ui-runtime-stack.md)
[![Qt 6](https://img.shields.io/badge/Qt-6.8%20LTS%20%2F%20QML-41CD52)](ui/)
[![License](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue)](LICENSE)

</div>

---

> [!IMPORTANT]
> **Early development. This is not yet a playable replacement for EDOPro.**
>
> There is no duel field, no deck builder, and no way to play a game. What exists is an
> architecture, a verified build baseline, a regression harness, and one compiled UI shell.
>
> **If you want to duel today, use [EDOPro](https://github.com/edo9300/edopro).** This
> project would not exist without it.

## What this changes, and what it does not

EDOPro automates one of the most rules-dense card games ever designed, across the entire
card pool, maintained by volunteers over years. **That part already works, and this project
does not touch it.**

```mermaid
flowchart LR
    CS["Project Ignis<br/>CardScripts (Lua)"]:::keep
    OC["ocgcore<br/>rules engine"]:::keep
    SM["semantic<br/>client model"]:::build
    UI["Qt 6 / QML<br/>presentation"]:::build

    CS --> OC --> SM --> UI

    classDef keep fill:#1E2128,stroke:#3A404C,color:#9AA0AA
    classDef build fill:#3A3218,stroke:#C9A227,color:#C9A227
```

<div align="center"><sub>Grey is preserved upstream work. Gold is what this project builds.</sub></div>

<br>

What has aged is the layer around the engine. The client descends from YGOPro:
fixed-coordinate widgets on a patched Irrlicht 1.8, sized by manual DPI scaling rather
than a layout system, with no accessibility layer, no declarative focus model and no
animation framework. Those are consequences of a toolkit choice made long ago, not
failures of effort.

## The shell today

A real screenshot of the compiled application — not a mockup.

<div align="center">
<img src="docs/assets/shell.png" alt="The edopro-next Qt/QML shell: navigation rail, home screen with honest project status, and live build metadata" width="88%">
</div>

<div align="center"><sub>Captured from the running binary via <code>--capture</code>. Every status shown is real, and four of five subsystems say <em>planned</em>.</sub></div>

## Why edopro-next?

<table>
<tr>
<td width="33%" valign="top">

### Preserve

`ocgcore` and Project Ignis's Lua CardScripts stay **authoritative and untouched**. The
engine boundary is already a clean C ABI over a byte protocol, so a new client needs no
engine changes at all.

</td>
<td width="33%" valign="top">

### Separate

Upstream fuses protocol decode, state, animation and widgets in one 2,977-line function,
and stores render transforms in the same struct as a card's attack value. **Semantic state
gets its own layer.**

</td>
<td width="33%" valign="top">

### Modernise

Qt 6 / QML: responsive layouts instead of fixed coordinates, a real animation framework,
declarative and testable focus, native accessibility, and high-DPI handled by the toolkit.

</td>
</tr>
</table>

## What exists today

Nothing here is rounded up.

| | Status | |
|---|---|---|
| Upstream baseline builds and runs | ✅ **Working** | `EDOPro version 41.0.2` — [record](docs/BASELINE.md) |
| Architecture survey of upstream | ✅ **Working** | read from source — [survey](docs/architecture/current-edopro.md) |
| UI stack decision | ✅ **Decided** | Qt 6 / QML — [ADR 0001](docs/adr/0001-ui-runtime-stack.md) |
| Qt 6 / QML shell | ✅ **Working** | compiles, runs, zero QML errors |
| Design token system | ✅ **Working** | [`Theme.qml`](ui/qml/Theme.qml) |
| Recorded-protocol regression baseline | ✅ **Working** | golden traces over real duels — [how](docs/architecture/replay-regression.md) |
| Linux CI | ✅ **Working** | harness, Qt shell, upstream baseline |
| Engine re-simulation regression | ⬜ Not started | the layer that would prove *live* duel behaviour |
| Semantic duel model | 🔶 **In progress** | 34 of ~90 messages decoded, including bounded query-stream patches and fixture message closure; no renderer involved — [design](docs/architecture/semantic-model.md) |
| Legacy/model equivalence check | 🔶 **In progress** | opt-in live observer and real-state projection; fixture execution remains open — [scope](docs/architecture/live-semantic-observer.md) |
| Deck builder | ⬜ Not started | first screen to migrate |
| Duel field | ⬜ Not started | deliberately last |
| Windows / macOS builds | ⬜ Not attempted | Linux only so far |

## Making change provable

The property this project ultimately needs to be able to assert is *"this changed
presentation, not duel behaviour"*. Reaching it takes two layers. **The first one is
built.**

> **M1 establishes a deterministic recorded-protocol regression baseline.**

Real recorded duels are parsed into a deterministic text trace and compared against golden
files, so any change in how this project reads the duel protocol produces a readable diff
naming the exact message:

```diff
   62 MSG_SPSUMMONING              4
   63 MSG_SPSUMMONED               1
-  70 MSG_CHAINING                 7
+  70 MSG_CHAINING                 8
   71 MSG_CHAINED                  8
```

**What that does prove:** the replay parser, the message-id table and the normalisation
stay consistent, deterministically and on every supported Python — so the semantic decoding
work in M2 can be tested against real duels from its first commit.

**What it does not prove:** that a *live* duel still behaves the same. The fixtures are
frozen recordings and the harness never loads `ocgcore`, so no C++ change can fail this
suite. Proving live engine equivalence needs **Level 2** — re-simulating the recorded
inputs through a headless `ocgcore` and comparing against the recorded output. The fixtures
already carry every input that needs (seed, decks, ordered responses); what is missing is
an engine host, a pinned card database and pinned CardScripts. That is
[a milestone of its own](docs/architecture/replay-regression.md#5-the-chosen-mechanism-and-why),
not a footnote to this one.

The corpus is three sanitised real duels — no player identity, no card artwork, ~5 KB each
— covering turn progression, normal/special/set summons, **chains**, targeting, card
movement, battle and damage steps, draws, reveals and a terminal win, in both the recorded
output format and the input format. The harness is Python standard library only and runs
headlessly in under a second. [The investigation](docs/architecture/replay-regression.md)
sets out the full boundary.

### A second trace, one layer up

M1's trace is *structural*: message ids and payload digests, with no attempt to interpret
the bytes. M2 adds a *semantic* trace over the same fixtures, produced by the new decoder,
so a change in what the protocol is understood to **mean** also produces a readable diff:

```diff
   259 ChainLinkAdded link=1 instance=37 code=504700123 from=SZONE[p0:2]
-  299 CardMoved instance=37 SZONE[p0:2] -> GRAVE[p0:1] reason=RULE
+  299 CardMoved instance=37 SZONE[p0:2] -> GRAVE[p0:0] reason=RULE
   301 ChainEnded links=1
```

Both traces are kept. A container-parsing change moves the first; a decoder change moves
the second. Alongside the golden comparison, the semantic suite asserts directly that no
packet in either fixture is malformed, unknown or inconsistent, and that the model's
integrity invariants hold at the end of the duel — claims a golden file alone could not
make, because a golden blesses whatever it is shown.

## Architecture

```
gframe/          legacy Irrlicht client        upstream — touch minimally
ocgcore/         rules engine (submodule)      upstream — do not touch
client/          semantic duel model           ours — no Qt, no Irrlicht
ui/              Qt 6 / QML presentation       ours
tools/           Python tooling and harness    ours
tests/           fixtures and golden traces    ours
docs/            architecture, ADRs, roadmap   ours
```

New code lives in new directories, specifically so upstream merges stay tractable.

**Read next:** [architecture survey](docs/architecture/current-edopro.md) ·
[ADR 0001: UI stack](docs/adr/0001-ui-runtime-stack.md) ·
[semantic model](docs/architecture/semantic-model.md) ·
[ADR 0002: identity and events](docs/adr/0002-semantic-event-model.md) ·
[replay regression](docs/architecture/replay-regression.md) ·
[upstream policy](docs/UPSTREAM.md)

## Design principles

> **The cards are the product.**
> Chrome exists to present artwork and communicate rules state, then get out of the way.

> **The rules engine must not become the UI. The UI must not implement game rules.**
> UI code never decides legality, never computes what is targetable, never infers rules.

> **Motion communicates state change. It is never decoration.**
> And it must never obscure what is legal.

The full system — colour, spacing, typography, motion, focus — is in
[docs/DESIGN_SYSTEM.md](docs/DESIGN_SYSTEM.md).

## Roadmap

| | Milestone | |
|:--|:--|:--|
| **M0** | Foundation — architecture, baseline, ADR, shell | ✅ done |
| **M1** | **Make change provable** — regression harness, CI | 🔶 in progress |
| **M2** | **Semantic client model** — presentation-free duel state | 🔶 in progress |
| **M3** | Deck and card data — the first screen to migrate | ⬜ |
| **M4** | Low-risk screens — settings, replays, lobby | ⬜ |
| **M5** | Duel field — last, and highest risk | ⬜ |
| **M6** | Platform and input — Windows, macOS, controller, Steam Deck | ⬜ |

Sequenced so the repository is always in a usable state. Full detail, including what is
explicitly out of scope, in [docs/ROADMAP.md](docs/ROADMAP.md).

## Build

**Qt shell** — needs Qt 6.5+ (developed against 6.8.3 LTS):

```bash
cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ui/build
./ui/build/edopro_next_shell
```

**Semantic client model** — C++20 and nothing else. No Qt, no Irrlicht, no `ocgcore`:

```bash
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build client/build
ctest --test-dir client/build --output-on-failure
```

**Regression harness** — Python 3.10+, no dependencies. The semantic golden tests need the
command above to have been run; without it they skip:

```bash
python -m unittest discover -s tests -v
```

**Upstream client** — see [docs/BASELINE.md](docs/BASELINE.md) for the full record,
environment and the gotchas that cost real time.

## Credit

Everything that makes this project worth attempting was built by
**[Project Ignis](https://github.com/ProjectIgnis)** and
**[edo9300](https://github.com/edo9300)** — the duel engine, the card scripts, the
databases, and the years of correctness work behind them.

This project modernises a presentation layer. It does not reinvent the game.

Upstream's own README is preserved at [docs/upstream-README.md](docs/upstream-README.md);
the fork basis and merge policy are in [docs/UPSTREAM.md](docs/UPSTREAM.md).

## Licence

EDOPro is free software under the **GNU AGPL v3 or later**, and so is this fork. Upstream
copyright notices, [`LICENSE`](LICENSE), [`COPYING`](COPYING) and `notices/` are preserved
unchanged, and inherited code is not relicensed.

Card scripts, card databases and card artwork are **not** in this repository. They belong
to Project Ignis and other rights holders, and are fetched at runtime by the client.

**Yu-Gi-Oh! is a trademark of Shueisha and Konami.** This project is not affiliated with or
endorsed by Project Ignis, Konami or Shueisha.

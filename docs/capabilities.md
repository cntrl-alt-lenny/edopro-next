# What exists today

The detail behind the landing page's generated status block. **[`ROADMAP.md`](ROADMAP.md)
is the authority for status**; the landing page derives from it by
`tools/generate_readme_status.py`. This table is a hand-written, finer-grained snapshot,
last checked 2026-09-21, and nothing generates or checks it. If it disagrees with the
roadmap, the roadmap wins and this page is stale.

Nothing here is rounded up.

| | Status | |
|---|---|---|
| Upstream baseline builds and runs | ✅ **Working** | `EDOPro version 41.0.2` — [record](BASELINE.md) |
| Architecture survey of upstream | ✅ **Working** | read from source — [survey](architecture/current-edopro.md) |
| UI stack decision | ✅ **Decided** | Qt 6 / QML — [ADR 0001](adr/0001-ui-runtime-stack.md) |
| Qt 6 / QML shell | ✅ **Working** | compiles, runs, zero QML errors |
| Design token system | ✅ **Working** | [`Theme.qml`](../ui/qml/Theme.qml) |
| Recorded-protocol regression baseline | ✅ **Working** | golden traces over real duels — [how](architecture/replay-regression.md) |
| Linux CI | ✅ **Working** | regression harness, semantic client model, card/deck data and `policy/`, Qt shell, and the upstream baseline — [workflow](../.github/workflows/edopro-next.yml). That workflow runs on Linux only; upstream's cross-platform `edopro.yml` is disabled on this repository (`gh workflow list --all`) |
| Engine re-simulation regression | ⬜ Not started | the layer that would prove *live* duel behaviour |
| Semantic duel model | ✅ **Working** | 34 of ~90 messages decoded, including bounded query-stream patches and fixture message closure; no renderer involved — [design](architecture/semantic-model.md) |
| Legacy/model equivalence check | ✅ **Working** | reviewed, scoped structural equivalence over both committed YRPX fixtures — LP, turn, and structural card/material topology only; no card code/position, no `ocgcore` — [design](architecture/fixture-equivalence.md) |
| Card database facade | ✅ **Working** | reads Project Ignis `.cdb` files into a presentation-independent record; no legality — [design](architecture/card-database.md) |
| Deck model / `.ydk` codec | ✅ **Working** | presentation-independent deck value type, reads and writes Project Ignis's `.ydk` format; no legality of its own — driven by the deck builder core below via a Qt adapter, never reimplemented — [design](architecture/deck-model.md) |
| Fast card search | ✅ **Working** | structured, presentation-independent search over the full loaded card pool; measured comfortably fast with a plain linear scan, no legality — driven by the deck builder core below via a Qt adapter, never reimplemented — [design](architecture/card-search.md) |
| Deck legality / LFList policy | ✅ **Working** | `policy/`: presentation-independent validation reproducing upstream's source order and quirks; the deck builder below calls it through its Qt adapter to show advisory legality, never reimplementing it — [design](architecture/deck-legality.md), [ADR 0007](adr/0007-deck-legality-policy-module.md), [ADR 0010](adr/0010-deck-builder-ruleset-and-legality-ui.md) |
| Real upstream `.ydk` interoperability proof | ✅ **Working** | a `.ydk` from this project's own `save_ydk()` loads through the real, preserved `DeckManager::LoadDeckFromFile()`, against synthetic committed-safe data; format/loader level only, not upstream's GUI or file-picker path — [design](architecture/ydk-interoperability.md) |
| Deck builder UI | 🔶 **Core working** | search; adding a card puts it in Main or Extra by upstream's Extra Deck rule (computed by `policy/`, the same rule validation uses), or in Side on request; `.ydk` open/save, following the file's own sections; and advisory, non-blocking legality against a visible ruleset and banlist (computed by `policy/`), over a tested Qt adapter; no artwork, no structured search filters, no full keyboard/controller parity — [design](architecture/deck-builder-ui.md), [placement](architecture/deck-placement.md), [ADR 0010](adr/0010-deck-builder-ruleset-and-legality-ui.md), [ADR 0011](adr/0011-extra-deck-classification.md) |
| Duel field | ⬜ Not started | deliberately last |
| Windows / macOS builds | 🔶 **Built locally, not in CI** | Windows 11 / MSVC: all four modules (`client/`, `data/`, `policy/`, `ui/`) configure, build under `-DEDOPRO_NEXT_WERROR=ON` and pass CTest — [brief 005](briefs/archive/005-2026-08-31-windows-msvc-build.md). macOS / Apple clang: `client/`, `data/` and `policy/` build and their CTest suites pass; `ui/` on macOS has no recorded evidence, because the machine had no Qt — [brief 007](briefs/archive/007-2026-09-01-apple-clang-build.md), [`state-history.md`](state-history.md) ("Local toolchain, as last exercised"). Neither platform is in the active CI, which is Linux-only; the roadmap records the local builds as done and Windows/macOS CI as not started, which makes M6 in progress |

## The shell

A real screenshot of the compiled application — not a mockup.

<div align="center">
<img src="assets/shell.png" alt="The edopro-next Qt/QML shell's home screen as captured at the build named below: navigation rail, one status row per roadmap milestone, and live build metadata" width="88%">
</div>

<div align="center"><sub>Captured from the running binary (build <code>6e74e5fc</code>, macOS 27.0 arm64, Qt 6.11.1, offscreen platform, 1280&times;960 window) with <code>QT_QPA_PLATFORM=offscreen edopro_next_shell --capture shell.png --capture-width 1280 --capture-height 960</code>. The window is taller than the shell's 1280&times;800 default so that the whole home screen, build metadata and licence notice included, is in frame.</sub></div>

The home screen shows one status row per roadmap milestone, in the roadmap's own
vocabulary (`done`, `in progress`, `not started`). The words are typed into
[`ui/qml/screens/HomeScreen.qml`](../ui/qml/screens/HomeScreen.qml), because the compiled
shell cannot read `docs/` at run time, so they are checked instead:
[`tools/check_home_status.py`](../tools/check_home_status.py), run by the Python test suite
in CI, fails when a row's milestone, title or status disagrees with
[`ROADMAP.md`](ROADMAP.md), when a milestone has no row, or when a row's free text states
a status of its own.

That check covers the QML, not the image. The screenshot is a snapshot of the screen at
the build named above and is not regenerated when the roadmap moves, so if it ever
disagrees with the roadmap, the roadmap wins and the image is stale.

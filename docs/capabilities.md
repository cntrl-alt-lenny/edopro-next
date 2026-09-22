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
| Deck legality / LFList policy | ✅ **Working** (foundation only) | `policy/`: presentation-independent validation reproducing upstream's source order and quirks; nothing in `ui/` calls it, so the QML deck builder still shows no legality — [design](architecture/deck-legality.md), [ADR 0007](adr/0007-deck-legality-policy-module.md) |
| Real upstream `.ydk` interoperability proof | ✅ **Working** | a `.ydk` from this project's own `save_ydk()` loads through the real, preserved `DeckManager::LoadDeckFromFile()`, against synthetic committed-safe data; format/loader level only, not upstream's GUI or file-picker path — [design](architecture/ydk-interoperability.md) |
| Deck builder UI | 🔶 **Core working** | search, explicit Main/Extra/Side editing, `.ydk` open/save, over a tested Qt adapter; no legality, no artwork, no full keyboard/controller parity — [design](architecture/deck-builder-ui.md) |
| Duel field | ⬜ Not started | deliberately last |
| Windows / macOS builds | 🔶 **Built locally, not in CI** | Windows 11 / MSVC: all four modules (`client/`, `data/`, `policy/`, `ui/`) configure, build under `-DEDOPRO_NEXT_WERROR=ON` and pass CTest — [brief 005](briefs/archive/005-2026-08-31-windows-msvc-build.md). macOS / Apple clang: `client/`, `data/` and `policy/` build and pass; `ui/` could not be configured there because the machine has no Qt — [brief 007](briefs/archive/007-2026-09-01-apple-clang-build.md), [`state.md`](state.md) ("Local toolchain"). Neither is in the active CI (Linux-only), and the roadmap still lists platform builds and CI under M6 as not started |

## The shell, as first captured

A real screenshot of the compiled application — not a mockup.

<div align="center">
<img src="assets/shell.png" alt="The edopro-next Qt/QML shell: navigation rail, home screen with honest project status, and live build metadata" width="88%">
</div>

<div align="center"><sub>Captured from the running binary via <code>--capture</code> (build <code>94b15108</code>, 2026-08-24). Two of the five status rows say <em>working</em> and three say <em>planned</em>.</sub></div>

Two caveats. The screenshot's caption on the old landing page said "four of five
subsystems say planned"; the image itself shows three. And its status rows are
hard-coded in [`ui/qml/screens/HomeScreen.qml`](../ui/qml/screens/HomeScreen.qml), which
has not changed since the shell was first committed (`git log -- ui/qml/screens/HomeScreen.qml`
lists one commit). Its "Semantic client model: planned" and "Deck builder: planned" rows
therefore no longer match the roadmap, where the semantic client model (M2) is done and a
deck-builder core exists. Treat the image as a record of the first shell, not of current
status.

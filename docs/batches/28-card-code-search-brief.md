# EDOPro Next · batch 28-card-code-search · Builder

Normal: a UI search-adapter feature, then Brain reviews the exact delivery.
No automatic Verifier. Start alongside batch 27 from latest `origin/master`;
neither native navigation nor the planning PR is a prerequisite. Read
`docs/planning/parallel-workers.md` from the dispatch branch if unmerged.

Fetch origin; read current AGENTS.md, framework, Worker role, state and ADR
0005. Create `worker/28-card-code-search` in
`.worktrees/worker-28-card-code-search`, initialise `ocgcore`, run
`python3 tools/fw.py status`; inspect git rather than its known legacy
"EADME" mislabel. Never use retired `fw.py start/report`.

Goal: typing a known decimal card code in the existing search field finds
that exact card, without requiring its digits in the name or description.
This closes one real search gap, not the full legacy sigil grammar.

Brain's integration decision: after trimming outer whitespace, a complete
ASCII decimal input representing a nonzero uint32 code selects exact-code
mode only when that code exists. Leading zeroes are allowed. This mode
returns that card only if it passes every existing structured and advisory
visibility filter. Other input, zero, overflow or an unknown code retains
ordinary text search, ranking and result-cap behaviour. Clearing restores
browsing. Catalogue reload and filter/banlist changes refresh correctly.
Do not reinterpret `SearchQuery::exact_code`: ADR 0005 makes it a ranking
hint, not a restricting lookup.

Own `ui/src/deckbuilder/search_results_model.{h,cpp}`, any new search-input
adapter files, a dedicated `ui/tests/test_card_code_search.cpp`, and narrowly
scoped registration in `ui/CMakeLists.txt`/`ui/tests/CMakeLists.txt`.
Own `docs/architecture/card-code-search.md`, ADR 0014 for the deliberate
input/filter decision, and this batch's summary. Leave QML, application
startup, screen tests, existing adapter tests and batch 27's docs untouched.
Report any necessary boundary expansion to Brain before editing.

Re-read and quote `gframe/deck_con.cpp`'s `FilterCards` and
`gframe/bufferio.h`'s `GetVal`. Establish numeric parsing, missing-code
fallback and filter bypass from source, including unsigned overflow.
Record our trimmed, checked conversion and retained-filter behaviour as
deliberate upstream differences. Keep `data/`, `policy/`, engine, legacy
code, dependencies, settings, framework and shared status pages unchanged.
No complete search-parity or unchanged-duel-behaviour claim.

Evidence: synthetic-catalogue adapter tests that fail before the feature,
cover known/unknown codes, whitespace/leading zeroes, zero/overflow/malformed
input, restrictive filters, unofficial/banlist visibility, clearing and
catalogue reload. UI Debug/WERROR/UI_TESTS configure/build, CTest and strict
workflow offscreen survival/empty-stderr smoke; preserve failures. Run
generators `--check`, README status check, Python unittest discovery, golden
reproduction and unchanged diff, `fw.py check`, `git diff --check`.

Commit focused changes and `docs/batches/28-card-code-search.md` with Done,
Checked, Not checked, Failed or blocked (maximum 500 prose words). Record
source SHA, commands, real output, exits/skips/failures and suggested shared
page updates. Push on every exit. Check PR evidence before opening/updating
a PR; report CI at delivery SHA. Never merge/self-accept; finish with batch,
Builder, outcome and pushed commit.

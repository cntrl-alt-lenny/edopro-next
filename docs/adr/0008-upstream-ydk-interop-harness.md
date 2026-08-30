# ADR 0008 — A real upstream `.ydk` interoperability harness in `integration/legacy/`

## Context

M3's roadmap exit criterion (`docs/ROADMAP.md`) is *"a deck can be built and saved in the
new client, and opened by upstream EDOPro unchanged."* Before this slice, the second half of
that claim was "supported by construction" only: `edopro_next::data::save_ydk()`'s writer
matches `DeckManager::SaveDeck`'s own section syntax and `LoadCardList` accepts it, by
reading both sources side by side ([deck-model.md](../architecture/deck-model.md)) - but no
test anywhere in the repository actually hands this project's own output to the real,
preserved upstream loader. This ADR is that harness's own record of the choices that were
genuinely debatable, not merely "what upstream does" restated -
[ydk-interoperability.md](../architecture/ydk-interoperability.md) is the source-verified
account of the upstream semantics themselves.

A prior planning round considered two M3D3 candidates - UI-visible legality, and this
interoperability proof - and found the first blocked on a policy-source dependency
(`policy/`'s `ValidationPolicy` has no default and no session layer yet supplies one) that
this slice does not share. This ADR does not re-litigate that comparison; it records the
choices made *within* the interoperability-proof slice once it was selected.

## Decision 1 — A second, logical Premake target for `edopro_next_deck`, not a duplicate compile or a cross-build-system dependency

`data/`'s own standalone build (`data/CMakeLists.txt`) is CMake-based. The real `ygoprodll`
integration build this harness needs to link against is Premake-based, and had no Premake
project for `edopro_next_deck` before this slice.

### Options considered

1. **Compile `data/src/ydk.cpp` a second time, directly into `integration/legacy/`'s own
   Premake project** (e.g. listing it in `integration/legacy/premake5.lua`'s `files {}`).
   Rejected: this makes `integration/legacy/` the second place in the repository that
   compiles this file, with no mechanism to keep the two builds' compiler flags,
   preprocessor state, or (eventually) source in sync - exactly the "silently diverging
   local patch" CLAUDE.md's "Authoritative upstream components" section warns against, even
   though `ydk.cpp` is our own code, not upstream's. A future edit to `ydk.cpp` could pass
   `data/`'s own CTest suite while never having been compiled through the path this harness
   actually exercises.
2. **Make the Premake build consume `data/CMakeLists.txt`'s CMake-built static library
   directly** (e.g. shelling out to `cmake --build` from Premake, or linking the resulting
   `.a` file by a hardcoded path). Rejected: couples two independent build systems'
   invocation order and output layout together, in a way neither `client/premake5.lua` nor
   any other existing target in this repository does - a new, unprecedented kind of
   dependency for a comparatively small module.
3. **A new, logical Premake target, `data/premake5.lua`'s `edopro_next_deck` project,
   building the same `src/ydk.cpp` translation unit under Premake's own toolchain
   invocation** (chosen) - mirrors `client/premake5.lua`'s own established pattern and
   comment exactly ("a separate logical target... included only for the observer-enabled
   legacy build; the standalone developer/test build is still owned by
   client/CMakeLists.txt"). `data/CMakeLists.txt` remains the sole owner of the standalone
   developer/test build; this file is a second, logically-equivalent *build description* of
   the identical source file, not a fork of its content, and not a second implementation.

### Consequence

`data/src/ydk.cpp` is now built by two independent toolchain invocations (CMake, for `data/`'s
own CTest suite; Premake, for this harness) from the same unmodified source file. This is
the same shape `client/`'s own `edopro_next_client` target already established for
`client/src/**.cpp` before this ADR - not a new pattern, an extension of an existing one to a
second module. `data/premake5.lua`'s target deliberately compiles only `src/ydk.cpp` (never
`card_database.cpp`, `text_normalize.cpp`, or `card_search_index.cpp`, and links no SQLite),
proving by what it actually builds - not merely documenting - that this harness's dependency
on `data/` stays exactly as narrow as `edopro_next_deck`'s own CMake target already commits
to ([ADR 0004](0004-deck-model-ydk-codec.md)).

## Decision 2 — `LoadCardList`'s upstream visibility is not changed

`LoadCardList` (`gframe/deck_manager.cpp:272`) is declared `static` at file scope, with no
declaration in `deck_manager.h`.

### Options considered

1. **Remove `static` and add a declaration to `deck_manager.h`**, so this harness (or a
   future one) could call it and assert its intermediate `mainlist`/`extralist`/`sidelist`
   output directly, independent of `LoadDeck`'s classification step. Rejected: this is an
   upstream visibility change with no upstream behavioural motivation - `gframe/` is
   "upstream, touch minimally" per CLAUDE.md's own ownership table, and the only reason to
   change it here would be to manufacture a cleaner test seam for this project's own
   harness. That is exactly the kind of local divergence this project's merge policy
   (`docs/UPSTREAM.md`) exists to avoid accumulating.
2. **Reach `LoadCardList`'s grammar only transitively, through the real public entry point,
   `DeckManager::LoadDeckFromFile`** (chosen). This harness observes `LoadDeck`'s *output*
   (the classified `ygo::Deck`), and documents plainly
   ([ydk-interoperability.md](../architecture/ydk-interoperability.md)§2) that
   `LoadCardList` is therefore proven only as a component of that composed behaviour, not in
   isolation. A narrower, honestly-scoped claim was preferred over a broader one bought with
   an upstream source change.

### Consequence

A grammar-level regression in `LoadCardList` that `LoadDeck`'s own classification happened
to mask would not necessarily be caught by this harness. This is a real, accepted scope
limit, not an oversight - recorded explicitly rather than left for a reader to discover by
reading `deck_manager.cpp` themselves.

## Decision 3 — A fresh, minimal synthetic-database builder, not a reuse of `data/tests/synthetic_cdb.h`

Both `data/tests/synthetic_cdb.h` and this harness need the same `datas`/`texts` SQLite
schema shape.

### Options considered

1. **Include `data/tests/synthetic_cdb.h` from `integration/legacy/ydk_interop.cpp`**
   (considered first, per this slice's own task brief, which asked for this option to be
   audited before deciding against it). Technically possible - the header is self-contained,
   uses only `<sqlite3.h>` and the standard library, and neither build system nor CI
   framework is baked into it. Rejected anyway, for an ownership reason rather than a
   technical one: `data/tests/` is scoped to `data/`'s own CMake test targets
   (`test_card_search.cpp`, `bench_card_search.cpp`). `integration/legacy/` is a
   Premake-built, opt-in *production* harness leg (the observer/interop build), not a
   `data/` test - reaching into a sibling module's `tests/` directory from a non-test,
   different-build-system consumer would blur exactly the ownership boundary CLAUDE.md's
   "Where code belongs" table draws, and would leave `data/tests/synthetic_cdb.h` load-bearing
   for a target its own module's `CMakeLists.txt` never builds or tests.
2. **A tiny, integration-local synthetic-database builder inside `ydk_interop.cpp`**
   (chosen). Deliberately smaller than `data/tests/synthetic_cdb.h`: it only ever needs the
   three columns `DeckManager::LoadDeck`'s load path actually reads (`id`, `alias`, `type`),
   leaving every other column fixed at `0`, versus the full generality
   `data/tests/synthetic_cdb.h` offers its own two CMake consumers. A smaller, purpose-built
   implementation used for exactly what it needs was judged clearer than a larger shared one
   used only partially, especially once reused across a build-system boundary its original
   authors never had to consider.

### Consequence

The two schemas are structurally identical by construction (both cite the same upstream
query, `gframe/data_manager.cpp`'s `SELECT_STMT`) but are two independent pieces of code. A
future schema change in `DataManager::ParseDB` would need to be reflected in both places;
this is judged an acceptable, low-frequency cost against the ownership clarity gained.

## Decision 4 — Reuse the existing `EDOPRO_NEXT_SEMANTIC_OBSERVER` build leg, described honestly as unrelated

### Options considered

1. **A new, separate build define** (e.g. `EDOPRO_NEXT_INTEROP_HARNESS`), with its own CI
   matrix dimension. Rejected for this slice: it would double the already-expensive
   "Upstream EDOPro baseline" job's CI cost (`.github/workflows/edopro-next.yml`'s own
   comment already flags that job as "Expensive. Skipped on ordinary feature and
   documentation pushes.") for a harness that needs nothing the existing `observer=true` leg
   does not already build (the same `ygoprodll` binary, the same `EDOPRO_NEXT_
   SEMANTIC_OBSERVER`-gated `integration/legacy/` inclusion).
2. **Reuse `EDOPRO_NEXT_SEMANTIC_OBSERVER`/`--semantic-observer` as the build gate, with
   `ydk_interop.{h,cpp}` and its `gframe/` call sites explicitly commented as sharing this
   leg for CI-cost reasons only - not as a semantic-observer feature** (chosen). The two new
   CLI options (`--verify-ydk-interop`, `--verify-ydk-interop-fault`) are recognized the same
   way the two existing semantic-replay options are: unconditionally in `cli_args.h`'s
   `LAUNCH_PARAM` enum and `edopro_main.cpp`'s `GetOption()`, but only acted on inside the
   same `#if defined(EDOPRO_NEXT_SEMANTIC_OBSERVER)` block in `gframe.cpp` that already
   guards the replay-verifier calls - an ordinary, non-integration build recognizes and
   silently ignores the flags, exactly as it already does for the existing ones, and
   `integration/legacy/`'s object code (where the two `extern "C"` functions actually live)
   is not even compiled into that build at all.

### Consequence

The single opt-in `ygoprodll` binary produced by the `observer=true` CI leg now proves two
conceptually independent things - live legacy/semantic duel-state equivalence (M2's replay
verifier) and this slice's `.ydk` interoperability - for the cost of building it once. Should
either grow expensive or conceptually distinct enough to warrant its own build leg later,
splitting them apart is a mechanical follow-up, not a redesign.

## Decision 5 — Fault injection corrupts one already-observed comparison value, after the real load, never the parser input

Mirrors M2's own precedent exactly (`integration/legacy/replay_verifier.cpp`'s
`--semantic-verify-replay-fault`, which perturbs `dInfo.lp[0]` *after* the real legacy
handler has already run, never the replay bytes themselves).

### Options considered

1. **Feed a deliberately malformed `.ydk` to the real loader.** Rejected: this would prove
   `LoadCardList`'s malformed-input tolerance (already a `LoadCardList`-isolation concern
   this ADR's Decision 2 explicitly declines to claim), not that this harness's own
   comparator is live. It would also make the "same real loading path" claim this fault mode
   is supposed to preserve untrue - a different, malformed input is a different load, not
   the same one deliberately mis-checked.
2. **Deterministically corrupt one already-computed observed string, after
   `LoadDeckFromFile` has returned, before comparing it to the independently-derived
   expected value** (chosen) - `--verify-ydk-interop-fault` appends a fixed, reproducible
   suffix (`",424242"`) to the `separated=false` main-section observation. Two consecutive
   runs therefore produce byte-identical output (same fixed corruption every time), a
   non-zero exit both times, and a specific, greppable mismatch line - proving the
   comparator itself fails closed, exactly as M2's fault-injection CI step already
   establishes for the replay verifier, without touching `DeckManager` at all.

## Status

Accepted. Implemented in `data/premake5.lua`, `integration/legacy/{ydk_interop.h,
ydk_interop.cpp,premake5.lua}`, the top-level `premake5.lua`, three small `gframe/` call
sites (`cli_args.h`, `edopro_main.cpp`, `gframe.cpp`), and
`.github/workflows/edopro-next.yml`'s existing `observer=true` matrix leg. Full design
detail: [ydk-interoperability.md](../architecture/ydk-interoperability.md).

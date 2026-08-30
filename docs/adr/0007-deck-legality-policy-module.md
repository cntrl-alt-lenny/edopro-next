# ADR 0007 — A presentation-independent deck legality/policy module

## Context

M3's remaining unchecked "Deck builder UI" roadmap item (`docs/ROADMAP.md`) lists legality
(deck size, three-copy, banlists) as one of the pieces still missing after M3D1's functional
QML deck-builder core landed. `docs/architecture/card-database.md`, `docs/architecture/
card-search.md` and `docs/architecture/deck-builder-ui.md` all independently and repeatedly
describe legality as belonging in "an explicitly separate, explicitly reviewed layer above
`data/`" - this ADR is that layer's own record of the choices that were genuinely debatable,
not merely "what upstream does" restated (`docs/architecture/deck-legality.md` is the
source-verified account of the upstream semantics themselves).

## Decision 1 — A new sibling module, `policy/`, not a subdirectory of `data/`

### Options considered

1. **Inside `data/`, as a new source file** - the closest physical location to the types this
   module consumes (`CardRecord`, `Deck`). Rejected: `data/`'s own header doc comments
   describe it as "a data boundary, not a rules boundary" in multiple places, and its
   architecture docs explicitly point at legality as a *separate* layer *above* it, not a
   part of it. Growing `data/`'s own CMake target to include validation logic would blur
   that boundary exactly where the existing docs are most insistent it should not blur.
2. **A new top-level sibling module, `policy/`** (chosen) - mirrors `data/`'s own internal
   shape (`include/edopro_next/policy/...`, `src/...`, `tests/...`) and CMake conventions
   (a standalone `project()`, the same `EDOPRO_NEXT_WERROR` option, the same test-harness
   pattern), but is a structurally independent target that depends on `data/` rather than
   extending it. Keeps `data/`'s own identity ("never decides legality") intact by
   construction, not just by comment.
3. **Inside `client/`** - rejected immediately: `client/` is the duel-protocol semantic
   model, and deck construction/legality has nothing to do with an in-progress duel's wire
   protocol - the same reasoning ADR 0004 already used to reject putting the deck model
   there.

### Explicit, doubled dependency: both `edopro_next_data` and `edopro_next_deck`

`Deck` (`data/include/edopro_next/data/deck.h`) is currently header-only - it has no
corresponding `.cpp`, so linking `edopro_next_data` alone would happen to compile today,
purely because both `edopro_next_data` and `edopro_next_deck` expose the same shared
`data/include` tree as a `PUBLIC` include directory. Declaring `edopro_next_legality`'s
dependency on `edopro_next_data` only would hide a real conceptual dependency (this module
validates a `Deck`, which conceptually belongs to the deck-model target, not the
card-database target) behind an accident of the current header/source split.
`policy/CMakeLists.txt` links both explicitly, so the declared dependency graph stays
correct even if `Deck` ever grows a translation unit of its own.

## Decision 2 — `validate_deck()` returns the single first error, in upstream's own order; no "collect everything" mode in this slice

### Options considered

1. **First-error only** (chosen) - `gframe/deck_manager.cpp`'s own `CheckDeckContent`/
   `CheckCards` are themselves first-error functions: each returns the instant it finds a
   problem, in a fixed, source-verified order (`docs/architecture/deck-legality.md`§2).
   Reproducing exactly that order and stopping condition is what lets this module honestly
   claim "this matches upstream's own precedence" - a claim a richer API could not make
   without inventing an ordering upstream itself does not have.
2. **A second, "collect every finding" entry point alongside the first** - considered and
   explicitly deferred, not rejected outright: a future deck-builder UI showing every
   problem in a deck at once (not just the first) is a reasonable thing to eventually want.
   Deferred because it has no upstream source to be faithful *to* - upstream never collects
   multiple deck errors at once anywhere in `gframe/` - so its semantics (does it stop at
   the first per-card failure or continue past it? does it de-duplicate the same error type
   across cards?) would be a genuinely new design this module would be inventing, not
   reproducing. Building it in the same PR as the source-faithful reproduction above would
   blur which parts of this module are "verified against upstream" and which are "this
   project's own new design" - kept as two separable concerns, only the first of which this
   slice implements.
3. **A separate, looser "interactive builder" copy-limit query** (matching
   `DeckBuilder::push_main`/`check_limit`'s own, different, preventative rule set - see
   `docs/architecture/deck-legality.md`§2's distinction between the interactive builder and
   final validation) - also deferred for the same reason as option 2: it is a real, distinct
   concept, but adding it here would double this PR's scope beyond the one reviewed
   boundary (final, source-ordered validation) this slice exists to establish.

## Decision 3 — `ValidationPolicy` has no default constructor

### Options considered

1. **A default constructor producing "standard OCG/TCG, 40-60/0-15/0-15, no forbidden
   types"** - the single most common real ruleset, and the path of least resistance for any
   caller. Rejected: upstream itself has no such default baked into `CheckDeckContent`/
   `CheckDeckSize` - every one of these values comes from live host/session state
   (`gframe/network.h`'s `HostInfo`), which this project's UI does not have at this slice
   (no networking/hosting screen exists yet). Baking in a default here would make this
   module the *author* of a specific ruleset rather than a faithful evaluator of
   caller-supplied ones - exactly the kind of "invent the rules of Yu-Gi-Oh" this module
   exists to avoid, per CLAUDE.md's "the UI must not implement game rules" extended to this
   layer too.
2. **No default; every field explicit** (chosen) - `ValidationPolicy` (`validation_policy.h`)
   has no default constructor and no factory function producing "the" default policy. A
   caller - today, only this module's own tests - must choose every field. A future
   UI/session layer may define named convenience presets once it actually has a ruleset
   selection concept to attach them to; that is explicitly out of scope here.

## Decision 4 — Ritual placement is a boolean, not the loader's three-state `RITUAL_LOCATION`

Upstream's `RITUAL_LOCATION::{DEFAULT, MAIN, EXTRA}` (`gframe/deck.h`) belongs to
`LoadDeck`'s own *classification* step - deciding which section a freshly-loaded card lands
in, with `DEFAULT` meaning "Rush-conditional" (`docs/architecture/deck-legality.md`§7). This
module does not classify a `Deck`; it validates one whose section split already exists.
`CheckDeckContent` itself only ever receives a resolved **boolean**
(`rituals_in_extra`, `deck_manager.cpp:204`), derived elsewhere from a duel-rule flag - so
`ValidationPolicy::rituals_belong_in_extra` models that boolean, not the three-state loader
enum. Modelling the loader's own enum here would imply this module performs (or half-
performs) classification, which §0 of `docs/architecture/deck-legality.md` explicitly
disclaims.

## Decision 5 — The LFList hash's undefined-behavior domain is rejected, not reproduced or "fixed"

`gframe/deck_manager.cpp:80`'s hash expression computes two shift amounts directly from a
file-supplied, otherwise-unbounded integer (`27 + count`, `5 - count`). Outside
`count in [-26, 4]`, at least one of those shift amounts falls outside `[0, 31]`, which C++
defines as undefined behavior for any integer type, `[expr.shift]` - not "wraps", not "masks
to 5 bits" on any real hardware, genuinely undefined regardless of what a given compiler
happens to emit for it today.

### Options considered

1. **Reproduce the expression as-is, letting undefined behavior occur if a hand-edited file
   ever supplies an extreme count** - rejected outright: writing new C++ code that
   deliberately invokes undefined behavior is not something this project does, regardless of
   whether upstream's own compiled binary "happens to work" for a given toolchain.
2. **Invent a well-defined replacement formula for the out-of-domain case** (e.g. clamping
   the shift amount, or using a rotate-by-modulo-32 idiom) and call the result "the hash" -
   rejected: there is no such thing as "the correct hash" for a domain upstream's own source
   never defines. Inventing one and presenting it as source-equivalent would be a false
   claim, not a divergence honestly labeled as one.
3. **Fail closed: reject the line entirely for both `content` and `hash`** (chosen) - a count
   outside `[-26, 4]`'s content-line is treated exactly like a syntactically malformed line
   (`docs/architecture/deck-legality.md`§10). No hash value is manufactured for a domain
   upstream itself has none for; the rest of the file still parses normally.

## Decision 6 — `policy/` never touches `gframe/`, `ocgcore/`, or Qt

Every fact this module needs (deck-size bounds, allowed-card scope, forbidden types, the
ritual-placement boolean, the LFList to validate against) is modelled as plain,
caller-supplied data (`ValidationPolicy`, `LfList`) - never fetched by reading `gframe/`
global state (`gDataManager`, `gdeckManager`, `HostInfo`) directly, and never requiring
`ocgcore` or Qt to build or run. `policy/CMakeLists.txt` proves this the same way
`data/CMakeLists.txt` and `client/CMakeLists.txt` do: by what the target actually links, not
merely by a comment claiming it.

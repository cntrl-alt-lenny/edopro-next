# ADR 0002 — Card identity and the semantic event model

- **Status:** Accepted
- **Date:** 2026-08-25
- **Context commit:** upstream `54ea755a`
- **Supersedes:** nothing. Complements [ADR 0001](0001-ui-runtime-stack.md).

## Context

M2 introduces `client/`: the first presentation-free semantic model of EDOPro's duel
protocol, designed so a renderer can eventually read it without being a renderer. Every
screen after it — most of all the duel field in M5 — depends on the shape chosen here, so
the decisions worth arguing about are worth writing down.

Four of them turned out to be load-bearing:

1. how a card instance is identified;
2. whether persistent state and discrete events are the same thing;
3. what happens when the decoder cannot understand a packet;
4. what it means for a refused packet to leave state unchanged, and how that is enforced.

The source research these rest on is in
[semantic-model.md](../architecture/semantic-model.md).

---

## Decision 1 — Client-allocated `CardInstanceId`

**The protocol offers no stable identifier for a card, so the client allocates one.**

### What upstream actually does

A duel message locates a card with `loc_info`: controller, location bits, sequence,
position (`CoreUtils::ReadLocInfo`, `gframe/core_utils.cpp:281`). That is a coordinate.
The accompanying `code` field is the passcode, which is shared by every copy of a card and
is **0 whenever the recipient is not entitled to know it**.

The legacy client therefore identifies a card by the identity of its `ClientCard` heap
object, moving the same pointer between `ClientField`'s zone vectors as `MSG_MOVE` arrives
(`gframe/duelclient.cpp:3045`). Identity is real, but it is implicit, non-serialisable, and
inseparable from twenty fields of renderer state.

### Options considered

**A. Use the passcode.** Rejected outright. Three copies of one card share a passcode, and
an unknown card has none. This is not a close call; it is recorded because it is the
assumption someone will otherwise make.

**B. Use `(controller, zone, sequence)` as the key.** This is what the wire format does, and
it is what a naive port would adopt. Rejected: it changes on every move, and every pile
renumbers when a card is removed, so a UI holding such a key would silently start pointing
at a different card. A duel field animating a move needs a handle that survives the move.

**C. Client-allocated monotonic id.** *(chosen)* `enum class CardInstanceId : uint32_t`,
allocated by `DuelState` from 1, never reused within a duel. It is the legacy pointer
identity made explicit: the same lifetime, but serialisable, comparable, printable in a
golden trace, and free of any renderer coupling.

**D. Ask upstream to add an id to the protocol.** Not pursued. It would be a protocol
break for every client and server, to solve a problem that is entirely local to this one.

### Consequences

- `CardInstanceId` and `CardCode` are **both** strongly typed enums, precisely so the two
  32-bit numbers cannot be confused. `CardCode::None` is a normal, permanent state.
- Instance ids appear in golden traces, so an identity-tracking regression shows up as a
  legible diff rather than a mystery.
- `DuelState` keeps a card record after it leaves play, marked `tracked = false`, instead of
  deleting it as upstream does. Earlier events stay meaningful, and a stale reference is
  reported as an inconsistency rather than dereferenced.
- **The honest limit.** After `MSG_SHUFFLE_DECK`, upstream does not permute its deck vector
  — it cannot, because it is not told the permutation; it merely forgets every code. Our
  ids therefore survive a shuffle while the physical cards behind them do not. The id
  continues to denote "the card at that position in the client's model", which is all any
  client can know. Nothing downstream may assume more.

---

## Decision 2 — State and events are separate, and events are a `std::variant`

**`DuelState` answers "what is true now". `DuelEvent` answers "what just happened". Both
are needed, and neither is derivable from the other.**

A duel field wants the authoritative position of every card *and* the knowledge that a
particular card just moved from hand to a monster zone, so that it can animate that and not
the twelve other things that also changed. Deriving the second by diffing successive
snapshots would be both expensive and lossy — a diff cannot tell a discard apart from a
tribute.

### Why a variant rather than a class hierarchy

The event set is closed and small. A `std::variant` gives exhaustive handling that the
compiler checks, value semantics, no allocation, and no ownership question. A base class
with virtuals would give none of that in exchange for indirection. There is no requirement
here for open extension by third parties.

### The rule that keeps this honest

**Events carry facts, never animation instructions.** `CardMoved` says a card went from
`HAND[p0:2]` to `MZONE[p0:1]`. It does not say *slide it over twelve frames* — that is a
presentation decision derived from the fact, and putting it in the event would smuggle the
renderer back into the model. The legacy handlers interleave the two so thoroughly that
`MSG_MOVE` alone contains six distinct animation branches.

Two related choices follow from it:

- `CardMoved` reports the **resolved** destination, not the requested one. Piles renumber on
  insertion (`ClientField::AddCard`), so a UI told the requested sequence would animate to
  the wrong slot.
- `LifePointsChanged` carries `from`, `to` **and** the stated `amount`. Life points clamp at
  zero, so lethal damage states more than it removes. Both numbers are true and a UI wants
  each.

---

## Decision 3 — Four distinct refusals, not one error

**`UnknownMessage`, `UnsupportedMessage`, `Malformed` and `Inconsistent` are different
things and are reported as such.**

The initial M2 slice decoded 27 of upstream's ~90 messages. The query-stream slice adds
two state-synchronization messages. If everything the decoder declined were reported
as an error, the coverage report would be unreadable; if everything were reported as
"unsupported", a genuine layout bug would hide inside the noise. So:

| Status | Means | Who is at fault |
|---|---|---|
| `UnknownMessage` | id not in the generated table | the stream, or a stale `protocol_constants.h` |
| `UnsupportedMessage` | a real MSG_* this slice does not read | nobody; it is scope |
| `Malformed` | a message we claim to read, whose payload does not fit its layout | **us** |
| `Inconsistent` | parsed cleanly, refers to something impossible in the current state | us, or an earlier packet |

Each handler reads every field and asserts the reader is *exactly* exhausted before
producing anything. Trailing bytes are as much a failure as missing ones: a payload longer
than our layout means the layout is wrong, and passing it silently would be the worst
outcome available. (An earlier draft of this decision additionally claimed that handlers
mutate state only after every check passes, and that this discipline is what keeps a
refused packet from leaving state half-changed. That was false, and stayed false through
several handlers for as long as the guarantee depended on getting each one's internal
ordering right by hand. Decision 6 replaces it with a mechanism that cannot make that
mistake.)

The golden test asserts zero `Malformed`, zero `UnknownMessage` and zero `Inconsistent`
across both fixtures, and the blessing path refuses to write a golden that violates that.
Without the distinction, that assertion could not be written.

---

## Decision 4 — No test framework, and no dependencies at all

`client/` exists to have none. Adding GoogleTest to prove that would be self-defeating for
the sake of assertions that fit in one header, so `client/tests/test_support.h` is a
90-line registry-and-macros harness driven by CTest.

Equally, `client/` does not link the vendored LZMA in `gframe/lzma`, and the semantic trace
tool does not parse `.yrpX` containers. Container parsing is M1's job and already exists in
`tools/replaytrace`; the decoder's real input — from a socket, from `ocgcore`, or from a
replay — is a stream of duel messages, so that is the boundary the tool exposes. The Python
harness extracts the stream and pipes it in.

The cost is honest: the semantic golden tests need both a built `client/` and Python, and
they skip if the binary is absent. CI passes `--require` so a skip there is a failure.

---

## Decision 5 — Constants are generated, never copied

`client/include/edopro_next/client/protocol_constants.h` is generated from
`gframe/ocgapi_constants.h` and `gframe/common.h` by
`tools/generate_protocol_constants.py`, and CI fails on drift — the same pattern M1
established for the message-id table.

A constant that is hand-copied is a constant that silently diverges from the engine, and
this file holds 151 of them. The generator resolves literals and simple `|` compositions
only; anything else it skips and reports, rather than guessing at upstream arithmetic.

Direct `#include` of the upstream header was considered and rejected: it would put a
`gframe/` path on the include path of a library whose entire premise is not depending on
`gframe/`.

---

## Decision 6 — Decoding is transactional: a private copy, committed once

**`ProtocolDecoder::decode()` runs every handler against a copy of the caller's state and
assigns it back only when the result is `Decoded`. A refused packet therefore leaves state
unchanged regardless of how far the handler got before it found the reason to refuse.**

### The defect

A pre-merge review of this milestone found the claim in Decision 3's original wording — a
refused packet never leaves state half-changed — to be false, and asked whether it held.
Verified against source before anything was changed, per this project's own standard for
that kind of claim: it did not. `Decoding` held a direct reference to the caller's real
`DuelState`, so every `d.state().set_code(...)`/`move_card(...)`/`set_combat_stats(...)`
call inside a handler took effect immediately, whether or not that handler went on to
refuse the packet a few lines later. Four handlers actually did this, not the two the
review named:

- `MSG_MOVE`, ordinary move — a non-zero code is applied via `apply_code()` before
  `move_card()` is attempted; `move_card()` can still refuse (destination occupied,
  out of range, wrong player).
- `MSG_MOVE`, leaving play — the same `apply_code()` before `remove_card()`, which can
  refuse independently.
- `MSG_POS_CHANGE` — found during this audit, not named in the review: a non-zero code is
  applied before `set_position()`, which refuses a face-up/face-down flip inside the extra
  deck.
- `MSG_BATTLE` — the attacker's ATK/DEF is written before the defender is even looked up,
  and the defender lookup can fail.

### Options considered

**A. Reorder each handler so every check runs before any mutation.** Rejected as the
primary fix, though each handler above already reads its fields and checks the reader's
exhaustion first. Reordering is a per-handler discipline: it has to be re-applied
correctly by whoever writes the 63rd handler in some future slice, and getting it wrong
once is exactly how the four instances above arose in the first place from a design that
looked, at a glance, like it already had this property.

**B. Give `DuelState` its own undo log.** Rejected as unwarranted complexity for a model
this size, and it multiplies the surface a future change has to keep correct — every new
mutating method would need to also record how to undo itself.

**C. Decode against a private copy of the state; commit it back only on success.**
*(chosen)* `DuelState` was already fully value-typed and copy-assignable — verified by
enumerating its private members, none of which is a pointer, a reference, or anything else
that would make copying unsafe or partial; `DuelState::start()` already relies on
copy-assignment internally (`*this = DuelState{};`). `ProtocolDecoder::decode()` now reads:

```cpp
DuelState trial = state;
Decoding decoding(packet, trial, variant_);
decode_supported(packet.message, decoding);
// ... every check against `decoding` ...
state = std::move(trial);   // only reached once every check has passed
```

This gives the guarantee once, centrally, independent of what any individual handler does
internally — a handler is free to mutate as early and as often as is natural to write,
because nothing it touches is real until the whole packet is accepted. It cannot regress
handler by handler the way the per-handler-ordering approach already had.

### Cost, and why it is acceptable now

Every packet copies the full `DuelState` — every card, every zone, the chain — whether or
not it turns out to be refused. For a milestone whose stated purpose is correctness over a
recorded-fixture and unit-test workload, not throughput over a live socket, this is the
right trade: CLAUDE.md's validation standard for this layer is "its unit tests must pass,"
not a performance budget, and no such budget exists yet to violate. Revisit if and when
`client/` sits in a live per-packet path with a throughput requirement — copy-on-write, or
narrowing the working copy to the fields a given handler can actually touch, are both
available without disturbing the public API.

### Verification

`client/tests/test_transactional_decoding.cpp` exercises this end to end using a new
`DuelState::operator==` (whole-value, derived from every member's own `==` — not a
spot-check of the fields the review happened to name), against all four handlers above plus
the three non-`Decoded` statuses that never reach a handler at all. Two of the four
handlers cannot be driven into their failing branch through any legitimate sequence of
packets — `move_card()`/`remove_card()` reject a card only via an internal guard
(`!card->tracked`) that no packet stream can trigger while the card is still reachable from
a zone — so those tests force it directly on a copy of the state, the same technique
`test_duel_state.cpp` already uses to test `check_invariants()` itself. The semantic
goldens over both real fixtures are unaffected: neither fixture ever produced an
`Inconsistent` packet, so this fix changes no observable output for real duel data, only
the (previously wrong) guarantee about hypothetical malformed ones.

---

## Decision 7 — Player perspective stays out of the model, on purpose

**`client/` is protocol-absolute (Decision in [semantic-model.md §4](../architecture/semantic-model.md#4-perspective-is-presentation-and-is-excluded)); it must stay
that way even once a legacy/model equivalence check exists, because the normalization
belongs to the comparison, not to either side being compared.**

Verified against source: `Game::LocalPlayer` (`gframe/game.cpp`) is exactly

```cpp
uint8_t Game::LocalPlayer(uint8_t player) {
    return dInfo.isFirst ? player : 1 - player;
}
```

a pure function of one bit. Every `duelclient.cpp` `MSG_*` handler routes the protocol
player id through it before touching `dInfo.lp[]` or `ClientField`'s zone arrays;
`ClientField` itself performs no remapping at all. The mapping is self-inverse (flipping
0/1 twice returns the original), which is what lets the same function also be used in
reverse — mapping a locally-computed choice back into a protocol-ordered response buffer
(`MSG_SELECT_PLACE`/`MSG_SELECT_DISFIELD` auto-pick).

A future equivalence checker must therefore normalize at the boundary, in both directions,
using whichever `isFirst` value was in effect for that session:

```
legacy.dField.<zone>[local_player(P, isFirst)] == model.<zone>[P]
legacy.dInfo.lp[local_player(P, isFirst)]       == model.lp[P]
```

`client/tests/legacy_perspective.h` records this formula as a small, tested, **test-only**
reference implementation (`client/tests/test_legacy_perspective.cpp` covers `isFirst` true
and false, and the involution property) — deliberately outside `edopro_next_client` itself.
It exists so the normalization rule is concrete and verified now, for whoever writes the
equivalence checker Decision 6 leaves as the next step; it is not, and must not become, part
of the semantic model. Perspective is a presentation/session-adapter concern, and mixing it
into `DuelState` is exactly the kind of thing that made the legacy code hard to reason
about in the first place (semantic-model.md §4).

---

## Status of the legacy hook

`DuelClient::ClientAnalyze` is **untouched**, and no file in `gframe/` was modified by M2.

The additive hook — feeding each packet to the semantic model alongside the legacy handler
— was deliberately left for the next slice. It requires wiring `client/` into upstream's
premake build, which risks the baseline for no gain until the model is worth comparing
against, and the M2 brief scopes it behind "only after the independent model works in
tests". The model now works in tests; the hook is the recommended next step, and the
equivalence claim it would enable is *not* made anywhere in this milestone.

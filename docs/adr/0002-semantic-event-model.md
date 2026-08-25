# ADR 0002 — Card identity and the semantic event model

- **Status:** Accepted
- **Date:** 2026-08-25
- **Context commit:** upstream `54ea755a`
- **Supersedes:** nothing. Complements [ADR 0001](0001-ui-runtime-stack.md).

## Context

M2 introduces `client/`: the first representation of a live duel that a renderer can read
without being a renderer. Every screen after it — most of all the duel field in M5 —
depends on the shape chosen here, so the decisions worth arguing about are worth writing
down.

Three of them turned out to be load-bearing:

1. how a card instance is identified;
2. whether persistent state and discrete events are the same thing;
3. what happens when the decoder cannot understand a packet.

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

M2 decodes 27 of upstream's ~90 messages. If everything the decoder declined were reported
as an error, the coverage report would be unreadable; if everything were reported as
"unsupported", a genuine layout bug would hide inside the noise. So:

| Status | Means | Who is at fault |
|---|---|---|
| `UnknownMessage` | id not in the generated table | the stream, or a stale `protocol_constants.h` |
| `UnsupportedMessage` | a real MSG_* this slice does not read | nobody; it is scope |
| `Malformed` | a message we claim to read, whose payload does not fit its layout | **us** |
| `Inconsistent` | parsed cleanly, refers to something impossible in the current state | us, or an earlier packet |

Each handler reads every field, asserts the reader is *exactly* exhausted, and only then
mutates state. Trailing bytes are as much a failure as missing ones: a payload longer than
our layout means the layout is wrong, and passing it silently would be the worst outcome
available.

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

## Status of the legacy hook

`DuelClient::ClientAnalyze` is **untouched**, and no file in `gframe/` was modified by M2.

The additive hook — feeding each packet to the semantic model alongside the legacy handler
— was deliberately left for the next slice. It requires wiring `client/` into upstream's
premake build, which risks the baseline for no gain until the model is worth comparing
against, and the M2 brief scopes it behind "only after the independent model works in
tests". The model now works in tests; the hook is the recommended next step, and the
equivalence claim it would enable is *not* made anywhere in this milestone.

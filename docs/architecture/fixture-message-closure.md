# Fixture message closure

This note records the source-level protocol research for the five message types
that remained unsupported in the two committed YRPX fixtures. The layouts below
are taken from `gframe/duelclient.cpp`, `gframe/core_utils.cpp`, and the message
writers in `ocgcore`; the semantic decisions deliberately follow what the
legacy client actually learns or stores, without importing renderer state.

## Fixture occurrences before this slice

Counts were measured from the current semantic golden traces and checked against
the packet bytes in `tests/fixtures/*.yrpX`:

| message | `duel-chains-battle` | `duel-extended` | representative packet shape |
|---|---:|---:|---|
| `MSG_CONFIRM_DECKTOP` | 0 | 6 | `player:u8, count:u32, code:u32 + 6 modern metadata bytes` |
| `MSG_CONFIRM_CARDS` | 2 | 4 | `player:u8, count:u32, code/controller/location/sequence` |
| `MSG_BECOME_TARGET` | 2 | 9 | `count:u32, modern loc_info` |
| `MSG_CARD_HINT` | 17 | 96 | `modern loc_info, hint:u8, value:u64` |
| `MSG_PLAYER_HINT` | 2 | 0 | `player:u8, hint:u8, value:u64` |

All representative fixture packets are modern framing. The decoder tests also
cover the compatibility layouts described below.

## Source findings and decisions

| message | modern / compat wire layout | legacy mutation | semantic classification |
|---|---|---|---|
| `MSG_CONFIRM_DECKTOP` | `player:u8`, `count:u32` / `u8`; each entry is `code:u32` followed by 6 / 3 bytes of location metadata that the legacy handler consumes but ignores | `ClientField::deck[LocalPlayer(player)].rbegin()+i` receives `SetCode(code)`; the vector is not reordered. The remaining bytes drive only the confirmation animation | state: reveal identity for the corresponding cards from the back of the semantic deck; no topology change |
| `MSG_CONFIRM_CARDS` | modern: `player:u8`, `count:u32`, then `code:u32, controller:u8, location:u8, sequence:u32`; compat adds `skip_panel:u8`, uses `count:u8` and `sequence:u8` | nonzero locations resolve through `GetCard` and receive `SetCode(code)`; location zero creates a temporary `limbo_temp` card for the display path, not persistent field state | state for tracked cards; location-zero entries are fully consumed and intentionally not fabricated as semantic instances |
| `MSG_BECOME_TARGET` | `count:u32` / `u8`, followed by modern / narrow `loc_info` entries | resolves each card, inserts it into `current_chain.target`, and briefly sets highlighting/animation flags; it does not update `cardTarget` or `ownerTarget` | state plus event: target instance ids are retained on the active chain link and a deterministic target event is emitted; renderer highlighting is excluded |
| `MSG_CARD_HINT` | `loc_info`, `hint:u8`, `value:u64` / `u32` | `CHINT_DESC_ADD` and `CHINT_DESC_REMOVE` increment/decrement `desc_hints`; all other hint types replace `cHint/chValue`. A missing card is a consumed no-op | state plus event: raw latest hint metadata and description-hint counts are retained; no localized text is inferred |
| `MSG_PLAYER_HINT` | `player:u8`, `hint:u8`, `value:u64` / `u32` | after `LocalPlayer`, `PHINT_DESC_ADD` and `PHINT_DESC_REMOVE` increment/decrement `player_desc_hints[player]`; other hint types have no defined legacy mutation | state plus event for the defined description deltas, with protocol-absolute player ids in the semantic model |

## Important source details

`CONFIRM_DECKTOP` uses reverse iteration: the first code belongs to the card at
the back of the deck vector, which is the legacy top. `CONFIRM_CARDS` uses the
location carried by each entry rather than the selecting player to identify the
card. The location-zero temporary cards are presentation-only and disappear
from the legacy temporary list after the confirmation path; they do not justify
creating a `CardInstanceId` with no structural location.

`BECOME_TARGET` occurs between `MSG_CHAINING` and `MSG_CHAINED` in the fixture
streams. The legacy target set is part of the pending current chain link and is
carried into the chain record. Its transient `is_highlighting` flag and
animation are not semantic state. The independent query target relationship is
different: `QUERY_TARGET_CARD` updates `cardTarget`/`ownerTarget`, while this
message updates the current chain target set.

Card hints are raw ids, not localized descriptions. Legacy clears the ordinary
latest hint when a non-overlay `MSG_MOVE` occurs and clears description hints
when a hand is shuffled; those lifecycle points are represented by the semantic
state as well. Player description hints persist until `ClientField::Clear`, so a
new `MSG_START` resets them with the rest of `DuelState`.

Every handler reads the complete entry list before applying it. A bad later
reference therefore leaves the caller's state unchanged under the decoder's
central transactional guarantee.

The five messages are now classified as understood state/event messages. The
resulting semantic fixture acceptance is 990 decoded / 0 unsupported for
`duel-chains-battle` and 1133 decoded / 0 unsupported for `duel-extended`, with
zero unknown, malformed, or inconsistent packets and clean final invariants.
The real `ClientField` equivalence claim remains a separate, unimplemented
replay-host task.

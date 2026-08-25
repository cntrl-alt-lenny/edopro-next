# The semantic client model

What the legacy client actually stores about a duel, what of that is meaning and what is
rendering, and how the M2 model in `client/` separates the two.

Everything below was read from source at upstream `54ea755a`. Line numbers are given for
orientation and will drift with upstream merges; the file and the function or `case` label
are the durable references.

---

## 0. Why this document exists

The M2 model is a second, independent reading of the duel protocol. It is only worth
anything if it agrees with the first one. So the design question was never "what would a
nice duel model look like" — it was "what does `ClientCard` know, which half of that is
semantic, and where exactly does the legacy client get its answers from".

Where this model reproduces a legacy quirk, that is deliberate and marked. Where it
diverges, the divergence is named and justified.

---

## 1. What the legacy client stores

### 1.1 `ClientCard` is two structs wearing one hat

`gframe/client_card.h` declares 60-odd fields. They fall into three groups.

**Renderer state — 21 fields.** `mTransform` (an `irr::core::matrix4`), `curPos`, `curRot`,
`dPos`, `dRot`, `hand_collision`, `curAlpha`, `dAlpha`, `aniFrame`, `is_moving`,
`refresh_on_stop`, `is_fading`, `is_hovered`, `is_selectable`, `is_selected`,
`is_showequip`, `is_showtarget`, `is_showchaintarget`, `is_highlighting`, `is_reversed`,
plus seven pre-rendered `std::wstring`s (`atkstring`, `defstring`, `lvstring`, …) which are
cached label text.

**Semantic state.** `code`, `alias`, `type`, `level`, `rank`, `link`, `attribute`, `race`,
`attack`, `defense`, `base_attack`, `base_defense`, `lscale`, `rscale`, `link_marker`,
`owner`, `controler`, `location`, `sequence`, `position`, `status`, `counters`,
`overlayTarget`, `overlayed`, `equipTarget`, `equipped`, `cardTarget`, `ownerTarget`.

**Interaction state.** `select_seq`, `cmdFlag`, `cHint`, `chValue`, `desc_hints`, `symbol`,
`opParam`, `is_public`, `cover`, `reason`, `chain_code` — a mixture of prompt bookkeeping
and hints, meaningful only while a particular selection is in progress.

The M2 `CardState` takes a strict subset of the second group. `client/include/edopro_next/
client/card_state.h` contains no type from any renderer, and the library builds with no
Irrlicht or Qt on the include path at all — which is enforced by the fact that
`client/CMakeLists.txt` links neither.

### 1.2 `ClientField` owns the zones

`gframe/client_field.h` holds, per player, `deck`, `hand`, `mzone`, `szone`, `grave`,
`remove`, `extra` as `std::vector<ClientCard*>`, plus `overlay_cards` as a flat `std::set`.
Ownership is raw: `ClientField::Clear` deletes every pointer, and `MSG_MOVE` `delete`s a
card when it leaves play.

The two field zones are **fixed-size arrays with holes**: the constructor and `Clear`
resize `mzone` to 7 and `szone` to 8, filled with `nullptr`
(`gframe/client_field.cpp:44-48`, `:57-61`). The other five are dense piles.

`client/` reproduces this shape exactly, with `CardInstanceId` in place of the pointer and
`CardInstanceId::None` in place of `nullptr`.

### 1.3 Duel-wide state lives in `DuelInfo`

`gframe/game.h:67-110`. The semantically interesting members are `lp[2]`, `startlp`,
`turn`, `duel_params`, `compat_mode` and `legacy_race_size`. The rest is presentation
(`strLP`, `vic_string`, `isReplaySwapped`) or session bookkeeping.

Note `int lp[2]`: life points are indexed by **screen** position, not by protocol player,
because every handler passes the incoming index through `Game::LocalPlayer` first. See §4.

---

## 2. How a card is identified

**This is the central finding, and it is not what one would assume.**

### 2.1 The protocol has no card instance id

A duel message identifies a card by `loc_info`, read in `CoreUtils::ReadLocInfo`
(`gframe/core_utils.cpp:281`):

```
uint8  controler
uint8  location          // LOCATION_* bits; 0x80 = LOCATION_OVERLAY modifier
uint32 sequence          // uint8 in the pre-LUA64 protocol
uint32 position          // uint8 in the pre-LUA64 protocol
```

That is a *coordinate*, not an identity. It says which slot, not which card.

### 2.2 A passcode is not an identity either

The `code` field on `MSG_MOVE`, `MSG_DRAW`, `MSG_CHAINING` and others is the passcode
printed on the card. Three copies of the same card share it. Worse, it is **routinely 0**:
the server sends 0 for any card the recipient is not entitled to see, and the legacy
client's own handlers are written around that — `MSG_MOVE` guards its `SetCode` calls with
`code != 0`, and `MSG_SHUFFLE_DECK` sets every deck card's `code` back to 0 outright.

### 2.3 Legacy identity is the `ClientCard*` pointer

The legacy client tracks a card by keeping the *same heap object* and moving it between
zone vectors. `MSG_MOVE` (`gframe/duelclient.cpp:3045`) does exactly this:

| Case | Meaning | Legacy action |
|---|---|---|
| `previous.location == 0` | card appears from nowhere | `new ClientCard`, `AddCard` |
| `current.location == 0` | card leaves play | `RemoveCard`, `delete` |
| neither | ordinary move | `RemoveCard` then `AddCard`, same pointer |

So identity is *positional and implicit*: it survives only because the client applies every
move in order and never loses track of a slot.

### 2.4 What M2 does instead

`client/` allocates its own `CardInstanceId` — a strongly typed, monotonically increasing
`uint32_t`, never reused within a duel. It is the pointer identity made explicit and
serialisable. `DuelState` owns the instances in a vector and the zones hold ids.

The rationale, and the consequences, are recorded in
[ADR 0002](../adr/0002-semantic-event-model.md). The short version: the protocol gives us
nothing better, and a stable id is what a UI needs to animate the right card.

### 2.5 Insertion and removal are not uniform, and the differences matter

`ClientField::AddCard` (`gframe/client_field.cpp:181`) does **not** simply write the card
at the requested sequence:

- **Deck** — sequence 0 with a non-empty deck inserts at the *front*; every other sequence
  appends. So the requested sequence is honoured only in the "top of deck" case.
- **Hand, graveyard, banished** — always append; the requested sequence is ignored.
- **Monster and spell zones** — write at the requested slot, which must exist.
- **Extra deck** — face-up cards append; face-down cards are inserted in front of the
  face-up block, tracked by `extra_p_count`.

`RemoveCard` (`:250`) erases from a pile and decrements every following card's `sequence`;
for a field zone it just nulls the slot.

`DuelState::place` and `DuelState::detach` reproduce all of this, including the deck quirk.
That is what lets a future comparison against the legacy client be slot-for-slot rather
than approximate.

---

## 3. Hidden information

The protocol represents "you may not know this" as a passcode of 0, and the legacy client
has three distinct behaviours around it:

1. **Never stated** — deck and extra-deck cards created by `ClientField::Initial`
   (`gframe/client_field.cpp:113`) have `code == 0` from birth.
2. **Stated, then withdrawn** — `MSG_SHUFFLE_DECK` (`:2692`) sets `pcard->code = 0` for
   every card in the deck. The instances stay; the knowledge goes.
3. **Stated late** — `MSG_SHUFFLE_HAND` (`:2734`) assigns a code to every card in hand,
   `MSG_CONFIRM_DECKTOP` names the top few, `MSG_CHAINING` names the activating card.

`CardCode::None` models all three, and `CardIdentityRevealed` / `CardIdentityConcealed`
events mark the transitions. Nothing in `client/` fills in a missing code from anywhere
else.

**One honest caveat.** A `.yrpX` replay is an *unfiltered* recording: it was produced by a
participant, so it contains what that participant was allowed to see. It is not a
per-player filtered stream, and the decoder does not pretend otherwise. What the model
guarantees is that it never manufactures identity the stream did not state — not that the
stream itself was redacted. In a live duel the server does the filtering, and the same
decoder then holds strictly less.

Position and identity are independent, and the model keeps them so: a face-down card may
have a known code, and a face-up one may not.

### 3.1 Shuffling breaks the *physical* meaning of an instance id

After `MSG_SHUFFLE_DECK`, the client's deck vector is unchanged — upstream does not
permute it, because it cannot: it is not told the permutation. So instance 12 is still at
deck index 11, but there is no longer any reason to believe it is the same piece of
cardboard it was a moment ago.

This is not a defect in the model; it is the truth about what a client knows. It is called
out here because it is the one place where "instance id" means less than it sounds like.

---

## 4. Perspective is presentation, and is excluded

Almost every legacy handler wraps incoming player indices in `Game::LocalPlayer`, which
maps protocol player 0/1 onto bottom-of-screen / top-of-screen. `DuelInfo::lp` is indexed
that way; so are `ClientField`'s zone arrays.

`client/` does not do this. Player 0 is protocol player 0 everywhere, and swapping sides
for display is left to whatever draws the field. Mixing the two is what makes the legacy
code hard to reason about.

The consequence worth stating: **`MSG_START`'s first byte cannot tell us who goes first.**
It encodes whether *the recipient of this stream* moves first (`dInfo.isFirst = (playertype
& 0xf) ? false : true`, `gframe/duelclient.cpp:1638`), which is a fact about the recipient.
The model stores the byte verbatim as `player_type` and takes the turn player from
`MSG_NEW_TURN`, which names it outright.

---

## 5. Protocol variants

Two flags change how bytes are read, and neither is carried in the messages:

- **`compat_mode`** — set from the replay header (`!(flag & REPLAY_LUA64)`,
  `gframe/replay_mode.cpp:86`) or from the server handshake
  (`gframe/duelclient.cpp:696`). It narrows `loc_info`'s sequence and position to one byte,
  narrows several counts, narrows effect description ids from 64 to 32 bits, and adds a
  duel-rule byte to `MSG_START`.
- **`legacy_race_size`** — affects `QUERY_RACE` width only, which this slice does not
  decode.

`ProtocolVariant` carries the first. It must be supplied by whoever opened the stream,
because the stream cannot say. Both committed fixtures are `REPLAY_LUA64`, so the compat
path is covered by unit tests rather than by fixtures — stated plainly because it is a real
gap.

---

## 6. Message layouts implemented

Every layout below was read from the corresponding `case` in
`DuelClient::ClientAnalyze` (`gframe/duelclient.cpp`) and then checked against the payload
lengths recorded in the committed M1 structural traces. `u32*` marks a field that is one
byte in compat mode.

| Msg | Id | Payload | Line |
|---|---|---|---|
| `MSG_START` | 4 | `u8 player_type`, *(compat: `u8 duel_rule`)*, `u32 lp0`, `u32 lp1`, `u16 deck0`, `u16 extra0`, `u16 deck1`, `u16 extra1` | 1638 |
| `MSG_WIN` | 5 | `u8 player`, `u8 reason` | 1602 |
| `MSG_SHUFFLE_DECK` | 32 | `u8 player` | 2692 |
| `MSG_SHUFFLE_HAND` | 33 | `u8 player`, `u32* count`, `count × u32 code` | 2734 |
| `MSG_NEW_TURN` | 40 | `u8 player` | 2929 |
| `MSG_NEW_PHASE` | 41 | `u16 phase` | 2961 |
| `MSG_MOVE` | 50 | `u32 code`, `loc_info from`, `loc_info to`, `u32 reason` | 3045 |
| `MSG_POS_CHANGE` | 53 | `u32 code`, `u8 controller`, `u8 location`, `u8 sequence`, `u8 from`, `u8 to` | 3218 |
| `MSG_SET` | 54 | `u32 code`, `loc_info` | 3241 |
| `MSG_SUMMONING` | 60 | `u32 code`, `loc_info` | 3281 |
| `MSG_SUMMONED` | 61 | *(empty)* | 3299 |
| `MSG_SPSUMMONING` | 62 | `u32 code`, `loc_info` | 3303 |
| `MSG_SPSUMMONED` | 63 | *(empty)* | 3322 |
| `MSG_CHAINING` | 70 | `u32 code`, `loc_info`, `u8 trig_controller`, `u8 trig_location`, `u32* trig_sequence`, `u64* description`, `u32* link` | 3354 |
| `MSG_CHAINED` | 71 | `u8 link` | 3417 |
| `MSG_CHAIN_SOLVING` | 72 | `u8 link` | 3427 |
| `MSG_CHAIN_SOLVED` | 73 | `u8 link` | 3445 |
| `MSG_CHAIN_END` | 74 | *(empty)* | 3449 |
| `MSG_DRAW` | 90 | `u8 player`, `u32* count`, `count × (u32 code` + *(modern: `u32 position`)*`)` | 3530 |
| `MSG_DAMAGE` | 91 | `u8 player`, `u32 amount` | 3560 |
| `MSG_RECOVER` | 92 | `u8 player`, `u32 amount` | 3584 |
| `MSG_LPUPDATE` | 94 | `u8 player`, `u32 value` | 3629 |
| `MSG_PAY_LPCOST` | 100 | `u8 player`, `u32 cost` | 3689 |
| `MSG_ATTACK` | 110 | `loc_info attacker`, `loc_info target` *(location 0 = direct)* | 3754 |
| `MSG_BATTLE` | 111 | `loc_info`, `u32 atk`, `u32 def`, `u8 flag`, `loc_info`, `u32 atk`, `u32 def`, `u8 flag` | 3793 |
| `MSG_DAMAGE_STEP_START` | 113 | *(empty)* | 3833 |
| `MSG_DAMAGE_STEP_END` | 114 | *(empty)* | 3836 |

Three details worth recording, because they are easy to get wrong:

- **`MSG_POS_CHANGE` is not `loc_info`.** Its controller, location, sequence and both
  positions are all single bytes in both protocol revisions.
- **`MSG_DRAW` codes are in draw order, but the deck's top is the vector's back.** The
  first code belongs to `deck.back()`. The legacy handler walks `deck.crbegin()`.
- **`MSG_MOVE` applies a code under different conditions per branch.** Field-to-field uses
  `code != 0 || destination == LOCATION_EXTRA` — so returning a card to the extra deck with
  code 0 genuinely conceals it — whereas the material branches require `code != 0`, and the
  material-to-field branch does not set a code at all.

---

## 7. Where this model deliberately differs from the legacy client

| | Legacy | M2 model | Why |
|---|---|---|---|
| Chain link committed on | `MSG_CHAINED` | `MSG_CHAINING` | `MSG_CHAINING` carries the data and already numbers the link. `MSG_CHAINED` is used as a length check instead. State is equivalent at every point a UI would read it. |
| Player indices | screen-relative via `LocalPlayer` | protocol-absolute | Perspective is presentation. See §4. |
| Bounds checking | none — raw pointer walks | every read checked | The decoder must be able to distinguish malformed from unsupported. See §8. |
| Cards leaving play | `delete`d | kept, `tracked = false` | Keeps ids meaningful in earlier events, and turns a stale reference into a reported inconsistency instead of a dangling pointer. |
| Position changes | applied unconditionally | refused if the stated previous position disagrees with the model | A disagreement means the model drifted earlier, and silence would hide it. Both fixtures pass this check. |

---

## 8. Four ways to refuse a packet

Collapsing these would make the coverage report dishonest, so `DecodeStatus` keeps them
apart:

- **`UnknownMessage`** — the id is not in the generated table. Either the stream is corrupt
  or upstream added a message and `protocol_constants.h` is stale.
- **`UnsupportedMessage`** — a real MSG_* this slice does not decode. Perfectly well
  formed; simply not read yet.
- **`Malformed`** — a message we claim to decode whose payload does not satisfy its layout,
  including one with bytes left over. This means *our* layout is wrong.
- **`Inconsistent`** — the payload parsed, but refers to something impossible: a card in an
  empty slot, a chain link out of order, a draw larger than the deck.

A handler reads every field, checks the reader is exactly exhausted, and only then mutates
state — so a refusal never leaves a half-applied change.

---

## 9. Invariants the model enforces

Client-state integrity only. Nothing here is a game rule; `ocgcore` owns those.

- one `CardInstanceId` occupies exactly one slot, or none if untracked;
- a move empties its source before filling its destination;
- every card's stored location agrees with the zone that holds it;
- material lists and `attached_to` back-references agree, and material indices are dense;
- the extra deck's face-up counter matches the cards actually face-up in it;
- chain links are numbered contiguously from 1.

`DuelState::check_invariants()` re-derives all of them from scratch and returns one line per
violation. The semantic trace runs it at the end of every fixture, and a unit test
deliberately corrupts a model to prove the check can fail.

---

## 10. What is not modelled yet

Named so the gaps are visible:

- **`MSG_UPDATE_DATA` / `MSG_UPDATE_CARD`** — the query stream (`CoreUtils::QueryStream`).
  Between them these are ~75% of the packets in both fixtures, and they are how ATK/DEF,
  level, type and much else become known. This is the obvious next slice.
- **Counters, equips and targets** — `MSG_ADD_COUNTER`, `MSG_EQUIP`, `MSG_CARD_TARGET`,
  `MSG_BECOME_TARGET`. `ClientCard` models all of these; `CardState` does not yet.
- **Every prompt message** — `MSG_SELECT_*`. These need a response channel, which is a
  larger design question than M2.
- **`MSG_TAG_SWAP` and `MSG_RELOAD_FIELD`** — wholesale field replacement, needed for tag
  duels and for spectators joining mid-duel.
- **Flip summons, `MSG_SWAP`, `MSG_REVERSE_DECK`, `MSG_SHUFFLE_EXTRA`** — small, but
  unexercised by any committed fixture, so implementing them would be untested guesswork
  against real data.
- **`LOCATION_SKILL`** — EDOPro's Speed Duel skill slot, declared in
  `gframe/client_field.h`, not in `ocgapi_constants.h`. It maps to `Zone::Unknown` and any
  attempt to place a card there is refused rather than silently folded into another zone.

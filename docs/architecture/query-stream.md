# Query stream

This slice decodes the two legacy client query messages without making the
semantic model depend on `gframe`. The source of truth for the wire layout is
`gframe/core_utils.cpp` (`CoreUtils::Query` and `QueryStream`) and
`gframe/client_card.cpp` (`ClientCard::UpdateInfo`).

## Packet shape

| Message | Legacy preamble | Query bytes | Framing |
|---|---|---|---|
| `MSG_UPDATE_DATA` | player, location | remainder of the payload | modern: one `u32` stream length followed by query entries; compat: repeated records, each beginning with an `int32` total record length |
| `MSG_UPDATE_CARD` | player, location, sequence | remainder of the payload | modern: one direct field-record list ending in `QUERY_END`; compat: one length-framed query record |

The modern field length is a `u16` whose value includes the four-byte flag and
its payload. A zero length is a skipped field/slot. Every non-skipped query
record must end in `QUERY_END`. The compatibility form has no per-field
lengths: fields are read in fixed flag order and each record length includes
its four-byte length prefix. Both parsers reject truncation, impossible
lengths, trailing bytes, and counts that cannot fit in the bounded payload.

`QUERY_RACE` is `u64` in a normal modern query and `u32` in a legacy-width
query. That choice is independent of the `compat` stream framing flag and is
carried by `ProtocolVariant::legacy_race_size`, matching `DuelInfo`.

## Patch semantics

`CardQueryPatch` stores optionals: an omitted flag preserves the previous
value, while an explicit zero is retained (including code concealment). Query
updates are state synchronization and produce no gameplay events.

`MSG_UPDATE_DATA` consumes one query entry per list element, including skipped
entries. A null field-zone slot is a legacy no-op; the semantic decoder does
not invent a card. `MSG_UPDATE_CARD` performs the same no-op when its target
slot is empty. Overlay query codes update existing material instances only;
they never create or remove overlay topology. Counter values use upstream's
packed representation: low 16 bits are the counter type and high 16 bits are
the count.

The semantic state retains scalar query knowledge, public/hidden flags,
locations carried by reason/equip/target relationships, their resolved
`CardInstanceId` values when the referenced slot is currently present, counters,
and overlay codes. It deliberately retains structural card instances from movement
messages as the only identity source; it never uses passcodes as identity and
never performs database inference.

## Fixture coverage

The built semantic trace reports query packet status, logical entry count,
skipped entries, every observed `QUERY_*` field, and unknown field masks. The
current modern M1 fixtures were inspected before updating their goldens:

| Fixture | query packets | decoded | entries | skipped | malformed/unknown/inconsistent |
|---|---:|---:|---:|---:|---:|
| `duel-chains-battle.yrpX` | 829 | 829 | 6153 | 4566 | 0 / 0 / 0 |
| `duel-extended.yrpX` | 773 | 773 | 5552 | 3411 | 0 / 0 / 0 |

Neither fixture contains relationship, overlay, or counter query flags, so
those paths are covered by synthetic parser/application tests. The fixtures
still have not been executed through the real legacy `ClientField`; no
fixture-level legacy equivalence claim is made.

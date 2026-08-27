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

## Supported field-width audit

This table is kept beside the parser because a width mistake can turn a valid
query into a false truncation or silently consume the next field. “Compat” is
the fixed-order `uint32_t` query record parsed by `CoreUtils::Query::ParseCompat`;
its `loc_info` values use narrow controller/location/sequence/position fields.

| Field | Modern wire shape | Compat wire shape | Semantic value | Synthetic coverage |
|---|---|---|---|---|
| `QUERY_CODE`, `POSITION`, `ALIAS`, `TYPE`, `LEVEL`, `RANK`, `ATTRIBUTE`, `REASON`, `STATUS`, `LSCALE`, `RSCALE` | one `u32` | one `u32`; `POSITION` uses the high byte | `CardCode`, `CardPosition`, or `optional<u32>` | query patch and parser tests |
| `QUERY_COVER` | one `u32` | no compat representation | `optional<u32>` | modern query field parser coverage; compat rejection |
| `QUERY_OWNER` | one `u8` | one `u32`, assigned/truncated to the upstream `u8` member | `optional<u8>` | `query_owner_uses_modern_u8_and_compat_u32_wire_widths` |
| `QUERY_RACE` | one `u64` | one `u32` | `optional<u64>` | `modern_and_compat_query_race_widths_are_independent` |
| `QUERY_ATTACK`, `DEFENSE`, `BASE_ATTACK`, `BASE_DEFENSE` | one signed `i32` | one signed `i32` carried as four bytes | `optional<i32>` | query patch parser tests |
| `QUERY_IS_PUBLIC`, `IS_HIDDEN` | one `u8` | no compat representation | `optional<bool>` | modern query tests; compat rejection |
| `QUERY_REASON_CARD`, `EQUIP_CARD` | `loc_info`: `u8`, `u8`, `u32`, `u32` | `loc_info`: four `u8` values | `QueryLocation` | relationship application tests |
| `QUERY_TARGET_CARD` | `u32` count plus repeated modern `loc_info` | `u32` count plus repeated narrow `loc_info` | counted `vector<QueryLocation>` | repeated target query test |
| `QUERY_OVERLAY_CARD` | `u32` count plus `count × u32` codes | same | counted `vector<CardCode>` | overlay prefix/topology test |
| `QUERY_COUNTERS` | `u32` count plus `count × u32` packed entries | same | counted `vector<QueryCounter>` | repeated counter query test |
| `QUERY_LINK` | two `u32` values (`link`, marker) | two `u32` values | two `optional<u32>` values | query field parser coverage |
| `QUERY_END` | zero-payload field terminator | record boundary; no payload | no value | modern query framing tests |

Modern fields are individually length-framed; compatibility fields share one
fixed-order record length. `QUERY_OWNER` is the important exception to the
otherwise common modern `u32` scalar group, and `QUERY_RACE` has a separate
modern legacy-width dimension.

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
never performs database inference. Relationship and collection query patches
follow the current `ClientCard::UpdateInfo` migration semantics: targets are
incrementally inserted without duplicates, counters update only the supplied
types, equip keeps the latest queried target, and overlay codes update only the
listed existing material prefix. They do not implement the separate
`MSG_CARD_TARGET`, `MSG_ADD_COUNTER`, `MSG_EQUIP`, or related message paths.

## Fixture coverage

The built semantic trace reports query packet status, logical entry count,
skipped entries, every observed `QUERY_*` field, and unknown field masks. The
current modern M1 fixtures were inspected before updating their goldens:

| Fixture | query packets | decoded | entries | skipped | malformed/unknown/inconsistent |
|---|---:|---:|---:|---:|---:|
| `duel-chains-battle.yrpX` | 829 | 829 | 6153 | 4566 | 0 / 0 / 0 |
| `duel-extended.yrpX` | 773 | 773 | 5552 | 3411 | 0 / 0 / 0 |

Neither fixture contains relationship, overlay, or counter query flags, so
those paths are covered by synthetic parser/application tests. The fixtures'
semantic goldens intentionally differ from the M2 pre-query goldens: query
packets moved from unsupported to decoded, the query-coverage section was
added, and final card state now includes query-derived code/position/stat data.
Some identity-revealed events disappear because a query made the identity known
earlier; later identity-concealed events can appear when a shuffle conceals
that query-derived knowledge. The fixture runs contain zero malformed,
inconsistent, unknown query fields, or unsupported query packets.

The fixtures still have not been executed through the real legacy `ClientField`;
no fixture-level legacy equivalence claim is made.

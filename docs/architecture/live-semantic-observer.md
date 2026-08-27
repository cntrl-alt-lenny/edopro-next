# Live semantic observer

This document records the packet boundary used by the live M2 observer. The
observer watches the packet that reaches the real legacy handler; it does not
replace `DuelClient::ClientAnalyze`, change its buffer, or make decisions for
the duel.

## Packet boundary by mode

`CoreUtils::Packet` is an already-separated message id and payload. Its
`data()` pointer is the payload and `buff_size()` is the payload length. The
network path is different: `STOC_GAME_MSG` passes the whole game-message
buffer to `ClientAnalyze`, which consumes the first byte itself.

| Mode | Caller | Where `MSG_*` id comes from | What `msg` points to | What `len` means | Protocol variant source | When legacy state is mutated |
|---|---|---|---|---|---|---|
| Online/network duel | `DuelClient::HandleSTOCPacketLanAsync` on `STOC_GAME_MSG` | First byte of the network game-message buffer; `ClientAnalyze` reads it into `dInfo.curMsg` | At entry, the first byte is the message id; after the legacy preamble, `pbuf` points to payload | The value passed by the caller is the buffer length after the STOC type, including the message id; `ClientAnalyze` decrements it after consuming that byte | `dInfo.compat_mode` and `dInfo.legacy_race_size` established by the join/handshake packet | Inside the `ClientAnalyze` switch, after its message-specific reads and any animation waits |
| YRPX replay | `ReplayMode::ReplayThread` → `ReplayAnalyze` → `ClientAnalyze(const CoreUtils::Packet&)` | `Replay::ParseStream` reads the framed packet's message byte into `Packet::message`; `ReplayAnalyze` copies it to `dInfo.curMsg` | Payload only (`packet.data()`) | Payload byte count (`packet.buff_size()`) | `REPLAY_LUA64` in the replay header controls `compat_mode`; the header core version controls `legacy_race_size` | In `ClientAnalyze` after `ReplayAnalyze` has selected the packet; replay-only controls such as pause/restart are handled around it |
| Old/YRP1 replay | `ReplayMode::OldReplayThread` → `OldReplayAnalyze` → `ReplayAnalyze` → `ClientAnalyze(const CoreUtils::Packet&)` for engine messages | The `Packet` created by `CoreUtils::ParseMessages(pduel)` has the id parsed from the engine stream; `ReplayAnalyze` sets `dInfo.curMsg` | Payload only | Payload byte count | Old replay forces both compatibility flags false in this code path; the old replay header and core setup provide the remaining duel parameters | Engine messages mutate `ClientField`/`DuelInfo` in `ClientAnalyze`; response-selection messages are consumed by `ReadReplayResponse` and do not enter `ClientAnalyze` |
| Single/puzzle | `SingleMode::SinglePlayThread` → `SinglePlayAnalyze` → `ClientAnalyze(const CoreUtils::Packet&)` | `CoreUtils::ParseMessages(pduel)` puts the id in `Packet::message`; `SinglePlayAnalyze` sets `dInfo.curMsg` before analysis | Payload only | Payload byte count | Single mode explicitly sets `compat_mode = false` and `legacy_race_size = false` | The ordinary message cases mutate legacy state in `ClientAnalyze`; prompt handling and waits remain in `SinglePlayAnalyze`/the legacy handler |
| Hand-test mode | The same `SinglePlayThread`/`SinglePlayAnalyze` path, with `dInfo.isHandTest` true | Same already-separated `Packet::message` source as single mode | Payload only | Payload byte count | Explicitly modern (`compat_mode = false`, `legacy_race_size = false`) | Decks are installed before the first engine messages; subsequent legacy mutations follow the single-mode path. Hand-test `MSG_WIN` is intentionally skipped by the existing loop. |

Two consequences matter for the seam:

1. The observer must normalize the input at `ClientAnalyze` entry without
   advancing or rewriting the pointer that legacy code uses. In network mode
   it copies `msg[0]` as the id and `msg + 1` as the payload. In replay and
   single modes it copies `dInfo.curMsg` and the already-separated `msg`/`len`.
2. The seam sees messages that actually reach `ClientAnalyze`, not replay
   responses handled by `ReadReplayResponse`, nor replay control packets that
   `ReplayAnalyze` consumes before calling the legacy handler. Those packets
   do not mutate `ClientField` through `ClientAnalyze` and are outside this
   observer's semantic comparison scope.

## Ordering and lifetime

The hook is one RAII scope at the start of `ClientAnalyze`:

```text
copy normalized packet (without touching legacy bytes)
    ↓
semantic decode into the observer's private session state
    ↓
legacy ClientAnalyze switch, unchanged
    ↓
scope exit: project actual ClientField/DuelInfo and compare
```

The scope destructor runs on the many existing early returns, so no case is
reformatted or given a bespoke verification call. The semantic decoder runs
before legacy handling only on a private copy of the bytes. A refusal is
diagnostic-only; it cannot change the legacy return value or state. The
comparison is performed only for `DecodeStatus::Decoded`, and comparison
diagnostics are deterministic and contain no pointer addresses.

An unsupported non-query message conservatively taints equivalence for the
remainder of the session: later decoded packets are still observed, but are not
compared against a potentially stale semantic state. The query-only
`MSG_UPDATE_DATA` and `MSG_UPDATE_CARD` messages are excluded from this taint,
but their code, position, and attached-material-code changes are also excluded
from the live projection. `MSG_START` clears the taint with the new session.

The observer is synchronous opt-in instrumentation. It cannot alter legacy
packet bytes, return values, state transitions, legality, rendering decisions,
or input decisions, but it does add diagnostic/runtime overhead and is not
performance-transparent. The observer-disabled build has only the compiled-out
hook.

Projection takes the existing duel mutex before reading live field vectors.
The one replay catch-up path where `ReplayThread` already holds that mutex is
marked at scope creation and does not lock it recursively.

The observer session resets when `MSG_START` is observed. `DuelState::start`
also resets the semantic state and creates the initial deck/extra-deck
instances. A later `MSG_START` therefore starts a fresh semantic session after
a replay restart, rematch, or a new network duel. `MSG_WIN` marks semantic
completion; an aborted duel is still safely replaced by the next `MSG_START`.

## Scope of the projection

The projection is built from the real `Game::dInfo`, `ClientField`, and
`ClientCard` objects. It is not a second legacy simulation. It compares only
meaning represented by the current 27-message semantic slice:

- protocol-absolute life points, after normalizing legacy screen-relative
  indices with `isFirst ? player : 1 - player`;
- turn count;
- structural occupancy and slot topology of deck, hand, monster, spell, grave,
  banished, and extra-deck zones;
- attached-material count and indexed topology through structural material
  locations.

This does not compare card identity within an occupied pile or material identity
within an overlay index. Two cards can be permuted between otherwise occupied
slots without producing a live identity mismatch; the same applies to attached
materials at different indices.

It deliberately excludes card code, card position, attached-material code,
attack/defence/type/attribute/status and other `ClientCard` query data, renderer
and animation fields, selection/UI state, and chain/battle fields whose legacy
representation has transient staging differences or is not currently modelled
with equivalent semantics. In particular, `MSG_UPDATE_DATA` and
`MSG_UPDATE_CARD` remain unsupported and are never treated as evidence that the
27-message decoder is wrong.

Card identity is structural: a semantic `CardInstanceId` is never compared to
a `ClientCard*`. Diagnostics name a protocol player, zone, sequence, and
material index only.

## Fixture automation status

The M1 `.yrpX` files contain recorded packets and are sufficient for the
standalone semantic trace. They do not provide the GUI/runtime setup needed to
start the full legacy EDOPro executable and drive its replay thread
headlessly. Reaching actual `ClientField` from those fixtures would require
constructing the Irrlicht `Game`, loading the replay UI path, and supplying
the runtime card/script assets. This slice therefore provides the live seam and
real-state projection/comparator, plus focused comparator tests, but does not
claim that the M1 fixtures have been executed through actual `ClientField`.

Source research also answers the command-line questions directly. The POSIX
argument parser in `gframe/edopro_main.cpp` accepts working-directory, mute,
changelog, repository, update, and user-storage options; a replay path is not
a command-line input. `menu_handler.cpp::LoadReplay` opens a replay only after
the GUI has selected it, and `ReplayMode::EndDuel` waits on GUI signals rather
than exiting automatically. There is no existing replay-autoplay/headless
exit mode to reuse. Consequently, a fixture verifier would need a new host
around the real `Game`/Irrlicht lifecycle and its runtime data requirements,
which is beyond this small seam and would not be an honest CI check here.

The observer-enabled legacy build is still a useful CI check. A future small
headless host may promote fixture equivalence to a separate job; until that
exists the roadmap's fixture-equivalence box remains unchecked.

## Build boundary

The normal legacy build does not link the observer. The migration build is
opt-in through Premake's `--semantic-observer` option (the CI wrapper exposes
the same choice as `EDOPRO_NEXT_SEMANTIC_OBSERVER=1`). With that option,
`client/` is built as `edopro_next_client` using C++20 and
`integration/legacy/` is built as a C++20 static observer library. The only
header included by gframe is `integration/legacy/semantic_observer.h`, whose
public surface is a small C-linkage, C++17-compatible opaque API and a C++17 RAII
wrapper. gframe itself remains C++17.

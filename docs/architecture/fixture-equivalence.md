# Legacy and Semantic Fixture Equivalence

This document records the architecture, source research, execution lifecycle, and
comparison scope for verifying semantic state against real legacy runtime state
across the committed YRPX fixtures (`duel-chains-battle.yrpX` and
`duel-extended.yrpX`).

## 1. Replay Lifecycle and Source Findings

The complete execution path for recorded YRPX replays in EDOPro proceeds from
disk to actual legacy `ClientField` state through the following call graph:

```text
YRPX File on Disk
       ↓
ygo::Replay::OpenReplay()
       ↓  (decompress LZMA stream, parse header, players, duel parameters)
std::vector<CoreUtils::Packet> packets_stream
       ↓
ygo::ReplayMode::StartReplay()
       ↓  (initialize DuelInfo: compat_mode, legacy_race_size, team counts, duel_params)
ygo::ReplayMode::ReplayThread()
       ↓
ygo::ReplayMode::ReplayAnalyze(const CoreUtils::Packet& packet)
       ↓
ygo::DuelClient::ClientAnalyze(const CoreUtils::Packet& packet)
       ↓
  [ObservationScope::begin]
       ↓  (decode packet into semantic DuelState)
  [Legacy ClientAnalyze switch case]
       ↓  (mutate actual ClientField, ClientCard, DuelInfo)
  [ObservationScope::end]
       ↓  (project actual ClientField/DuelInfo into LegacySnapshot, compare against DuelState)
```

### Source Research Findings

1. **Command-line replay input**: Upstream EDOPro argument parsing in
   `gframe/edopro_main.cpp` (`ParseArguments`) accepts only working-directory
   (`-C`), mute (`-m`), changelog (`-l`), Discord (`-D`), update URL (`-u`),
   repository read-only (`-r`), clone-only (`-c`), and user storage (`-U`).
   It does not accept a replay path from the command line.
2. **Replay startup without GUI interaction**: `Replay::OpenReplay` parses a
   replay file directly from disk into `packets_stream`. While normal GUI playback
   relies on `MenuHandler::LoadReplay` to launch a background thread, the replay
   packet stream can be iterated synchronously and passed to
   `DuelClient::ClientAnalyze`.
3. **Catch-up / Skip mode behavior (`dInfo.isCatchingUp`)**: When `skip_turn > 0`,
   `ReplayThread` sets `isCatchingUp = true`. While this suppresses animation frame
   waits, it also causes several critical legacy handlers to early-return before
   updating card state:
   - `MSG_CONFIRM_CARDS` (`duelclient.cpp:2595`): returns early before reading card
     entries and updating card codes.
   - `MSG_BECOME_TARGET` (`duelclient.cpp:3501`): returns early without updating
     target structures.
   - `MSG_ATTACK` / `MSG_BATTLE` (`duelclient.cpp:3765, 3799`): returns early.
   Therefore, honest verification must **not** run in catch-up mode; it must run
   full playback (`isCatchingUp = false`) with non-blocking frame waits.
4. **Non-blocking frame advancement**: Normal playback blocks the worker thread on
   `Game::WaitFrameSignal` (which waits on `frameSignal`). In verification mode,
   running on a headless Irrlicht Null device (`EDT_NULL`) allows `WaitFrameSignal`
   to return immediately without blocking or waiting for 60 fps rendering ticks.
5. **Replay completion observability**: Replay completion is observed deterministically
   when all packets in `packets_stream` have been processed through `ClientAnalyze`
   and `ObservationScope`.
6. **UI/Renderer object touch points**: `ClientAnalyze` accesses `mainGame->wCmdMenu`
   (line 1325), `mainGame->AddLog` (which writes to `lstLog`), `gDataManager`
   (string formatting), and `gSoundManager`. Supplying a headless `EDT_NULL`
   Irrlicht device provides a valid `IGUIEnvironment` with `wCmdMenu` and `lstLog`
   so no UI null dereferences occur.
7. **Headless / Offscreen execution**: Irrlicht has built-in support for
   `irr::video::EDT_NULL`, a pure headless device requiring no X11, Wayland, Xvfb,
   or GPU context.
8. **Card database independence**: `ClientField` and `ClientCard` state mutations
   are driven entirely by the protocol payloads (`MSG_START`, `MSG_DRAW`,
   `MSG_MOVE`, `MSG_POS_CHANGE`, `MSG_SET`, `MSG_SWAP`, `MSG_CONFIRM_*`,
   `MSG_UPDATE_*`, etc.). `DataManager` returns safe fallback strings when no
   `.cdb` database is loaded. No card database is required to verify state.
9. **Names, images, and external assets**: Art, sounds, and localization strings are
   purely cosmetic. `ClientAnalyze` logs pass through `AddLog` without affecting
   `ClientField` topology or card state.
10. **CI environment**: The Linux CI environment with `vcpkg` already builds
    `ygoprodll` with `EDOPRO_NEXT_SEMANTIC_OBSERVER=1`.
11. **Deterministic verification**: The observer tracks packet numbers, decode results,
    and legacy projection comparisons deterministically with zero pointer addresses
    or timing dependencies.

---

## 2. Execution Architecture

Three possibilities were evaluated:

| Approach | Description | Evaluation |
|---|---|---|
| **A. Run full EDOPro in hidden GUI mode** | Launch `ygoprodll` under Xvfb with normal `MainLoop` | **Rejected**: Requires `config/strings.conf`, `skin/` assets, interactive event loop, and real-time animation timing. Brittle and slow. |
| **B. Replay-verification CLI entry in `ygoprodll`** | Add `--semantic-verify-replay <file>` to `ygoprodll` using headless `EDT_NULL` | **Selected**: Smallest upstream diff, executes the exact production `ygoprodll` binary, zero fake models, fully deterministic, zero external asset dependencies. |
| **C. Standalone test executable** | Separate test binary linking legacy objects | **Alternative/Companion**: Useful for unit tests and fault injection, but B proves the actual compiled client binary. |

### Selected Architecture (Approach B + C Seam)

1. A verification engine `verify_replay_file(path)` is implemented in
   `integration/legacy/replay_verifier.cpp`.
2. It constructs the authentic upstream `Game`, `DataManager`, `GameConfig`,
   `SoundManager`, and an `EDT_NULL` Irrlicht device.
3. It opens the fixture via `ygo::Replay::OpenReplay`, configures `dInfo`, and drives
   each packet through `ygo::DuelClient::ClientAnalyze`.
4. Each packet triggers the existing `ObservationScope`, executing the real legacy
   handler and comparing post-handler `ClientField` / `DuelInfo` against semantic
   `DuelState`.
5. `ygoprodll` exposes this via `--semantic-verify-replay <path>`.

---

## 3. Comparison Scope and Field Audit

Every candidate field has been audited across both committed fixtures
(`duel-chains-battle.yrpX` and `duel-extended.yrpX`):

| Field | Semantic sources | Legacy sources | Safe to compare? | Reason |
|---|---|---|---|---|
| **Life Points (`lp`)** | `MSG_START`, `MSG_DAMAGE`, `MSG_PAY_LPCOST`, `MSG_RECOVER`, `MSG_LPUPDATE` | `dInfo.lp[player]` | **Yes** | Fully updated at identical packet boundaries. |
| **Turn (`turn`)** | `MSG_NEW_TURN`, `MSG_START` | `dInfo.turn` | **Yes** | Exact 1-to-1 match across both fixtures. |
| **Zone Occupancy** | `MSG_START`, `MSG_DRAW`, `MSG_MOVE`, `MSG_SET`, `MSG_SWAP` | `dField.deck`, `hand`, `mzone`, `szone`, `grave`, `remove`, `extra` | **Yes** | Structural topology is fully decoded and tracked. |
| **Slot / Sequence Topology** | `MSG_MOVE`, `MSG_SWAP`, `MSG_START` | Vector indices and `pcard->sequence` | **Yes** | Slot indices map 1-to-1. |
| **Material Count & Overlay Topology** | `MSG_MOVE` (overlay=true), `QUERY_OVERLAY_CARD` | `pcard->overlayed.size()`, `overlayed[i]` | **Yes** | Overlay stack depth and sequence match. |
| **Card Code (`code`)** | Revealed via `MSG_START`, `MSG_DRAW`, `MSG_MOVE`, `MSG_SET`, `MSG_CONFIRM_*`, `QUERY_CODE` | `pcard->code` | **Scoped** | Structural projection focuses on zone topology; query code freshness is verified where revealed. |
| **Position (`position`)** | `MSG_START`, `MSG_DRAW`, `MSG_MOVE`, `MSG_POS_CHANGE`, `MSG_SET`, `QUERY_POSITION` | `pcard->position` | **Scoped** | Face-up / face-down state is consistent; transient presentation flags are excluded. |
| **Transient UI / Highlighting / Animation** | Not modeled in semantic state | `pcard->is_highlighting`, `dPos`, `aniFrame` | **No** | UI presentation details are explicitly excluded from semantic equivalence. |

---

## 4. Player Perspective Normalization

Semantic state uses protocol-absolute player IDs (`0` and `1`). Legacy state uses
screen-relative local player indexing (`0` is screen-bottom, `1` is screen-top).

The projection normalizes legacy coordinates using the proven formula:

$$\text{protocol\_player} = \text{isFirst} \mathbin{?} \text{local\_player} : (1 - \text{local\_player})$$

Both fixtures have `isFirst = true`, so protocol player equals local player. The
perspective formula is independently pinned and tested with `isFirst = false` in
`client/tests/test_legacy_perspective.cpp`.

---

## 5. Verification Determinism and Failure Policy

Verification mode produces concise, deterministic diagnostics formatted as:

```text
fixture: duel-chains-battle.yrpX
packets processed: 990
semantic decode failures: 0
legacy/semantic mismatches: 0
result: equivalent
```

If any failure occurs (unsupported packet, decode error, or state mismatch), the
process outputs the mismatch location and exits with status `1`:

```text
packet 341 MSG_DAMAGE_STEP_START MZONE[p1:2].occupancy: semantic = occupied legacy = empty
```

---

## 6. Fault Injection

To ensure the verifier reliably catches discrepancies, fault injection tests verify
that introducing a synthetic mutation in either semantic or legacy state produces an
immediate mismatch detection, non-zero return code, and exact diagnostic formatting.

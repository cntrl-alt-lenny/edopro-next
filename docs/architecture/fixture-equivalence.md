# Legacy and Semantic Fixture Equivalence

This document describes the opt-in verifier for the committed YRPX fixtures:
`duel-chains-battle.yrpX` and `duel-extended.yrpX`.

The verifier is a diagnostic comparison of the semantic client model with the
real legacy client state. It is not a proof of the duel engine or of complete
legacy-client equivalence.

## Execution path

The verifier deliberately uses the direct packet path:

```text
YRPX file
  -> Replay::OpenReplay()
  -> Replay::packets_stream
  -> DuelClient::ClientAnalyze(packet)
       -> ObservationScope::begin()
       -> legacy ClientAnalyze switch
       -> ObservationScope::end()
```

`ReplayMode::StartReplay`, `ReplayMode::ReplayThread`, and
`ReplayMode::ReplayAnalyze` are not part of this CLI path. The verifier
initializes the required `DuelInfo` replay fields, then passes every packet in
`packets_stream` synchronously to `DuelClient::ClientAnalyze`.

The observer is an opt-in C++20 integration target. The legacy `gframe`
boundary remains C++17 and sees only a small C-compatible RAII observer seam.
No `ocgcore` simulation is performed.

## Full legacy semantics, suppressed presentation

Verification sets `dInfo.isCatchingUp = false`. Catch-up is not equivalent to
playback: several upstream handlers return before completing semantic updates
when that flag is set, including confirmation, targeting, attack, and battle
messages.

The verifier instead uses an explicit verification-active seam to suppress
presentation-only work. It avoids GUI widgets, animation waits, camera/driver
coordinate work, and other presentation helpers while retaining the legacy
state mutations needed by the comparison. The verifier constructs a value-only
`Game` plus the required data/configuration/sound managers; it does not create
an Irrlicht device, renderer, fake GUI controls, or a second legacy model.

This is a scoped diagnostic path. It does not claim that every internal legacy
handler or every transient UI side effect has been exercised.

## Comparison scope

After each packet, the observer projects value data from the real legacy
`DuelInfo` and `ClientField` and compares it with the semantic state. The
comparator currently checks exactly:

- both players' life points;
- turn number;
- tracked-card occupancy and normalized controller/zone/sequence location;
- material count and overlay/material topology.

It does not compare card codes, card positions, query statistics, transient
selection/highlight flags, animation transforms, logs, sounds, or widgets.
Those fields are intentionally outside this verifier's current equivalence
claim.

The projection normalizes legacy screen-relative player indices back to the
protocol's player IDs using `Game::LocalPlayer`. The formula is independently
covered by the perspective unit test.

## Failure policy and diagnostics

The verifier fails closed. A successful result requires all of the following:

- every packet in `Replay::packets_stream` was observed;
- every packet decoded successfully;
- no observer or comparison exception occurred;
- a comparison was performed for every expected packet;
- no mismatch was reported; and
- replay processing completed.

The CLI reports expected packets, processed packets, semantic decode failures,
observer failures, comparison failures, comparisons performed, mismatch count,
completion, and the final result. A mismatch includes its packet number,
message name, field, semantic value, and legacy value.

Normal verification is available in an observer-enabled build as:

```text
ygoprodll --semantic-verify-replay tests/fixtures/duel-chains-battle.yrpX
```

The current fixture packet counts are 990 and 1133 respectively. CI runs both
fixtures and requires the command to return zero.

## Fault-path coverage

The hidden observer-only CLI argument
`--semantic-verify-replay-fault` applies a deterministic synthetic legacy LP
mutation at a fixed packet. CI runs it twice, requires a non-zero exit, checks
for an LP mismatch diagnostic, and compares the two diagnostic logs. This
proves the verifier's failure path is active and deterministic; it is not a
production feature.

## Replay buffer correctness

`Replay::ReadData` resizes the packet buffer before copying payload bytes. This
matters because `ParseStream` reuses a `CoreUtils::Packet`; without clearing a
zero-length payload, bytes from the preceding packet can remain visible. The
two fixtures contain zero-length packets, so this is part of the real replay
input path rather than a synthetic test-only accommodation.

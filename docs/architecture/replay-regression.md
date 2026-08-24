# Replay-driven regression testing

Answers the open question raised in [`current-edopro.md`](current-edopro.md):

> Can `.yrp` / `.yrpX` replays be replayed headlessly through `ocgcore` to produce a
> deterministic duel message stream suitable for regression fixtures?

**Answer: yes, and better than expected — for the common case no engine is needed at all.**

Traced against upstream commit `54ea755a`.

## 1. There are two replay formats, and they are fundamentally different

| | `.yrp` (YRP1) | `.yrpX` (YRPX) |
|---|---|---|
| Magic (`ReplayHeader.id`) | `0x31707279` | `0x58707279` |
| Contains | decks, duel params, **player responses** | the **recorded duel message stream** |
| To play back | **re-simulate** through `ocgcore` | replay the stored messages |
| Needs card scripts / database | **yes** | **no** |
| Upstream entry point | `ReplayMode::OldReplayThread` | `ReplayMode::ReplayThread` |

This distinction is the whole finding. A YRP1 is an *input recording*; a YRPX is an
*output recording*.

Modern EDOPro writes `.yrpX`, and — importantly — **embeds the original YRP1 inside it**
as a single packet with the client-side pseudo-message `OLD_REPLAY_MODE` (231, defined in
`gframe/common.h`). Both fixtures in this repository contain one. So a single `.yrpX` file
carries *both* the message stream and everything needed to re-derive it.

## 2. What each file actually contains

Layout from `gframe/replay.h` and `Replay::OpenReplayFromBuffer`:

```
ReplayHeader            id, version, flag, timestamp, datasize, hash, props[8]   (32 bytes)
ExtendedReplayHeader    + header_version, seed[4]                                (+40 bytes)
body                    LZMA1-raw, props from the header, size = datasize
```

The body then holds player names, duel parameters, and either a packet stream (YRPX) or
decks plus responses (YRP1). Packet framing is `uint8 message, uint32 length, bytes`.

`ExtendedReplayHeader` carries **`seed[4]`** — the full 256-bit state of the
`Xoshiro256**` generator (`gframe/RNG/Xoshiro256.hpp`). This is the crux of determinism.

Observed in `tests/fixtures/duel-chains-battle.yrpX`: the outer YRPX header's seed is all
zeros, while the **embedded YRP1 carries the real seed** —
`0x6915be3c09b48e7c 0xc2a4080ecf445567 0xeb63645e7696d093 0x84b4e8aa0bbe0f6b` — with
`start_lp=8000`, `start_hand=5`, `draw_count=1`. That is expected: the streamed replay
does not need a seed, because it does not re-simulate.

## 3. How replay execution currently drives the engine

### YRPX — no engine involved

`ReplayMode::ReplayThread` (`gframe/replay_mode.cpp:77`) iterates the stored stream and
hands each packet straight to the client:

```cpp
auto& current_stream = cur_replay.packets_stream;
for(auto it = current_stream.begin(); is_continuing && !exit_pending && it != current_stream.end();) {
    is_continuing = ReplayAnalyze((*it));
```

`ReplayMode::pduel` exists in this file but is vestigial — it is only ever set to
`nullptr`, and the `end_duel` call is commented out. **Streamed playback never touches
`ocgcore`.**

### YRP1 — full re-simulation

`ReplayMode::OldReplayThread` (`gframe/old_replay_mode.cpp`) does the real thing:

```cpp
const auto& seed = replay_header.seed;
OCG_Player team = { start_lp, start_hand, draw_count };
pduel = mainGame->SetupDuel({ { seed[0], seed[1], seed[2], seed[3] },
                              cur_yrp->params.duel_flags, team, team });
...  pduel->DuelNewCard(&card_info);   // every card of every deck
     pduel->StartDuel();
// then, repeatedly:
     pduel->DuelProcess();
     CoreUtils::ParseMessages(pduel.get());
// and when the engine asks for a decision:
     cur_yrp->GetNextResponse(res);
     pduel->DuelSetResponse(res->response.data(), res->length);
```

So a YRP1 replay is exactly: **seed + duel flags + decks + an ordered list of responses**.
Nothing else feeds the engine.

## 4. Is it deterministic?

**Yes, by construction**, for the engine path:

- **RNG** is seeded explicitly from the replay header. `ocgcore` uses no other entropy
  source in the duel loop.
- **Responses** come from the file in fixed order via `Replay::GetNextResponse`, iterating
  a `std::vector<ReplayResponse>`. There is no interactive input.
- **Decks** are fixed in the file and registered before `StartDuel`.
- **Duel flags / LP / hand / draw** are fixed in the file.

**Rendered timing does not affect the message stream.** The pause, step, skip-turn and
swap-field controls in `replay_mode.cpp` gate *presentation* only — they decide when
`ReplayAnalyze` is called and whether the UI mutex is held. The engine loop in
`OldReplayThread` advances on `DuelProcess`, driven by replay responses, not by frame
timing. There is no wall-clock input to the duel.

The one genuine caveat: determinism holds **for a fixed `ocgcore` and a fixed set of
CardScripts**. Both are versioned outside this repository and both are authoritative. A
CardScript change legitimately changes behaviour — the harness would correctly report a
diff, and that diff would be a true positive, not a flaw.

## 5. The chosen mechanism, and why

Two levels are possible. This milestone implements the first.

### Level 1 (implemented) — trace the recorded stream, no engine

Parse the `.yrpX`, normalise its message stream, compare to a golden file.

```
  .yrpX fixture
        |
   header + LZMA1 body            tools/replaytrace/reader.py
        |
   packet stream                  uint8 msg, uint32 len, bytes
        |
   deterministic text trace       tools/replaytrace/trace.py
        |
   golden comparison              tests/test_replay_trace.py
```

Chosen first because it is *the most trustworthy step available today*:

- **No `ocgcore`, no CardScripts, no card database, no build.** Runs anywhere Python runs.
- **No network fetches in CI**, so no flakiness and nothing to cache.
- Tests exactly the thing M2 will change — our reading of the duel message protocol.
- Fixtures are real recorded duels, not synthesised approximations.

It is implemented in Python (standard library only). `CLAUDE.md` permits Python for
tooling and tests; it is not in the render or input path.

### Level 2 (future) — re-simulate through `ocgcore`

Feed the embedded YRP1's seed, decks and responses to a headless `ocgcore` and compare the
*produced* stream against the *recorded* one. This is strictly stronger: it verifies the
engine, not just our parsing.

It is deliberately not attempted yet because it requires the CardScripts and card database
at a pinned revision, plus a compiled `ocgcore` — real CI cost and a network dependency,
for a check that only becomes necessary once we start modifying engine-adjacent code.
Everything needed is already present in the fixtures, so this is additive when wanted.

## 6. What the trace deliberately excludes

The golden format must contain behaviour and nothing else. Excluded, with reasons:

| Excluded | Why |
|---|---|
| `timestamp` | Wall-clock of recording. Not behaviour. |
| `hash` | Not populated by current EDOPro (observed `0`). |
| `props`, `datasize` | LZMA tuning and compressed size — storage details. |
| Player names | Identity, not behaviour, and a privacy footgun. |
| File paths | Would embed the machine that produced the trace. |
| Pointers / addresses | Never stable. Asserted absent by a test. |

Payloads over 32 bytes are rendered as `len=N sha256:<16 hex>` so the file stays readable
while remaining fully sensitive to change.

## 7. What could be reused unchanged, and what could not

**Reusable as-is.** `gframe/replay.cpp` and `replay.h` already parse both formats
correctly, and `gframe/old_replay_mode.cpp` already contains the whole engine-driving
loop. Level 2 would reuse these almost verbatim.

**Would need extraction.** Both are coupled to `mainGame`:

- `mainGame->SetupDuel(...)` and `mainGame->LoadScript(...)` are `Game` methods, so the
  engine setup path drags in the Irrlicht-owning object.
- `OldReplayThread` writes duel state into `mainGame->dInfo` as it goes.

Extracting a headless duel runner therefore means lifting `SetupDuel` / `LoadScript` out
of `Game` into a presentation-free helper. That is a genuine M2 task and exactly the kind
of seam the architecture survey identified.

The Level 1 harness sidesteps this entirely, which is why it comes first.

## 8. Risks and unknowns

- **Trace granularity is structural, not semantic.** It records message ids and payload
  digests, not decoded fields. A change that alters payload *contents* is caught; a change
  that alters *interpretation* of unchanged bytes is not. That is acceptable now — there
  is no semantic decoder yet — and is the natural extension in M2.
- **Two fixtures is a small corpus.** They cover a lot (see below) but not everything:
  no Link or Pendulum summons, no tag duel, no relay, no single/hand-test mode, no
  surrender or timeout paths.
- **Fixtures pin a client version** (`0x000b0029`). A future upstream format change would
  require regenerating them; the reader would fail loudly rather than silently mis-parse.
- **Level 2 remains unproven.** Nothing here demonstrates that a re-simulated stream
  matches the recorded one. It is well-evidenced but untested, and should not be described
  as working until it is.

## 9. Fixture coverage

Both fixtures are real duels, sanitised by `tools/make_fixture.py` (player names replaced;
message stream verified byte-identical before and after).

| | `duel-chains-battle` | `duel-extended` |
|---|---|---|
| Size / packets | 4.5 KB / 990 | 6.3 KB / 1133 |
| Turn progression | `NEW_TURN` ×5, `NEW_PHASE` ×23 | ✓ |
| Summons | Normal ×2, Special ×4, Set ×3 | ✓ |
| **Chains** | `CHAINING`/`CHAINED`/`SOLVING`/`SOLVED` ×8, `CHAIN_END` ×6 | ✓ |
| Targeting | `BECOME_TARGET` ×2 | ✓ |
| Card movement | `MOVE` ×32, `POS_CHANGE` ×2 | ✓ |
| Battle | `ATTACK` ×3, `BATTLE` ×2, damage step | ✓ |
| Draw / damage | `DRAW` ×8, `DAMAGE` | ✓ |
| Selection / reveal | `CONFIRM_CARDS` ×2, `SHUFFLE_*` | ✓ |
| Terminal | `WIN` | ✓ |

## 10. Verification performed

The harness was proved to *detect* change, not merely to run: perturbing a single golden
line (`MSG_CHAINING 8` → `7`) produced a failing test with a unified diff naming the exact
message and count, and restoring it returned the suite to green.

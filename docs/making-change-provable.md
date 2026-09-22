# Making change provable

The property this project ultimately needs to be able to assert is *"this changed
presentation, not duel behaviour"*. Reaching it takes two layers. **The first one is
built.**

> **M1 establishes a deterministic recorded-protocol regression baseline.**

Real recorded duels are parsed into a deterministic text trace and compared against golden
files, so any change in how this project reads the duel protocol produces a readable diff
naming the exact message:

```diff
   62 MSG_SPSUMMONING              4
   63 MSG_SPSUMMONED               1
-  70 MSG_CHAINING                 7
+  70 MSG_CHAINING                 8
   71 MSG_CHAINED                  8
```

**What that does prove:** the replay parser, the message-id table and the normalisation
stay consistent, deterministically and on every supported Python (CI tests 3.10 and 3.12)
— so the semantic decoding work in M2 can be tested against real duels from its first
commit.

**What it does not prove:** that a *live* duel still behaves the same. The fixtures are
frozen recordings and the harness never loads `ocgcore`, so no C++ change can fail this
suite. Proving live engine equivalence needs **Level 2** — re-simulating the recorded
inputs through a headless `ocgcore` and comparing against the recorded output. The fixtures
already carry every input that needs (seed, decks, ordered responses); what is missing is
an engine host, a pinned card database and pinned CardScripts. That is
[a milestone of its own](architecture/replay-regression.md#5-the-chosen-mechanism-and-why),
not a footnote to this one.

The corpus is three sanitised fixture files: two real duels in the recorded output format
(`.yrpX`) and one recording in the input format (`.yrp`), so both parsers are exercised by
real data. They are between 1 and 7 KB each, with no player identity and no card artwork.
They cover turn progression, normal/special/set summons, **chains**, targeting, card
movement, battle and damage steps, draws, reveals and a terminal win. The harness is Python
standard library only and runs headlessly, in well under a second on this Windows machine
(`python -m unittest tests.test_replay_trace`: 0.07 s). [The investigation](architecture/replay-regression.md)
sets out the full boundary, and its section 9 lists what each fixture covers.

## A second trace, one layer up

M1's trace is *structural*: message ids and payload digests, with no attempt to interpret
the bytes. M2 adds a *semantic* trace over the same fixtures, produced by the new decoder,
so a change in what the protocol is understood to **mean** also produces a readable diff:

```diff
   259 ChainLinkAdded link=1 instance=37 code=504700123 from=SZONE[p0:2]
-  299 CardMoved instance=37 SZONE[p0:2] -> GRAVE[p0:1] reason=RULE
+  299 CardMoved instance=37 SZONE[p0:2] -> GRAVE[p0:0] reason=RULE
   301 ChainEnded links=1
```

Both traces are kept. A container-parsing change moves the first; a decoder change moves
the second. Alongside the golden comparison, the semantic suite asserts directly that no
packet in either fixture is malformed, unknown or inconsistent, and that the model's
integrity invariants hold at the end of the duel — claims a golden file alone could not
make, because a golden blesses whatever it is shown.

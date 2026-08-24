# Vision

## The sentence

> Preserve the engine. Expose clean semantics. Modernise the client.

Not *"rewrite EDOPro in my favourite language."* The distinction resolves most design
arguments before they start.

## What is actually being fixed

EDOPro automates one of the most rules-dense card games ever designed, across the whole
card pool, correctly, maintained by volunteers over years. That is the hard, valuable,
irreplaceable part — and it already works.

What has aged is the layer around it. The client descends from YGOPro: fixed-coordinate
widgets on a patched Irrlicht 1.8, with no layout system, no DPI awareness, no
accessibility layer, no declarative focus model and no animation framework. Those are not
failures of effort; they are the consequences of a toolkit choice made a long time ago.

So the project's scope is narrow on purpose: **replace presentation, preserve behaviour.**

## What success looks like

Someone opens it and thinks *"this is EDOPro?"* — and then plays a duel that behaves
exactly as it always did.

Concretely, in rough priority:

- Card artwork is dominant, sharp and fast to load
- Chain state, targeting and legality are obvious without prior knowledge
- Typography and spacing feel deliberate at every window size
- Motion explains what just happened, and never hides what is legal
- Keyboard, mouse and controller are all first-class
- It starts fast and feels native, on desktop and eventually on a Steam Deck
- It is honest about what it does not do yet

## The boundary that makes it possible

> The rules engine must not become the UI. The UI must not implement game rules.

The client should consume a semantic model — players, life points, zones, positions,
phase, chain, prompts, legal choices, valid targets, revealed information, history — and
send back responses. It should never infer rules, never compute legality, and never hold
a transform matrix in the same struct as a card's attack value.

That boundary is not only about tidiness. It is what lets the same model serve the UI,
tests, tooling and eventually AI, and what makes it possible to prove that a change
altered presentation and not duel behaviour.

## What this project will not do

- Reimplement Yu-Gi-Oh's rules
- Replace Project Ignis's Lua CardScripts — they are authoritative and worth protecting
- Clone Master Duel's visual identity
- Rewrite working engine logic because it looks old
- Ship a feature in the README before it exists

## On borrowing from Forge

MTG Forge is admirable because semantic operations are reusable: a card declares
*"destroy all creatures"* and the engine, the AI and the UI all understand it without
bespoke code per card.

The lesson worth taking is **not** "replace Lua." It is that *semantic meaning should be
reusable across cards, AI, UI and tooling.* A future optional metadata layer alongside
the CardScripts could power explanations, badges, search, tutorials, accessibility and AI
reasoning — while `ocgcore` and the Lua remain the only things that decide legality.

That is a research track, deliberately placed after the client work, and it must never
become a second source of truth about the rules.

## Temperament

- Incremental over heroic. The repository should always be in a usable state.
- Reversible over clever.
- Measured over assumed — especially performance claims.
- Honest over impressive. A short accurate README beats a long aspirational one.
- Respectful of what exists. This project stands on years of other people's careful work.

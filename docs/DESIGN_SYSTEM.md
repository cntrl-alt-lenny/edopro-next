# Design system

The implementation is [`ui/qml/Theme.qml`](../ui/qml/Theme.qml). That file is the single
source of truth; this document explains the reasoning so the tokens are extended
consistently rather than accumulating one-off values.

## The governing idea

**The cards are the product.** Every other decision follows from that. Chrome exists to
present artwork and communicate rules state, and then to get out of the way. When a
choice is ambiguous, prefer the quieter option.

This is why the palette is near-neutral, why there is exactly one accent colour, and why
there are no gradients, glass effects or glows anywhere in the system.

## Explicitly rejected

Named so they do not creep back in: large gradients, glassmorphism, neon accents,
uniformly heavy corner rounding, web-dashboard card-grid aesthetics, small grey body
text, and animation that exists to be noticed.

## Colour

Dark-first, with surfaces carrying a very slight cool cast so that warm card artwork
reads correctly against them.

| Token | Role |
|---|---|
| `bg` | Application background |
| `surface` | Panels, rails, inset regions |
| `surfaceRaised` | Elements above a surface |
| `surfaceHover` | Hover state only |
| `border` / `borderStrong` | Hairline separation. Preferred over shadows |
| `textPrimary` / `textSecondary` / `textTertiary` | A three-step hierarchy, no more |
| `accent` | Muted gold. **Selection and focus only.** Never a large fill |
| `success` / `warning` / `danger` | Status, desaturated to sit in the palette |

The accent is deliberately restrained gold rather than a saturated brand colour: it reads
as considered next to Yu-Gi-Oh's card frames without imitating them.

A light theme is not yet implemented. The token structure supports one — screens reference
role names, never literal colours — but claiming light support before it exists would
violate the project's honesty rules.

## Spacing

A 4 px base scale: `space1` 4, `space2` 8, `space3` 12, `space4` 16, `space5` 24,
`space6` 32, `space7` 48, `space8` 64.

Layouts compose these. A raw pixel number in a screen is a bug; if a value is genuinely
needed, add it here with a name.

## Typography

Sizes are in **points**, not pixels, so they track platform DPI scaling.

| Token | Use |
|---|---|
| `textDisplay` | One per screen at most |
| `textTitle` | Section titles |
| `textHeading` | Small-caps headings, with positive tracking |
| `textBody` | Default |
| `textCaption` | Metadata, status, supporting detail |

Tracking is part of the system: display sizes get slightly negative tracking, small-caps
headings get positive tracking. That is most of what separates deliberate typography from
default typography.

Monospace (`fontFamilyMono`) is reserved for machine facts — versions, hashes, identifiers.

## Motion

Motion communicates state change. It is never decoration.

Durations: `durFast` 120 ms (hover, colour), `durNormal` 180 ms (layout, selection),
`durSlow` 260 ms (screen-level transitions). Easing is `OutCubic` normally and `OutQuint`
for emphasis — both decelerate, so motion settles rather than snapping.

Motion is justified for: a card changing zone, chain formation and resolution, target
arrows, card reveal, draw, life-point change, phase transition, selection confirmation.

**Animation must never obscure rules state.** If a player cannot tell what is legal
because something is mid-animation, the animation is wrong.

**Reduced motion** is honoured through `Theme.enableMotion`; every animation multiplies
its duration by `motionScale`, so disabling it makes transitions instant rather than
broken.

## Layout and focus

Layout responds to **available width against `breakpointCompact`**, never to a fixed
resolution. The navigation rail collapses to icons below it. `contentMaxWidth` keeps long
text readable on wide displays.

Targets: 1280×720 minimum desktop, 1920×1080 common, 2560×1440+, high-DPI, and 1280×800
for the Steam Deck — which is the shell's default window size, deliberately.

Focus is a first-class concern, not an afterthought:

- Every interactive element is reachable by keyboard and shows a visible focus ring.
- The focus ring is drawn outside the fill so gaining focus never shifts layout.
- Controller support will map onto this same focus model. It must never be implemented as
  mouse-cursor emulation.
- Because focus is declarative, it is testable — which is the point.

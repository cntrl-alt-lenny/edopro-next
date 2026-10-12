# ADR 0014 — Known decimal codes in the search box retain filters

## Status

Proposed for Brain review in batch 28; implements Brain's dispatch decision.

## Context

Users need to find a known card by code without its digits appearing in its
name or description. Upstream's numeric shortcut bypasses all property and
visibility filters and wraps overflowing input. ADR 0005 defines
`SearchQuery::exact_code` as a ranking hint, which must remain composable.
Source quotations and precise differences are in
[card-code-search.md](../architecture/card-code-search.md).

## Decision

The UI input adapter trims outer whitespace and selects lookup only for a
complete ASCII decimal, nonzero uint32 present in the current catalogue.
Leading zeroes are allowed; conversion checks overflow. Known codes return
only that card, subject to every structured and advisory visibility filter.
Other inputs retain ordinary text search, ranking and result cap. A known
but excluded card yields no results. Clearing restores browsing; refreshes
re-evaluate existence and active filters.

## Alternatives and consequences

Copying upstream's bypass would contradict visible active filters, including
unofficial and whitelist restrictions. Reinterpreting `exact_code` as a
restriction would break the reviewed data API. Rejecting every numeric-looking
unknown input would lose existing name/description matches. These alternatives
are rejected in favour of a bounded adapter with no data/policy changes.

This deliberately differs from upstream's untrimmed, wrapping conversion and
filter bypass. It does not add `||`, `&&`, sigils or full keyboard-search parity.
The existing search index evaluates structured constraints; the adapter
restricts identity afterward. This retains one filtering implementation and
requires no dependency or QML control. It scans the normal snapshot, with no
new performance guarantee.

Brief-ID: 015-2026-09-22-deck-builder-legality-ui

Status: active

## MODE: IMPLEMENTATION

## Goal

A user building a deck in the QML deck builder can see whether that deck
would be accepted, and why not, against a banlist and a ruleset that they
chose, visibly. Legality is still computed only by
`policy::validate_deck()`. `ui/` supplies the inputs and renders the result;
it never decides a rule.

## Why this is next

This is the last unfinished legality work in M3, the milestone the roadmap
currently cares about. `policy/` has computed deck legality, faithfully to
upstream, since 2026-08-31, but nothing in `ui/` calls it: a deck built in the
new client shows no legality at all.

## The decision this round implements (owner, 2026-08-31)

Brief 001's accepted research,
[`docs/architecture/deck-builder-legality.md`](../architecture/deck-builder-legality.md),
found that upstream's deck editor never calls
`CheckDeckContent`/`CheckDeckSize`. Those run only at duel entry. For five of
`policy::ValidationPolicy`'s six fields, upstream has no editor-time value at
all. §7 of that document set out three options. **The owner chose option
(b):** give the deck builder a **visible, named ruleset choice now**, and do
not defer legality to M4. The owner accepted the risk that M4's lobby work
may later want a different mechanism.

What that decision implies, as constraints with their sources:

- **Banlist.** `lflist` gets its own user-visible banlist selection, with
  upstream precedent in the editor's `cbDBLFList` (research §7.1). "No banlist
  selected" is an explicit choice that maps to `std::nullopt`, distinct from a
  concrete "N/A" list (`deck-legality.md` §5). Banlist files come from paths
  the user supplies explicitly, following the existing `--card-db` convention.
  Never a fabricated fallback (`deck-builder-ui.md` §4). Never commit a
  real banlist file.
- **Ruleset.** The other five fields come from a **ruleset the user can see
  and change**, whose name and values this project owns and documents as its
  own, not upstream's (research §7.2(b)). It is never a hidden default.
- **Advisory, not blocking.** Legality is presented as advice ("would not be
  accepted at duel entry: …"), never as a gate on editing or saving. Upstream's
  editor enforces none of this (research §7.3).
- **Boundaries.**
  - `validate_deck()` remains the only place legality is computed
    (`deck-legality.md` §12).
  - Any ruleset object is plain data built in `ui/`'s C++ adapter layer,
    never in QML and never inside `policy/` (ADR 0006, ADR 0007 Decision 3).
  - QML only renders the resulting errors.
- **ADR.** Record the ruleset decision in a new ADR, alongside ADR 0006 and
  ADR 0007.

## Base and branch

Branch `m3/deck-builder-legality-ui`. Its first commit is Brain's close-out
of round 014; it touches only `docs/briefs/`, `docs/state.md` and
`docs/agents/model-notes.md`. Continue on top of it. Do not rebase or
force-push. The close-out is part of the reviewed range: report any error in
it, and do not rewrite it.

## Required investigation

1. **Where each ruleset value comes from.** For every value your ruleset(s)
   supply to the five non-banlist fields, find the closest upstream source —
   for example the defaults upstream's host settings use (`HostInfo` in
   `gframe/network.h` and wherever its defaults are set) — and quote it at
   file and line. Where no upstream source exists, say so and state the value
   as this project's own choice. Never present an invented value as
   upstream's.
2. **How many rulesets.** Decide how many named rulesets to offer (the
   research suggests one, "Standard OCG/TCG"), and argue it. More rulesets
   mean more invented surface.
3. **Presentation.** Decide how each `DeckValidationError` is presented to
   the user, and how the display stays current as the deck, banlist or
   ruleset changes. Keep all rule text out of QML: error kinds are named by
   the adapter.

## Scope

- **`ui/` C++ adapter layer**, `ui/src/deckbuilder/` or a sibling you argue
  for. This covers the banlist loading and selection, the ruleset type and its
  mapping to `ValidationPolicy`, and exposing validation results to QML.
- **Linking `policy/` into the `ui/` build**, the way `data/` is already
  linked.
- **`DeckBuilderScreen.qml`**, and any small component it needs, to show the
  two selections and the advisory result.
- **Tests:**
  - adapter tests for the ruleset-to-policy mapping;
  - null versus "N/A" banlist behaviour;
  - errors surfaced for a deliberately illegal deck;
  - a QML-level test that the screen loads and shows the result.

  Use only synthetic fixtures.
- **Documentation:**
  - the new ADR;
  - `docs/architecture/deck-builder-ui.md` updated;
  - `docs/ROADMAP.md` M3's deck-builder item updated truthfully. The item
    stays unchecked, because its other parts (filters, artwork, keyboard
    parity, classification) remain.

  Then regenerate the README status block with
  `python tools/generate_readme_status.py` so the suite stays green.

## Non-scope

- Automatic Main/Extra classification, artwork, the legacy sigil search
  grammar, structured filters, keyboard/controller parity.
- Any change to `policy/`'s behaviour or public contract. If you find a
  defect there, stop and report it.
- `gframe/`, `ocgcore/`, `integration/legacy/`, `.github/workflows/`,
  repository settings, the duel field.
- No card artwork, `.cdb` or banlist data committed.

## Protected invariants

- The UI implements no game rule. A reviewer must be able to find every
  legality decision inside `policy/`.
- Existing deck-builder behaviour is unchanged:
  - `.ydk` open, save and new;
  - the dirty-state contract;
  - explicit section editing.

  Existing `ui/` tests survive; name any changed assertion with its before
  and after property.
- `client/`, `data/` and `policy/` stay free of Qt.

## Acceptance criteria

- `ui/` configures, builds and passes `ctest` with
  `-DEDOPRO_NEXT_UI_TESTS=ON` on this Windows machine (MSVC, Qt 6.8.3,
  vcpkg; see `AGENTS.md`'s Windows notes). The offscreen clean-QML-load check
  from `.github/workflows/edopro-next.yml` also passes.
- `data/` and `policy/` still configure, build and pass `ctest`.
- A test demonstrates, through the adapter, that an illegal deck shows a
  specific error under the chosen ruleset and banlist, and that "no banlist"
  and "N/A" behave differently where `policy/` says they do.
- The Python suite and the three generator `--check`s pass from Git Bash,
  including the README status check.
- CI is green at the final head.
- The PR body begins `DO NOT MERGE — under review` and passes
  `python tools/check_pr_evidence.py`.

## Required evidence

- Checkout check, and the tool, model and OS (the OS from `cmd /c ver`).
- The investigation-1 table, with each value, its upstream file:line or "this
  project's choice", and a quote.
- Real configure, build and `ctest` output for `ui/`, `data/` and `policy/`,
  with counts.
- The offscreen QML check.
- **Presentation evidence:** at least one real capture of the deck builder
  showing the banlist and ruleset selections and an advisory result for an
  illegal deck. `edopro_next_shell --capture <path>` exists for this. Say what
  you verified visually and what you did not.
- The Python suite and generator output.
- Check-run conclusions at the final head.
- Any sentence removed from a durable document, listed explicitly.
- What you did not run.
- No replay-harness result is evidence about duel behaviour; this round must
  not claim it.

## Completion-report schema

The standard report in
[`docs/agents/roles/worker.md`](../agents/roles/worker.md), plus: the ruleset
values table, the number of rulesets and why, the presentation decision, and
the ADR's number and title.

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

## Reopened corrections S1-S3

Brain reviewed PR #31 at head `259b7bcd1dadc676009e4143a6bc14adb1bccd4a` and
reopened the round. Recorded here verbatim, as the round's authoritative text;
everything else in the round is accepted and must not be reworked.

S1 — docs/adr/0010-deck-builder-ruleset-and-legality-ui.md cites upstream
source that does not say what the ADR claims. Observed by Brain at 259b7bcd.
The ADR names constants that do not exist anywhere in gframe/: MIN_MAIN_DECK,
MAX_MAIN_DECK, MAX_EXTRA_DECK, MAX_SIDE_DECK, "ALLOWED_OCG_TCG = 0x3" and
RULE_RITUAL_IN_EXTRA (grep finds none). It cites gframe/deck_con.cpp:1159-1162,
which is card-search text matching, and generic_duel.cpp lines 371-384 for
content those lines do not contain. It gives allowed_cards as
policy::CardScope::Any, while the code uses policy::AllowedCardPool::OcgAndTcg.
AGENTS.md makes upstream source the arbiter: quote what you actually read.
Required outcome: every upstream citation in ADR 0010,
ui/src/deckbuilder/ruleset.cpp's comments and in
docs/architecture/deck-builder-ui.md points at a file:line you re-read, with a
verbatim quote of what is there. Remove any value, constant or line that is
not in the source.

S2 — the "Standard OCG/TCG" ruleset uses AllowedCardPool::OcgAndTcg and
presents it as upstream-grounded, but upstream's default host card pool
differs. gframe/game_config.inl:21 sets `OPTION(uint32_t, lastallowedcards,
3)`, game.cpp:1160 selects that index in cbRule, duelclient.cpp:229 sends it
as `info.rule`, and generic_duel.cpp:381 casts it to DuelAllowedCards, whose
index 3 (gframe/deck_manager.h:32-37) is ALLOWED_CARDS_WITH_PRERELEASE.
Required outcome: the ruleset's card pool either matches upstream's default,
or is recorded plainly in ADR 0010 as this project's own choice that diverges
from upstream's default, with the divergence and its reason stated. CLAUDE.md
and AGENTS.md require deliberate divergence from upstream to be recorded,
never silent. The same honesty applies to every other ruleset field: say for
each whether it matches an upstream default, or is this project's choice.

S3 — with the default selection "No banlist", the deck builder reports "Deck
is legal for duel entry under this ruleset and banlist." for decks that duel
entry with any concrete banlist would reject. Faithfully to upstream's null
LFList (policy/src/deck_validation.cpp:262-270,
docs/architecture/deck-legality.md section 5), policy::validate_deck() then
skips the card-scope, section-placement and three-copy checks. So a deck with
four copies of one card, or an Extra Deck monster in the Main Deck, shows as
legal, and nothing on screen says those checks are off. Required outcome:
whenever no banlist is selected, the screen makes visible that these checks
are not being made. The wording and its derivation live in the C++ adapter,
never in QML, and policy/'s behaviour stays unchanged. Tests must pin that
state, and must fail at 259b7bcd. Whether "No banlist" should stay the
default selection is your call to argue in the report; it must remain a
visible, distinct choice either way.

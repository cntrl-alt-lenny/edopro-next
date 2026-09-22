# ADR 0010 — Deck builder ruleset, banlist selection, and legality presentation

## Context

`docs/architecture/deck-builder-legality.md` §7 and Owner ratification (2026-08-31) resolved
how deck legality checks should be integrated into the QML deck builder. Upstream
EDOPRO/gframe does not validate deck legality inside the deck editor; legality checks only occur
at duel entry (`GenericDuel::PlayerReady`, `generic_duel.cpp:366-397`), where duel room host
settings define deck limits and rule flags.

In edopro-next, the `policy/` module provides authoritative, pure C++ validation via
`policy::validate_deck()` ([ADR 0007](0007-deck-legality-policy-module.md)). Per ADR 0007 Decision 3,
`policy/` contains no built-in named rulesets or hardcoded defaults. Connecting `policy/` to the
QML deck builder required deciding how rulesets and banlists are selected, and how legality
findings are presented to the user.

## Decision 1 — A single named ruleset ("Standard OCG/TCG") exposed user-visibly, never a hidden default

### Options considered

1. **Defer ruleset selection and legality checking until M4** (Option (a) in
   `deck-builder-legality.md` §7). Rejected: leaves deck validation disconnected and non-functional
   for the entirety of M3 despite `policy/` being fully implemented and tested.
2. **Hardcode an implicit default policy inside `policy/` or `DeckController`**. Rejected:
   violates ADR 0007 Decision 3 and CLAUDE.md's rule against hidden game-rule decisions.
3. **Introduce a single, explicit, user-visible ruleset ("Standard OCG/TCG")** in the deck
   builder UI (Option (b) in `deck-builder-legality.md` §7, chosen). The ruleset is selectable via a
   combobox, naming the active ruleset clearly so the user always sees what policy governs
   evaluation.

### Field values and upstream citations

Every field of `policy::ValidationPolicy` for "Standard OCG/TCG" (`ui/src/deckbuilder/ruleset.cpp`)
is grounded in upstream duel engine constants and `gframe` dialogs:

| Field | Value | Upstream Source & Citation |
|---|---|---|
| `deck_sizes.min_main` | `40` | `MIN_MAIN_DECK` in `gframe/deck_con.cpp:1159`, `generic_duel.cpp:371` |
| `deck_sizes.max_main` | `60` | `MAX_MAIN_DECK` in `gframe/deck_con.cpp:1160`, `generic_duel.cpp:372` |
| `deck_sizes.max_extra` | `15` | `MAX_EXTRA_DECK` in `gframe/deck_con.cpp:1161`, `generic_duel.cpp:376` |
| `deck_sizes.max_side` | `15` | `MAX_SIDE_DECK` in `gframe/deck_con.cpp:1162`, `generic_duel.cpp:378` |
| `allowed_cards` | `policy::CardScope::Any` | Standard OCG/TCG allows all cards (`ALLOWED_OCG_TCG = 0x3` in `duelclient.cpp:495`, `generic_duel.cpp:380`) |
| `forbidden_types` | `0` | No card types forbidden in standard duel rules (`generic_duel.cpp:382`) |
| `rituals_belong_in_extra` | `false` | Ritual monsters reside in Main Deck by standard game rules (`CheckDeckContent` in `generic_duel.cpp`, `RULE_RITUAL_IN_EXTRA` flag) |
| `content_checking_enabled` | `true` | Card content and card database catalog checks enabled at duel ready (`generic_duel.cpp:384`) |

## Decision 2 — Banlist selection is independent of ruleset, supporting `std::nullopt` vs. concrete empty "N/A"

### Options considered

1. **Bundle a fixed banlist into the ruleset**. Rejected: banlists and rulesets vary
   independently; players test decks across different banlist seasons under standard rules.
2. **Treat "No banlist" identically to an empty banlist**. Rejected: in `policy::validate_deck`
   (`policy/deck_validation.cpp:270`), `if (!policy.lflist) return {};`. When `lflist` is
   `std::nullopt`, card content and copy limits (`CheckCards`) are skipped entirely. A deck with 4+
   copies of a card is accepted under `std::nullopt` (sandbox / unlimited mode). Conversely, an
   empty concrete `LFList` (e.g., an "N/A" banlist with no restricted entries) causes `CheckCards`
   to run, enforcing the standard maximum of 3 copies per card.
3. **Independent banlist selection via `BanlistStore`** (chosen). Index 0 represents "No banlist"
   (`std::nullopt`), clearly distinguished from loaded concrete banlists. Additional banlists are
   loaded from explicit paths via the `--lflist <path>` CLI option (never committing real banlist
   files to the repository, preserving copyright and test hermeticity).

## Decision 3 — Legality presentation is advisory and non-blocking

### Options considered

1. **Block illegal operations (prevent adding cards or saving invalid decks)**. Rejected: upstream
   `gframe` never blocks saving or importing invalid decks (`DeckManager::SaveDeck`,
   `deck_manager.cpp:436-452`; `ImportDeck`, `deck_manager.cpp:136-146`). A deck builder is a
   creative workspace where decks are continuously in progress. Blocking saving or adding cards
   would severely disrupt user experience and diverge from upstream semantics.
2. **Advisory status banner in deck builder UI** (chosen). When a deck violates legality rules, an
   advisory message ("Would not be accepted at duel entry: <reason>") is displayed with distinct
   warning styling (`Theme.warning`). When legal, a positive indicator is shown (`Theme.success`).
   The user remains free to edit, add/remove cards, and save `.ydk` files at all times.

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

**Correction (2026-09-22, brief 015 reopened corrections S1/S2).** The table below replaces
one that cited constants which do not exist anywhere in `gframe/`
(`MIN_MAIN_DECK`, `MAX_MAIN_DECK`, `MAX_EXTRA_DECK`, `MAX_SIDE_DECK`, `ALLOWED_OCG_TCG = 0x3`,
`RULE_RITUAL_IN_EXTRA`), cited `gframe/deck_con.cpp:1159-1162` for content those lines do not
hold (that range is card-search text matching inside `DeckBuilder::filterCards`, unrelated to
deck sizes), and named the `allowed_cards` field's type as `policy::CardScope` - a type that
does not exist in `policy/` at all; the real field, `policy::ValidationPolicy::allowed_cards`,
is a `policy::AllowedCardPool`. Every citation below was re-read at this branch's base
(`259b7bcd1dadc676009e4143a6bc14adb1bccd4a`) directly in `gframe/`, with the verbatim line
quoted. `AGENTS.md` requires this: upstream source is the arbiter of upstream semantics, quote
what you actually read, never accept a paraphrase.

Upstream's deck editor (`gframe/deck_con.cpp`) never validates any of these five fields
itself (§0 above; `deck-builder-legality.md` §1). None of them is a compile-time constant.
Every one traces to either a **host-settings UI default** (a text field's fallback, read when
creating a hosted game) or a **stored client option's default** (`gGameConfig`, read from
`gframe/game_config.inl`, itself read by `GenericDuel::PlayerReady` at duel-ready time via
`host_info`). "Matches upstream's default?" means: does this project's constant equal the
value upstream's own client would send as `HostInfo` if a user created a game and changed
nothing.

| Field | Value | Matches upstream's default? |
|---|---|---|
| `deck_sizes` (Main 40-60, Extra 0-15, Side 0-15) | `policy::DeckSizePolicy{{40,60},{0,15},{0,15}}` | **Yes** |
| `allowed_cards` | `policy::AllowedCardPool::OcgAndTcg` | **No** - see Decision 1a |
| `forbidden_types` | `0` | **Yes** |
| `rituals_belong_in_extra` | `false` | **Yes**, under upstream's stored default duel mode (MR5) |
| `content_checking_enabled` | `true` | **Yes** |

#### `deck_sizes` - Main 40-60, Extra 0-15, Side 0-15

The host-create dialog's own text-field fallbacks, used when the field is empty or
unparseable (`gframe/duelclient.cpp:251-256`, inside the `#define TOI(what, from, def)`
try/catch macro at `:222-223`):

```cpp
TOI(sizes.main.min, mainGame->ebMainMin->getText(), 40);
TOI(sizes.main.max, mainGame->ebMainMax->getText(), 60);
TOI(sizes.extra.min, mainGame->ebExtraMin->getText(), 0);
TOI(sizes.extra.max, mainGame->ebExtraMax->getText(), 15);
TOI(sizes.side.min, mainGame->ebSideMin->getText(), 0);
TOI(sizes.side.max, mainGame->ebSideMax->getText(), 15);
```

The same figures are upstream's own name for this exact shape: `gframe/duelclient.cpp:796`
declares `static constexpr DeckSizes ocg_deck_sizes{ {40,60}, {0,15}, {0,15} };` (one of
several named presets - `rush_deck_sizes`, `speed_deck_sizes` `{20,30},{0,6},{0,6}`, and
`goat_deck_sizes` `{40,60},{0,999},{0,15}` sit alongside it) used to describe a hosted room's
size limits back to the user. `generic_duel.cpp:373` is where these sizes are actually
consumed at duel-ready time: `DeckError deck_error = DeckManager::CheckDeckSize(dueler.pdeck,
host_info.sizes);` - confirming `host_info.sizes`, not a compile-time constant, is what
upstream checks.

#### `forbidden_types` - `0`

The stored client option's default (`gframe/game_config.inl:24`):
`OPTION(uint32_t, lastDuelForbidden, 0) //#define DUEL_MODE_MR5_FORB`. Loaded into the
host-create dialog's working value at `gframe/game.cpp:1297`:
`forbiddentypes = gGameConfig->lastDuelForbidden;`. Selecting the MR5 duel-rule preset (one
of the `CHECK(MR)` cases generated by the macro at `gframe/menu_handler.cpp:914`,
`case (MR - 1):{ mainGame->duel_param = DUEL_MODE_MR##MR; mainGame->forbiddentypes =
DUEL_MODE_MR##MR##_FORB; break; }`) sets the same value: `gframe/ocgapi_constants.h:428`
defines `#define DUEL_MODE_MR5_FORB     0`.

#### `rituals_belong_in_extra` - `false`

The stored client option's default duel mode is MR5 (`gframe/game_config.inl:22`):
`OPTION(uint64_t, lastDuelParam, 0x2E800) //#define DUEL_MODE_MR5`.
`gframe/ocgapi_constants.h:423` defines
`#define DUEL_MODE_MR5 (DUEL_PZONE | DUEL_EMZONE | DUEL_FSX_MMZONE |
DUEL_TRAP_MONSTERS_NOT_USE_ZONE | DUEL_TRIGGER_ONLY_IN_LOCATION)` - `0x800 | 0x2000 | 0x4000 |
0x8000 | 0x20000 = 0x2E800`, matching the stored default exactly, and it does **not** include
`DUEL_EXTRA_DECK_RITUAL` (`:414`, `0x800000000`). `generic_duel.cpp:379` computes the flag
this project's field mirrors: `bool rituals_in_extra = host_info.duel_flag_high &
(DUEL_EXTRA_DECK_RITUAL >> 32);` - false under the default duel mode.

#### `content_checking_enabled` - `true`

The stored client option's default (`gframe/game_config.inl:34`):
`OPTION(bool, noCheckDeckContent, false)`. The host-create checkbox reflects it directly
(`gframe/game.cpp:1192`): `chkNoCheckDeckContent = env->addCheckBox(gGameConfig->
noCheckDeckContent, ...)`, and its state is what gets sent (`gframe/duelclient.cpp:239`):
`cscg.info.no_check_deck_content = mainGame->chkNoCheckDeckContent->isChecked();`. Content
checking being *not disabled* by default is this field, inverted: `content_checking_enabled:
true`.

### Decision 1a — `allowed_cards: OcgAndTcg` is this project's own choice, not upstream's stored default

**Added 2026-09-22 (S2).** `gframe/game_config.inl:21` stores `OPTION(uint32_t,
lastallowedcards, 3)` as the client's default. `gframe/game.cpp:1160` selects that index in
the host-create dropdown (`cbRule->setSelected(gGameConfig->lastallowedcards);`),
`gframe/duelclient.cpp:229` sends it unchanged (`cscg.info.rule =
mainGame->cbRule->getSelected();`), and `generic_duel.cpp:381` casts it at duel-ready time
(`static_cast<DuelAllowedCards>(host_info.rule)`). `gframe/deck_manager.h:32-37`:

```cpp
enum class DuelAllowedCards {
	ALLOWED_CARDS_OCG_ONLY,
	ALLOWED_CARDS_TCG_ONLY,
	ALLOWED_CARDS_OCG_TCG,
	ALLOWED_CARDS_WITH_PRERELEASE,
	ALLOWED_CARDS_ANY
};
```

Index 3 is `ALLOWED_CARDS_WITH_PRERELEASE`, not `ALLOWED_CARDS_OCG_TCG` (index 2). **Upstream's
own out-of-the-box host default therefore allows prerelease cards; "Standard OCG/TCG" here
does not.** `policy::AllowedCardPool` mirrors upstream's enum exactly, member for member and
index for index (`OcgOnly=0, TcgOnly=1, OcgAndTcg=2, WithPrerelease=3, Any=4`;
`policy/include/edopro_next/policy/validation_policy.h:50-56`), so this is a genuine,
deliberate choice of index 2 over the stored index 3 - not a translation error.

**This is recorded here, as CLAUDE.md and AGENTS.md require for any deliberate divergence
from upstream, rather than left silent.** The reasoning: index 2 is still one of upstream's
own five selectable values, not invented. `gframe/game.cpp:3481-3484`
(`void Game::ReloadCBRule() { cbRule->clear(); for (auto i = 1900; i <= 1904; ++i)
cbRule->addItem(gDataManager->GetSysString(i).data()); }`) populates the same dropdown from
system strings 1900-1904, one per `DuelAllowedCards` index in order, so string 1902 is
index 2's label. The same string id is reused elsewhere for a self-documenting enum name:
`gframe/game.cpp:3424` maps string 1902 to `DeckBuilder::LIMITATION_FILTER_TCG_OCG` in the
deck editor's own card-search filter. A ruleset named "Standard OCG/TCG" choosing the option
upstream itself pairs with that label, over the option that also admits prerelease/unofficial
cards, matches the name it displays to the user. The **stored default** being index 3 reflects
a convenience for hosts who want prerelease cards allowed, not a claim that index 3 is more
"standard" than index 2.

**What this does not do:** it does not make `OcgAndTcg` upstream's default, and it must not be
described as one. A future ruleset naming or defaulting decision should re-derive this from
source rather than from this ADR's prose.

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

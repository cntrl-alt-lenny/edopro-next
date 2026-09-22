// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ruleset.h"

namespace edopro_next::ui {

policy::ValidationPolicy Ruleset::makePolicy(std::optional<policy::LfList> lflist) const {
    return policy::ValidationPolicy(
        deckSizes,
        allowedCards,
        forbiddenTypes,
        ritualsBelongInExtra,
        contentCheckingEnabled,
        std::move(lflist)
    );
}

const std::vector<Ruleset>& availableRulesets() {
    // Exactly one named ruleset to keep the invented surface minimal until
    // M4 lobby host negotiation exists (ADR 0010).
    //
    // Upstream citations for "Standard OCG/TCG", re-verified against gframe/
    // directly (brief 015 reopened corrections S1/S2; ADR 0010 has the full
    // per-field quotes this comment summarises - an earlier version of this
    // comment cited generic_duel.cpp:251-256 and deck_manager.h:33, neither
    // of which holds what it claimed):
    // - deck_sizes: Main 40-60, Extra 0-15, Side 0-15 - upstream's own
    //   host-create text-field fallbacks, gframe/duelclient.cpp:251-256
    //   (matches the named preset ocg_deck_sizes at :796); consumed via
    //   host_info.sizes at generic_duel.cpp:373.
    // - allowed_cards: AllowedCardPool::OcgAndTcg (index 2) - one of
    //   upstream's five DuelAllowedCards values (gframe/deck_manager.h:32-37,
    //   ALLOWED_CARDS_OCG_TCG at index 2), cast from host_info.rule at
    //   generic_duel.cpp:381. This is NOT upstream's stored default, which is
    //   index 3/WithPrerelease (game_config.inl:21, lastallowedcards=3) - a
    //   deliberate divergence, recorded and argued in ADR 0010 Decision 1a.
    // - forbidden_types: 0 - stored default gGameConfig->lastDuelForbidden=0
    //   (game_config.inl:24), loaded at game.cpp:1297; the MR5 preset sets
    //   the same value (DUEL_MODE_MR5_FORB=0, ocgapi_constants.h:428).
    // - rituals_belong_in_extra: false - upstream's stored default duel mode
    //   is MR5 (game_config.inl:22, lastDuelParam=0x2E800), which does not
    //   set DUEL_EXTRA_DECK_RITUAL (ocgapi_constants.h:414,423); the flag
    //   this field mirrors is computed at generic_duel.cpp:379.
    // - content_checking_enabled: true - stored default
    //   noCheckDeckContent=false (game_config.inl:34), reflected by the
    //   host-create checkbox at game.cpp:1192 and sent at duelclient.cpp:239.
    static const std::vector<Ruleset> kRulesets = {
        Ruleset{
            QStringLiteral("standard_ocg_tcg"),
            QStringLiteral("Standard OCG/TCG"),
            policy::DeckSizePolicy{
                policy::SectionSizeRange{40, 60}, // Main
                policy::SectionSizeRange{0, 15},  // Extra
                policy::SectionSizeRange{0, 15}   // Side
            },
            policy::AllowedCardPool::OcgAndTcg,
            0,     // forbidden_types: none
            false, // rituals_belong_in_extra: false in modern Master Rules
            true   // content_checking_enabled: true
        }
    };
    return kRulesets;
}

} // namespace edopro_next::ui

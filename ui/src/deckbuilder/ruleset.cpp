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
    // Upstream citations for "Standard OCG/TCG":
    // - deck_sizes: Main 40-60, Extra 0-15, Side 0-15
    //   gframe/duelclient.cpp:251-256, 796; generic_duel.cpp:251-256; menu_handler.cpp:975
    // - allowed_cards: AllowedCardPool::OcgAndTcg
    //   gframe/deck_manager.h:33; generic_duel.cpp:381; game.cpp:3483-3484 (string 1902)
    // - forbidden_types: 0
    //   gframe/game_config.inl:24; duelclient.cpp:702; menu_handler.cpp:923
    // - rituals_belong_in_extra: false
    //   gframe/generic_duel.cpp:379-380; game_config.inl:22 (default MR5)
    // - content_checking_enabled: true
    //   gframe/network.h:45; game_config.inl:34 (noCheckDeckContent=false); duelclient.cpp:239
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

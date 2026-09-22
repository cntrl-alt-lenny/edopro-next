// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Ruleset definition and preset provider for the deck builder.
//
// CLAUDE.md: The rules engine must not become the UI; the UI must not
// implement game rules.
//
// Upstream's deck editor (gframe/deck_con.cpp) never calls CheckDeckContent
// or CheckDeckSize (docs/architecture/deck-builder-legality.md §1). Upstream
// sources its validation parameters from HostInfo at duel entry. Because this
// project has no lobby/session layer at M3, ADR 0010 defines an explicit,
// named, user-visible ruleset in the UI adapter layer, rather than inventing
// a hidden default inside policy/ (ADR 0007 Decision 3).

#pragma once

#include <QString>
#include <optional>
#include <vector>

#include "edopro_next/policy/lf_list.h"
#include "edopro_next/policy/validation_policy.h"

namespace edopro_next::ui {

struct Ruleset {
    QString id;
    QString displayName;
    policy::DeckSizePolicy deckSizes;
    policy::AllowedCardPool allowedCards;
    std::uint32_t forbiddenTypes;
    bool ritualsBelongInExtra;
    bool contentCheckingEnabled;

    policy::ValidationPolicy makePolicy(std::optional<policy::LfList> lflist) const;
};

// Returns the list of user-visible rulesets available to the deck builder.
// Currently returns exactly 1 ruleset: "Standard OCG/TCG", whose values are
// documented against upstream sources in ADR 0010 and deck-builder-legality.md.
const std::vector<Ruleset>& availableRulesets();

} // namespace edopro_next::ui

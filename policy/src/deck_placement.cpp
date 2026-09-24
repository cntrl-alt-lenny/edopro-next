// SPDX-License-Identifier: AGPL-3.0-or-later

#include "edopro_next/policy/deck_placement.h"

#include <cstdint>

namespace edopro_next::policy {

namespace {

// Verified against ocgcore/ocgapi_constants.h:33-58 (TYPE_*) and
// gframe/data_manager.h:29 (SCOPE_RUSH), the same sources
// deck_validation.cpp cites for its own copies of these bits.
constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeFusion = 0x40;
constexpr std::uint32_t kTypeRitual = 0x80;
constexpr std::uint32_t kTypeSynchro = 0x2000;
constexpr std::uint32_t kTypeToken = 0x4000;
constexpr std::uint32_t kTypeXyz = 0x800000;
constexpr std::uint32_t kTypeLink = 0x4000000;

constexpr std::uint32_t kScopeRush = 0x200;

} // namespace

bool is_ritual_monster(const data::CardRecord& record) noexcept {
	return (record.type & (kTypeMonster | kTypeRitual)) == (kTypeMonster | kTypeRitual);
}

bool is_rush(const data::CardRecord& record) noexcept {
	return (record.scope & kScopeRush) != 0;
}

bool is_token(const data::CardRecord& record) noexcept {
	return (record.type & kTypeToken) != 0;
}

bool is_extra_deck_type(const data::CardRecord& record) noexcept {
	// deck_manager.cpp:336-337.
	if(record.type & (kTypeFusion | kTypeSynchro | kTypeXyz))
		return true;
	// deck_manager.cpp:338-339, literally
	// `card->type & (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER)`, with
	// `card == cd` at every call site: the parenthesised `&&` is a bool (0 or
	// 1), and 1 is TYPE_MONSTER, so the line is true exactly when the card is
	// both a Link card and a Monster. docs/architecture/deck-placement.md §2.1.
	if((record.type & kTypeLink) && (record.type & kTypeMonster))
		return true;
	return false;
}

bool belongs_in_extra_deck(const data::CardRecord& record, RitualPlacement rituals) noexcept {
	if(is_extra_deck_type(record))
		return true;
	// deck_manager.cpp:340-346.
	if(is_ritual_monster(record)) {
		switch(rituals) {
		case RitualPlacement::RushInExtra:
			return is_rush(record);
		case RitualPlacement::Main:
			return false;
		case RitualPlacement::Extra:
			return true;
		}
	}
	return false;
}

CardPlacement classify_card(const data::CardDatabase& database, data::CardCode code,
							RitualPlacement rituals) noexcept {
	const auto* record = database.find(code);
	if(!record)
		return CardPlacement::Unknown;
	if(is_token(*record))
		return CardPlacement::Token;
	return belongs_in_extra_deck(*record, rituals) ? CardPlacement::Extra : CardPlacement::Main;
}

} // namespace edopro_next::policy

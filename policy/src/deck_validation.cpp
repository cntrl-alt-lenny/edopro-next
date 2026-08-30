// SPDX-License-Identifier: AGPL-3.0-or-later

#include "edopro_next/policy/deck_validation.h"

#include <map>
#include <optional>
#include <vector>

namespace edopro_next::policy {

namespace {

// Verified against ocgcore's own public header (ocgcore/ocgapi_constants.h)
// for the bits it defines, and gframe/data_manager.h for TYPE_SKILL (a
// legacy-client-only extension bit ocgcore itself does not define) -
// matching the exact citation precedent already established in
// ui/src/deckbuilder/card_entry.cpp and data/src/card_search_index.cpp.
// Used only to reproduce upstream's own deck-validation checks; never to
// decide deck-section membership for editing, which stays the user's own
// explicit choice (see data/'s own Deck.h).
constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeSpell = 0x2;
constexpr std::uint32_t kTypeTrap = 0x4;
constexpr std::uint32_t kTypeFusion = 0x40;
constexpr std::uint32_t kTypeRitual = 0x80;
constexpr std::uint32_t kTypeSynchro = 0x2000;
constexpr std::uint32_t kTypeXyz = 0x800000;
constexpr std::uint32_t kTypeLink = 0x4000000;
constexpr std::uint32_t kTypeSkill = 0x8000000;

// Verified against gframe/data_manager.h's SCOPE_* block.
constexpr std::uint32_t kScopeOcg = 0x1;
constexpr std::uint32_t kScopeTcg = 0x2;
constexpr std::uint32_t kScopePrerelease = 0x100;
constexpr std::uint32_t kScopeLegend = 0x400;

// Mirrors gframe/data_manager.h:92-94's CardDataC::isRitualMonster()
// exactly: `(type & (TYPE_MONSTER|TYPE_RITUAL)) == (TYPE_MONSTER|TYPE_RITUAL)`.
bool is_ritual_monster(const data::CardRecord& record) {
	return (record.type & (kTypeMonster | kTypeRitual)) == (kTypeMonster | kTypeRitual);
}

// Mirrors gframe/deck_manager.cpp:335-338's is_extra_deck_card lambda's
// Fusion/Synchro/Xyz/Link(-monster) half only - Ritual is handled
// separately by the two call sites below, exactly as upstream's own
// additionalCheck callbacks do (deck_manager.cpp:219-234), because Ritual
// placement additionally depends on policy.rituals_belong_in_extra.
bool is_unconditionally_extra_deck_type(const data::CardRecord& record) {
	if(record.type & (kTypeFusion | kTypeSynchro | kTypeXyz))
		return true;
	if((record.type & kTypeLink) && (record.type & kTypeMonster))
		return true;
	return false;
}

// Mirrors gframe/deck_manager.cpp:140-146's DeckManager::TypeCount exactly.
int type_count(const std::vector<data::CardCode>& cards, const data::CardDatabase& database,
			   std::uint32_t type_mask) {
	int count = 0;
	for(auto code : cards) {
		const auto* record = database.find(code);
		// Precondition: the caller (validate_deck) has already run the
		// unknown-card check and returned before any of these counting
		// helpers run, so every code here resolves. Guarded defensively
		// rather than asserted, since an unresolved code here would
		// otherwise silently under-count instead of visibly failing -
		// treated as "does not match any type", exactly as upstream's own
		// TypeCount would if handed a null CardDataC* (which upstream's
		// own callers never do, for the same reason).
		if(!record)
			continue;
		if(record->type & type_mask)
			++count;
	}
	return count;
}

// Mirrors gframe/deck_manager.cpp:148-155's DeckManager::CountLegends
// exactly.
int count_legends(const std::vector<data::CardCode>& cards, const data::CardDatabase& database,
				   std::uint32_t type_mask) {
	int count = 0;
	for(auto code : cards) {
		const auto* record = database.find(code);
		if(!record)
			continue;
		if((record->scope & kScopeLegend) && (record->type & type_mask))
			++count;
	}
	return count;
}

// Mirrors gframe/data_manager.h:74-85's CardDataC::IsInArtworkOffsetRange():
// an unsigned-subtraction idiom for "the absolute difference is less than
// 10", correct under wraparound regardless of which of the two values is
// larger - reproduced with the exact same two-sided check rather than a
// signed subtraction + abs(), to match upstream's own arithmetic exactly
// (both give the same answer for any inputs that do not overflow a
// int64_t, but the unsigned form is what upstream's source actually says).
bool is_in_artwork_offset_range(data::CardCode code, data::CardCode alias) {
	constexpr std::uint32_t kOffset = 10;
	const auto c = data::to_number(code);
	const auto a = data::to_number(alias);
	return (a - c < kOffset) || (c - a < kOffset);
}

// Mirrors gframe/deck_manager.h:23-30's LFList::GetLimitationIterator
// exactly: look up by exact `code` first; only on a miss, and only if
// `alias` is present, fall back to `alias` - and even then, only if the
// list is not a whitelist, or the code/alias pair is within the artwork-
// offset window. This is deliberately a DIFFERENT resolution order than
// the shared copy-count key below (which prefers alias first) - both are
// reproduced exactly as upstream keeps them distinct.
std::optional<std::int32_t> limitation_for(const LfList& list, data::CardCode code,
											data::CardCode alias) {
	if(auto it = list.content.find(code); it != list.content.end())
		return it->second;
	if(alias != data::CardCode::None) {
		if(!list.whitelist || is_in_artwork_offset_range(code, alias))
			if(auto it = list.content.find(alias); it != list.content.end())
				return it->second;
	}
	return std::nullopt;
}

// Mirrors gframe/deck_manager.cpp:157-203's CheckCards - one section's
// worth of scope + zone(via zone_check) + copy-count + banlist checking,
// against a SHARED copy-count map threaded across all three sections
// (`ccount`), exactly matching upstream's own single banlist_content_t
// instance reused across the three CheckCards() calls inside
// CheckDeckContent. `zone_check` returns DeckErrorType::None for a card in
// a valid zone; Side's caller passes one that always does.
template <typename ZoneCheck>
DeckValidationError check_cards(const std::vector<data::CardCode>& cards,
								 const data::CardDatabase& database, const LfList& lflist,
								 AllowedCardPool allowed,
								 std::map<data::CardCode, int>& ccount, ZoneCheck&& zone_check) {
	for(auto code : cards) {
		const auto* record = database.find(code);
		if(!record)
			continue; // see type_count()'s own precondition note.

		// Mirrors gframe/deck_manager.cpp:164-187's switch, including the
		// CHECK_UNOFFICIAL(cit) macro's `ot > 0x3` magnitude test
		// (deck_manager.cpp:165) - a MAGNITUDE comparison, not a bitmask
		// test, preserved exactly as upstream's own observed behavior
		// rather than "corrected" to `!(ot & ~0x3)`. See
		// docs/architecture/deck-legality.md#scope for the citation and
		// why this project reproduces it deliberately.
		switch(allowed) {
		case AllowedCardPool::OcgOnly:
			if(record->scope > 0x3)
				return {DeckErrorType::UnofficialCard, {}, code};
			if(!(record->scope & kScopeOcg))
				return {DeckErrorType::TcgOnly, {}, code};
			break;
		case AllowedCardPool::TcgOnly:
			if(record->scope > 0x3)
				return {DeckErrorType::UnofficialCard, {}, code};
			if(!(record->scope & kScopeTcg))
				return {DeckErrorType::OcgOnly, {}, code};
			break;
		case AllowedCardPool::OcgAndTcg:
			if(record->scope > 0x3)
				return {DeckErrorType::UnofficialCard, {}, code};
			break;
		case AllowedCardPool::WithPrerelease:
			if(!(record->scope & kScopeOcg) && !(record->scope & kScopeTcg) &&
			   !(record->scope & kScopePrerelease))
				return {DeckErrorType::UnofficialCard, {}, code};
			break;
		case AllowedCardPool::Any:
			break;
		}

		if(const auto zone_error = zone_check(*record); zone_error != DeckErrorType::None)
			return {zone_error, {}, code};

		// gframe/deck_manager.cpp:192: `cit->alias ? cit->alias : cit->code`
		// - alias-preferred, the OPPOSITE resolution order from
		// limitation_for()'s code-first lookup above. Both are upstream's
		// own, reproduced exactly as distinct.
		const auto count_key = record->alias != data::CardCode::None ? record->alias : code;
		const int dc = ++ccount[count_key];
		if(dc > 3)
			return {DeckErrorType::CardCount, {}, code};

		const auto limit = limitation_for(lflist, code, record->alias);
		if((limit && dc > *limit) || (!limit && lflist.whitelist))
			return {DeckErrorType::Lflist, {}, code};
	}
	return {};
}

} // namespace

DeckValidationError validate_deck(const data::Deck& deck, const data::CardDatabase& database,
								   const ValidationPolicy& policy) {
	// Step 1: deck size - gframe/deck_manager.cpp:238-258's CheckDeckSize,
	// in Main-then-Extra-then-Side order. Skill cards are subtracted from
	// the counted Main size, matching upstream exactly.
	const auto main_skills = type_count(deck.main, database, kTypeSkill);
	const auto main_counted = deck.main.size() - static_cast<std::size_t>(main_skills);
	if(!policy.deck_sizes.main.contains(main_counted)) {
		DeckValidationError error;
		error.type = DeckErrorType::MainCount;
		error.count = {static_cast<std::uint32_t>(main_counted), policy.deck_sizes.main.min,
					   policy.deck_sizes.main.max};
		return error;
	}
	if(!policy.deck_sizes.extra.contains(deck.extra.size())) {
		DeckValidationError error;
		error.type = DeckErrorType::ExtraCount;
		error.count = {static_cast<std::uint32_t>(deck.extra.size()), policy.deck_sizes.extra.min,
					   policy.deck_sizes.extra.max};
		return error;
	}
	if(!policy.deck_sizes.side.contains(deck.side.size())) {
		DeckValidationError error;
		error.type = DeckErrorType::SideCount;
		error.count = {static_cast<std::uint32_t>(deck.side.size()), policy.deck_sizes.side.min,
					   policy.deck_sizes.side.max};
		return error;
	}

	// gframe/generic_duel.cpp:374: content checking (and the unknown-card
	// check that gates it, immediately below) is skipped entirely when
	// disabled - only deck size was ever checked.
	if(!policy.content_checking_enabled)
		return {};

	// Step 2: unknown-card condition - see deck_validation.h's own
	// "Unknown-card semantics" note for why this is this module's own
	// deterministic Main-then-Extra-then-Side scan, not a claim of
	// matching any specific upstream load mode's `errorcode`.
	for(const auto* section : {&deck.main, &deck.extra, &deck.side}) {
		for(auto code : *section) {
			if(!database.find(code))
				return {DeckErrorType::UnknownCard, {}, code};
		}
	}

	// Step 3: content validation - gframe/deck_manager.cpp:204-236's
	// CheckDeckContent, in its own exact internal order.
	if(type_count(deck.main, database, policy.forbidden_types) > 0 ||
	   type_count(deck.extra, database, policy.forbidden_types) > 0 ||
	   type_count(deck.side, database, policy.forbidden_types) > 0)
		return {DeckErrorType::ForbiddenType, {}, data::CardCode::None};

	if(count_legends(deck.main, database, kTypeMonster) +
		   count_legends(deck.extra, database, kTypeMonster) >
	   1)
		return {DeckErrorType::TooManyLegends, {}, data::CardCode::None};
	if(count_legends(deck.main, database, kTypeSpell) > 1)
		return {DeckErrorType::TooManyLegends, {}, data::CardCode::None};
	if(count_legends(deck.main, database, kTypeTrap) > 1)
		return {DeckErrorType::TooManyLegends, {}, data::CardCode::None};

	if(type_count(deck.main, database, kTypeSkill) > 1)
		return {DeckErrorType::TooManySkills, {}, data::CardCode::None};

	// gframe/deck_manager.cpp:217-218: `if(!lflist) return ret;` - a null
	// LFList causes CheckDeckContent to return HERE, before any CheckCards
	// pass ever runs. This is NOT the same state as a concrete LfList with
	// empty content (upstream's own synthetic "N/A" list): that state
	// still runs every CheckCards pass below, simply finding no
	// banlist-specific restriction for any card. See ValidationPolicy's
	// own doc comment for why these are two distinct states, not one
	// collapsed into the other.
	if(!policy.lflist)
		return {};

	std::map<data::CardCode, int> ccount;

	auto main_zone_check = [&](const data::CardRecord& record) -> DeckErrorType {
		if(is_unconditionally_extra_deck_type(record))
			return DeckErrorType::ExtraCount;
		if(is_ritual_monster(record) && policy.rituals_belong_in_extra)
			return DeckErrorType::ExtraCount;
		return DeckErrorType::None;
	};
	if(auto error = check_cards(deck.main, database, *policy.lflist, policy.allowed_cards, ccount,
								 main_zone_check);
	   error)
		return error;

	auto extra_zone_check = [&](const data::CardRecord& record) -> DeckErrorType {
		if(is_ritual_monster(record)) {
			if(!policy.rituals_belong_in_extra)
				return DeckErrorType::ExtraCount;
		} else if(!is_unconditionally_extra_deck_type(record)) {
			return DeckErrorType::ExtraCount;
		}
		return DeckErrorType::None;
	};
	if(auto error = check_cards(deck.extra, database, *policy.lflist, policy.allowed_cards, ccount,
								 extra_zone_check);
	   error)
		return error;

	auto no_zone_check = [](const data::CardRecord&) -> DeckErrorType {
		return DeckErrorType::None;
	};
	return check_cards(deck.side, database, *policy.lflist, policy.allowed_cards, ccount,
						no_zone_check);
}

} // namespace edopro_next::policy

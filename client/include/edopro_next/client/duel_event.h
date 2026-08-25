// The "what just happened" half of the semantic model.
//
// State and events answer different questions and both are needed. A Qt duel
// field will read DuelState for authoritative display and consume DuelEvent
// to decide what to animate - but the events themselves contain no animation
// instructions. "CardMoved from HAND[p0:2] to MZONE[p0:1]" is an event;
// "slide it over 12 frames" is a presentation decision derived from it.
//
// A std::variant is used rather than a base class with virtuals: the set of
// events is closed, exhaustive handling is worth compile-time checking, and
// nothing here needs to be polymorphically owned.
#ifndef EDOPRO_NEXT_CLIENT_DUEL_EVENT_H
#define EDOPRO_NEXT_CLIENT_DUEL_EVENT_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "edopro_next/client/card_identity.h"
#include "edopro_next/client/card_location.h"
#include "edopro_next/client/card_state.h"
#include "edopro_next/client/duel_state.h"

namespace edopro_next::client {

enum class SummonKind : std::uint8_t { Normal, Special, Flip };

std::string_view summon_kind_name(SummonKind kind) noexcept;
std::string_view life_change_reason_name(LifeChangeReason reason) noexcept;

struct DuelStarted {
	// MSG_START's opaque first byte. See DuelState::start: it describes the
	// recipient of the stream, not which player moves first.
	std::uint8_t player_type = 0;
	std::array<std::int64_t, kPlayerCount> life{};
	std::array<DeckSizes, kPlayerCount> decks{};
};

struct TurnStarted {
	std::uint32_t turn = 0;
	PlayerId player = 0;
};

struct PhaseChanged {
	Phase phase = Phase::Unknown;
};

struct CardsDrawn {
	PlayerId player = 0;
	std::vector<CardInstanceId> cards;
};

// A card the client had never seen appears. Tokens do this, and so does any
// card the protocol introduces straight into a zone.
struct CardCreated {
	CardInstanceId card = CardInstanceId::None;
	CardLocation at{};
	CardCode code = CardCode::None;
};

struct CardMoved {
	CardInstanceId card = CardInstanceId::None;
	CardLocation from{};
	CardLocation to{};
	std::uint32_t reason = 0;
};

struct CardRemoved {
	CardInstanceId card = CardInstanceId::None;
	CardLocation from{};
	std::uint32_t reason = 0;
};

// The client learns what a card is. Emitted only on an actual change, so a
// message that restates a code the client already had produces nothing.
struct CardIdentityRevealed {
	CardInstanceId card = CardInstanceId::None;
	CardCode code = CardCode::None;
};

// The client stops being entitled to know what a card is - shuffling a deck
// is the ordinary case. Not the same as the card leaving play.
struct CardIdentityConcealed {
	CardInstanceId card = CardInstanceId::None;
};

struct PositionChanged {
	CardInstanceId card = CardInstanceId::None;
	CardPosition from{};
	CardPosition to{};
};

struct LifePointsChanged {
	PlayerId player = 0;
	std::int64_t from = 0;
	std::int64_t to = 0;
	LifeChangeReason reason = LifeChangeReason::Update;
	// The amount the protocol stated, which is not always `from - to`:
	// life points are clamped at zero, so lethal damage states more than it
	// removes. Both are kept because both are true and a UI wants each.
	std::int64_t amount = 0;
};

// MSG_SET announces that a card was set. The legacy client draws only a
// message from it; the actual placement arrives separately via MSG_MOVE.
struct CardSetAnnounced {
	CardLocation at{};
	CardCode code = CardCode::None;
};

struct SummonAnnounced {
	SummonKind kind = SummonKind::Normal;
	CardCode code = CardCode::None;
	CardLocation at{};
};

struct SummonCompleted {
	SummonKind kind = SummonKind::Normal;
};

struct ChainLinkAdded {
	ChainLink link{};
};

struct ChainLinkResolving {
	std::uint32_t link = 0;
};

struct ChainLinkResolved {
	std::uint32_t link = 0;
};

struct ChainEnded {
	// How many links the chain had when it ended.
	std::uint32_t links = 0;
};

struct AttackDeclared {
	CardInstanceId attacker = CardInstanceId::None;
	// Empty for a direct attack.
	CardInstanceId target = CardInstanceId::None;
	bool direct = false;
};

// MSG_BATTLE restates both combatants' current ATK and DEF. It is the only
// message in this slice that carries them, so it is also the only way a card
// in this model acquires combat stats.
struct CombatStatsRevealed {
	CardInstanceId card = CardInstanceId::None;
	std::int32_t attack = 0;
	std::int32_t defense = 0;
};

struct DamageStepStarted {};
struct DamageStepEnded {};

struct DeckShuffled {
	PlayerId player = 0;
	std::size_t cards = 0;
};

struct HandShuffled {
	PlayerId player = 0;
	std::size_t cards = 0;
};

struct DuelEnded {
	// Empty for a draw, or when the protocol named a non-duelist.
	std::optional<PlayerId> winner;
	std::uint8_t reason = 0;
};

using DuelEvent = std::variant<
	DuelStarted,
	TurnStarted,
	PhaseChanged,
	CardsDrawn,
	CardCreated,
	CardMoved,
	CardRemoved,
	CardIdentityRevealed,
	CardIdentityConcealed,
	PositionChanged,
	LifePointsChanged,
	CardSetAnnounced,
	SummonAnnounced,
	SummonCompleted,
	ChainLinkAdded,
	ChainLinkResolving,
	ChainLinkResolved,
	ChainEnded,
	AttackDeclared,
	CombatStatsRevealed,
	DamageStepStarted,
	DamageStepEnded,
	DeckShuffled,
	HandShuffled,
	DuelEnded>;

// One deterministic line per event. This is the golden-file representation,
// so it must never contain a pointer, an address, a timestamp or anything else
// that varies between machines.
std::string to_string(const DuelEvent& event);

// The event's type name on its own ("CardMoved"), for coverage summaries.
std::string_view event_name(const DuelEvent& event);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_DUEL_EVENT_H

// The authoritative "what is true now" half of the semantic model.
//
// DuelState answers questions. It does not decide anything: it enforces
// *client-state integrity* (a card is in exactly one slot, a move empties its
// source, chain links are contiguous) and never game rules. Whether a summon
// was legal is ocgcore's business, and ocgcore has already decided by the time
// a message reaches us.
//
// Zone bookkeeping deliberately mirrors ClientField::AddCard / RemoveCard in
// gframe/client_field.cpp, quirks included, so that the two models can be
// compared slot for slot when the legacy hook is added. Where a quirk exists
// it is called out at the point it is reproduced.
#ifndef EDOPRO_NEXT_CLIENT_DUEL_STATE_H
#define EDOPRO_NEXT_CLIENT_DUEL_STATE_H

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "edopro_next/client/card_identity.h"
#include "edopro_next/client/card_location.h"
#include "edopro_next/client/card_state.h"

namespace edopro_next::client {

struct CardQueryPatch;

enum class Phase : std::uint8_t {
	Unknown = 0,
	Draw,
	Standby,
	Main1,
	BattleStart,
	BattleStep,
	Damage,
	DamageCalculation,
	Battle,
	Main2,
	End,
};

Phase phase_from_protocol(std::uint32_t phase) noexcept;
std::string_view phase_name(Phase phase) noexcept;

// One link of the current chain, as stated by MSG_CHAINING.
struct ChainLink {
	// 1-based, as the protocol counts them.
	std::uint32_t link = 0;
	CardCode code = CardCode::None;
	// The card whose effect this is, when the client can identify it.
	CardInstanceId card = CardInstanceId::None;
	CardLocation card_location{};
	// Where the effect is being activated *from*, which is not always where
	// the card is (an effect activated in the graveyard, for example).
	PlayerId triggering_controller = 0;
	Zone triggering_zone = Zone::None;
	std::uint32_t triggering_sequence = 0;
	// Opaque string id for the effect description. Resolving it needs the
	// card database, which this layer deliberately does not have.
	std::uint64_t description = 0;
	// MSG_BECOME_TARGET adds the referenced instances to the pending/current
	// chain link. This is distinct from query-derived card target relations.
	std::vector<CardInstanceId> targets;
	bool resolving = false;
	bool resolved = false;

	friend bool operator==(const ChainLink&, const ChainLink&) = default;
};

struct DeckSizes {
	std::uint16_t main = 0;
	std::uint16_t extra = 0;
};

// Why life points changed. The protocol uses a different message for each, and
// a UI wants to distinguish them, so the distinction is preserved.
enum class LifeChangeReason : std::uint8_t { Damage, Recover, Cost, Update };

class DuelState {
public:
	// Mutating calls return an empty optional on success, or a short
	// description of the integrity violation they refused to perform. The
	// state is never left half-mutated by a failed call.
	using Error = std::optional<std::string>;

	// --- lifecycle ---

	// A default-constructed state is empty but structurally valid: the two
	// addressable field zones already have their fixed slots.
	DuelState();

	bool started() const noexcept { return started_; }
	// Resets everything and creates the initial deck and extra-deck cards,
	// all with unknown identity, exactly as ClientField::Initial does.
	//
	// `player_type` is MSG_START's first byte verbatim. It describes the
	// stream's *recipient* - whether they move first, and whether they are a
	// spectator - so it cannot say which absolute player takes the first
	// turn, and nothing here treats it as if it could. Turn order arrives
	// with MSG_NEW_TURN, which names the player outright.
	void start(std::uint8_t player_type, const std::array<std::int64_t, kPlayerCount>& life,
			   const std::array<DeckSizes, kPlayerCount>& decks);

	std::uint8_t player_type() const noexcept { return player_type_; }

	// --- turn, phase, life ---

	std::uint32_t turn() const noexcept { return turn_; }
	PlayerId turn_player() const noexcept { return turn_player_; }
	Phase phase() const noexcept { return phase_; }
	void begin_turn(PlayerId player);
	void set_phase(Phase phase) noexcept { phase_ = phase; }

	std::int64_t life_points(PlayerId player) const;
	// Life points are clamped at zero, matching the legacy client. Returns
	// the value actually stored.
	std::int64_t set_life_points(PlayerId player, std::int64_t value);

	// --- cards ---

	const CardState* find(CardInstanceId id) const noexcept;
	CardState* find(CardInstanceId id) noexcept;

	// Every instance ever allocated, in allocation order. Includes cards that
	// have stopped being tracked.
	const std::vector<CardState>& cards() const noexcept { return cards_; }

	const std::vector<CardInstanceId>& zone(PlayerId player, Zone zone) const;

	// The card at a location, or CardInstanceId::None if the slot is empty,
	// out of range, or refers to a zone that is not tracked.
	CardInstanceId at(const CardLocation& location) const;

	// Creates a new instance and places it. Fails only if the destination is
	// not a placeable location.
	Error create_card(const CardLocation& destination, CardCode code, CardPosition position,
					  CardInstanceId* created);

	// Moves an existing instance. `destination.sequence` is a request: pile
	// zones recompute it on insertion, exactly as ClientField::AddCard does,
	// so read the card's location back afterwards for the resolved value.
	//
	// An empty `position` leaves the card's position alone. That is not
	// laziness: when a card becomes material the protocol reuses the position
	// field of loc_info as the material index, so there is no position to
	// apply and inventing one would be a fabrication.
	Error move_card(CardInstanceId id, const CardLocation& destination,
					std::optional<CardPosition> position);

	// Removes an instance from play. The record survives, with tracked=false.
	Error remove_card(CardInstanceId id);

	Error set_position(CardInstanceId id, CardPosition position);
	Error set_code(CardInstanceId id, CardCode code);
	Error set_combat_stats(CardInstanceId id, std::int32_t attack, std::int32_t defense);
	Error apply_card_hint(CardInstanceId id, std::uint8_t type, std::uint64_t value);
	void clear_card_description_hints(CardInstanceId id);
	// Applies a query as a state patch. It emits no gameplay event: query
	// packets synchronize client knowledge and are not actions in the duel.
	Error apply_query_patch(CardInstanceId id, const CardQueryPatch& patch);

	// --- chain ---

	const std::vector<ChainLink>& chain() const noexcept { return chain_; }
	// Links must arrive in order; a gap or a repeat is an integrity error.
	Error push_chain_link(const ChainLink& link);
	Error add_chain_targets(const std::vector<CardInstanceId>& targets);
	Error mark_chain_resolving(std::uint32_t link);
	Error mark_chain_resolved(std::uint32_t link);
	void clear_chain() noexcept { chain_.clear(); }

	const std::map<std::uint64_t, std::uint32_t>& player_description_hints(
		PlayerId player) const;
	Error apply_player_hint(PlayerId player, std::uint8_t type, std::uint64_t value);

	// --- battle ---

	CardInstanceId attacker() const noexcept { return attacker_; }
	CardInstanceId attack_target() const noexcept { return attack_target_; }
	void set_attack(CardInstanceId attacker, CardInstanceId target) noexcept;

	// --- outcome ---

	bool finished() const noexcept { return finished_; }
	// Empty when the duel was a draw, or when the protocol named a
	// non-duelist.
	std::optional<PlayerId> winner() const noexcept { return winner_; }
	std::uint8_t win_reason() const noexcept { return win_reason_; }
	void finish(std::optional<PlayerId> winner, std::uint8_t reason);

	// --- integrity ---

	// Re-derives every invariant from scratch and returns one line per
	// violation. Empty means the model is self-consistent. Used by tests and
	// by the trace tool; cheap enough to call per duel, not per packet.
	std::vector<std::string> check_invariants() const;

	// Whole-state structural equality: every private member below is a value
	// type with its own `==`, so this compares the complete model, not a
	// hand-picked subset of fields. It is what lets a test assert "this
	// packet changed nothing" without having to enumerate what "nothing"
	// means. See ProtocolDecoder::decode(), which relies on DuelState being
	// cheaply, correctly copyable to give its all-or-nothing guarantee.
	friend bool operator==(const DuelState&, const DuelState&) = default;

private:
	std::vector<CardInstanceId>& mutable_zone(PlayerId player, Zone zone);
	// Everything that could refuse a placement, checked before anything is
	// mutated, so that place() cannot fail half way and leave the model in a
	// state no message produced.
	Error validate_destination(const CardState& card, const CardLocation& destination) const;
	void place(CardState& card, const CardLocation& destination,
			   std::optional<CardPosition> position);
	Error detach(CardState& card);

	bool started_ = false;
	std::uint8_t player_type_ = 0;
	std::uint32_t turn_ = 0;
	PlayerId turn_player_ = 0;
	Phase phase_ = Phase::Unknown;
	std::array<std::int64_t, kPlayerCount> life_{};
	std::array<std::vector<CardInstanceId>, kPlayerCount * kZoneCount> zones_{};
	// Face-up cards sit at the back of the extra deck and face-down ones are
	// inserted in front of them. ClientField tracks the split with this
	// counter; reproducing it keeps our sequences equal to the legacy ones.
	std::array<std::uint32_t, kPlayerCount> extra_face_up_{};
	std::vector<CardState> cards_;
	std::vector<ChainLink> chain_;
	std::array<std::map<std::uint64_t, std::uint32_t>, kPlayerCount> player_desc_hints_{};
	CardInstanceId attacker_ = CardInstanceId::None;
	CardInstanceId attack_target_ = CardInstanceId::None;
	bool finished_ = false;
	std::optional<PlayerId> winner_;
	std::uint8_t win_reason_ = 0;
};

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_DUEL_STATE_H

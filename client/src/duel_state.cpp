#include "edopro_next/client/duel_state.h"

#include "edopro_next/client/protocol_constants.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace edopro_next::client {
namespace {

std::size_t zone_slot(PlayerId player, Zone zone) {
	return static_cast<std::size_t>(player) * kZoneCount + static_cast<std::size_t>(zone);
}

std::string describe(const CardLocation& location) {
	return to_string(location);
}

const std::vector<CardInstanceId> kEmptyZone{};

} // namespace

Phase phase_from_protocol(std::uint32_t phase) noexcept {
	switch(phase) {
	case protocol::PHASE_DRAW:
		return Phase::Draw;
	case protocol::PHASE_STANDBY:
		return Phase::Standby;
	case protocol::PHASE_MAIN1:
		return Phase::Main1;
	case protocol::PHASE_BATTLE_START:
		return Phase::BattleStart;
	case protocol::PHASE_BATTLE_STEP:
		return Phase::BattleStep;
	case protocol::PHASE_DAMAGE:
		return Phase::Damage;
	case protocol::PHASE_DAMAGE_CAL:
		return Phase::DamageCalculation;
	case protocol::PHASE_BATTLE:
		return Phase::Battle;
	case protocol::PHASE_MAIN2:
		return Phase::Main2;
	case protocol::PHASE_END:
		return Phase::End;
	default:
		return Phase::Unknown;
	}
}

std::string_view phase_name(Phase phase) noexcept {
	switch(phase) {
	case Phase::Draw:
		return "DRAW";
	case Phase::Standby:
		return "STANDBY";
	case Phase::Main1:
		return "MAIN1";
	case Phase::BattleStart:
		return "BATTLE_START";
	case Phase::BattleStep:
		return "BATTLE_STEP";
	case Phase::Damage:
		return "DAMAGE";
	case Phase::DamageCalculation:
		return "DAMAGE_CAL";
	case Phase::Battle:
		return "BATTLE";
	case Phase::Main2:
		return "MAIN2";
	case Phase::End:
		return "END";
	case Phase::Unknown:
		break;
	}
	return "UNKNOWN";
}

DuelState::DuelState() {
	// The two field zones are addressable arrays with permanent, possibly
	// empty slots, unlike the piles which grow and shrink. Sizing them here
	// means every lookup can trust the shape.
	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		for(const auto zone : {Zone::MonsterZone, Zone::SpellZone})
			zones_[zone_slot(player, zone)].assign(field_zone_capacity(zone),
												   CardInstanceId::None);
	}
}

void DuelState::start(std::uint8_t player_type,
					  const std::array<std::int64_t, kPlayerCount>& life,
					  const std::array<DeckSizes, kPlayerCount>& decks) {
	*this = DuelState{};
	started_ = true;
	player_type_ = player_type;
	life_ = life;
	// turn_player_ stays 0 until MSG_NEW_TURN names it. MSG_START cannot say.

	// ClientField::Initial fills both piles with face-down, unidentified
	// cards. The client is told how many there are and nothing else, which is
	// exactly right: at this point it is not entitled to know any of them.
	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		const auto sizes = decks[player];
		for(std::uint32_t i = 0; i < sizes.main; ++i) {
			CardInstanceId created = CardInstanceId::None;
			create_card(CardLocation{player, Zone::Deck, i, false, 0}, CardCode::None,
						CardPosition{protocol::POS_FACEDOWN_DEFENSE}, &created);
		}
		for(std::uint32_t i = 0; i < sizes.extra; ++i) {
			CardInstanceId created = CardInstanceId::None;
			create_card(CardLocation{player, Zone::ExtraDeck, i, false, 0}, CardCode::None,
						CardPosition{protocol::POS_FACEDOWN_DEFENSE}, &created);
		}
	}
}

void DuelState::begin_turn(PlayerId player) {
	++turn_;
	turn_player_ = player;
}

std::int64_t DuelState::life_points(PlayerId player) const {
	return is_duelist(player) ? life_[player] : 0;
}

std::int64_t DuelState::set_life_points(PlayerId player, std::int64_t value) {
	if(!is_duelist(player))
		return 0;
	// The legacy client clamps at zero (see MSG_DAMAGE and MSG_PAY_LPCOST in
	// gframe/duelclient.cpp). Lethal damage therefore states more than it
	// removes, which is why LifePointsChanged carries both.
	life_[player] = std::max<std::int64_t>(0, value);
	return life_[player];
}

const CardState* DuelState::find(CardInstanceId id) const noexcept {
	const auto index = to_number(id);
	if(index == 0 || index > cards_.size())
		return nullptr;
	return &cards_[index - 1];
}

CardState* DuelState::find(CardInstanceId id) noexcept {
	return const_cast<CardState*>(std::as_const(*this).find(id));
}

const std::vector<CardInstanceId>& DuelState::zone(PlayerId player, Zone zone) const {
	if(!is_duelist(player) || !is_trackable(zone))
		return kEmptyZone;
	return zones_[zone_slot(player, zone)];
}

std::vector<CardInstanceId>& DuelState::mutable_zone(PlayerId player, Zone zone) {
	return zones_[zone_slot(player, zone)];
}

CardInstanceId DuelState::at(const CardLocation& location) const {
	if(!is_duelist(location.controller))
		return CardInstanceId::None;

	if(location.overlay) {
		CardLocation host_location = location;
		host_location.overlay = false;
		host_location.overlay_index = 0;
		const auto host = at(host_location);
		const auto* card = find(host);
		if(card == nullptr || location.overlay_index >= card->materials.size())
			return CardInstanceId::None;
		return card->materials[location.overlay_index];
	}

	if(!is_trackable(location.zone))
		return CardInstanceId::None;
	const auto& contents = zone(location.controller, location.zone);
	if(location.sequence >= contents.size())
		return CardInstanceId::None;
	return contents[location.sequence];
}

DuelState::Error DuelState::validate_destination(const CardState& card,
												const CardLocation& destination) const {
	if(!is_duelist(destination.controller))
		return "destination names player " + player_name(destination.controller);

	if(destination.overlay) {
		CardLocation host_location = destination;
		host_location.overlay = false;
		host_location.overlay_index = 0;
		if(find(at(host_location)) == nullptr)
			return "no card at " + describe(host_location) + " to attach material to";
		return std::nullopt;
	}

	if(!is_trackable(destination.zone))
		return "cannot place a card in " + std::string(zone_name(destination.zone));

	if(is_field_zone(destination.zone)) {
		const auto& contents = zone(destination.controller, destination.zone);
		if(destination.sequence >= contents.size())
			return describe(destination) + " is out of range";
		// Occupied by this very card is fine: a card may be told to stay put.
		const auto occupant = contents[destination.sequence];
		if(occupant != CardInstanceId::None && occupant != card.id)
			return describe(destination) + " is already occupied";
	}
	return std::nullopt;
}

void DuelState::place(CardState& card, const CardLocation& destination,
					  std::optional<CardPosition> position) {
	if(destination.overlay) {
		CardLocation host_location = destination;
		host_location.overlay = false;
		host_location.overlay_index = 0;
		const auto host_id = at(host_location);
		auto* host = find(host_id);
		host->materials.push_back(card.id);
		card.attached_to = host_id;
		card.location = host_location;
		card.location.overlay = true;
		card.location.overlay_index = static_cast<std::uint32_t>(host->materials.size() - 1);
		if(position)
			card.position = *position;
		return;
	}

	card.attached_to = CardInstanceId::None;
	if(position)
		card.position = *position;

	auto& contents = mutable_zone(destination.controller, destination.zone);

	if(is_field_zone(destination.zone)) {
		contents[destination.sequence] = card.id;
		card.location = destination;
		card.location.overlay = false;
		card.location.overlay_index = 0;
	} else {
		// Piles. The insertion index is not always the sequence the protocol
		// asked for; ClientField::AddCard recomputes it, and reproducing that
		// exactly is what keeps our sequences comparable with the legacy ones.
		std::size_t index = contents.size();
		if(destination.zone == Zone::Deck) {
			// Sequence 0 means "on top of the deck", which is the front of
			// the vector. Any other sequence appends. Faithful to upstream,
			// oddity included: a card returned to the middle of the deck
			// lands at the bottom, because the client is not told where it
			// really went.
			if(destination.sequence == 0 && !contents.empty())
				index = 0;
		} else if(destination.zone == Zone::ExtraDeck) {
			// Face-up extra-deck cards accumulate at the back; face-down ones
			// go in front of them.
			if(!card.position.face_up())
				index = contents.size() - extra_face_up_[destination.controller];
		}

		contents.insert(contents.begin() + static_cast<std::ptrdiff_t>(index), card.id);
		card.location = destination;
		card.location.overlay = false;
		card.location.overlay_index = 0;
		card.location.sequence = static_cast<std::uint32_t>(index);

		// Everything after the insertion point shifted up by one.
		for(std::size_t i = index + 1; i < contents.size(); ++i) {
			if(auto* shifted = find(contents[i]); shifted != nullptr)
				shifted->location.sequence = static_cast<std::uint32_t>(i);
		}

		if(destination.zone == Zone::ExtraDeck && card.position.face_up())
			++extra_face_up_[destination.controller];
	}

	// A card's material carries a copy of the host's location (see the
	// attach branch above), because at() needs to resolve `HOST[..]#N`
	// without a two-step lookup. That copy goes stale the moment the host
	// itself is placed anywhere - a repositioning effect, a controller
	// change - unless it is refreshed here too. Skipped by every other
	// caller of place() only because they don't have to think about it;
	// this one does, since it is the only place a host's own location is
	// finalised.
	for(const auto material_id : card.materials) {
		if(auto* material = find(material_id); material != nullptr) {
			material->location.controller = card.location.controller;
			material->location.zone = card.location.zone;
			material->location.sequence = card.location.sequence;
		}
	}
}

DuelState::Error DuelState::detach(CardState& card) {
	const auto location = card.location;
	if(location.zone == Zone::None)
		return std::nullopt;

	if(location.overlay) {
		auto* host = find(card.attached_to);
		if(host == nullptr)
			return "material " + to_string(card.id) + " has no host";
		auto& materials = host->materials;
		const auto it = std::find(materials.begin(), materials.end(), card.id);
		if(it == materials.end())
			return "material " + to_string(card.id) + " is not listed on its host";
		materials.erase(it);
		for(std::size_t i = 0; i < materials.size(); ++i) {
			if(auto* moved = find(materials[i]); moved != nullptr)
				moved->location.overlay_index = static_cast<std::uint32_t>(i);
		}
		card.attached_to = CardInstanceId::None;
		card.location = CardLocation{};
		return std::nullopt;
	}

	if(!is_trackable(location.zone))
		return "cannot remove a card from " + std::string(zone_name(location.zone));

	auto& contents = mutable_zone(location.controller, location.zone);
	if(location.sequence >= contents.size() || contents[location.sequence] != card.id)
		return to_string(card.id) + " is not at " + describe(location);

	if(is_field_zone(location.zone)) {
		contents[location.sequence] = CardInstanceId::None;
	} else {
		contents.erase(contents.begin() + static_cast<std::ptrdiff_t>(location.sequence));
		for(std::size_t i = location.sequence; i < contents.size(); ++i) {
			if(auto* shifted = find(contents[i]); shifted != nullptr)
				shifted->location.sequence = static_cast<std::uint32_t>(i);
		}
		if(location.zone == Zone::ExtraDeck && card.position.face_up())
			--extra_face_up_[location.controller];
	}

	card.location = CardLocation{};
	return std::nullopt;
}

DuelState::Error DuelState::create_card(const CardLocation& destination, CardCode code,
										CardPosition position, CardInstanceId* created) {
	CardState card;
	card.id = static_cast<CardInstanceId>(static_cast<std::uint32_t>(cards_.size() + 1));
	card.code = code;
	card.position = position;

	if(auto error = validate_destination(card, destination))
		return error;

	cards_.push_back(card);
	place(cards_.back(), destination, position);
	if(created != nullptr)
		*created = cards_.back().id;
	return std::nullopt;
}

DuelState::Error DuelState::move_card(CardInstanceId id, const CardLocation& destination,
									  std::optional<CardPosition> position) {
	auto* card = find(id);
	if(card == nullptr)
		return "no such card instance " + to_string(id);
	if(!card->tracked)
		return to_string(id) + " has already left play";

	// Validated against the state *before* the card is lifted, so a refused
	// move changes nothing at all. The one wrinkle this creates - a field slot
	// that the card itself occupies - is allowed explicitly.
	if(auto error = validate_destination(*card, destination))
		return error;
	if(auto error = detach(*card))
		return error;
	place(*card, destination, position);
	return std::nullopt;
}

DuelState::Error DuelState::remove_card(CardInstanceId id) {
	auto* card = find(id);
	if(card == nullptr)
		return "no such card instance " + to_string(id);
	if(!card->tracked)
		return to_string(id) + " has already left play";

	// Material is normally moved away by its own MSG_MOVE before the host
	// leaves. If any is still attached, it has nowhere to be, so it stops
	// being tracked too rather than dangling.
	const auto materials = card->materials;
	for(const auto material : materials) {
		if(auto* attached = find(material); attached != nullptr) {
			attached->attached_to = CardInstanceId::None;
			attached->location = CardLocation{};
			attached->tracked = false;
		}
	}
	card = find(id);
	card->materials.clear();

	if(auto error = detach(*card))
		return error;
	card->tracked = false;
	return std::nullopt;
}

DuelState::Error DuelState::set_position(CardInstanceId id, CardPosition position) {
	auto* card = find(id);
	if(card == nullptr)
		return "no such card instance " + to_string(id);
	// The extra deck splits face-up from face-down cards, so a position change
	// there would move the card. Nothing in this slice can produce one, and
	// silently corrupting the split would be worse than refusing.
	if(card->location.zone == Zone::ExtraDeck && card->position.face_up() != position.face_up())
		return "changing face-up state in the extra deck is not modelled";
	card->position = position;
	return std::nullopt;
}

DuelState::Error DuelState::set_code(CardInstanceId id, CardCode code) {
	auto* card = find(id);
	if(card == nullptr)
		return "no such card instance " + to_string(id);
	card->code = code;
	return std::nullopt;
}

DuelState::Error DuelState::set_combat_stats(CardInstanceId id, std::int32_t attack,
											 std::int32_t defense) {
	auto* card = find(id);
	if(card == nullptr)
		return "no such card instance " + to_string(id);
	card->attack = attack;
	card->defense = defense;
	return std::nullopt;
}

DuelState::Error DuelState::push_chain_link(const ChainLink& link) {
	const auto expected = static_cast<std::uint32_t>(chain_.size() + 1);
	if(link.link != expected)
		return "chain link " + std::to_string(link.link) + " arrived where " +
			   std::to_string(expected) + " was expected";
	chain_.push_back(link);
	return std::nullopt;
}

DuelState::Error DuelState::mark_chain_resolving(std::uint32_t link) {
	if(link == 0 || link > chain_.size())
		return "chain link " + std::to_string(link) + " is not on the chain";
	chain_[link - 1].resolving = true;
	return std::nullopt;
}

DuelState::Error DuelState::mark_chain_resolved(std::uint32_t link) {
	if(link == 0 || link > chain_.size())
		return "chain link " + std::to_string(link) + " is not on the chain";
	chain_[link - 1].resolving = false;
	chain_[link - 1].resolved = true;
	return std::nullopt;
}

void DuelState::set_attack(CardInstanceId attacker, CardInstanceId target) noexcept {
	attacker_ = attacker;
	attack_target_ = target;
}

void DuelState::finish(std::optional<PlayerId> winner, std::uint8_t reason) {
	finished_ = true;
	winner_ = winner;
	win_reason_ = reason;
}

std::vector<std::string> DuelState::check_invariants() const {
	std::vector<std::string> problems;

	// Where each card claims to be, versus where the zones say it is.
	std::vector<int> occurrences(cards_.size(), 0);

	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		for(std::size_t z = 0; z < kZoneCount; ++z) {
			const auto as_zone = static_cast<Zone>(z);
			const auto& contents = zones_[zone_slot(player, as_zone)];
			if(!is_trackable(as_zone)) {
				if(!contents.empty())
					problems.push_back("untrackable zone " + std::string(zone_name(as_zone)) +
									   " for " + player_name(player) + " holds cards");
				continue;
			}
			if(is_field_zone(as_zone) && contents.size() != field_zone_capacity(as_zone))
				problems.push_back(std::string(zone_name(as_zone)) + " for " +
								   player_name(player) + " has " +
								   std::to_string(contents.size()) + " slots, expected " +
								   std::to_string(field_zone_capacity(as_zone)));

			for(std::size_t i = 0; i < contents.size(); ++i) {
				const auto id = contents[i];
				if(id == CardInstanceId::None) {
					if(is_pile(as_zone))
						problems.push_back("empty slot inside pile " +
										   std::string(zone_name(as_zone)) + " for " +
										   player_name(player));
					continue;
				}
				const auto* card = find(id);
				if(card == nullptr) {
					problems.push_back("zone holds unknown instance " + to_string(id));
					continue;
				}
				++occurrences[to_number(id) - 1];
				if(!card->tracked)
					problems.push_back(to_string(id) + " is untracked but still in a zone");
				if(card->location.overlay || card->location.controller != player ||
				   card->location.zone != as_zone || card->location.sequence != i)
					problems.push_back(to_string(id) + " claims " +
									   describe(card->location) + " but sits in " +
									   describe(CardLocation{player, as_zone,
															 static_cast<std::uint32_t>(i),
															 false, 0}));
			}
		}
	}

	for(const auto& card : cards_) {
		for(std::size_t i = 0; i < card.materials.size(); ++i) {
			const auto id = card.materials[i];
			const auto* material = find(id);
			if(material == nullptr) {
				problems.push_back("material list holds unknown instance " + to_string(id));
				continue;
			}
			++occurrences[to_number(id) - 1];
			if(material->attached_to != card.id)
				problems.push_back(to_string(id) + " is listed on " + to_string(card.id) +
								   " but attached to " + to_string(material->attached_to));
			if(!material->location.overlay || material->location.overlay_index != i)
				problems.push_back(to_string(id) + " has material index " +
								   std::to_string(material->location.overlay_index) +
								   ", listed at " + std::to_string(i));
			if(material->location.controller != card.location.controller ||
			   material->location.zone != card.location.zone ||
			   material->location.sequence != card.location.sequence)
				problems.push_back(to_string(id) + " is material on " + describe(card.location) +
								   " but names host location " +
								   describe(CardLocation{material->location.controller,
														 material->location.zone,
														 material->location.sequence, false,
														 0}));
		}
	}

	for(const auto& card : cards_) {
		const auto count = occurrences[to_number(card.id) - 1];
		if(card.tracked && count != 1)
			problems.push_back(to_string(card.id) + " occupies " + std::to_string(count) +
							   " slots, expected exactly 1");
		if(!card.tracked && count != 0)
			problems.push_back(to_string(card.id) + " is untracked but occupies " +
							   std::to_string(count) + " slots");
		if(!card.tracked && card.location.zone != Zone::None)
			problems.push_back(to_string(card.id) + " is untracked but claims " +
							   describe(card.location));
	}

	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		const auto& extra = zones_[zone_slot(player, Zone::ExtraDeck)];
		std::uint32_t face_up = 0;
		for(const auto id : extra) {
			if(const auto* card = find(id); card != nullptr && card->position.face_up())
				++face_up;
		}
		if(face_up != extra_face_up_[player])
			problems.push_back("extra deck face-up count for " + player_name(player) + " is " +
							   std::to_string(extra_face_up_[player]) + ", counted " +
							   std::to_string(face_up));
	}

	for(std::size_t i = 0; i < chain_.size(); ++i) {
		if(chain_[i].link != i + 1)
			problems.push_back("chain position " + std::to_string(i) + " holds link " +
							   std::to_string(chain_[i].link));
	}

	return problems;
}

} // namespace edopro_next::client

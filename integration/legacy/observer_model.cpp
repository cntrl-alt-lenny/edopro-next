#include "observer_model.h"

#include <algorithm>
#include <sstream>

#include "edopro_next/client/protocol_constants.h"

namespace edopro_next::legacy_observer {
namespace {

using client::to_number;

std::string message_name(std::uint8_t message) {
	using namespace client::protocol;
	switch(message) {
	case MSG_START: return "MSG_START";
	case MSG_WIN: return "MSG_WIN";
	case MSG_NEW_TURN: return "MSG_NEW_TURN";
	case MSG_NEW_PHASE: return "MSG_NEW_PHASE";
	case MSG_MOVE: return "MSG_MOVE";
	case MSG_POS_CHANGE: return "MSG_POS_CHANGE";
	case MSG_CHAINING: return "MSG_CHAINING";
	case MSG_CHAINED: return "MSG_CHAINED";
	case MSG_CHAIN_SOLVING: return "MSG_CHAIN_SOLVING";
	case MSG_CHAIN_SOLVED: return "MSG_CHAIN_SOLVED";
	case MSG_CHAIN_END: return "MSG_CHAIN_END";
	case MSG_DRAW: return "MSG_DRAW";
	case MSG_DAMAGE: return "MSG_DAMAGE";
	case MSG_RECOVER: return "MSG_RECOVER";
	case MSG_LPUPDATE: return "MSG_LPUPDATE";
	case MSG_PAY_LPCOST: return "MSG_PAY_LPCOST";
	case MSG_ATTACK: return "MSG_ATTACK";
	case MSG_BATTLE: return "MSG_BATTLE";
	default: break;
	}
	return "MSG_0x" + [&] {
		std::ostringstream out;
		out << std::hex << static_cast<unsigned>(message);
		return out.str();
	}();
}

std::string location_name(const CardLocation& location) {
	return client::to_string(location);
}

std::string code_name(CardCode code) {
	return client::is_known(code) ? std::to_string(to_number(code)) : "unknown";
}

std::string position_name(CardPosition position) {
	return position.known() ? std::to_string(position.bits()) : "unknown";
}

bool location_less(const CardLocation& left, const CardLocation& right) {
	if(left.controller != right.controller)
		return left.controller < right.controller;
	if(left.zone != right.zone)
		return left.zone < right.zone;
	if(left.sequence != right.sequence)
		return left.sequence < right.sequence;
	if(left.overlay != right.overlay)
		return left.overlay < right.overlay;
	return left.overlay_index < right.overlay_index;
}

const ProjectedCard* find_card(const std::vector<ProjectedCard>& cards,
								 const CardLocation& location) {
		const auto it = std::find_if(cards.begin(), cards.end(), [&](const auto& card) {
			return card.location == location;
		});
		return it == cards.end() ? nullptr : &*it;
}

void add_mismatch(EquivalenceResult& result, std::uint64_t packet, std::uint8_t message,
				  std::string field, std::string semantic, std::string legacy) {
	result.mismatches.push_back({packet, message, std::move(field), std::move(semantic), std::move(legacy)});
}

} // namespace

std::string Mismatch::format() const {
	return "packet " + std::to_string(packet) + " " + message_name(message) + " " + field
		+ ": semantic = " + semantic + " legacy = " + legacy;
}

EquivalenceResult compare(const DuelState& semantic, const LegacySnapshot& legacy,
						 std::uint64_t packet, std::uint8_t message) {
	EquivalenceResult result;
	for(PlayerId player = 0; player < client::kPlayerCount; ++player) {
		if(semantic.life_points(player) != legacy.life[player])
			add_mismatch(result, packet, message, "p" + std::to_string(player) + ".life",
						  std::to_string(semantic.life_points(player)),
						  std::to_string(legacy.life[player]));
	}
	if(semantic.turn() != legacy.turn)
		add_mismatch(result, packet, message, "turn", std::to_string(semantic.turn()),
					  std::to_string(legacy.turn));

	std::vector<ProjectedCard> semantic_cards;
	for(const auto& card : semantic.cards()) {
		if(!card.tracked)
			continue;
		ProjectedCard projected{card.location, card.code, card.position};
		for(const auto material_id : card.materials) {
			if(const auto* material = semantic.find(material_id); material != nullptr)
				projected.materials.push_back(material->code);
		}
		semantic_cards.push_back(std::move(projected));
	}

	auto sort_cards = [](auto& cards) {
		std::sort(cards.begin(), cards.end(), [](const auto& left, const auto& right) {
			return location_less(left.location, right.location);
		});
	};
	sort_cards(semantic_cards);
	// LegacyProjection already returns structural locations, but sorting here
	// makes the comparator deterministic even for a hand-built test snapshot.
	std::vector<ProjectedCard> legacy_cards = legacy.cards;
	sort_cards(legacy_cards);

	std::vector<CardLocation> locations;
	locations.reserve(semantic_cards.size() + legacy_cards.size());
	for(const auto& card : semantic_cards)
		locations.push_back(card.location);
	for(const auto& card : legacy_cards)
		locations.push_back(card.location);
	std::sort(locations.begin(), locations.end(), location_less);
	locations.erase(std::unique(locations.begin(), locations.end()), locations.end());

	for(const auto& location : locations) {
		const auto* semantic_card = find_card(semantic_cards, location);
		const auto* legacy_card = find_card(legacy_cards, location);
		if(semantic_card == nullptr) {
			add_mismatch(result, packet, message, location_name(location) + ".occupancy", "empty",
						  "occupied");
			continue;
		}
		if(legacy_card == nullptr) {
			add_mismatch(result, packet, message, location_name(location) + ".occupancy", "occupied",
						  "empty");
			continue;
		}
		// An unknown semantic code is an intentional scope exclusion: query
		// packets can teach legacy a code before this slice can know it. The
		// reverse direction remains useful evidence of a real divergence.
		if(client::is_known(semantic_card->code) && semantic_card->code != legacy_card->code)
			add_mismatch(result, packet, message, location_name(location) + ".code",
						  code_name(semantic_card->code), code_name(legacy_card->code));
		if(semantic_card->position.known() && semantic_card->position != legacy_card->position)
			add_mismatch(result, packet, message, location_name(location) + ".position",
						  position_name(semantic_card->position), position_name(legacy_card->position));
		if(semantic_card->materials.size() != legacy_card->materials.size())
			add_mismatch(result, packet, message, location_name(location) + ".materials.count",
						  std::to_string(semantic_card->materials.size()),
						  std::to_string(legacy_card->materials.size()));
		const auto material_count = std::min(semantic_card->materials.size(), legacy_card->materials.size());
		for(std::size_t i = 0; i < material_count; ++i) {
			if(client::is_known(semantic_card->materials[i]) &&
				semantic_card->materials[i] != legacy_card->materials[i])
				add_mismatch(result, packet, message,
							  location_name(location) + ".material[" + std::to_string(i) + "].code",
							  code_name(semantic_card->materials[i]),
							  code_name(legacy_card->materials[i]));
		}
	}
	return result;
}

void ObserverSession::reset(client::ProtocolVariant variant) {
	decoder_ = client::ProtocolDecoder{variant};
	state_ = DuelState{};
	packet_number_ = 0;
	++session_number_;
}

client::DecodeResult ObserverSession::observe(const client::Packet& packet,
														 client::ProtocolVariant variant) {
	if(packet.message == client::protocol::MSG_START)
		reset(variant);
	++packet_number_;
	return decoder_.decode(packet, state_);
}

} // namespace edopro_next::legacy_observer

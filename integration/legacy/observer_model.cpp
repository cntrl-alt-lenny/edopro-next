#include "observer_model.h"

#include <algorithm>
#include <sstream>

namespace edopro_next::legacy_observer {
namespace {

std::string canonical_message_name(std::uint8_t message) {
	const auto canonical = client::protocol::message_name(message);
	if(!canonical.empty())
		return std::string(canonical);
	return [&] {
		std::ostringstream out;
		out << "MSG_0x" << std::hex << static_cast<unsigned>(message);
		return out.str();
	}();
}

std::string location_name(const CardLocation& location) {
	return client::to_string(location);
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

std::string message_name(std::uint8_t message) {
	return canonical_message_name(message);
}

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
		ProjectedCard projected{card.location, static_cast<std::uint32_t>(card.materials.size())};
		semantic_cards.push_back(std::move(projected));
		for(std::size_t index = 0; index < card.materials.size(); ++index) {
			ProjectedCard material_projection;
			material_projection.location = CardLocation{
				card.location.controller, card.location.zone, card.location.sequence, true,
				static_cast<std::uint32_t>(index)};
			semantic_cards.push_back(std::move(material_projection));
		}
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
		if(semantic_card->material_count != legacy_card->material_count)
			add_mismatch(result, packet, message, location_name(location) + ".materials.count",
						  std::to_string(semantic_card->material_count),
												 std::to_string(legacy_card->material_count));
	}
	return result;
}

void ObserverSession::reset(client::ProtocolVariant variant) {
	decoder_ = client::ProtocolDecoder{variant};
	state_ = DuelState{};
	comparison_available_ = true;
	packet_number_ = 0;
	++session_number_;
}

bool ObserverSession::is_query_message(std::uint8_t message) noexcept {
	return message == client::protocol::MSG_UPDATE_DATA ||
		message == client::protocol::MSG_UPDATE_CARD;
}

client::DecodeResult ObserverSession::observe(const client::Packet& packet,
														 client::ProtocolVariant variant) {
	if(packet.message == client::protocol::MSG_START)
		reset(variant);
	++packet_number_;
	const auto result = decoder_.decode(packet, state_);
	// Query packets only carry fields outside the live projection when they are
	// unsupported. They are therefore safe to ignore for comparison purposes;
	// an unsupported non-query packet is conservatively treated as a possible
	// structural state transition until the next explicit session boundary.
	if(result.status != client::DecodeStatus::Decoded &&
		(result.status != client::DecodeStatus::UnsupportedMessage ||
		 !is_query_message(packet.message)))
		comparison_available_ = false;
	return result;
}

} // namespace edopro_next::legacy_observer

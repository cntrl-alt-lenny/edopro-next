#include "observer_model.h"

#include "client_card.h"
#include "client_field.h"
#include "game.h"

#include <algorithm>
#include <utility>

namespace edopro_next::legacy_observer {
namespace {

using client::CardLocation;
using client::PlayerId;

void append_card(const ygo::ClientCard* card, bool is_first, std::vector<ProjectedCard>& output) {
	if(card == nullptr)
		return;
	const auto zone = client::zone_from_protocol(card->location);
	if(!client::is_trackable(zone))
		return;

	ProjectedCard projected;
	projected.location = CardLocation{
		protocol_player_from_local(card->controler, is_first), zone, card->sequence, false, 0};
	projected.material_count = static_cast<std::uint32_t>(card->overlayed.size());
	projected.code = static_cast<client::CardCode>(card->code);
	projected.position = client::CardPosition{card->position};
	projected.attack = card->attack;
	projected.defense = card->defense;
	output.push_back(std::move(projected));

	for(std::size_t index = 0; index < card->overlayed.size(); ++index) {
		const auto* material = card->overlayed[index];
		if(material == nullptr)
			continue;
		projected.material_codes.push_back(static_cast<client::CardCode>(material->code));
		ProjectedCard material_projection;
		material_projection.location = CardLocation{
			protocol_player_from_local(card->controler, is_first), zone, card->sequence, true,
			static_cast<std::uint32_t>(index)};
		output.push_back(std::move(material_projection));
	}
}

template <typename CardVector>
void append_zone(const CardVector& cards, bool is_first, std::vector<ProjectedCard>& output) {
	for(const auto* card : cards)
		append_card(card, is_first, output);
}

} // namespace

LegacySnapshot project_legacy_state(const void* legacy_game) {
	LegacySnapshot snapshot;
	const auto* game = static_cast<const ygo::Game*>(legacy_game);
	if(game == nullptr)
		return snapshot;

	const bool is_first = game->dInfo.isFirst;
	for(PlayerId protocol_player = 0; protocol_player < client::kPlayerCount; ++protocol_player) {
		const auto local_player = protocol_player_from_local(protocol_player, is_first);
		snapshot.life[protocol_player] = game->dInfo.lp[local_player];
	}
	snapshot.turn = static_cast<std::uint32_t>(std::max(game->dInfo.turn, 0));

	for(PlayerId local_player = 0; local_player < client::kPlayerCount; ++local_player) {
		append_zone(game->dField.deck[local_player], is_first, snapshot.cards);
		append_zone(game->dField.hand[local_player], is_first, snapshot.cards);
		append_zone(game->dField.mzone[local_player], is_first, snapshot.cards);
		append_zone(game->dField.szone[local_player], is_first, snapshot.cards);
		append_zone(game->dField.grave[local_player], is_first, snapshot.cards);
		append_zone(game->dField.remove[local_player], is_first, snapshot.cards);
		append_zone(game->dField.extra[local_player], is_first, snapshot.cards);
	}
	return snapshot;
}

} // namespace edopro_next::legacy_observer

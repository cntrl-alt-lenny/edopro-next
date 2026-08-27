#include "edopro_next/client/duel_event.h"

#include "edopro_next/client/protocol_constants.h"

#include <array>
#include <utility>

namespace edopro_next::client {
namespace {

std::string hex(std::uint32_t value) {
	static constexpr char digits[] = "0123456789abcdef";
	std::string out = "0x";
	bool leading = true;
	for(int shift = 28; shift >= 0; shift -= 4) {
		const auto nibble = (value >> shift) & 0xfu;
		if(nibble == 0 && leading && shift != 0)
			continue;
		leading = false;
		out.push_back(digits[nibble]);
	}
	return out;
}

// REASON_* flags, rendered symbolically so a diff names the cause rather than
// showing a changed hex constant. Any bit upstream adds is kept as hex rather
// than dropped, so nothing is silently lost.
std::string reason_names(std::uint32_t reason) {
	static constexpr std::pair<std::uint32_t, std::string_view> kFlags[] = {
		{protocol::REASON_DESTROY, "DESTROY"},
		{protocol::REASON_RELEASE, "RELEASE"},
		{protocol::REASON_TEMPORARY, "TEMPORARY"},
		{protocol::REASON_MATERIAL, "MATERIAL"},
		{protocol::REASON_SUMMON, "SUMMON"},
		{protocol::REASON_BATTLE, "BATTLE"},
		{protocol::REASON_EFFECT, "EFFECT"},
		{protocol::REASON_COST, "COST"},
		{protocol::REASON_ADJUST, "ADJUST"},
		{protocol::REASON_LOST_TARGET, "LOST_TARGET"},
		{protocol::REASON_RULE, "RULE"},
		{protocol::REASON_SPSUMMON, "SPSUMMON"},
		{protocol::REASON_DISSUMMON, "DISSUMMON"},
		{protocol::REASON_FLIP, "FLIP"},
		{protocol::REASON_DISCARD, "DISCARD"},
		{protocol::REASON_RDAMAGE, "RDAMAGE"},
		{protocol::REASON_RRECOVER, "RRECOVER"},
		{protocol::REASON_RETURN, "RETURN"},
		{protocol::REASON_FUSION, "FUSION"},
		{protocol::REASON_SYNCHRO, "SYNCHRO"},
		{protocol::REASON_RITUAL, "RITUAL"},
		{protocol::REASON_XYZ, "XYZ"},
		{protocol::REASON_REPLACE, "REPLACE"},
		{protocol::REASON_DRAW, "DRAW"},
		{protocol::REASON_REDIRECT, "REDIRECT"},
		{protocol::REASON_LINK, "LINK"},
	};
	if(reason == 0)
		return "-";
	std::string out;
	std::uint32_t covered = 0;
	for(const auto& [bit, name] : kFlags) {
		covered |= bit;
		if((reason & bit) == 0)
			continue;
		if(!out.empty())
			out += '|';
		out += name;
	}
	if(const auto leftover = reason & ~covered; leftover != 0) {
		if(!out.empty())
			out += '|';
		out += hex(leftover);
	}
	return out;
}

std::string id_list(const std::vector<CardInstanceId>& ids) {
	std::string out = "[";
	for(std::size_t i = 0; i < ids.size(); ++i) {
		if(i != 0)
			out += ',';
		out += to_string(ids[i]);
	}
	out += ']';
	return out;
}

} // namespace

std::string_view summon_kind_name(SummonKind kind) noexcept {
	switch(kind) {
	case SummonKind::Normal:
		return "NORMAL";
	case SummonKind::Special:
		return "SPECIAL";
	case SummonKind::Flip:
		return "FLIP";
	}
	return "UNKNOWN";
}

std::string_view life_change_reason_name(LifeChangeReason reason) noexcept {
	switch(reason) {
	case LifeChangeReason::Damage:
		return "DAMAGE";
	case LifeChangeReason::Recover:
		return "RECOVER";
	case LifeChangeReason::Cost:
		return "COST";
	case LifeChangeReason::Update:
		return "UPDATE";
	}
	return "UNKNOWN";
}

std::string_view event_name(const DuelEvent& event) {
	return std::visit(
		[](const auto& value) -> std::string_view {
			using T = std::decay_t<decltype(value)>;
			if constexpr(std::is_same_v<T, DuelStarted>)
				return "DuelStarted";
			else if constexpr(std::is_same_v<T, TurnStarted>)
				return "TurnStarted";
			else if constexpr(std::is_same_v<T, PhaseChanged>)
				return "PhaseChanged";
			else if constexpr(std::is_same_v<T, CardsDrawn>)
				return "CardsDrawn";
			else if constexpr(std::is_same_v<T, CardCreated>)
				return "CardCreated";
			else if constexpr(std::is_same_v<T, CardMoved>)
				return "CardMoved";
			else if constexpr(std::is_same_v<T, CardRemoved>)
				return "CardRemoved";
			else if constexpr(std::is_same_v<T, CardIdentityRevealed>)
				return "CardIdentityRevealed";
			else if constexpr(std::is_same_v<T, CardIdentityConcealed>)
				return "CardIdentityConcealed";
			else if constexpr(std::is_same_v<T, PositionChanged>)
				return "PositionChanged";
			else if constexpr(std::is_same_v<T, LifePointsChanged>)
				return "LifePointsChanged";
			else if constexpr(std::is_same_v<T, CardSetAnnounced>)
				return "CardSetAnnounced";
			else if constexpr(std::is_same_v<T, SummonAnnounced>)
				return "SummonAnnounced";
			else if constexpr(std::is_same_v<T, SummonCompleted>)
				return "SummonCompleted";
			else if constexpr(std::is_same_v<T, ChainLinkAdded>)
				return "ChainLinkAdded";
			else if constexpr(std::is_same_v<T, ChainLinkResolving>)
				return "ChainLinkResolving";
			else if constexpr(std::is_same_v<T, ChainLinkResolved>)
				return "ChainLinkResolved";
			else if constexpr(std::is_same_v<T, CardsBecameTargets>)
				return "CardsBecameTargets";
			else if constexpr(std::is_same_v<T, CardHintChanged>)
				return "CardHintChanged";
			else if constexpr(std::is_same_v<T, PlayerHintChanged>)
				return "PlayerHintChanged";
			else if constexpr(std::is_same_v<T, ChainEnded>)
				return "ChainEnded";
			else if constexpr(std::is_same_v<T, AttackDeclared>)
				return "AttackDeclared";
			else if constexpr(std::is_same_v<T, CombatStatsRevealed>)
				return "CombatStatsRevealed";
			else if constexpr(std::is_same_v<T, DamageStepStarted>)
				return "DamageStepStarted";
			else if constexpr(std::is_same_v<T, DamageStepEnded>)
				return "DamageStepEnded";
			else if constexpr(std::is_same_v<T, DeckShuffled>)
				return "DeckShuffled";
			else if constexpr(std::is_same_v<T, HandShuffled>)
				return "HandShuffled";
			else
				return "DuelEnded";
		},
		event);
}

std::string to_string(const DuelEvent& event) {
	std::string out(event_name(event));
	out += ' ';

	std::visit(
		[&out](const auto& value) {
			using T = std::decay_t<decltype(value)>;
			if constexpr(std::is_same_v<T, DuelStarted>) {
				out += "player_type=" + std::to_string(static_cast<unsigned>(value.player_type));
				out += " lp=[" + std::to_string(value.life[0]) + "," +
					   std::to_string(value.life[1]) + "]";
				out += " deck=[" + std::to_string(value.decks[0].main) + "," +
					   std::to_string(value.decks[1].main) + "]";
				out += " extra=[" + std::to_string(value.decks[0].extra) + "," +
					   std::to_string(value.decks[1].extra) + "]";
			} else if constexpr(std::is_same_v<T, TurnStarted>) {
				out += "player=" + player_name(value.player);
				out += " turn=" + std::to_string(value.turn);
			} else if constexpr(std::is_same_v<T, PhaseChanged>) {
				out += phase_name(value.phase);
			} else if constexpr(std::is_same_v<T, CardsDrawn>) {
				out += "player=" + player_name(value.player);
				out += " cards=" + id_list(value.cards);
			} else if constexpr(std::is_same_v<T, CardCreated>) {
				out += "instance=" + to_string(value.card);
				out += " at=" + to_string(value.at);
				out += " code=" + to_string(value.code);
			} else if constexpr(std::is_same_v<T, CardMoved>) {
				out += "instance=" + to_string(value.card);
				out += " " + to_string(value.from) + " -> " + to_string(value.to);
				out += " reason=" + reason_names(value.reason);
			} else if constexpr(std::is_same_v<T, CardRemoved>) {
				out += "instance=" + to_string(value.card);
				out += " from=" + to_string(value.from);
				out += " reason=" + reason_names(value.reason);
			} else if constexpr(std::is_same_v<T, CardIdentityRevealed>) {
				out += "instance=" + to_string(value.card);
				out += " code=" + to_string(value.code);
			} else if constexpr(std::is_same_v<T, CardIdentityConcealed>) {
				out += "instance=" + to_string(value.card);
			} else if constexpr(std::is_same_v<T, PositionChanged>) {
				out += "instance=" + to_string(value.card);
				out += " " + to_string(value.from) + " -> " + to_string(value.to);
			} else if constexpr(std::is_same_v<T, LifePointsChanged>) {
				out += "player=" + player_name(value.player);
				out += " " + std::to_string(value.from) + " -> " + std::to_string(value.to);
				out += " reason=";
				out += life_change_reason_name(value.reason);
				out += " amount=" + std::to_string(value.amount);
			} else if constexpr(std::is_same_v<T, CardSetAnnounced>) {
				out += "at=" + to_string(value.at);
				out += " code=" + to_string(value.code);
			} else if constexpr(std::is_same_v<T, SummonAnnounced>) {
				out += "kind=";
				out += summon_kind_name(value.kind);
				out += " code=" + to_string(value.code);
				out += " at=" + to_string(value.at);
			} else if constexpr(std::is_same_v<T, SummonCompleted>) {
				out += "kind=";
				out += summon_kind_name(value.kind);
			} else if constexpr(std::is_same_v<T, ChainLinkAdded>) {
				const auto& link = value.link;
				out += "link=" + std::to_string(link.link);
				out += " instance=" + to_string(link.card);
				out += " code=" + to_string(link.code);
				out += " from=" + to_string(link.card_location);
				out += " trigger=" + std::string(zone_name(link.triggering_zone)) + "[" +
					   player_name(link.triggering_controller) + ":" +
					   std::to_string(link.triggering_sequence) + "]";
				out += " desc=" + std::to_string(link.description);
			} else if constexpr(std::is_same_v<T, ChainLinkResolving>) {
				out += "link=" + std::to_string(value.link);
			} else if constexpr(std::is_same_v<T, ChainLinkResolved>) {
				out += "link=" + std::to_string(value.link);
			} else if constexpr(std::is_same_v<T, CardsBecameTargets>) {
				out += "cards=" + id_list(value.cards);
			} else if constexpr(std::is_same_v<T, CardHintChanged>) {
				out += "instance=" + to_string(value.card);
				out += " type=" + std::to_string(static_cast<unsigned>(value.type));
				out += " value=" + std::to_string(value.value);
			} else if constexpr(std::is_same_v<T, PlayerHintChanged>) {
				out += "player=" + player_name(value.player);
				out += " type=" + std::to_string(static_cast<unsigned>(value.type));
				out += " value=" + std::to_string(value.value);
			} else if constexpr(std::is_same_v<T, ChainEnded>) {
				out += "links=" + std::to_string(value.links);
			} else if constexpr(std::is_same_v<T, AttackDeclared>) {
				out += "attacker=" + to_string(value.attacker);
				out += value.direct ? " target=direct"
									: " target=" + to_string(value.target);
			} else if constexpr(std::is_same_v<T, CombatStatsRevealed>) {
				out += "instance=" + to_string(value.card);
				out += " atk=" + std::to_string(value.attack);
				out += " def=" + std::to_string(value.defense);
			} else if constexpr(std::is_same_v<T, DamageStepStarted> ||
								std::is_same_v<T, DamageStepEnded>) {
				// Nothing to say beyond the name.
			} else if constexpr(std::is_same_v<T, DeckShuffled>) {
				out += "player=" + player_name(value.player);
				out += " cards=" + std::to_string(value.cards);
			} else if constexpr(std::is_same_v<T, HandShuffled>) {
				out += "player=" + player_name(value.player);
				out += " cards=" + std::to_string(value.cards);
			} else {
				out += "winner=";
				out += value.winner ? player_name(*value.winner) : std::string("none");
				out += " reason=" + std::to_string(static_cast<unsigned>(value.reason));
			}
		},
		event);

	while(!out.empty() && out.back() == ' ')
		out.pop_back();
	return out;
}

} // namespace edopro_next::client

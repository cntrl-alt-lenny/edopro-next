#include "edopro_next/client/card_identity.h"
#include "edopro_next/client/card_location.h"
#include "edopro_next/client/card_state.h"
#include "edopro_next/client/protocol_constants.h"

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

} // namespace

std::string player_name(PlayerId player) {
	return "p" + std::to_string(static_cast<unsigned>(player));
}

std::string to_string(CardInstanceId id) {
	if(id == CardInstanceId::None)
		return "-";
	return std::to_string(to_number(id));
}

std::string to_string(CardCode code) {
	if(!is_known(code))
		return "?";
	return std::to_string(to_number(code));
}

Zone zone_from_protocol(std::uint32_t location) noexcept {
	// LOCATION_OVERLAY is a modifier on a base location, not a location of
	// its own; CardLocation carries it separately.
	const auto base = location & ~protocol::LOCATION_OVERLAY;
	switch(base) {
	case 0:
		return Zone::None;
	case protocol::LOCATION_DECK:
		return Zone::Deck;
	case protocol::LOCATION_HAND:
		return Zone::Hand;
	case protocol::LOCATION_MZONE:
		return Zone::MonsterZone;
	case protocol::LOCATION_SZONE:
		return Zone::SpellZone;
	case protocol::LOCATION_GRAVE:
		return Zone::Graveyard;
	case protocol::LOCATION_REMOVED:
		return Zone::Banished;
	case protocol::LOCATION_EXTRA:
		return Zone::ExtraDeck;
	default:
		// Includes EDOPro's LOCATION_SKILL and any combination upstream may
		// introduce. Reported, never silently folded into a modelled zone.
		return Zone::Unknown;
	}
}

std::string_view zone_name(Zone zone) noexcept {
	switch(zone) {
	case Zone::None:
		return "NONE";
	case Zone::Deck:
		return "DECK";
	case Zone::Hand:
		return "HAND";
	case Zone::MonsterZone:
		return "MZONE";
	case Zone::SpellZone:
		return "SZONE";
	case Zone::Graveyard:
		return "GRAVE";
	case Zone::Banished:
		return "REMOVED";
	case Zone::ExtraDeck:
		return "EXTRA";
	case Zone::Unknown:
		break;
	}
	return "UNKNOWN";
}

std::string to_string(const CardLocation& location) {
	if(location.zone == Zone::None)
		return "-";
	std::string out(zone_name(location.zone));
	out += '[';
	out += player_name(location.controller);
	out += ':';
	out += std::to_string(location.sequence);
	out += ']';
	if(location.overlay) {
		out += '#';
		out += std::to_string(location.overlay_index);
	}
	return out;
}

std::string to_string(CardPosition position) {
	switch(position.bits()) {
	case 0:
		return "-";
	case protocol::POS_FACEUP_ATTACK:
		return "FACEUP_ATTACK";
	case protocol::POS_FACEDOWN_ATTACK:
		return "FACEDOWN_ATTACK";
	case protocol::POS_FACEUP_DEFENSE:
		return "FACEUP_DEFENSE";
	case protocol::POS_FACEDOWN_DEFENSE:
		return "FACEDOWN_DEFENSE";
	case protocol::POS_FACEUP:
		return "FACEUP";
	case protocol::POS_FACEDOWN:
		return "FACEDOWN";
	case protocol::POS_ATTACK:
		return "ATTACK";
	case protocol::POS_DEFENSE:
		return "DEFENSE";
	default:
		return hex(position.bits());
	}
}

} // namespace edopro_next::client

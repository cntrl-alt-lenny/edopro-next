// Where a card is, semantically.
//
// The protocol describes a location as (controller, location bits, sequence,
// position). The location byte is a bitmask in which LOCATION_OVERLAY (0x80)
// is a *modifier*: it means "attached to the card at the base location and
// sequence", and in that case the protocol reuses the `position` field as the
// index within that card's material pile. See gframe/core_utils.cpp
// (ReadLocInfo) and the MSG_MOVE handler in gframe/duelclient.cpp.
//
// Nothing here describes where a card is drawn. A zone plus a sequence is the
// whole of it; screen coordinates are the presentation layer's business.
#ifndef EDOPRO_NEXT_CLIENT_CARD_LOCATION_H
#define EDOPRO_NEXT_CLIENT_CARD_LOCATION_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "edopro_next/client/card_identity.h"

namespace edopro_next::client {

// Zones the client tracks. `None` is the protocol's location 0, which MSG_MOVE
// uses to mean "off the field entirely" - the source of a card being created
// and the destination of one being removed. `Unknown` is any location value
// upstream may add, or that this build does not model (LOCATION_SKILL, for
// instance); it is never silently treated as one of the others.
enum class Zone : std::uint8_t {
	None = 0,
	Deck,
	Hand,
	MonsterZone,
	SpellZone,
	Graveyard,
	Banished,
	ExtraDeck,
	Unknown,
};

inline constexpr std::size_t kZoneCount = static_cast<std::size_t>(Zone::Unknown) + 1;

// Translate the protocol's location bits, ignoring LOCATION_OVERLAY, which is
// carried separately on CardLocation.
Zone zone_from_protocol(std::uint32_t location) noexcept;

// True for zones the client models as an ordered pile that grows and shrinks
// (deck, hand, graveyard, banished, extra deck). Contrast with the two field
// zones, which are fixed arrays of addressable slots that may be empty.
constexpr bool is_pile(Zone zone) noexcept {
	switch(zone) {
	case Zone::Deck:
	case Zone::Hand:
	case Zone::Graveyard:
	case Zone::Banished:
	case Zone::ExtraDeck:
		return true;
	default:
		return false;
	}
}

constexpr bool is_field_zone(Zone zone) noexcept {
	return zone == Zone::MonsterZone || zone == Zone::SpellZone;
}

constexpr bool is_trackable(Zone zone) noexcept {
	return is_pile(zone) || is_field_zone(zone);
}

// Slot counts for the two addressable zones, matching ClientField's fixed
// vectors in gframe/client_field.cpp: 5 main monster zones plus 2 extra
// monster zones, and 5 spell/trap zones plus field, plus 2 pendulum zones.
constexpr std::size_t field_zone_capacity(Zone zone) noexcept {
	switch(zone) {
	case Zone::MonsterZone:
		return 7;
	case Zone::SpellZone:
		return 8;
	default:
		return 0;
	}
}

std::string_view zone_name(Zone zone) noexcept;

struct CardLocation {
	PlayerId controller = 0;
	Zone zone = Zone::None;
	std::uint32_t sequence = 0;
	// Attached as material to the card at (controller, zone, sequence).
	bool overlay = false;
	// Index within that card's material pile. Only meaningful when `overlay`.
	std::uint32_t overlay_index = 0;

	friend bool operator==(const CardLocation&, const CardLocation&) = default;
};

// "HAND[p0:2]", "MZONE[p1:4]", "MZONE[p1:4]#1" for material, "-" for None.
std::string to_string(const CardLocation& location);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_CARD_LOCATION_H

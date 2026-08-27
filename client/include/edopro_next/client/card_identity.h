// Identity types for the semantic duel model.
//
// Two different 32-bit numbers describe a card and confusing them is the
// single easiest mistake to make in this layer, so neither is a bare integer:
//
//   CardCode        the passcode printed on the card. Shared by every copy.
//   CardInstanceId  one physical card in one duel. Unique, client-allocated.
//
// A passcode is emphatically *not* an identity: a deck may hold three copies
// of the same card, and the protocol sends a passcode of 0 for any card the
// client is not entitled to see. See docs/adr/0002-semantic-event-model.md.
#ifndef EDOPRO_NEXT_CLIENT_CARD_IDENTITY_H
#define EDOPRO_NEXT_CLIENT_CARD_IDENTITY_H

#include <cstdint>
#include <string>

namespace edopro_next::client {

// One physical card, tracked from the moment the client first learns it
// exists until it stops being tracked. Allocated by DuelState, monotonically
// increasing, never reused within a duel.
enum class CardInstanceId : std::uint32_t { None = 0 };

// A card passcode. `None` means "this client does not know what this card is",
// which is a normal, permanent possibility - not an error and not a default to
// be filled in later from some other source.
enum class CardCode : std::uint32_t { None = 0 };

constexpr std::uint32_t to_number(CardInstanceId id) noexcept {
	return static_cast<std::uint32_t>(id);
}

constexpr std::uint32_t to_number(CardCode code) noexcept {
	return static_cast<std::uint32_t>(code);
}

constexpr bool is_known(CardCode code) noexcept {
	return code != CardCode::None;
}

// Player index exactly as the protocol carries it: 0 and 1 are the duelists.
// This is never the local/remote distinction - which player sits at the bottom
// of the screen is a presentation decision and does not belong here.
using PlayerId = std::uint8_t;

inline constexpr PlayerId kPlayerCount = 2;

constexpr bool is_duelist(PlayerId player) noexcept {
	return player < kPlayerCount;
}

// "p0" / "p1", or "p<n>" for the protocol's non-duelist values (2 = none,
// 3 = all), which must still render deterministically. Named rather than
// overloaded on to_string, because PlayerId is an alias for an integer type
// and an overload on it would swallow unrelated integer arguments.
std::string player_name(PlayerId player);
std::string to_string(CardInstanceId id);
std::string to_string(CardCode code);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_CARD_IDENTITY_H

// What the client knows about one card instance.
//
// Everything here is either something the protocol stated or something derived
// from it. There is deliberately no room for a transform, a texture, an alpha,
// an animation frame or a widget pointer: compare gframe/client_card.h, where
// twenty renderer fields sit in front of the twelve semantic ones. Separating
// them is the point of M2.
#ifndef EDOPRO_NEXT_CLIENT_CARD_STATE_H
#define EDOPRO_NEXT_CLIENT_CARD_STATE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "edopro_next/client/card_identity.h"
#include "edopro_next/client/card_location.h"

namespace edopro_next::client {

// The protocol's position bits (POS_FACEUP_ATTACK and friends). Kept as bits
// rather than collapsed to an enum because the protocol legitimately sends
// combinations: a card in hand is reported as POS_FACEDOWN, which is both
// face-down attack and face-down defence.
class CardPosition {
public:
	constexpr CardPosition() = default;
	explicit constexpr CardPosition(std::uint8_t bits) noexcept : bits_(bits) {}

	constexpr std::uint8_t bits() const noexcept { return bits_; }
	// The protocol uses 0 to mean "no position stated", which happens for
	// cards in a pile where the concept does not apply.
	constexpr bool known() const noexcept { return bits_ != 0; }
	constexpr bool face_up() const noexcept { return (bits_ & 0x5u) != 0; }
	constexpr bool face_down() const noexcept { return (bits_ & 0xau) != 0; }
	constexpr bool attack() const noexcept { return (bits_ & 0x3u) != 0; }
	constexpr bool defense() const noexcept { return (bits_ & 0xcu) != 0; }

	friend bool operator==(CardPosition, CardPosition) = default;

private:
	std::uint8_t bits_ = 0;
};

// "FACEUP_ATTACK", "FACEDOWN", "-" when unstated, "0x<bits>" for a combination
// upstream may add. Never lossy and never guessed.
std::string to_string(CardPosition position);

struct CardState {
	CardInstanceId id = CardInstanceId::None;

	// CardCode::None means the client has not been told what this card is.
	// That is a first-class state, not a placeholder: a face-down card, a
	// card in the opponent's hand and a card in a shuffled deck are all
	// legitimately unknown, and may become known - or unknown again - later.
	CardCode code = CardCode::None;

	CardLocation location{};
	CardPosition position{};

	// Set only when the protocol has stated them (currently MSG_BATTLE).
	// Absent means unknown, not zero.
	std::optional<std::int32_t> attack;
	std::optional<std::int32_t> defense;

	// Material attached to this card, in protocol order.
	std::vector<CardInstanceId> materials;
	// Set when this card is itself material on another card.
	CardInstanceId attached_to = CardInstanceId::None;

	// False once the card leaves tracked play (MSG_MOVE to location 0). The
	// record is kept so the id stays meaningful in earlier events and so a
	// later reference to a stale id is reported rather than mistaken for a
	// live card.
	bool tracked = true;

	bool identity_known() const noexcept { return is_known(code); }

	// Whole-value equality, used to prove a refused packet left a DuelState
	// byte-for-byte unchanged. Every member above is itself a value type with
	// its own `==`, so this is exact, not a spot check of a few fields.
	friend bool operator==(const CardState&, const CardState&) = default;
};

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_CARD_STATE_H

#ifndef EDOPRO_NEXT_LEGACY_OBSERVER_MODEL_H
#define EDOPRO_NEXT_LEGACY_OBSERVER_MODEL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "edopro_next/client/card_state.h"
#include "edopro_next/client/duel_state.h"
#include "edopro_next/client/packet.h"
#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/protocol_decoder.h"

namespace edopro_next::legacy_observer {

using client::CardCode;
using client::CardLocation;
using client::CardPosition;
using client::DuelState;
using client::PlayerId;
using client::Zone;

// A value-only representation extracted from the real ClientField. It is
// intentionally not a second legacy model: production code constructs this
// only by walking Game::dField and Game::dInfo.
struct ProjectedCard {
	CardLocation location{};
	CardCode code = CardCode::None;
	CardPosition position{};
	std::vector<CardCode> materials;
};

struct LegacySnapshot {
	std::array<std::int64_t, client::kPlayerCount> life{};
	std::uint32_t turn = 0;
	std::vector<ProjectedCard> cards;
};

struct Mismatch {
	std::uint64_t packet = 0;
	std::uint8_t message = 0;
	std::string field;
	std::string semantic;
	std::string legacy;

	std::string format() const;
};

struct EquivalenceResult {
	std::vector<Mismatch> mismatches;

	bool equivalent() const noexcept { return mismatches.empty(); }
};

// Game::LocalPlayer is the only perspective conversion needed here. Keeping
// this in the adapter, rather than in client/, preserves protocol-absolute
// semantic player ids.
constexpr PlayerId protocol_player_from_local(PlayerId local, bool is_first) noexcept {
	return is_first ? local : static_cast<PlayerId>(1 - local);
}

EquivalenceResult compare(const DuelState& semantic, const LegacySnapshot& legacy,
						 std::uint64_t packet, std::uint8_t message);

// Implemented in the legacy adapter. The opaque parameter keeps the C++17
// gframe-facing header free of Game/Irrlicht declarations.
LegacySnapshot project_legacy_state(const void* legacy_game);

// A session owns exactly one semantic state. MSG_START is the explicit
// boundary used by all supported live paths; resetting before it also handles
// replay restart and rematch without retaining CardInstanceIds from the old
// duel.
class ObserverSession {
public:
	ObserverSession() = default;

	client::DecodeResult observe(const client::Packet& packet,
								client::ProtocolVariant variant);
	const DuelState& state() const noexcept { return state_; }
	std::uint64_t packet_number() const noexcept { return packet_number_; }
	std::uint64_t session_number() const noexcept { return session_number_; }
	void reset(client::ProtocolVariant variant);

private:
	client::ProtocolDecoder decoder_{};
	DuelState state_{};
	std::uint64_t packet_number_ = 0;
	std::uint64_t session_number_ = 0;
};

} // namespace edopro_next::legacy_observer

#endif // EDOPRO_NEXT_LEGACY_OBSERVER_MODEL_H

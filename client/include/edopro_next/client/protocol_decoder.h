// Raw MSG_* packet -> semantic events, applied to a DuelState.
//
// This is the only place in the client that knows the wire format. It replaces
// nothing: DuelClient::ClientAnalyze continues to decode the same stream for
// the legacy renderer. The two are independent by design, so the new model can
// be developed and tested without touching the duel path.
//
// The decoder does not interpret the duel. It does not decide legality, it
// does not infer anything the protocol did not say, and it does not invent a
// card's identity. When it cannot do something it says which of four different
// things went wrong, because collapsing them would make the coverage report a
// lie.
#ifndef EDOPRO_NEXT_CLIENT_PROTOCOL_DECODER_H
#define EDOPRO_NEXT_CLIENT_PROTOCOL_DECODER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "edopro_next/client/duel_event.h"
#include "edopro_next/client/duel_state.h"
#include "edopro_next/client/packet.h"

namespace edopro_next::client {

enum class DecodeStatus : std::uint8_t {
	// Fully understood; state updated and events produced.
	Decoded,
	// A message upstream defines that this slice does not decode yet. The
	// state is untouched and correct as far as it goes, but incomplete.
	UnsupportedMessage,
	// An id upstream does not define at all. Either the stream is corrupt or
	// upstream has added a message and our generated table is stale.
	UnknownMessage,
	// A message we do decode whose payload could not be read: too short, or
	// with bytes left over. This is the only status that indicates a bug in
	// this decoder or a genuinely broken stream.
	Malformed,
	// The payload read cleanly but refers to something impossible in the
	// current state - a card in an empty slot, a chain link out of order. The
	// state is left unchanged.
	Inconsistent,
};

std::string_view decode_status_name(DecodeStatus status) noexcept;

struct DecodeResult {
	DecodeStatus status = DecodeStatus::UnknownMessage;
	std::uint8_t message = 0;
	// Empty when Decoded. Otherwise a short, deterministic explanation.
	std::string detail;
	std::vector<DuelEvent> events;

	bool ok() const noexcept { return status == DecodeStatus::Decoded; }
};

// Wire-format differences that are not carried in the messages themselves.
struct ProtocolVariant {
	// Pre-LUA64 servers and replays narrow loc_info sequence and position to
	// one byte each, and several counts and description ids likewise. The
	// legacy client derives this from the replay header flag REPLAY_LUA64
	// (gframe/replay_mode.cpp) or from the server handshake
	// (gframe/duelclient.cpp), never from the message stream - so it must be
	// supplied by whoever opened the stream.
	bool compat = false;

	friend bool operator==(const ProtocolVariant&, const ProtocolVariant&) = default;
};

class ProtocolDecoder {
public:
	ProtocolDecoder() = default;
	explicit ProtocolDecoder(ProtocolVariant variant) noexcept : variant_(variant) {}

	const ProtocolVariant& variant() const noexcept { return variant_; }

	// Decodes one packet. `state` is left exactly as it was for any result
	// other than Decoded - Malformed and Inconsistent included, however far a
	// handler got into mutating before it discovered the packet must be
	// refused. Enforced by decoding against a private copy and committing it
	// back only once the packet is fully accepted; see the implementation.
	DecodeResult decode(const Packet& packet, DuelState& state);

	// Message ids this build decodes, ascending. Used by the coverage report
	// and by tests, so that "supported" is never a hand-maintained list in
	// prose that drifts from the switch statement.
	static const std::vector<std::uint8_t>& supported_messages();
	static bool supports(std::uint8_t message) noexcept;

private:
	ProtocolVariant variant_{};
};

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_PROTOCOL_DECODER_H

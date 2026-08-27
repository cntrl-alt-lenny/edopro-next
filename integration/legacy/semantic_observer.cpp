#include "semantic_observer.h"

#include "observer_model.h"

#include "game.h"

#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>

namespace {

using edopro_next::client::DecodeResult;
using edopro_next::client::Packet;
using edopro_next::client::ProtocolVariant;
using edopro_next::legacy_observer::EquivalenceResult;
using edopro_next::legacy_observer::ObserverSession;

struct PendingObservation {
	std::uint8_t message = 0;
	std::uint64_t packet = 0;
	bool compare = false;
	bool legacy_state_lock_held = false;
	void* game = nullptr;
};

struct ObserverRuntime {
	ObserverSession session;
	std::mutex mutex;

	void report_decode(std::uint64_t packet, std::uint8_t message,
						 const DecodeResult& result, bool newly_tainted) {
		if(result.status == edopro_next::client::DecodeStatus::Decoded)
			return;
		if(result.status == edopro_next::client::DecodeStatus::UnsupportedMessage) {
			if(edopro_next::legacy_observer::ObserverSession::is_query_message(message) ||
				!newly_tainted)
				return;
			std::fprintf(stderr,
				"edopro-next semantic observer: packet %llu message 0x%02x %s "
				"is an unsupported packet; "
				"equivalence checks suspended for this session\n",
				static_cast<unsigned long long>(packet), static_cast<unsigned>(message),
				edopro_next::legacy_observer::message_name(message).c_str());
			return;
		}
		std::fprintf(stderr, "edopro-next semantic observer: packet %llu message 0x%02x %s: %s\n",
					 static_cast<unsigned long long>(packet), static_cast<unsigned>(message),
					 std::string(edopro_next::client::decode_status_name(result.status)).c_str(),
					 result.detail.c_str());
	}

	void report_compare(const EquivalenceResult& result) {
		for(const auto& mismatch : result.mismatches)
			std::fprintf(stderr, "edopro-next semantic observer: %s\n", mismatch.format().c_str());
	}
};

ObserverRuntime& runtime() {
	static ObserverRuntime instance;
	return instance;
}

} // namespace

extern "C" void* edopro_next_semantic_observer_begin(
		std::uint8_t message, const std::uint8_t* payload, std::uint32_t payload_length,
		bool compat, bool legacy_race_size, bool legacy_state_lock_held, void* legacy_game) noexcept {
	try {
		auto& instance = runtime();
		std::lock_guard<std::mutex> lock(instance.mutex);
		Packet packet;
		packet.message = message;
		if(payload != nullptr)
			packet.payload.assign(payload, payload + payload_length);
		const bool comparison_was_available = instance.session.comparison_available();
		const auto result = instance.session.observe(packet, ProtocolVariant{compat, legacy_race_size});
		const bool newly_tainted = comparison_was_available &&
			!instance.session.comparison_available();
		instance.report_decode(instance.session.packet_number(), message, result, newly_tainted);
		// The game pointer is read only at scope exit, after ClientAnalyze's
		// legacy switch has run. Store it immediately after the safe decode.
		auto token = std::make_unique<PendingObservation>();
		token->message = message;
		token->packet = instance.session.packet_number();
		token->compare = result.status == edopro_next::client::DecodeStatus::Decoded &&
			instance.session.comparison_available();
		token->legacy_state_lock_held = legacy_state_lock_held;
		token->game = legacy_game;
		return token.release();
	} catch(const std::exception& error) {
		std::fprintf(stderr, "edopro-next semantic observer disabled for packet: %s\n", error.what());
	} catch(...) {
		std::fprintf(stderr, "edopro-next semantic observer disabled for packet: unknown exception\n");
	}
	return nullptr;
}

extern "C" void edopro_next_semantic_observer_end(void* opaque) noexcept {
	if(opaque == nullptr)
		return;
	std::unique_ptr<PendingObservation> token(static_cast<PendingObservation*>(opaque));
	if(!token->compare || token->game == nullptr)
		return;
	try {
		auto& instance = runtime();
		std::lock_guard<std::mutex> lock(instance.mutex);
		auto* game = static_cast<ygo::Game*>(token->game);
		std::unique_lock<epro::mutex> game_lock;
		if(!token->legacy_state_lock_held)
			game_lock = std::unique_lock<epro::mutex>(game->gMutex);
		const auto legacy = edopro_next::legacy_observer::project_legacy_state(token->game);
		const auto result = edopro_next::legacy_observer::compare(
			instance.session.state(), legacy, token->packet, token->message);
		if(!result.equivalent())
			instance.report_compare(result);
	} catch(const std::exception& error) {
		std::fprintf(stderr, "edopro-next semantic observer comparison skipped: %s\n", error.what());
	} catch(...) {
		std::fprintf(stderr, "edopro-next semantic observer comparison skipped: unknown exception\n");
	}
}

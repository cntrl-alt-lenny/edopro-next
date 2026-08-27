#ifndef EDOPRO_NEXT_REPLAY_VERIFIER_H
#define EDOPRO_NEXT_REPLAY_VERIFIER_H

#include <cstdint>

extern "C" {
int edopro_next_verify_replay_cli(const char* path) noexcept;
}

#if defined(__cplusplus) && __cplusplus >= 202002L
#include "observer_model.h"
#include <string>
#include <vector>

namespace edopro_next::legacy_observer {

struct ReplayVerificationStats {
	std::string fixture_name;
	std::uint64_t packets_processed = 0;
	std::uint64_t decode_failures = 0;
	std::vector<Mismatch> mismatches;
	bool completed = false;

	bool equivalent() const noexcept {
		return completed && decode_failures == 0 && mismatches.empty();
	}
};

void set_active_verification_stats(ReplayVerificationStats* stats) noexcept;
ReplayVerificationStats* get_active_verification_stats() noexcept;

ReplayVerificationStats verify_replay(const std::string& path, bool inject_fault = false);
int verify_replay_cli(const std::string& path, bool inject_fault = false);

} // namespace edopro_next::legacy_observer
#endif

#endif // EDOPRO_NEXT_REPLAY_VERIFIER_H

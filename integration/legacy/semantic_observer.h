#ifndef EDOPRO_NEXT_SEMANTIC_OBSERVER_H
#define EDOPRO_NEXT_SEMANTIC_OBSERVER_H

// This header is intentionally C++17-compatible. gframe includes it without
// including any C++20 semantic-model header.
#include <cstdint>

extern "C" {

void* edopro_next_semantic_observer_begin(std::uint8_t message,
										const std::uint8_t* payload,
										std::uint32_t payload_length,
										bool compat,
										bool legacy_state_lock_held,
										void* legacy_game) noexcept;
void edopro_next_semantic_observer_end(void* token) noexcept;

}

#if defined(EDOPRO_NEXT_SEMANTIC_OBSERVER)
namespace edopro_next::legacy_observer {

class ObservationScope final {
public:
	ObservationScope(std::uint8_t message, const std::uint8_t* payload,
					 std::uint32_t payload_length, bool compat, bool legacy_state_lock_held,
					 void* legacy_game) noexcept
		: token_(edopro_next_semantic_observer_begin(message, payload, payload_length,
													 compat, legacy_state_lock_held, legacy_game)) {}
	ObservationScope(const ObservationScope&) = delete;
	ObservationScope& operator=(const ObservationScope&) = delete;
	~ObservationScope() { edopro_next_semantic_observer_end(token_); }

private:
	void* token_ = nullptr;
};

} // namespace edopro_next::legacy_observer
#else
namespace edopro_next::legacy_observer {

class ObservationScope final {
public:
	ObservationScope(std::uint8_t, const std::uint8_t*, std::uint32_t, bool, bool, void*) noexcept {}
};

} // namespace edopro_next::legacy_observer
#endif

#endif // EDOPRO_NEXT_SEMANTIC_OBSERVER_H

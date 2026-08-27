// Builds MSG_* payloads by hand, so tests can state a layout explicitly.
//
// Fixtures prove the decoder reads real streams. This proves it reads the
// layouts we believe it reads - including the ones no committed fixture
// exercises (the compat widths) and the ones no healthy stream contains
// (truncated payloads, impossible references).
#ifndef EDOPRO_NEXT_CLIENT_TEST_PACKET_BUILDER_H
#define EDOPRO_NEXT_CLIENT_TEST_PACKET_BUILDER_H

#include <cstdint>
#include <vector>

#include "edopro_next/client/packet.h"

namespace edopro_next::testing {

class PayloadBuilder {
public:
	PayloadBuilder& u8(std::uint8_t value) {
		bytes_.push_back(value);
		return *this;
	}
	PayloadBuilder& u16(std::uint16_t value) { return little(value, 2); }
	PayloadBuilder& u32(std::uint32_t value) { return little(value, 4); }
	PayloadBuilder& u64(std::uint64_t value) { return little(value, 8); }

	// A loc_info, at whichever width the protocol revision uses.
	PayloadBuilder& loc(std::uint8_t controller, std::uint8_t location, std::uint32_t sequence,
						std::uint32_t position, bool compat = false) {
		u8(controller).u8(location);
		if(compat)
			u8(static_cast<std::uint8_t>(sequence)).u8(static_cast<std::uint8_t>(position));
		else
			u32(sequence).u32(position);
		return *this;
	}

	std::vector<std::uint8_t> take() const { return bytes_; }

	client::Packet packet(std::uint8_t message) const { return client::Packet{message, bytes_}; }

private:
	PayloadBuilder& little(std::uint64_t value, int width) {
		for(int i = 0; i < width; ++i)
			bytes_.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu));
		return *this;
	}

	std::vector<std::uint8_t> bytes_;
};

} // namespace edopro_next::testing

#endif // EDOPRO_NEXT_CLIENT_TEST_PACKET_BUILDER_H

#include "edopro_next/client/packet.h"

namespace edopro_next::client {

std::optional<std::vector<Packet>> parse_packet_stream(std::span<const std::uint8_t> data,
													   std::string* error) {
	const auto fail = [error](std::string message) -> std::optional<std::vector<Packet>> {
		if(error != nullptr)
			*error = std::move(message);
		return std::nullopt;
	};

	std::vector<Packet> packets;
	std::size_t offset = 0;
	while(offset < data.size()) {
		constexpr std::size_t kHeader = sizeof(std::uint8_t) + sizeof(std::uint32_t);
		if(data.size() - offset < kHeader)
			return fail("stream ends mid-header at byte " + std::to_string(offset));

		PacketReader header(data.subspan(offset, kHeader));
		Packet packet;
		packet.message = header.u8();
		const auto length = header.u32();
		offset += kHeader;

		if(length > data.size() - offset)
			return fail("packet at byte " + std::to_string(offset - kHeader) + " declares " +
						std::to_string(length) + " bytes, only " +
						std::to_string(data.size() - offset) + " remain");

		const auto payload = data.subspan(offset, length);
		packet.payload.assign(payload.begin(), payload.end());
		offset += length;
		packets.push_back(std::move(packet));
	}
	return packets;
}

} // namespace edopro_next::client

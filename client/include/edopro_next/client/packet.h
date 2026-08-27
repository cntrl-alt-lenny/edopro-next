// A duel message as it arrives, and a bounds-checked way to read one.
//
// This mirrors CoreUtils::Packet in gframe/core_utils.h - an id plus an opaque
// payload - because that is genuinely the shape of the input, whether it came
// from a socket, from ocgcore directly, or from a recorded replay.
//
// PacketReader exists because gframe reads payloads with a bare pointer and no
// bounds checks at all (BufferIO::Read on a raw uint8_t*). That is safe there
// only because the engine is trusted. A decoder that must tell "malformed"
// apart from "unsupported" cannot afford to walk off the end of a buffer.
#ifndef EDOPRO_NEXT_CLIENT_PACKET_H
#define EDOPRO_NEXT_CLIENT_PACKET_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace edopro_next::client {

struct Packet {
	std::uint8_t message = 0;
	std::vector<std::uint8_t> payload;
};

// Little-endian reader over a payload. Every read is checked; once a read
// fails the reader stays failed, so a caller may read a whole message and
// test once at the end rather than after every field.
class PacketReader {
public:
	explicit PacketReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

	bool failed() const noexcept { return failed_; }
	std::size_t remaining() const noexcept { return failed_ ? 0 : data_.size() - offset_; }
	std::size_t consumed() const noexcept { return offset_; }
	bool exhausted() const noexcept { return !failed_ && offset_ == data_.size(); }

	std::uint8_t u8() noexcept { return read<std::uint8_t>(); }
	std::uint16_t u16() noexcept { return read<std::uint16_t>(); }
	std::uint32_t u32() noexcept { return read<std::uint32_t>(); }
	std::uint64_t u64() noexcept { return read<std::uint64_t>(); }
	std::int32_t i32() noexcept { return static_cast<std::int32_t>(read<std::uint32_t>()); }

	// Older protocol revisions narrow several fields to a single byte. Both
	// widths are read through here so the choice is visible at every call
	// site rather than hidden in a helper that consults global state, as
	// gframe's CompatRead does.
	std::uint32_t u8_or_u32(bool narrow) noexcept { return narrow ? u8() : u32(); }
	std::uint64_t u32_or_u64(bool narrow) noexcept { return narrow ? u32() : u64(); }

	// Marks the reader failed. Used when a payload parses but is refused for
	// a reason the reader itself cannot see.
	void fail() noexcept { failed_ = true; }

private:
	// Assembled byte by byte rather than memcpy'd, so the decoder produces the
	// same values on a big-endian host. The traces are golden files; they may
	// not depend on the machine that rendered them.
	template <typename T>
	T read() noexcept {
		if(failed_ || data_.size() - offset_ < sizeof(T)) {
			failed_ = true;
			return T{};
		}
		std::uint64_t value = 0;
		for(std::size_t i = 0; i < sizeof(T); ++i)
			value |= static_cast<std::uint64_t>(data_[offset_ + i]) << (8 * i);
		offset_ += sizeof(T);
		return static_cast<T>(value);
	}

	std::span<const std::uint8_t> data_;
	std::size_t offset_ = 0;
	bool failed_ = false;
};

// Splits a framed packet stream into packets. The framing is upstream's own,
// as stored in the body of a .yrpX replay and parsed by Replay::ParseStream:
//
//     uint8  message id
//     uint32 payload length, little-endian
//     bytes  payload
//
// Returns nothing and sets `error` when the framing does not hold. Trailing
// bytes too short to be a header are an error too: a stream that does not end
// on a packet boundary has been truncated.
std::optional<std::vector<Packet>> parse_packet_stream(std::span<const std::uint8_t> data,
													   std::string* error);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_PACKET_H

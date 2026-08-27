#include "edopro_next/client/query.h"
#include "edopro_next/client/protocol_constants.h"

#include <algorithm>

namespace edopro_next::client {
namespace {

class Reader {
public:
	explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
	std::size_t remaining() const noexcept { return failed_ ? 0 : bytes_.size() - at_; }
	bool failed() const noexcept { return failed_; }
	bool empty() const noexcept { return !failed_ && at_ == bytes_.size(); }
	void fail() noexcept { failed_ = true; }
	std::uint8_t u8() noexcept { return read<std::uint8_t>(); }
	std::uint16_t u16() noexcept { return read<std::uint16_t>(); }
	std::uint32_t u32() noexcept { return read<std::uint32_t>(); }
	std::uint64_t u64() noexcept { return read<std::uint64_t>(); }
	std::int32_t i32() noexcept { return static_cast<std::int32_t>(u32()); }

private:
	template <typename T> T read() noexcept {
		if(failed_ || bytes_.size() - at_ < sizeof(T)) {
			failed_ = true;
			return T{};
		}
		std::uint64_t value = 0;
		for(std::size_t i = 0; i < sizeof(T); ++i)
			value |= static_cast<std::uint64_t>(bytes_[at_ + i]) << (8 * i);
		at_ += sizeof(T);
		return static_cast<T>(value);
	}
	std::span<const std::uint8_t> bytes_;
	std::size_t at_ = 0;
	bool failed_ = false;
};

constexpr std::uint32_t kKnownQueryMask =
	protocol::QUERY_CODE | protocol::QUERY_POSITION | protocol::QUERY_ALIAS |
	protocol::QUERY_TYPE | protocol::QUERY_LEVEL | protocol::QUERY_RANK |
	protocol::QUERY_ATTRIBUTE | protocol::QUERY_RACE | protocol::QUERY_ATTACK |
	protocol::QUERY_DEFENSE | protocol::QUERY_BASE_ATTACK | protocol::QUERY_BASE_DEFENSE |
	protocol::QUERY_REASON | protocol::QUERY_REASON_CARD | protocol::QUERY_EQUIP_CARD |
	protocol::QUERY_TARGET_CARD | protocol::QUERY_OVERLAY_CARD | protocol::QUERY_COUNTERS |
	protocol::QUERY_OWNER | protocol::QUERY_STATUS | protocol::QUERY_IS_PUBLIC |
	protocol::QUERY_LSCALE | protocol::QUERY_RSCALE | protocol::QUERY_LINK |
	protocol::QUERY_IS_HIDDEN | protocol::QUERY_COVER | protocol::QUERY_END;

void note_fields(QueryCoverage& coverage, std::uint32_t flags) {
	for(std::uint32_t bit = 1; bit != 0; bit <<= 1)
		if((flags & bit) != 0)
			++coverage.fields[bit];
}

QueryLocation read_location(Reader& reader, bool compat) {
	QueryLocation result;
	const auto controller = reader.u8();
	const auto raw_zone = reader.u8();
	const auto sequence = compat ? reader.u8() : reader.u32();
	const auto position = compat ? reader.u8() : reader.u32();
	const auto zone = zone_from_protocol(raw_zone);
	result.location = CardLocation{controller, zone, sequence,
		(raw_zone & protocol::LOCATION_OVERLAY) != 0,
		(raw_zone & protocol::LOCATION_OVERLAY) != 0 ? position : 0};
	return result;
}

bool known_and_supported(std::uint32_t flags, QueryParseResult& result) {
	const auto unknown = flags & ~kKnownQueryMask;
	if(unknown == 0)
		return true;
	for(std::uint32_t bit = 1; bit != 0; bit <<= 1)
		if((unknown & bit) != 0)
			result.coverage.unknown_fields.push_back(bit);
	result.unsupported = true;
	result.detail = "query contains unknown field mask " + std::to_string(unknown);
	return false;
}

bool parse_flag_payload(Reader& reader, std::uint32_t flag, bool compat, bool legacy_race_size,
						CardQueryPatch& patch, QueryParseResult& result) {
	const auto scalar32 = [&reader] { return reader.u32(); };
	if(compat && (flag == protocol::QUERY_IS_PUBLIC || flag == protocol::QUERY_IS_HIDDEN ||
					  flag == protocol::QUERY_COVER)) {
		result.unsupported = true;
		result.detail = "compat query field " + std::to_string(flag) +
						" has no legacy wire representation";
		return false;
	}
	switch(flag) {
	case protocol::QUERY_CODE: patch.code = static_cast<CardCode>(scalar32()); break;
	case protocol::QUERY_POSITION: patch.position = CardPosition{static_cast<std::uint8_t>(compat ? scalar32() >> 24 : scalar32())}; break;
	case protocol::QUERY_ALIAS: patch.alias = scalar32(); break;
	case protocol::QUERY_TYPE: patch.type = scalar32(); break;
	case protocol::QUERY_LEVEL: patch.level = scalar32(); break;
	case protocol::QUERY_RANK: patch.rank = scalar32(); break;
	case protocol::QUERY_ATTRIBUTE: patch.attribute = scalar32(); break;
	case protocol::QUERY_RACE: patch.race = (compat || legacy_race_size) ? reader.u32() : reader.u64(); break;
	case protocol::QUERY_ATTACK: patch.attack = reader.i32(); break;
	case protocol::QUERY_DEFENSE: patch.defense = reader.i32(); break;
	case protocol::QUERY_BASE_ATTACK: patch.base_attack = reader.i32(); break;
	case protocol::QUERY_BASE_DEFENSE: patch.base_defense = reader.i32(); break;
	case protocol::QUERY_REASON: patch.reason = scalar32(); break;
	case protocol::QUERY_REASON_CARD: patch.reason_card = read_location(reader, compat); break;
	case protocol::QUERY_EQUIP_CARD: patch.equip_card = read_location(reader, compat); break;
	case protocol::QUERY_TARGET_CARD: {
		const auto count = scalar32();
		const auto width = compat ? std::size_t{4} : std::size_t{10};
		if(count > reader.remaining() / width) { reader.fail(); return false; }
		std::vector<QueryLocation> values;
		values.reserve(count);
		for(std::uint32_t i = 0; i < count; ++i)
			values.push_back(read_location(reader, compat));
		patch.target_cards = std::move(values);
		break;
	}
	case protocol::QUERY_OVERLAY_CARD: {
		const auto count = scalar32();
		if(count > reader.remaining() / sizeof(std::uint32_t)) { reader.fail(); return false; }
		std::vector<CardCode> values;
		values.reserve(count);
		for(std::uint32_t i = 0; i < count; ++i)
			values.push_back(static_cast<CardCode>(scalar32()));
		patch.overlay_cards = std::move(values);
		break;
	}
	case protocol::QUERY_COUNTERS: {
		const auto count = scalar32();
		if(count > reader.remaining() / sizeof(std::uint32_t)) { reader.fail(); return false; }
		std::vector<QueryCounter> values;
		values.reserve(count);
		for(std::uint32_t i = 0; i < count; ++i) {
			const auto packed = scalar32();
			values.push_back(QueryCounter{static_cast<std::uint16_t>(packed & 0xffffu),
				static_cast<std::uint16_t>(packed >> 16)});
		}
		patch.counters = std::move(values);
		break;
	}
	case protocol::QUERY_OWNER: patch.owner = scalar32(); break;
	case protocol::QUERY_STATUS: patch.status = scalar32(); break;
	case protocol::QUERY_IS_PUBLIC: patch.is_public = reader.u8() != 0; break;
	case protocol::QUERY_LSCALE: patch.lscale = scalar32(); break;
	case protocol::QUERY_RSCALE: patch.rscale = scalar32(); break;
	case protocol::QUERY_LINK: patch.link = scalar32(); patch.link_marker = scalar32(); break;
	case protocol::QUERY_IS_HIDDEN: patch.is_hidden = reader.u8() != 0; break;
	case protocol::QUERY_COVER: patch.cover = scalar32(); break;
	case protocol::QUERY_END: break;
	default: result.unsupported = true; result.detail = "unknown query field"; return false;
	}
	return !reader.failed();
}

} // namespace

QueryParseResult parse_query_stream(std::span<const std::uint8_t> data,
								 bool compat, bool legacy_race_size) {
	// The implementation is indexed so a declared field size can never escape
	// the enclosing stream. Keeping this separate from PacketReader also avoids
	// exposing raw spans as part of the general packet API.
	QueryParseResult result;
	if(compat) {
		std::size_t at = 0;
		while(at < data.size()) {
			if(data.size() - at < 4) { result.detail = "truncated compat query length"; return result; }
			const auto start = at;
			const auto size = static_cast<std::uint32_t>(data[at]) |
				(static_cast<std::uint32_t>(data[at + 1]) << 8) |
				(static_cast<std::uint32_t>(data[at + 2]) << 16) |
				(static_cast<std::uint32_t>(data[at + 3]) << 24);
			if(size < 4 || size > data.size() - at) { result.detail = "invalid compat query record length"; return result; }
			++result.coverage.entries;
			if(size <= 8) { ++result.coverage.skipped; result.entries.push_back(QueryEntry{true, {}}); at += size; continue; }
			Reader reader(data.subspan(at + 4, size - 4));
			const auto flags = reader.u32();
			note_fields(result.coverage, flags);
			if(!known_and_supported(flags, result) || (flags & (protocol::QUERY_IS_PUBLIC | protocol::QUERY_IS_HIDDEN | protocol::QUERY_COVER)) != 0) return result;
			QueryEntry entry;
			entry.patch.flags = flags;
			for(std::uint32_t bit = 1; bit != 0; bit <<= 1) {
				if((flags & bit) == 0) continue;
				if(!parse_flag_payload(reader, bit, true, legacy_race_size, entry.patch, result)) { result.detail = result.detail.empty() ? "malformed compat query field" : result.detail; return result; }
			}
			if(!reader.empty()) { result.detail = "compat query record has trailing bytes"; return result; }
			result.entries.push_back(std::move(entry));
			at = start + size;
		}
		result.valid = true;
		return result;
	}

	if(data.size() < 4) { result.detail = "truncated modern query stream length"; return result; }
	const auto stream_size = static_cast<std::uint32_t>(data[0]) |
		(static_cast<std::uint32_t>(data[1]) << 8) |
		(static_cast<std::uint32_t>(data[2]) << 16) |
		(static_cast<std::uint32_t>(data[3]) << 24);
	if(stream_size != data.size() - 4) { result.detail = "modern query stream length does not match payload"; return result; }
	std::size_t at = 4;
	bool open = false;
	while(at < data.size()) {
		if(data.size() - at < 2) { result.detail = "truncated modern query field length"; return result; }
		const auto size = static_cast<std::uint16_t>(data[at]) |
			(static_cast<std::uint16_t>(data[at + 1]) << 8);
		at += 2;
		if(size == 0) {
			if(open) { result.detail = "skipped query field before QUERY_END"; return result; }
			++result.coverage.entries;
			++result.coverage.skipped; result.entries.push_back(QueryEntry{true, {}}); continue;
		}
		if(size < 4 || static_cast<std::size_t>(size) > data.size() - at) { result.detail = "invalid modern query field length"; return result; }
		Reader field(data.subspan(at, size));
		const auto flags = field.u32();
		note_fields(result.coverage, flags);
		if(!known_and_supported(flags, result)) {
			// Modern field framing makes an unknown future field safely skippable.
			// It still refuses the packet for application, because silently
			// applying only part of a query would make equivalence dishonest.
			if(!open) {
				++result.coverage.entries;
				result.entries.push_back(QueryEntry{false, {}});
				open = true;
			}
			at += size;
			continue;
		}
		if(flags == protocol::QUERY_END) {
			if(!field.empty()) { result.detail = "QUERY_END has a payload"; return result; }
			if(!open) { result.detail = "QUERY_END without a query entry"; return result; }
			open = false;
			at += size;
			continue;
		}
		// Start/continue the current non-skipped query. A zero-sized record is
		// the only representation of a skipped slot.
		if(!open) {
			++result.coverage.entries;
			result.entries.push_back(QueryEntry{false, {}});
			open = true;
		}
		result.entries.back().patch.flags |= flags;
		if(!parse_flag_payload(field, flags, false, legacy_race_size, result.entries.back().patch, result) || !field.empty()) {
			result.detail = field.failed() ? "modern query field payload is truncated" : "modern query field has trailing bytes";
			return result;
		}
		at += size;
	}
	// A non-empty modern record must have ended with QUERY_END. Since the
	// generated stream always emits it, accepting an unterminated patch would
	// turn truncation into a valid state update.
	if(open) {
		result.detail = "modern query entry has no QUERY_END";
		return result;
	}
	if(result.unsupported)
		return result;
	result.valid = true;
	return result;
}

QueryParseResult parse_query_record(std::span<const std::uint8_t> data,
								 bool compat, bool legacy_race_size) {
	if(!compat) {
		if(data.empty()) {
			QueryParseResult result;
			result.detail = "empty modern query record";
			return result;
		}
		std::vector<std::uint8_t> framed(4 + data.size());
		const auto size = static_cast<std::uint32_t>(data.size());
		for(unsigned i = 0; i < 4; ++i)
			framed[i] = static_cast<std::uint8_t>((size >> (8 * i)) & 0xffu);
		std::copy(data.begin(), data.end(), framed.begin() + 4);
		return parse_query_stream(framed, false, legacy_race_size);
	}
	const auto result = parse_query_stream(data, true, legacy_race_size);
	if(result.valid && result.entries.size() != 1) {
		QueryParseResult invalid = result;
		invalid.valid = false;
		invalid.detail = "compat MSG_UPDATE_CARD contains multiple query records";
		return invalid;
	}
	return result;
}

} // namespace edopro_next::client

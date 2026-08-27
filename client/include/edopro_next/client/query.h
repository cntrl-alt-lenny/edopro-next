#ifndef EDOPRO_NEXT_CLIENT_QUERY_H
#define EDOPRO_NEXT_CLIENT_QUERY_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "edopro_next/client/card_identity.h"
#include "edopro_next/client/card_location.h"
#include "edopro_next/client/card_state.h"

namespace edopro_next::client {

// A location carried inside QUERY_REASON_CARD, QUERY_EQUIP_CARD, or
// QUERY_TARGET_CARD. It is kept as a semantic location, never as a pointer
// or a screen-relative index. Overlay position is interpreted by the parser.
struct QueryLocation {
	CardLocation location{};
	friend bool operator==(const QueryLocation&, const QueryLocation&) = default;
};

struct QueryCounter {
	std::uint16_t type = 0;
	std::uint16_t count = 0;
	friend bool operator==(const QueryCounter&, const QueryCounter&) = default;
};

// Optional values are intentional: a query is a patch, and an omitted field
// preserves the previous value. This also distinguishes an explicit zero
// (including code concealment) from a value the stream never stated.
struct CardQueryPatch {
	std::uint32_t flags = 0;
	std::optional<CardCode> code;
	std::optional<CardPosition> position;
	std::optional<std::uint32_t> alias;
	std::optional<std::uint32_t> type;
	std::optional<std::uint32_t> level;
	std::optional<std::uint32_t> rank;
	std::optional<std::uint32_t> attribute;
	std::optional<std::uint64_t> race;
	std::optional<std::int32_t> attack;
	std::optional<std::int32_t> defense;
	std::optional<std::int32_t> base_attack;
	std::optional<std::int32_t> base_defense;
	std::optional<std::uint32_t> reason;
	std::optional<QueryLocation> reason_card;
	std::optional<QueryLocation> equip_card;
	std::optional<std::vector<QueryLocation>> target_cards;
	std::optional<std::vector<CardCode>> overlay_cards;
	std::optional<std::vector<QueryCounter>> counters;
	std::optional<std::uint32_t> owner;
	std::optional<std::uint32_t> status;
	std::optional<bool> is_public;
	std::optional<std::uint32_t> lscale;
	std::optional<std::uint32_t> rscale;
	std::optional<std::uint32_t> link;
	std::optional<std::uint32_t> link_marker;
	std::optional<bool> is_hidden;
	std::optional<std::uint32_t> cover;

	friend bool operator==(const CardQueryPatch&, const CardQueryPatch&) = default;
};

struct QueryEntry {
	bool skipped = false;
	CardQueryPatch patch;
	friend bool operator==(const QueryEntry&, const QueryEntry&) = default;
};

struct QueryCoverage {
	std::size_t entries = 0;
	std::size_t skipped = 0;
	// Counts are keyed by the canonical QUERY_* bit, in ascending order when
	// rendered. QUERY_END is counted too, making framing coverage explicit.
	std::map<std::uint32_t, std::size_t> fields;
	std::vector<std::uint32_t> unknown_fields;
};

struct QueryParseResult {
	bool valid = false;
	bool unsupported = false;
	std::string detail;
	std::vector<QueryEntry> entries;
	QueryCoverage coverage;
};

// Parse the bytes following MSG_UPDATE_DATA's player/location header. Modern
// streams begin with a byte count excluding that count; compat streams are a
// sequence of records whose individual length includes its four-byte length.
QueryParseResult parse_query_stream(std::span<const std::uint8_t> data,
								 bool compat, bool legacy_race_size = false);

// Parse the single query carried by MSG_UPDATE_CARD. Modern messages contain
// the field records directly; compatibility messages contain one length-
// framed record. This is deliberately separate from the field-list stream
// used by MSG_UPDATE_DATA.
QueryParseResult parse_query_record(std::span<const std::uint8_t> data,
								 bool compat, bool legacy_race_size = false);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_QUERY_H

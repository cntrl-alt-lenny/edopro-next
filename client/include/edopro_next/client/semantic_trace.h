// Deterministic, diffable rendering of a decoded packet stream.
//
// This is the M2 counterpart to the structural trace in tools/replaytrace: the
// same fixtures, read one layer higher. Where the structural trace says
// "message 50, 28 bytes, sha256:...", this says "CardMoved instance=7
// HAND[p0:2] -> MZONE[p0:1]".
//
// It carries the same contract as the structural trace, for the same reason -
// it is a golden file:
//
//   * byte-for-byte reproducible on any machine, in any locale;
//   * nothing environmental: no pointers, no paths, no wall-clock;
//   * legible diffs that name what changed.
//
// It also carries an honest coverage report. A stream this build decodes only
// a quarter of must say so in numbers, not hide behind a single percentage.
#ifndef EDOPRO_NEXT_CLIENT_SEMANTIC_TRACE_H
#define EDOPRO_NEXT_CLIENT_SEMANTIC_TRACE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "edopro_next/client/packet.h"
#include "edopro_next/client/protocol_decoder.h"

namespace edopro_next::client {

inline constexpr int kSemanticTraceVersion = 1;

struct TraceOptions {
	// The fixture's logical name, never a path: a trace may not embed the
	// machine that produced it.
	std::string source_name = "unnamed";
	ProtocolVariant variant{};
};

struct Coverage {
	std::size_t packets = 0;
	std::size_t decoded = 0;
	std::size_t unsupported = 0;
	std::size_t unknown = 0;
	std::size_t malformed = 0;
	std::size_t inconsistent = 0;
	// Per message id, how many packets landed in each status.
	std::map<std::uint8_t, std::map<DecodeStatus, std::size_t>> by_message;
};

struct TraceResult {
	std::string text;
	Coverage coverage;
	// Left over from the last packet, so callers can inspect the state a
	// stream produced without re-running it.
	DuelState state;
};

TraceResult render_semantic_trace(const std::vector<Packet>& packets, const TraceOptions& options);

} // namespace edopro_next::client

#endif // EDOPRO_NEXT_CLIENT_SEMANTIC_TRACE_H

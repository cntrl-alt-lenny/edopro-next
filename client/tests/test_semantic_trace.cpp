// The trace renderer and the packet framing it is fed.
//
// The trace is a golden file, so the properties worth pinning are that it is
// reproducible, that it embeds nothing environmental, and that its coverage
// numbers account for every packet rather than rounding to a flattering
// percentage.
#include "packet_builder.h"
#include "test_support.h"

#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/semantic_trace.h"

#include <string>
#include <vector>

using namespace edopro_next::client;
using edopro_next::testing::PayloadBuilder;
namespace proto = edopro_next::client::protocol;

namespace {

std::vector<Packet> sample_stream() {
	std::vector<Packet> packets;
	packets.push_back(PayloadBuilder()
						  .u8(0)
						  .u32(8000)
						  .u32(8000)
						  .u16(3)
						  .u16(0)
						  .u16(3)
						  .u16(0)
						  .packet(proto::MSG_START));
	packets.push_back(Packet{proto::MSG_UPDATE_DATA, {0, 1, 2}});
	packets.push_back(PayloadBuilder().u8(0).packet(proto::MSG_NEW_TURN));
	packets.push_back(PayloadBuilder().u16(proto::PHASE_DRAW).packet(proto::MSG_NEW_PHASE));
	packets.push_back(PayloadBuilder()
						  .u8(0)
						  .u32(1)
						  .u32(1234)
						  .u32(proto::POS_FACEDOWN_DEFENSE)
						  .packet(proto::MSG_DRAW));
	packets.push_back(PayloadBuilder().u8(1).u32(1500).packet(proto::MSG_DAMAGE));
	// An id upstream does not define at all.
	packets.push_back(Packet{9, {}});
	// A supported message with a payload that cannot satisfy its layout.
	packets.push_back(Packet{proto::MSG_NEW_TURN, {}});
	packets.push_back(PayloadBuilder().u8(0).u8(1).packet(proto::MSG_WIN));
	return packets;
}

bool contains(const std::string& haystack, const std::string& needle) {
	return haystack.find(needle) != std::string::npos;
}

std::size_t count_lines(const std::string& text) {
	std::size_t lines = 0;
	for(const char c : text)
		lines += (c == '\n') ? 1 : 0;
	return lines;
}

} // namespace

EDOPRO_TEST(packet_framing_round_trips) {
	std::vector<std::uint8_t> stream;
	const auto append = [&stream](std::uint8_t message,
								  const std::vector<std::uint8_t>& payload) {
		stream.push_back(message);
		for(int i = 0; i < 4; ++i)
			stream.push_back(static_cast<std::uint8_t>((payload.size() >> (8 * i)) & 0xffu));
		stream.insert(stream.end(), payload.begin(), payload.end());
	};
	append(proto::MSG_NEW_TURN, {0});
	append(proto::MSG_CHAIN_END, {});
	append(proto::MSG_DAMAGE, {1, 2, 3, 4, 5});

	std::string error;
	const auto packets = parse_packet_stream(stream, &error);
	EDOPRO_CHECK(packets.has_value());
	if(!packets)
		return;
	EDOPRO_CHECK_EQ(packets->size(), std::size_t{3});
	EDOPRO_CHECK_EQ((*packets)[1].message, proto::MSG_CHAIN_END);
	EDOPRO_CHECK((*packets)[1].payload.empty());
	EDOPRO_CHECK_EQ((*packets)[2].payload.size(), std::size_t{5});
}

EDOPRO_TEST(truncated_framing_is_rejected_not_guessed) {
	std::string error;
	// A header claiming more payload than exists.
	const std::vector<std::uint8_t> overlong{proto::MSG_DAMAGE, 0x10, 0, 0, 0, 1, 2};
	EDOPRO_CHECK(!parse_packet_stream(overlong, &error).has_value());
	EDOPRO_CHECK(!error.empty());

	// A trailing fragment too short to be a header.
	const std::vector<std::uint8_t> stub{proto::MSG_DAMAGE, 0, 0};
	EDOPRO_CHECK(!parse_packet_stream(stub, &error).has_value());

	// Empty is a valid stream of no packets, not an error.
	const auto empty = parse_packet_stream({}, &error);
	EDOPRO_CHECK(empty.has_value());
	if(empty)
		EDOPRO_CHECK(empty->empty());
}

EDOPRO_TEST(rendering_is_deterministic) {
	const auto packets = sample_stream();
	TraceOptions options;
	options.source_name = "sample";
	const auto first = render_semantic_trace(packets, options);
	const auto second = render_semantic_trace(packets, options);
	EDOPRO_CHECK_EQ(first.text, second.text);
	EDOPRO_CHECK(!first.text.empty());
	EDOPRO_CHECK_EQ(first.text.back(), '\n');
}

EDOPRO_TEST(coverage_accounts_for_every_packet) {
	const auto packets = sample_stream();
	TraceOptions options;
	options.source_name = "sample";
	const auto trace = render_semantic_trace(packets, options);
	const auto& coverage = trace.coverage;

	EDOPRO_CHECK_EQ(coverage.packets, packets.size());
	EDOPRO_CHECK_EQ(coverage.decoded + coverage.unsupported + coverage.unknown +
						coverage.malformed + coverage.inconsistent,
					packets.size());

	// The three refusals are counted apart, and the trace says so in words.
	EDOPRO_CHECK_EQ(coverage.unsupported, std::size_t{1});
	EDOPRO_CHECK_EQ(coverage.unknown, std::size_t{1});
	EDOPRO_CHECK_EQ(coverage.malformed, std::size_t{1});
	EDOPRO_CHECK_EQ(coverage.inconsistent, std::size_t{0});
	EDOPRO_CHECK(contains(trace.text, "MSG_UPDATE_DATA"));
	EDOPRO_CHECK(contains(trace.text, "UNKNOWN_9"));
	EDOPRO_CHECK(contains(trace.text, "malformed"));
}

EDOPRO_TEST(the_trace_reports_state_and_invariants) {
	const auto packets = sample_stream();
	TraceOptions options;
	options.source_name = "sample";
	const auto trace = render_semantic_trace(packets, options);

	EDOPRO_CHECK(contains(trace.text, "source: sample"));
	EDOPRO_CHECK(contains(trace.text, "protocol: modern"));
	EDOPRO_CHECK(contains(trace.text, "invariants: ok"));
	EDOPRO_CHECK(contains(trace.text, "lp: p0=8000 p1=6500"));
	EDOPRO_CHECK(contains(trace.text, "TurnStarted player=p0 turn=1"));
	EDOPRO_CHECK(contains(trace.text, "PhaseChanged DRAW"));
	EDOPRO_CHECK(contains(trace.text, "finished: winner=p0 reason=1"));
	EDOPRO_CHECK(trace.state.check_invariants().empty());
	EDOPRO_CHECK(count_lines(trace.text) > 20);
}

EDOPRO_TEST(the_trace_embeds_nothing_environmental) {
	// The same properties the structural M1 trace promises: no paths, no
	// addresses, nothing that differs between two machines.
	const auto packets = sample_stream();
	TraceOptions options;
	options.source_name = "sample";
	const auto trace = render_semantic_trace(packets, options);
	for(const auto* banned : {"/home/", "C:\\", "/mnt/", "0x7f"})
		EDOPRO_CHECK(!contains(trace.text, banned));
}

EDOPRO_TEST(a_compat_stream_is_labelled_as_one) {
	TraceOptions options;
	options.source_name = "sample";
	options.variant.compat = true;
	const auto trace = render_semantic_trace({}, options);
	EDOPRO_CHECK(contains(trace.text, "protocol: compat"));
	EDOPRO_CHECK(contains(trace.text, "packets: 0"));
	EDOPRO_CHECK(contains(trace.text, "started: no"));
}

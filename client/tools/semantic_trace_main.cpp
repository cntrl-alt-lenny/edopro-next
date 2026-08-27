// CLI: render a semantic trace for a framed duel-message stream.
//
//     semantic_trace <stream.pkts> [--name NAME] [--compat] [-o OUT]
//
// The input is a packet stream, not a replay file. Reading .yrpX containers -
// header, LZMA body, player names, embedded YRP1 - is M1's job and lives in
// tools/replaytrace; duplicating it here would mean maintaining two container
// parsers to no benefit. More importantly, a stream of packets is what the
// real client will hand this decoder, whether it came from a socket or from
// ocgcore, so that is the interface worth testing.
//
// tests/test_semantic_trace.py extracts the stream from a fixture with the
// existing reader and pipes it here.
#include "edopro_next/client/semantic_trace.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int usage() {
	std::cerr << "usage: semantic_trace <stream.pkts> [--name NAME] [--compat] [-o OUT]\n";
	return 2;
}

} // namespace

int main(int argc, char** argv) {
	std::string input;
	std::string output;
	edopro_next::client::TraceOptions options;
	bool name_given = false;

	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--compat") {
			options.variant.compat = true;
		} else if(arg == "--name" && i + 1 < argc) {
			options.source_name = argv[++i];
			name_given = true;
		} else if((arg == "-o" || arg == "--output") && i + 1 < argc) {
			output = argv[++i];
		} else if(!arg.empty() && arg[0] == '-') {
			return usage();
		} else if(input.empty()) {
			input = arg;
		} else {
			return usage();
		}
	}
	if(input.empty())
		return usage();

	std::ifstream in(input, std::ios::binary);
	if(!in) {
		std::cerr << "error: cannot open " << input << "\n";
		return 1;
	}
	const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
										 std::istreambuf_iterator<char>());

	std::string error;
	const auto packets = edopro_next::client::parse_packet_stream(data, &error);
	if(!packets) {
		std::cerr << "error: " << input << ": " << error << "\n";
		return 1;
	}

	if(!name_given) {
		// Fall back to the file's stem so the trace still names something, but
		// never a directory: a golden file may not record where it was built.
		auto stem = input;
		if(const auto slash = stem.find_last_of("/\\"); slash != std::string::npos)
			stem = stem.substr(slash + 1);
		if(const auto dot = stem.find_last_of('.'); dot != std::string::npos && dot != 0)
			stem = stem.substr(0, dot);
		options.source_name = stem;
	}

	const auto trace = edopro_next::client::render_semantic_trace(*packets, options);

	if(output.empty()) {
		std::cout << trace.text;
	} else {
		std::ofstream out(output, std::ios::binary);
		if(!out) {
			std::cerr << "error: cannot write " << output << "\n";
			return 1;
		}
		out << trace.text;
	}
	return 0;
}

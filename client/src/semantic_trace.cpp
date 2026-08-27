#include "edopro_next/client/semantic_trace.h"

#include "edopro_next/client/protocol_constants.h"

#include <algorithm>
#include <array>

namespace edopro_next::client {
namespace {

std::string pad_right(std::string_view text, std::size_t width) {
	std::string out(text);
	while(out.size() < width)
		out.push_back(' ');
	return out;
}

std::string pad_left(const std::string& text, std::size_t width) {
	std::string out;
	while(out.size() + text.size() < width)
		out.push_back(' ');
	out += text;
	return out;
}

std::string message_label(std::uint8_t id) {
	const auto name = protocol::message_name(id);
	if(name.empty())
		return "UNKNOWN_" + std::to_string(static_cast<unsigned>(id));
	return std::string(name);
}

std::string query_field_label(std::uint32_t id) {
	using namespace protocol;
	switch(id) {
	case QUERY_CODE: return "QUERY_CODE";
	case QUERY_POSITION: return "QUERY_POSITION";
	case QUERY_ALIAS: return "QUERY_ALIAS";
	case QUERY_TYPE: return "QUERY_TYPE";
	case QUERY_LEVEL: return "QUERY_LEVEL";
	case QUERY_RANK: return "QUERY_RANK";
	case QUERY_ATTRIBUTE: return "QUERY_ATTRIBUTE";
	case QUERY_RACE: return "QUERY_RACE";
	case QUERY_ATTACK: return "QUERY_ATTACK";
	case QUERY_DEFENSE: return "QUERY_DEFENSE";
	case QUERY_BASE_ATTACK: return "QUERY_BASE_ATTACK";
	case QUERY_BASE_DEFENSE: return "QUERY_BASE_DEFENSE";
	case QUERY_REASON: return "QUERY_REASON";
	case QUERY_REASON_CARD: return "QUERY_REASON_CARD";
	case QUERY_EQUIP_CARD: return "QUERY_EQUIP_CARD";
	case QUERY_TARGET_CARD: return "QUERY_TARGET_CARD";
	case QUERY_OVERLAY_CARD: return "QUERY_OVERLAY_CARD";
	case QUERY_COUNTERS: return "QUERY_COUNTERS";
	case QUERY_OWNER: return "QUERY_OWNER";
	case QUERY_STATUS: return "QUERY_STATUS";
	case QUERY_IS_PUBLIC: return "QUERY_IS_PUBLIC";
	case QUERY_LSCALE: return "QUERY_LSCALE";
	case QUERY_RSCALE: return "QUERY_RSCALE";
	case QUERY_LINK: return "QUERY_LINK";
	case QUERY_IS_HIDDEN: return "QUERY_IS_HIDDEN";
	case QUERY_COVER: return "QUERY_COVER";
	case QUERY_END: return "QUERY_END";
	default: return "QUERY_0x" + std::to_string(id);
	}
}

// One "<id> <name> <count>" line per message type, ascending by id.
void write_message_table(std::vector<std::string>& out,
						 const std::map<std::uint8_t, std::size_t>& counts) {
	if(counts.empty()) {
		out.emplace_back("(none)");
		return;
	}
	for(const auto& [id, count] : counts) {
		out.push_back(pad_left(std::to_string(static_cast<unsigned>(id)), 3) + " " +
					  pad_right(message_label(id), 24) + " " + std::to_string(count));
	}
}

std::map<std::uint8_t, std::size_t> counts_for(const Coverage& coverage, DecodeStatus status) {
	std::map<std::uint8_t, std::size_t> out;
	for(const auto& [id, statuses] : coverage.by_message) {
		if(const auto it = statuses.find(status); it != statuses.end())
			out[id] = it->second;
	}
	return out;
}

void write_zones(std::vector<std::string>& out, const DuelState& state) {
	static constexpr std::array<Zone, 7> kOrder = {
		Zone::Deck,	  Zone::Hand,	  Zone::MonsterZone, Zone::SpellZone,
		Zone::Graveyard, Zone::Banished, Zone::ExtraDeck,
	};

	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		for(const auto zone : kOrder) {
			const auto& contents = state.zone(player, zone);
			std::size_t occupied = 0;
			for(const auto id : contents)
				occupied += (id != CardInstanceId::None) ? 1 : 0;
			out.push_back(player_name(player) + " " + std::string(zone_name(zone)) + " " +
						  std::to_string(occupied));
			for(std::size_t i = 0; i < contents.size(); ++i) {
				const auto id = contents[i];
				if(id == CardInstanceId::None)
					continue;
				const auto* card = state.find(id);
				std::string line = "  [" + std::to_string(i) + "] instance=" + to_string(id) +
								   " code=" + to_string(card->code) +
								   " pos=" + to_string(card->position);
				if(card->attack)
					line += " atk=" + std::to_string(*card->attack);
				if(card->defense)
					line += " def=" + std::to_string(*card->defense);
				out.push_back(std::move(line));
				for(std::size_t m = 0; m < card->materials.size(); ++m) {
					const auto* material = state.find(card->materials[m]);
					out.push_back("    #" + std::to_string(m) +
								  " instance=" + to_string(card->materials[m]) +
								  " code=" + to_string(material->code));
				}
			}
		}
	}
}

} // namespace

TraceResult render_semantic_trace(const std::vector<Packet>& packets,
								  const TraceOptions& options) {
	TraceResult result;
	ProtocolDecoder decoder(options.variant);

	std::vector<std::string> events;
	std::vector<std::string> problems;
	result.coverage.packets = packets.size();

	for(std::size_t index = 0; index < packets.size(); ++index) {
		const auto outcome = decoder.decode(packets[index], result.state);
		const bool is_query = packets[index].message == protocol::MSG_UPDATE_DATA ||
			packets[index].message == protocol::MSG_UPDATE_CARD;
		if(is_query) {
			++result.coverage.query_packets;
			switch(outcome.status) {
			case DecodeStatus::Decoded: ++result.coverage.query_decoded; break;
			case DecodeStatus::UnsupportedMessage: ++result.coverage.query_unsupported; break;
			case DecodeStatus::Malformed: ++result.coverage.query_malformed; break;
			case DecodeStatus::Inconsistent: ++result.coverage.query_inconsistent; break;
			default: break;
			}
		}
		if(outcome.query_coverage) {
			result.coverage.query_entries += outcome.query_coverage->entries;
			result.coverage.query_skipped += outcome.query_coverage->skipped;
			for(const auto& [field, count] : outcome.query_coverage->fields)
				result.coverage.query_fields[field] += count;
			result.coverage.unknown_query_fields.insert(result.coverage.unknown_query_fields.end(),
				outcome.query_coverage->unknown_fields.begin(), outcome.query_coverage->unknown_fields.end());
		}
		++result.coverage.by_message[packets[index].message][outcome.status];
		switch(outcome.status) {
		case DecodeStatus::Decoded:
			++result.coverage.decoded;
			break;
		case DecodeStatus::UnsupportedMessage:
			++result.coverage.unsupported;
			break;
		case DecodeStatus::UnknownMessage:
			++result.coverage.unknown;
			break;
		case DecodeStatus::Malformed:
			++result.coverage.malformed;
			break;
		case DecodeStatus::Inconsistent:
			++result.coverage.inconsistent;
			break;
		}

		// Unsupported messages are expected and are counted, not listed one by
		// one. The other two refusals are defects and every one is named.
		if(outcome.status == DecodeStatus::Malformed ||
		   outcome.status == DecodeStatus::Inconsistent) {
			problems.push_back(pad_left(std::to_string(index), 5) + " " +
							   std::string(decode_status_name(outcome.status)) + " " +
							   outcome.detail);
		}
		for(const auto& event : outcome.events)
			events.push_back(pad_left(std::to_string(index), 5) + " " + to_string(event));
	}

	std::vector<std::string> out;
	auto add = [&out](std::string line) { out.push_back(std::move(line)); };

	add("# edopro-next semantic trace v" + std::to_string(kSemanticTraceVersion));
	add("source: " + options.source_name);
	add(std::string("protocol: ") + (options.variant.compat ? "compat" : "modern"));
	add("packets: " + std::to_string(result.coverage.packets));
	add("");

	add("## coverage");
	add("decoded: " + std::to_string(result.coverage.decoded));
	add("unsupported: " + std::to_string(result.coverage.unsupported));
	add("unknown: " + std::to_string(result.coverage.unknown));
	add("malformed: " + std::to_string(result.coverage.malformed));
	add("inconsistent: " + std::to_string(result.coverage.inconsistent));
	add("query_packets: " + std::to_string(result.coverage.query_packets));
	add("query_decoded: " + std::to_string(result.coverage.query_decoded));
	add("query_unsupported: " + std::to_string(result.coverage.query_unsupported));
	add("query_malformed: " + std::to_string(result.coverage.query_malformed));
	add("query_inconsistent: " + std::to_string(result.coverage.query_inconsistent));
	add("query_entries: " + std::to_string(result.coverage.query_entries));
	add("query_skipped: " + std::to_string(result.coverage.query_skipped));
	add("query_fields:");
	for(const auto& [field, count] : result.coverage.query_fields)
		add("  " + query_field_label(field) + " " + std::to_string(count));
	add("unknown_query_fields: " + std::to_string(result.coverage.unknown_query_fields.size()));
	add("");
	add("### decoded message types");
	write_message_table(out, counts_for(result.coverage, DecodeStatus::Decoded));
	add("");
	add("### known but not implemented");
	write_message_table(out, counts_for(result.coverage, DecodeStatus::UnsupportedMessage));
	add("");
	add("### unknown message ids");
	write_message_table(out, counts_for(result.coverage, DecodeStatus::UnknownMessage));
	add("");
	add("### refused packets");
	if(problems.empty())
		add("(none)");
	else
		out.insert(out.end(), problems.begin(), problems.end());
	add("");

	add("## semantic events");
	add("count: " + std::to_string(events.size()));
	out.insert(out.end(), events.begin(), events.end());
	add("");

	add("## final state");
	add(std::string("started: ") + (result.state.started() ? "yes" : "no"));
	add("player_type: " + std::to_string(static_cast<unsigned>(result.state.player_type())));
	add("turn: " + std::to_string(result.state.turn()) +
		" player=" + player_name(result.state.turn_player()));
	add("phase: " + std::string(phase_name(result.state.phase())));
	add("lp: " + player_name(0) + "=" + std::to_string(result.state.life_points(0)) + " " +
		player_name(1) + "=" + std::to_string(result.state.life_points(1)));
	add("chain: " + std::to_string(result.state.chain().size()));
	if(result.state.finished()) {
		const auto winner = result.state.winner();
		add("finished: winner=" + (winner ? player_name(*winner) : std::string("none")) +
			" reason=" + std::to_string(static_cast<unsigned>(result.state.win_reason())));
	} else {
		add("finished: no");
	}

	std::size_t untracked = 0;
	for(const auto& card : result.state.cards())
		untracked += card.tracked ? 0 : 1;
	add("instances: " + std::to_string(result.state.cards().size()) +
		" untracked=" + std::to_string(untracked));

	const auto violations = result.state.check_invariants();
	add("invariants: " + (violations.empty() ? std::string("ok")
											 : std::to_string(violations.size()) + " violated"));
	for(const auto& violation : violations)
		add("  " + violation);
	add("");

	add("### zones");
	write_zones(out, result.state);

	std::string text;
	for(const auto& line : out) {
		text += line;
		text += '\n';
	}
	result.text = std::move(text);
	return result;
}

} // namespace edopro_next::client

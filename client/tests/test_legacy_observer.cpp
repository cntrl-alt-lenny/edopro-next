#include "test_support.h"

#include "observer_model.h"

using namespace edopro_next::client;
using namespace edopro_next::legacy_observer;
namespace proto = edopro_next::client::protocol;

namespace {

Packet start_packet(std::uint16_t main = 2, std::uint16_t extra = 1) {
	return Packet{proto::MSG_START,
		{0, 0x40, 0x1f, 0, 0, 0x40, 0x1f, 0, 0,
		  static_cast<std::uint8_t>(main), 0, static_cast<std::uint8_t>(extra), 0,
		  static_cast<std::uint8_t>(main), 0, static_cast<std::uint8_t>(extra), 0}};
}

LegacySnapshot snapshot_for_start(std::uint16_t main = 2, std::uint16_t extra = 1) {
	LegacySnapshot snapshot;
	snapshot.life = {8000, 8000};
	for(PlayerId player = 0; player < kPlayerCount; ++player) {
		for(std::uint32_t sequence = 0; sequence < main; ++sequence)
			snapshot.cards.push_back({{player, Zone::Deck, sequence, false, 0},
				CardCode::None, CardPosition{proto::POS_FACEDOWN_DEFENSE}, {}});
		for(std::uint32_t sequence = 0; sequence < extra; ++sequence)
			snapshot.cards.push_back({{player, Zone::ExtraDeck, sequence, false, 0},
				CardCode::None, CardPosition{proto::POS_FACEDOWN_DEFENSE}, {}});
	}
	return snapshot;
}

} // namespace

EDOPRO_TEST(perspective_normalization_handles_both_directions) {
	EDOPRO_CHECK_EQ(protocol_player_from_local(0, true), 0);
	EDOPRO_CHECK_EQ(protocol_player_from_local(1, true), 1);
	EDOPRO_CHECK_EQ(protocol_player_from_local(0, false), 1);
	EDOPRO_CHECK_EQ(protocol_player_from_local(1, false), 0);
}

EDOPRO_TEST(empty_and_occupied_slots_compare_structurally) {
	DuelState semantic;
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(1, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	semantic = session.state();
	auto legacy = snapshot_for_start(1, 0);
	legacy.cards.push_back({{0, Zone::MonsterZone, 3, false, 0}, CardCode{123},
									CardPosition{proto::POS_FACEUP_ATTACK}, {}});
	const auto mismatch = compare(semantic, legacy, 12, proto::MSG_MOVE);
	EDOPRO_CHECK(!mismatch.equivalent());
	EDOPRO_CHECK_EQ(mismatch.mismatches.front().field, "MZONE[p0:3].occupancy");
}

EDOPRO_TEST(dense_pile_order_and_extra_face_up_split_are_compared) {
	DuelState semantic;
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(2, 2), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	semantic = session.state();
	EDOPRO_CHECK(!semantic.create_card({0, Zone::ExtraDeck, 0, false, 0}, CardCode{73915052},
									 CardPosition{proto::POS_FACEUP_ATTACK}, nullptr));
	auto legacy = snapshot_for_start(2, 2);
	legacy.cards.push_back({{0, Zone::ExtraDeck, 2, false, 0}, CardCode{73915052},
									CardPosition{proto::POS_FACEUP_ATTACK}, {}});
	const auto mismatch = compare(semantic, legacy, 27, proto::MSG_MOVE);
	EDOPRO_CHECK(mismatch.equivalent());
}

EDOPRO_TEST(unknown_semantic_code_is_query_scope_not_a_false_mismatch) {
	DuelState semantic;
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	semantic = session.state();
	LegacySnapshot legacy{{8000, 8000}, 0,
		{{{0, Zone::MonsterZone, 0, false, 0}, CardCode{123},
			CardPosition{proto::POS_FACEUP_ATTACK}, {}}}};
	// A query packet may teach legacy a code that this slice has not decoded.
	// It must not be reported until the semantic side knows a conflicting code.
	EDOPRO_CHECK(compare(semantic, legacy, 86, proto::MSG_UPDATE_CARD).equivalent() == false);
	// The occupied slot is still a real mismatch; isolate code scope by using
	// the actual semantic occupant for the second assertion.
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!semantic.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode::None,
									 CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	EDOPRO_CHECK(compare(semantic, legacy, 87, proto::MSG_UPDATE_CARD).equivalent());
}

EDOPRO_TEST(known_code_and_material_structure_produce_deterministic_diagnostics) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	DuelState semantic = session.state();
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId material = CardInstanceId::None;
	EDOPRO_CHECK(!semantic.create_card({0, Zone::MonsterZone, 2, false, 0}, CardCode{42},
									CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!semantic.create_card({0, Zone::Hand, 0, false, 0}, CardCode{99},
									CardPosition{proto::POS_FACEDOWN}, &material));
	EDOPRO_CHECK(!semantic.move_card(material, {0, Zone::MonsterZone, 2, true, 0},
										CardPosition{proto::POS_FACEUP_ATTACK}));
	LegacySnapshot legacy{{8000, 8000}, 0,
		{{{0, Zone::MonsterZone, 2, false, 0}, CardCode{41},
			CardPosition{proto::POS_FACEUP_ATTACK}, {CardCode{99}}},
		 {{0, Zone::MonsterZone, 2, true, 0}, CardCode{99},
			CardPosition{proto::POS_FACEUP_ATTACK}, {}}}};
	const auto first = compare(semantic, legacy, 377, proto::MSG_MOVE);
	const auto second = compare(semantic, legacy, 377, proto::MSG_MOVE);
	EDOPRO_CHECK(!first.equivalent());
	EDOPRO_CHECK_EQ(first.mismatches.size(), second.mismatches.size());
	EDOPRO_CHECK_EQ(first.mismatches.front().format(), second.mismatches.front().format());
	EDOPRO_CHECK(first.mismatches.front().format().find("packet 377 MSG_MOVE") != std::string::npos);
	EDOPRO_CHECK(first.mismatches.front().format().find("MZONE[p0:2].code") != std::string::npos);
}

EDOPRO_TEST(observer_session_resets_and_ignores_unsupported_messages) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(1, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	const auto first_session = session.session_number();
	const auto unsupported = session.observe(
		Packet{proto::MSG_UPDATE_DATA, {0, 1, 2, 3}}, ProtocolVariant{});
	EDOPRO_CHECK_EQ(unsupported.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK_EQ(session.session_number(), first_session);
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(session.session_number(), first_session + 1);
	EDOPRO_CHECK_EQ(session.state().cards().size(), 0u);
}

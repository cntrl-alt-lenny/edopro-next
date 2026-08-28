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
			snapshot.cards.push_back({{player, Zone::Deck, sequence, false, 0}, 0});
		for(std::uint32_t sequence = 0; sequence < extra; ++sequence)
			snapshot.cards.push_back({{player, Zone::ExtraDeck, sequence, false, 0}, 0});
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
	legacy.cards.push_back({{0, Zone::MonsterZone, 3, false, 0}, 0});
	const auto mismatch = compare(semantic, legacy, 12, proto::MSG_MOVE);
	EDOPRO_CHECK(!mismatch.equivalent());
	EDOPRO_CHECK_EQ(mismatch.mismatches.front().field, "MZONE[p0:3].occupancy");
}

EDOPRO_TEST(dense_pile_slot_topology_and_extra_face_up_split_are_compared) {
	DuelState semantic;
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(2, 2), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	semantic = session.state();
	EDOPRO_CHECK(!semantic.create_card({0, Zone::ExtraDeck, 0, false, 0}, CardCode{73915052},
									 CardPosition{proto::POS_FACEUP_ATTACK}, nullptr));
	auto legacy = snapshot_for_start(2, 2);
	legacy.cards.push_back({{0, Zone::ExtraDeck, 2, false, 0}, 0});
	const auto mismatch = compare(semantic, legacy, 27, proto::MSG_MOVE);
	EDOPRO_CHECK(mismatch.equivalent());
}

EDOPRO_TEST(different_pile_slot_topology_fails_structural_equivalence) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(2, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	const auto semantic = session.state();
	auto legacy = snapshot_for_start(2, 0);
	legacy.cards[1].location.sequence = 2;
	const auto mismatch = compare(semantic, legacy, 31, proto::MSG_MOVE);
	EDOPRO_CHECK(!mismatch.equivalent());
}

EDOPRO_TEST(different_material_count_or_topology_fails_structural_equivalence) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	DuelState semantic = session.state();
	CardInstanceId material = CardInstanceId::None;
	EDOPRO_CHECK(!semantic.create_card({0, Zone::MonsterZone, 1, false, 0}, CardCode::None,
									 CardPosition{proto::POS_FACEUP_ATTACK}, nullptr));
	EDOPRO_CHECK(!semantic.create_card({0, Zone::Hand, 0, false, 0}, CardCode{99},
									 CardPosition{proto::POS_FACEDOWN}, &material));
	EDOPRO_CHECK(!semantic.move_card(material, {0, Zone::MonsterZone, 1, true, 0},
									 CardPosition{proto::POS_FACEUP_ATTACK}));
	auto legacy = snapshot_for_start(0, 0);
	legacy.cards.push_back({{0, Zone::MonsterZone, 1, false, 0}, 0});
	const auto mismatch = compare(semantic, legacy, 32, proto::MSG_MOVE);
	EDOPRO_CHECK(!mismatch.equivalent());
}

EDOPRO_TEST(life_points_and_turn_are_live_equivalence_fields) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	const auto semantic = session.state();
	auto legacy = snapshot_for_start(0, 0);
	legacy.life[1] = 7000;
	legacy.turn = 1;
	const auto mismatch = compare(semantic, legacy, 33, proto::MSG_NEW_TURN);
	EDOPRO_CHECK(!mismatch.equivalent());
	EDOPRO_CHECK_EQ(mismatch.mismatches.size(), 2u);
}

EDOPRO_TEST(query_mutated_identity_position_and_material_code_are_outside_live_scope) {
	DuelState semantic;
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	semantic = session.state();
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId material = CardInstanceId::None;
	EDOPRO_CHECK(!semantic.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{73915052},
									 CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!semantic.create_card({0, Zone::Hand, 0, false, 0}, CardCode{99},
									 CardPosition{proto::POS_FACEDOWN}, &material));
	EDOPRO_CHECK(!semantic.move_card(material, {0, Zone::MonsterZone, 0, true, 0},
									 CardPosition{proto::POS_FACEUP_ATTACK}));
	LegacySnapshot legacy = snapshot_for_start(0, 0);
	legacy.cards.push_back({{0, Zone::MonsterZone, 0, false, 0}, 1});
	legacy.cards.push_back({{0, Zone::MonsterZone, 0, true, 0}, 0});
	// The real legacy projection intentionally has no fields for these query-
	// sensitive values until the query stream has an equivalent freshness model.
	EDOPRO_CHECK(compare(semantic, legacy, 86, proto::MSG_UPDATE_CARD).equivalent());

	semantic.find(host)->code = CardCode{123};
	semantic.find(host)->position = CardPosition{proto::POS_FACEUP_DEFENSE};
	semantic.find(material)->code = CardCode{456};
	EDOPRO_CHECK(compare(semantic, legacy, 87, proto::MSG_UPDATE_CARD).equivalent());
}

EDOPRO_TEST(structural_mismatch_diagnostics_are_deterministic) {
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
		{{{0, Zone::MonsterZone, 2, false, 0}, 0},
		 {{0, Zone::MonsterZone, 2, true, 0}, 0}}};
	const auto first = compare(semantic, legacy, 377, proto::MSG_MOVE);
	const auto second = compare(semantic, legacy, 377, proto::MSG_MOVE);
	EDOPRO_CHECK(!first.equivalent());
	EDOPRO_CHECK_EQ(first.mismatches.size(), second.mismatches.size());
	EDOPRO_CHECK_EQ(first.mismatches.front().format(), second.mismatches.front().format());
	EDOPRO_CHECK(first.mismatches.front().format().find("packet 377 MSG_MOVE") != std::string::npos);
	EDOPRO_CHECK(first.mismatches.front().format().find("MZONE[p0:2]") != std::string::npos);
}

EDOPRO_TEST(observer_session_resets_and_ignores_unsupported_query_messages) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(1, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	const auto first_session = session.session_number();
	const auto skipped = session.observe(
		Packet{proto::MSG_UPDATE_DATA,
			{0, proto::LOCATION_DECK, 6, 0, 0, 0, 4, 0, 0, 0, 0, 0x40}}, ProtocolVariant{});
	EDOPRO_CHECK_EQ(skipped.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(session.comparison_available());
	const auto skipped_card = session.observe(
		Packet{proto::MSG_UPDATE_CARD,
			{0, proto::LOCATION_DECK, 0, 4, 0, 0, 0, 0, 0x40}}, ProtocolVariant{});
	EDOPRO_CHECK_EQ(skipped_card.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(session.comparison_available());
	EDOPRO_CHECK_EQ(session.session_number(), first_session);
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(session.session_number(), first_session + 1);
	EDOPRO_CHECK_EQ(session.state().cards().size(), 0u);
}

EDOPRO_TEST(unsupported_non_query_packet_conservatively_taints_until_start) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	EDOPRO_CHECK(session.comparison_available());
	const auto unsupported = session.observe(Packet{proto::MSG_SWAP, {}}, ProtocolVariant{});
	EDOPRO_CHECK_EQ(unsupported.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(!session.comparison_available());
	EDOPRO_CHECK_EQ(session.observe(Packet{proto::MSG_NEW_TURN, {0}}, ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	EDOPRO_CHECK(!session.comparison_available());
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
					DecodeStatus::Decoded);
	EDOPRO_CHECK(session.comparison_available());
}

EDOPRO_TEST(partially_understood_query_is_safe_but_remains_unsupported) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
		DecodeStatus::Decoded);
	// Modern unknown query fields are refused, but the state before the packet
	// remains unchanged and the live observer keeps structural comparison
	// available because query-sensitive values are outside its current scope.
	const auto packet = Packet{proto::MSG_UPDATE_DATA,
		{0, proto::LOCATION_DECK, 6, 0, 0, 0, 4, 0, 0, 0, 0, 0x40}};
	const auto result = session.observe(packet, ProtocolVariant{});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(session.comparison_available());
	EDOPRO_CHECK_EQ(session.state().cards().size(), 0u);
}

EDOPRO_TEST(fault_injection_detects_synthetic_life_point_mismatch) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(5, 1), ProtocolVariant{}).status,
		DecodeStatus::Decoded);
	auto legacy = snapshot_for_start(5, 1);
	// Synthetic fault: mutate legacy life point
	legacy.life[0] += 500;
	const auto result = compare(session.state(), legacy, 50, proto::MSG_DAMAGE);
	EDOPRO_CHECK(!result.equivalent());
	EDOPRO_CHECK_EQ(result.mismatches.size(), 1u);
	EDOPRO_CHECK_EQ(result.mismatches.front().field, "p0.life");
	EDOPRO_CHECK_EQ(result.mismatches.front().semantic, "8000");
	EDOPRO_CHECK_EQ(result.mismatches.front().legacy, "8500");
}

EDOPRO_TEST(fault_injection_detects_synthetic_zone_card_mismatch) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(5, 1), ProtocolVariant{}).status,
		DecodeStatus::Decoded);
	auto legacy = snapshot_for_start(5, 1);
	// Synthetic fault: remove a card from legacy deck snapshot
	legacy.cards.pop_back();
	const auto result = compare(session.state(), legacy, 51, proto::MSG_DRAW);
	EDOPRO_CHECK(!result.equivalent());
	EDOPRO_CHECK_EQ(result.mismatches.size(), 1u);
	EDOPRO_CHECK(result.mismatches.front().field.find(".occupancy") != std::string::npos);
	EDOPRO_CHECK_EQ(result.mismatches.front().semantic, "occupied");
	EDOPRO_CHECK_EQ(result.mismatches.front().legacy, "empty");
}

EDOPRO_TEST(fault_injection_detects_synthetic_material_count_mismatch) {
	ObserverSession session;
	EDOPRO_CHECK_EQ(session.observe(start_packet(0, 0), ProtocolVariant{}).status,
		DecodeStatus::Decoded);
	DuelState semantic = session.state();
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId material = CardInstanceId::None;
	EDOPRO_CHECK(!semantic.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!semantic.create_card({0, Zone::Hand, 0, false, 0}, CardCode{2},
		CardPosition{proto::POS_FACEDOWN}, &material));
	EDOPRO_CHECK(!semantic.move_card(material, {0, Zone::MonsterZone, 0, true, 0},
		CardPosition{proto::POS_FACEUP_ATTACK}));
	auto legacy = snapshot_for_start(0, 0);
	legacy.cards.push_back({{0, Zone::MonsterZone, 0, false, 0}, 2}); // Synthetic fault: claims 2 materials instead of 1
	legacy.cards.push_back({{0, Zone::MonsterZone, 0, true, 0}, 0});
	legacy.cards.push_back({{0, Zone::MonsterZone, 0, true, 1}, 0});
	const auto result = compare(semantic, legacy, 52, proto::MSG_MOVE);
	EDOPRO_CHECK(!result.equivalent());
	EDOPRO_CHECK_EQ(result.mismatches.size(), 2u); // occupancy of material 1 plus count mismatch on host
}

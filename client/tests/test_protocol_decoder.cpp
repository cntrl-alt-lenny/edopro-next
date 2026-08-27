// The decoder: payload layouts, the four ways a packet can be refused, and
// the state each supported message produces.
#include "packet_builder.h"
#include "test_support.h"

#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/protocol_decoder.h"

#include <algorithm>
#include <tuple>

using namespace edopro_next::client;
using edopro_next::testing::PayloadBuilder;
namespace proto = edopro_next::client::protocol;

namespace {

// An id in a gap of upstream's message numbering: 8 is MSG_REQUEST_DECK and
// 10 is MSG_SELECT_BATTLECMD, so 9 is defined by nothing.
constexpr std::uint8_t kUndefinedMessageId = 9;

Packet start_packet(std::uint16_t main = 5, std::uint16_t extra = 1) {
	return PayloadBuilder()
		.u8(0)
		.u32(8000)
		.u32(8000)
		.u16(main)
		.u16(extra)
		.u16(main)
		.u16(extra)
		.packet(proto::MSG_START);
}

Packet variant_start_packet(bool compat, std::uint16_t main = 5, std::uint16_t extra = 1) {
	if(!compat)
		return start_packet(main, extra);
	return PayloadBuilder()
		.u8(0).u8(0)
		.u32(8000).u32(8000)
		.u16(main).u16(extra)
		.u16(main).u16(extra)
		.packet(proto::MSG_START);
}

Packet draw_packet(std::uint8_t player, const std::vector<std::uint32_t>& codes) {
	PayloadBuilder builder;
	builder.u8(player).u32(static_cast<std::uint32_t>(codes.size()));
	for(const auto code : codes)
		builder.u32(code).u32(proto::POS_FACEDOWN_DEFENSE);
	return builder.packet(proto::MSG_DRAW);
}

std::vector<std::uint8_t> query_field(std::uint32_t flag, const std::vector<std::uint8_t>& payload) {
	PayloadBuilder b;
	b.u16(static_cast<std::uint16_t>(4 + payload.size())).u32(flag);
	for(const auto byte : payload) b.u8(byte);
	return b.take();
}

std::vector<std::uint8_t> modern_query(const std::vector<std::vector<std::uint8_t>>& fields) {
	std::vector<std::uint8_t> body;
	for(const auto& field : fields) body.insert(body.end(), field.begin(), field.end());
	PayloadBuilder b;
	b.u32(static_cast<std::uint32_t>(body.size()));
	for(const auto byte : body) b.u8(byte);
	return b.take();
}

std::vector<std::uint8_t> query_u32(std::uint32_t value) {
	return PayloadBuilder().u32(value).take();
}

Packet move_packet(std::uint32_t code, std::uint8_t from_location, std::uint32_t from_sequence,
				   std::uint8_t to_location, std::uint32_t to_sequence,
				   std::uint32_t to_position, std::uint32_t reason = 0,
				   std::uint8_t controller = 0) {
	return PayloadBuilder()
		.u32(code)
		.loc(controller, from_location, from_sequence, proto::POS_FACEDOWN)
		.loc(controller, to_location, to_sequence, to_position)
		.u32(reason)
		.packet(proto::MSG_MOVE);
}

std::uint8_t raw_location(Zone zone) {
	switch(zone) {
	case Zone::Deck: return proto::LOCATION_DECK;
	case Zone::Hand: return proto::LOCATION_HAND;
	case Zone::MonsterZone: return proto::LOCATION_MZONE;
	case Zone::SpellZone: return proto::LOCATION_SZONE;
	case Zone::Graveyard: return proto::LOCATION_GRAVE;
	case Zone::Banished: return proto::LOCATION_REMOVED;
	case Zone::ExtraDeck: return proto::LOCATION_EXTRA;
	default: return 0;
	}
}

Packet confirm_decktop_packet(std::uint8_t player, const std::vector<std::uint32_t>& codes,
							 bool compat = false) {
	PayloadBuilder builder;
	builder.u8(player);
	if(compat) builder.u8(static_cast<std::uint8_t>(codes.size()));
	else builder.u32(static_cast<std::uint32_t>(codes.size()));
	for(const auto code : codes) {
		builder.u32(code);
		if(compat) builder.u8(0).u8(proto::LOCATION_DECK).u8(0);
		else builder.u8(0).u8(proto::LOCATION_DECK).u32(0);
	}
	return builder.packet(proto::MSG_CONFIRM_DECKTOP);
}

Packet confirm_cards_packet(std::uint8_t player,
							 const std::vector<std::tuple<std::uint32_t, std::uint8_t,
															 std::uint8_t, std::uint32_t>>& entries,
							 bool compat = false) {
	PayloadBuilder builder;
	builder.u8(player);
	if(compat) builder.u8(0).u8(static_cast<std::uint8_t>(entries.size()));
	else builder.u32(static_cast<std::uint32_t>(entries.size()));
	for(const auto& [code, controller, location, sequence] : entries) {
		builder.u32(code).u8(controller).u8(location);
		if(compat) builder.u8(static_cast<std::uint8_t>(sequence));
		else builder.u32(sequence);
	}
	return builder.packet(proto::MSG_CONFIRM_CARDS);
}

Packet become_target_packet(const std::vector<CardLocation>& locations, bool compat = false) {
	PayloadBuilder builder;
	if(compat) builder.u8(static_cast<std::uint8_t>(locations.size()));
	else builder.u32(static_cast<std::uint32_t>(locations.size()));
	for(const auto& location : locations)
		builder.loc(location.controller, raw_location(location.zone),
				location.sequence, 0, compat);
	return builder.packet(proto::MSG_BECOME_TARGET);
}

Packet card_hint_packet(const CardLocation& location, std::uint8_t type, std::uint64_t value,
						bool compat = false) {
	PayloadBuilder builder;
	builder.loc(location.controller, raw_location(location.zone), location.sequence,
				0, compat).u8(type);
	if(compat) builder.u32(static_cast<std::uint32_t>(value));
	else builder.u64(value);
	return builder.packet(proto::MSG_CARD_HINT);
}

Packet player_hint_packet(std::uint8_t player, std::uint8_t type, std::uint64_t value,
						  bool compat = false) {
	PayloadBuilder builder;
	builder.u8(player).u8(type);
	if(compat) builder.u32(static_cast<std::uint32_t>(value));
	else builder.u64(value);
	return builder.packet(proto::MSG_PLAYER_HINT);
}

struct Fixture {
	ProtocolDecoder decoder;
	DuelState state;

	DecodeResult run(const Packet& packet) { return decoder.decode(packet, state); }

	DecodeResult expect_decoded(const Packet& packet) {
		auto result = run(packet);
		EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
		if(result.status != DecodeStatus::Decoded)
			edopro_next::testing::report_failure(__FILE__, __LINE__, result.detail);
		return result;
	}
};

Fixture started(std::uint16_t main = 5, std::uint16_t extra = 1) {
	Fixture fixture;
	fixture.expect_decoded(start_packet(main, extra));
	return fixture;
}

} // namespace

EDOPRO_TEST(undefined_ids_are_unknown_not_malformed) {
	Fixture fixture;
	const auto result = fixture.run(Packet{kUndefinedMessageId, {1, 2, 3}});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::UnknownMessage);
	EDOPRO_CHECK(result.events.empty());
}

EDOPRO_TEST(query_messages_are_supported_and_bad_framing_is_malformed) {
	Fixture fixture;
	const auto result = fixture.run(Packet{proto::MSG_UPDATE_DATA, {0, 1, 2, 3}});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
	EDOPRO_CHECK(ProtocolDecoder::supports(proto::MSG_UPDATE_DATA));
}

EDOPRO_TEST(short_payload_of_a_supported_message_is_malformed) {
	Fixture fixture;
	const auto result = fixture.run(Packet{proto::MSG_NEW_TURN, {}});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
}

EDOPRO_TEST(trailing_bytes_are_malformed) {
	// An over-long payload means our layout is wrong, so it must not pass
	// silently just because the fields we wanted happened to be there.
	Fixture fixture;
	const auto result = fixture.run(Packet{proto::MSG_NEW_TURN, {0, 0}});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
}

EDOPRO_TEST(every_supported_id_is_dispatched) {
	// Guards against kSupported and the switch drifting apart: an id listed as
	// supported but missing from the switch would show up here as unsupported.
	Fixture fixture;
	for(const auto id : ProtocolDecoder::supported_messages()) {
		EDOPRO_CHECK(!protocol::message_name(id).empty());
		const auto result = fixture.run(Packet{id, {}});
		EDOPRO_CHECK(result.status != DecodeStatus::UnsupportedMessage);
		EDOPRO_CHECK(result.status != DecodeStatus::UnknownMessage);
	}
}

EDOPRO_TEST(start_populates_life_and_piles) {
	Fixture fixture;
	const auto result = fixture.expect_decoded(start_packet(40, 15));
	EDOPRO_CHECK_EQ(result.events.size(), std::size_t{1});
	EDOPRO_CHECK(std::holds_alternative<DuelStarted>(result.events.front()));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{8000});
	EDOPRO_CHECK_EQ(fixture.state.zone(1, Zone::Deck).size(), std::size_t{40});
	EDOPRO_CHECK_EQ(fixture.state.zone(1, Zone::ExtraDeck).size(), std::size_t{15});
}

EDOPRO_TEST(turn_and_phase_come_from_their_own_messages) {
	auto fixture = started();
	fixture.expect_decoded(PayloadBuilder().u8(1).packet(proto::MSG_NEW_TURN));
	EDOPRO_CHECK_EQ(fixture.state.turn(), std::uint32_t{1});
	EDOPRO_CHECK_EQ(fixture.state.turn_player(), PlayerId{1});

	fixture.expect_decoded(
		PayloadBuilder().u16(proto::PHASE_MAIN2).packet(proto::MSG_NEW_PHASE));
	EDOPRO_CHECK_EQ(fixture.state.phase(), Phase::Main2);

	// An unmodelled phase value must render deterministically, not be dropped.
	fixture.expect_decoded(PayloadBuilder().u16(0x4000).packet(proto::MSG_NEW_PHASE));
	EDOPRO_CHECK_EQ(fixture.state.phase(), Phase::Unknown);
}

EDOPRO_TEST(new_turn_naming_a_non_duelist_is_inconsistent) {
	auto fixture = started();
	const auto result = fixture.run(
		PayloadBuilder().u8(proto::PLAYER_ALL).packet(proto::MSG_NEW_TURN));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK_EQ(fixture.state.turn(), std::uint32_t{0});
}

EDOPRO_TEST(draw_moves_from_the_top_of_the_deck) {
	auto fixture = started(5, 0);
	const auto deck = fixture.state.zone(0, Zone::Deck);
	const auto top = deck.back();
	const auto next = deck[deck.size() - 2];

	const auto result = fixture.expect_decoded(draw_packet(0, {1111, 2222}));
	EDOPRO_CHECK_EQ(fixture.state.zone(0, Zone::Deck).size(), std::size_t{3});
	EDOPRO_CHECK_EQ(fixture.state.zone(0, Zone::Hand).size(), std::size_t{2});
	// The first code in the payload belongs to the first card drawn, which is
	// the one at the back of the pile.
	EDOPRO_CHECK_EQ(fixture.state.find(top)->code, static_cast<CardCode>(1111u));
	EDOPRO_CHECK_EQ(fixture.state.find(next)->code, static_cast<CardCode>(2222u));
	EDOPRO_CHECK_EQ(fixture.state.zone(0, Zone::Hand)[0], top);

	const auto* drawn = std::get_if<CardsDrawn>(&result.events.back());
	EDOPRO_CHECK(drawn != nullptr);
	if(drawn != nullptr)
		EDOPRO_CHECK_EQ(drawn->cards.size(), std::size_t{2});
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(draw_beyond_the_deck_is_inconsistent_and_changes_nothing) {
	auto fixture = started(2, 0);
	const auto result = fixture.run(draw_packet(0, {1, 2, 3}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK_EQ(fixture.state.zone(0, Zone::Deck).size(), std::size_t{2});
	EDOPRO_CHECK(fixture.state.zone(0, Zone::Hand).empty());
}

EDOPRO_TEST(an_absurd_count_fails_fast_as_malformed) {
	auto fixture = started();
	const auto packet =
		PayloadBuilder().u8(0).u32(0xffffffffu).packet(proto::MSG_DRAW);
	const auto result = fixture.run(packet);
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
}

EDOPRO_TEST(move_tracks_one_instance_across_zones) {
	auto fixture = started(5, 0);
	fixture.expect_decoded(draw_packet(0, {1111}));
	const auto id = fixture.state.zone(0, Zone::Hand).front();

	const auto result = fixture.expect_decoded(
		move_packet(1111, proto::LOCATION_HAND, 0, proto::LOCATION_MZONE, 2,
					proto::POS_FACEUP_ATTACK, proto::REASON_SUMMON));

	EDOPRO_CHECK_EQ(fixture.state.at(CardLocation{0, Zone::MonsterZone, 2, false, 0}), id);
	EDOPRO_CHECK(fixture.state.zone(0, Zone::Hand).empty());
	EDOPRO_CHECK_EQ(fixture.state.find(id)->position, CardPosition{proto::POS_FACEUP_ATTACK});

	const auto* moved = std::get_if<CardMoved>(&result.events.back());
	EDOPRO_CHECK(moved != nullptr);
	if(moved != nullptr) {
		EDOPRO_CHECK_EQ(moved->card, id);
		EDOPRO_CHECK_EQ(moved->from.zone, Zone::Hand);
		EDOPRO_CHECK_EQ(moved->to.zone, Zone::MonsterZone);
		EDOPRO_CHECK_EQ(moved->reason, proto::REASON_SUMMON);
	}
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(move_reports_the_resolved_sequence_not_the_requested_one) {
	// Piles renumber on insertion, so the event must carry where the card
	// actually landed. A UI told otherwise would animate to the wrong slot.
	auto fixture = started(5, 0);
	fixture.expect_decoded(draw_packet(0, {1111, 2222}));
	const auto second = fixture.state.zone(0, Zone::Hand)[1];

	const auto result = fixture.expect_decoded(
		move_packet(2222, proto::LOCATION_HAND, 1, proto::LOCATION_GRAVE, 0,
					proto::POS_FACEUP, proto::REASON_EFFECT));
	const auto* moved = std::get_if<CardMoved>(&result.events.back());
	EDOPRO_CHECK(moved != nullptr);
	if(moved != nullptr) {
		EDOPRO_CHECK_EQ(moved->card, second);
		EDOPRO_CHECK_EQ(moved->to.sequence, std::uint32_t{0});
	}
}

EDOPRO_TEST(move_from_nowhere_creates_and_move_to_nowhere_removes) {
	auto fixture = started(0, 0);
	const auto created = fixture.expect_decoded(
		move_packet(4444, 0, 0, proto::LOCATION_MZONE, 1, proto::POS_FACEUP_ATTACK));
	EDOPRO_CHECK_EQ(fixture.state.cards().size(), std::size_t{1});
	const auto* token = std::get_if<CardCreated>(&created.events.front());
	EDOPRO_CHECK(token != nullptr);

	const auto id = fixture.state.at(CardLocation{0, Zone::MonsterZone, 1, false, 0});
	EDOPRO_CHECK(id != CardInstanceId::None);

	const auto removed = fixture.expect_decoded(
		move_packet(4444, proto::LOCATION_MZONE, 1, 0, 0, 0, proto::REASON_RULE));
	EDOPRO_CHECK(std::holds_alternative<CardRemoved>(removed.events.back()));
	EDOPRO_CHECK(!fixture.state.find(id)->tracked);
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(move_from_an_empty_slot_is_inconsistent) {
	auto fixture = started(5, 0);
	const auto result = fixture.run(
		move_packet(0, proto::LOCATION_MZONE, 4, proto::LOCATION_GRAVE, 0, proto::POS_FACEUP));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(fixture.state.zone(0, Zone::Graveyard).empty());
}

EDOPRO_TEST(life_point_messages_agree_with_the_legacy_client) {
	auto fixture = started(0, 0);

	auto result = fixture.expect_decoded(
		PayloadBuilder().u8(0).u32(1500).packet(proto::MSG_DAMAGE));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{6500});
	const auto* change = std::get_if<LifePointsChanged>(&result.events.front());
	EDOPRO_CHECK(change != nullptr);
	if(change != nullptr)
		EDOPRO_CHECK_EQ(change->reason, LifeChangeReason::Damage);

	fixture.expect_decoded(PayloadBuilder().u8(0).u32(500).packet(proto::MSG_RECOVER));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{7000});

	fixture.expect_decoded(PayloadBuilder().u8(0).u32(1000).packet(proto::MSG_PAY_LPCOST));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{6000});

	fixture.expect_decoded(PayloadBuilder().u8(0).u32(200).packet(proto::MSG_LPUPDATE));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{200});

	// Lethal damage states more than it removes, and the event keeps both.
	result = fixture.expect_decoded(
		PayloadBuilder().u8(0).u32(9999).packet(proto::MSG_DAMAGE));
	EDOPRO_CHECK_EQ(fixture.state.life_points(0), std::int64_t{0});
	change = std::get_if<LifePointsChanged>(&result.events.front());
	EDOPRO_CHECK(change != nullptr);
	if(change != nullptr) {
		EDOPRO_CHECK_EQ(change->to, std::int64_t{0});
		EDOPRO_CHECK_EQ(change->amount, std::int64_t{9999});
	}
}

EDOPRO_TEST(a_chain_builds_resolves_and_ends) {
	auto fixture = started(5, 0);
	fixture.expect_decoded(draw_packet(0, {1111}));
	fixture.expect_decoded(move_packet(1111, proto::LOCATION_HAND, 0, proto::LOCATION_SZONE, 0,
									   proto::POS_FACEUP));

	const auto chaining = PayloadBuilder()
							  .u32(1111)
							  .loc(0, proto::LOCATION_SZONE, 0, proto::POS_FACEUP)
							  .u8(0)
							  .u8(proto::LOCATION_SZONE)
							  .u32(0)
							  .u64(4242)
							  .u32(1)
							  .packet(proto::MSG_CHAINING);
	const auto result = fixture.expect_decoded(chaining);
	EDOPRO_CHECK_EQ(fixture.state.chain().size(), std::size_t{1});
	EDOPRO_CHECK_EQ(fixture.state.chain().front().description, std::uint64_t{4242});
	EDOPRO_CHECK(std::holds_alternative<ChainLinkAdded>(result.events.back()));

	fixture.expect_decoded(PayloadBuilder().u8(1).packet(proto::MSG_CHAINED));
	fixture.expect_decoded(PayloadBuilder().u8(1).packet(proto::MSG_CHAIN_SOLVING));
	EDOPRO_CHECK(fixture.state.chain().front().resolving);
	fixture.expect_decoded(PayloadBuilder().u8(1).packet(proto::MSG_CHAIN_SOLVED));
	EDOPRO_CHECK(fixture.state.chain().front().resolved);

	const auto ended = fixture.expect_decoded(Packet{proto::MSG_CHAIN_END, {}});
	EDOPRO_CHECK(fixture.state.chain().empty());
	const auto* end = std::get_if<ChainEnded>(&ended.events.front());
	EDOPRO_CHECK(end != nullptr);
	if(end != nullptr)
		EDOPRO_CHECK_EQ(end->links, std::uint32_t{1});
}

EDOPRO_TEST(a_chain_link_out_of_order_is_inconsistent) {
	auto fixture = started(0, 0);
	const auto chaining = PayloadBuilder()
							  .u32(1111)
							  .loc(0, proto::LOCATION_GRAVE, 0, proto::POS_FACEUP)
							  .u8(0)
							  .u8(proto::LOCATION_GRAVE)
							  .u32(0)
							  .u64(0)
							  .u32(3)
							  .packet(proto::MSG_CHAINING);
	const auto result = fixture.run(chaining);
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(fixture.state.chain().empty());
}

EDOPRO_TEST(a_chain_link_from_an_untracked_place_still_records_the_link) {
	// Nothing guarantees the client can see the activating card. The link is
	// real either way, and must not be dropped just because it names no
	// instance.
	auto fixture = started(0, 0);
	const auto chaining = PayloadBuilder()
							  .u32(1111)
							  .loc(0, proto::LOCATION_MZONE, 0, proto::POS_FACEUP)
							  .u8(0)
							  .u8(proto::LOCATION_MZONE)
							  .u32(0)
							  .u64(0)
							  .u32(1)
							  .packet(proto::MSG_CHAINING);
	fixture.expect_decoded(chaining);
	EDOPRO_CHECK_EQ(fixture.state.chain().size(), std::size_t{1});
	EDOPRO_CHECK_EQ(fixture.state.chain().front().card, CardInstanceId::None);
	EDOPRO_CHECK_EQ(fixture.state.chain().front().code, static_cast<CardCode>(1111u));
}

EDOPRO_TEST(attack_resolves_both_ends_and_direct_attacks) {
	auto fixture = started(0, 0);
	fixture.expect_decoded(
		move_packet(1, 0, 0, proto::LOCATION_MZONE, 0, proto::POS_FACEUP_ATTACK));
	const auto attacker = fixture.state.at(CardLocation{0, Zone::MonsterZone, 0, false, 0});

	const auto direct = PayloadBuilder()
							.loc(0, proto::LOCATION_MZONE, 0, 0)
							.loc(0, 0, 0, 0)
							.packet(proto::MSG_ATTACK);
	const auto result = fixture.expect_decoded(direct);
	const auto* declared = std::get_if<AttackDeclared>(&result.events.front());
	EDOPRO_CHECK(declared != nullptr);
	if(declared != nullptr) {
		EDOPRO_CHECK(declared->direct);
		EDOPRO_CHECK_EQ(declared->attacker, attacker);
		EDOPRO_CHECK_EQ(declared->target, CardInstanceId::None);
	}

	const auto missing = PayloadBuilder()
							 .loc(0, proto::LOCATION_MZONE, 0, 0)
							 .loc(1, proto::LOCATION_MZONE, 3, 0)
							 .packet(proto::MSG_ATTACK);
	EDOPRO_CHECK_EQ(fixture.run(missing).status, DecodeStatus::Inconsistent);
}

EDOPRO_TEST(battle_is_the_only_source_of_combat_stats) {
	auto fixture = started(0, 0);
	fixture.expect_decoded(
		move_packet(1, 0, 0, proto::LOCATION_MZONE, 0, proto::POS_FACEUP_ATTACK));
	const auto id = fixture.state.at(CardLocation{0, Zone::MonsterZone, 0, false, 0});
	EDOPRO_CHECK(!fixture.state.find(id)->attack.has_value());

	const auto battle = PayloadBuilder()
							.loc(0, proto::LOCATION_MZONE, 0, 0)
							.u32(1800)
							.u32(1000)
							.u8(0)
							.loc(0, 0, 0, 0)
							.u32(0)
							.u32(0)
							.u8(0)
							.packet(proto::MSG_BATTLE);
	const auto result = fixture.expect_decoded(battle);
	EDOPRO_CHECK_EQ(result.events.size(), std::size_t{1});
	EDOPRO_CHECK_EQ(fixture.state.find(id)->attack.value(), 1800);
	EDOPRO_CHECK_EQ(fixture.state.find(id)->defense.value(), 1000);
}

EDOPRO_TEST(position_change_must_agree_with_the_model) {
	auto fixture = started(0, 0);
	fixture.expect_decoded(
		move_packet(7, 0, 0, proto::LOCATION_MZONE, 0, proto::POS_FACEDOWN_DEFENSE));
	const auto id = fixture.state.at(CardLocation{0, Zone::MonsterZone, 0, false, 0});

	// MSG_POS_CHANGE narrows sequence and both positions to one byte in both
	// protocol revisions, unlike loc_info.
	const auto good = PayloadBuilder()
						  .u32(7)
						  .u8(0)
						  .u8(proto::LOCATION_MZONE)
						  .u8(0)
						  .u8(proto::POS_FACEDOWN_DEFENSE)
						  .u8(proto::POS_FACEUP_ATTACK)
						  .packet(proto::MSG_POS_CHANGE);
	fixture.expect_decoded(good);
	EDOPRO_CHECK_EQ(fixture.state.find(id)->position, CardPosition{proto::POS_FACEUP_ATTACK});

	const auto stale = PayloadBuilder()
						   .u32(7)
						   .u8(0)
						   .u8(proto::LOCATION_MZONE)
						   .u8(0)
						   .u8(proto::POS_FACEDOWN_DEFENSE)
						   .u8(proto::POS_FACEUP_DEFENSE)
						   .packet(proto::MSG_POS_CHANGE);
	EDOPRO_CHECK_EQ(fixture.run(stale).status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK_EQ(fixture.state.find(id)->position, CardPosition{proto::POS_FACEUP_ATTACK});
}

EDOPRO_TEST(compat_streams_use_narrow_location_fields) {
	ProtocolDecoder compat{ProtocolVariant{true}};
	DuelState state;

	// MSG_START carries an extra duel-rule byte in the old protocol.
	const auto start = PayloadBuilder()
						   .u8(0)
						   .u8(5)
						   .u32(8000)
						   .u32(8000)
						   .u16(1)
						   .u16(0)
						   .u16(1)
						   .u16(0)
						   .packet(proto::MSG_START);
	EDOPRO_CHECK_EQ(compat.decode(start, state).status, DecodeStatus::Decoded);

	// ... and one-byte counts, with the top code bit reserved.
	const auto draw = PayloadBuilder().u8(0).u8(1).u32(0x80000009u).packet(proto::MSG_DRAW);
	EDOPRO_CHECK_EQ(compat.decode(draw, state).status, DecodeStatus::Decoded);
	const auto id = state.zone(0, Zone::Hand).front();
	EDOPRO_CHECK_EQ(state.find(id)->code, static_cast<CardCode>(9u));

	const auto move = PayloadBuilder()
						  .u32(9)
						  .loc(0, proto::LOCATION_HAND, 0, proto::POS_FACEDOWN, true)
						  .loc(0, proto::LOCATION_MZONE, 1, proto::POS_FACEUP_ATTACK, true)
						  .u32(proto::REASON_SUMMON)
						  .packet(proto::MSG_MOVE);
	EDOPRO_CHECK_EQ(compat.decode(move, state).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(state.at(CardLocation{0, Zone::MonsterZone, 1, false, 0}), id);

	// The same bytes read as a modern stream are simply too short.
	ProtocolDecoder modern;
	DuelState fresh;
	EDOPRO_CHECK_EQ(modern.decode(start, fresh).status, DecodeStatus::Malformed);
}

EDOPRO_TEST(modern_update_data_consumes_skips_and_applies_query_patch) {
	auto fixture = started(1, 0);
	std::vector<std::vector<std::uint8_t>> fields;
	fields.push_back(query_field(proto::QUERY_CODE, query_u32(73915052)));
	fields.push_back(query_field(proto::QUERY_POSITION, query_u32(proto::POS_FACEUP_ATTACK)));
	fields.push_back(query_field(proto::QUERY_ATTACK, query_u32(1900)));
	fields.push_back(query_field(proto::QUERY_END, {}));
	PayloadBuilder payload;
	payload.u8(0).u8(proto::LOCATION_DECK);
	for(const auto byte : modern_query(fields)) payload.u8(byte);
	const auto result = fixture.run(payload.packet(proto::MSG_UPDATE_DATA));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK(result.query_coverage.has_value());
	const auto id = fixture.state.zone(0, Zone::Deck).front();
	EDOPRO_CHECK_EQ(fixture.state.find(id)->code, static_cast<CardCode>(73915052u));
	EDOPRO_CHECK_EQ(fixture.state.find(id)->position, CardPosition{proto::POS_FACEUP_ATTACK});
	EDOPRO_CHECK_EQ(fixture.state.find(id)->attack.value(), 1900);
}

EDOPRO_TEST(modern_update_card_is_direct_field_stream_and_zero_is_concealment) {
	auto fixture = started(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 2, false, 0}, CardCode{77},
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	const auto query_fields = std::vector<std::vector<std::uint8_t>>{
		query_field(proto::QUERY_CODE, query_u32(0)), query_field(proto::QUERY_END, {})};
	PayloadBuilder payload;
	payload.u8(0).u8(proto::LOCATION_MZONE).u8(2);
	for(const auto& field : query_fields)
		for(const auto byte : field) payload.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(payload.packet(proto::MSG_UPDATE_CARD)).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(id)->code, CardCode::None);
}

EDOPRO_TEST(query_patch_preserves_omitted_fields_and_applies_relationships_safely) {
	auto fixture = started(0, 0);
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId target = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{77},
		CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 1, false, 0}, CardCode{88},
		CardPosition{proto::POS_FACEUP_ATTACK}, &target));
	const auto initial = std::vector<std::vector<std::uint8_t>>{
		query_field(proto::QUERY_CODE, query_u32(1234)),
		query_field(proto::QUERY_END, {})};
	PayloadBuilder first;
	first.u8(0).u8(proto::LOCATION_MZONE).u8(0);
	for(const auto& field : initial) for(const auto byte : field) first.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(first.packet(proto::MSG_UPDATE_CARD)).status, DecodeStatus::Decoded);

	const auto target_payload = PayloadBuilder().u32(1).loc(0, proto::LOCATION_MZONE, 1, 0).take();
	const auto equip_payload = PayloadBuilder().loc(0, proto::LOCATION_MZONE, 1, 0).take();
	const auto overlay_payload = PayloadBuilder().u32(1).u32(4321).take();
	const auto counters_payload = PayloadBuilder().u32(1).u32((3u << 16) | 5u).take();
	const auto fields = std::vector<std::vector<std::uint8_t>>{
		query_field(proto::QUERY_POSITION, query_u32(proto::POS_FACEUP_DEFENSE)),
		query_field(proto::QUERY_TARGET_CARD, target_payload),
		query_field(proto::QUERY_EQUIP_CARD, equip_payload),
		query_field(proto::QUERY_COUNTERS, counters_payload),
		query_field(proto::QUERY_END, {})};
	PayloadBuilder second;
	second.u8(0).u8(proto::LOCATION_MZONE).u8(0);
	for(const auto& field : fields) for(const auto byte : field) second.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(second.packet(proto::MSG_UPDATE_CARD)).status, DecodeStatus::Decoded);
	const auto* card = fixture.state.find(host);
	EDOPRO_CHECK_EQ(card->code, static_cast<CardCode>(1234u));
	EDOPRO_CHECK_EQ(card->position, CardPosition{proto::POS_FACEUP_DEFENSE});
	EDOPRO_CHECK_EQ(card->targets.size(), std::size_t{1});
	EDOPRO_CHECK_EQ(card->targets.front(), (CardLocation{0, Zone::MonsterZone, 1, false, 0}));
	EDOPRO_CHECK_EQ(card->equip_target.value(), (CardLocation{0, Zone::MonsterZone, 1, false, 0}));
	EDOPRO_CHECK_EQ(card->counters.at(5), std::uint16_t{3});

	CardInstanceId material = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::Hand, 0, false, 0}, CardCode{99},
		CardPosition{proto::POS_FACEDOWN}, &material));
	EDOPRO_CHECK(!fixture.state.move_card(material, {0, Zone::MonsterZone, 0, true, 0},
		CardPosition{proto::POS_FACEUP_ATTACK}));
	const auto material_query = std::vector<std::vector<std::uint8_t>>{
		query_field(proto::QUERY_OVERLAY_CARD, overlay_payload), query_field(proto::QUERY_END, {})};
	PayloadBuilder third;
	third.u8(0).u8(proto::LOCATION_MZONE).u8(0);
	for(const auto& field : material_query) for(const auto byte : field) third.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(third.packet(proto::MSG_UPDATE_CARD)).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(material)->code, static_cast<CardCode>(4321u));
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(query_position_uses_extra_deck_invariant_and_remains_transactional) {
	auto fixture = started(0, 2);
	const auto id = fixture.state.zone(0, Zone::ExtraDeck).front();
	const auto unchanged = fixture.state;

	PayloadBuilder ordinary;
	ordinary.u8(0).u8(proto::LOCATION_EXTRA).u8(0);
	for(const auto& field : std::vector<std::vector<std::uint8_t>>{
			query_field(proto::QUERY_POSITION, query_u32(proto::POS_FACEDOWN_DEFENSE)),
			query_field(proto::QUERY_END, {})})
		for(const auto byte : field) ordinary.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(ordinary.packet(proto::MSG_UPDATE_CARD)).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK(fixture.state.check_invariants().empty());

	PayloadBuilder transition;
	transition.u8(0).u8(proto::LOCATION_EXTRA).u8(0);
	for(const auto& field : std::vector<std::vector<std::uint8_t>>{
			query_field(proto::QUERY_POSITION, query_u32(proto::POS_FACEUP_ATTACK)),
			query_field(proto::QUERY_END, {})})
		for(const auto byte : field) transition.u8(byte);
	const auto result = fixture.run(transition.packet(proto::MSG_UPDATE_CARD));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(fixture.state == unchanged);
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
	EDOPRO_CHECK_EQ(fixture.state.find(id)->position,
		CardPosition{proto::POS_FACEDOWN_DEFENSE});
}

EDOPRO_TEST(repeated_query_collections_match_legacy_incremental_update_semantics) {
	auto fixture = started(0, 0);
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId target_a = CardInstanceId::None;
	CardInstanceId target_b = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 1, false, 0}, CardCode{2},
		CardPosition{proto::POS_FACEUP_ATTACK}, &target_a));
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 2, false, 0}, CardCode{3},
		CardPosition{proto::POS_FACEUP_ATTACK}, &target_b));

	auto update = [&](std::vector<std::vector<std::uint8_t>> fields) {
		PayloadBuilder packet;
		packet.u8(0).u8(proto::LOCATION_MZONE).u8(0);
		for(const auto& field : fields)
			for(const auto byte : field) packet.u8(byte);
		return fixture.run(packet.packet(proto::MSG_UPDATE_CARD));
	};
	const auto target = [](std::uint32_t sequence) {
		return PayloadBuilder().u32(1).loc(0, proto::LOCATION_MZONE, sequence,
			proto::POS_FACEUP_ATTACK).take();
	};
	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_TARGET_CARD, target(1)),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_TARGET_CARD, target(2)),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(host)->targets.size(), std::size_t{2});
	EDOPRO_CHECK_EQ(fixture.state.find(host)->target_instances[0], target_a);
	EDOPRO_CHECK_EQ(fixture.state.find(host)->target_instances[1], target_b);

	const auto counter = [](std::uint16_t type, std::uint16_t count) {
		return PayloadBuilder().u32(1).u32((static_cast<std::uint32_t>(count) << 16) | type).take();
	};
	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_COUNTERS, counter(5, 3)),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_COUNTERS, counter(6, 4)),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(host)->counters.size(), std::size_t{2});
	EDOPRO_CHECK_EQ(fixture.state.find(host)->counters.at(5), std::uint16_t{3});
	EDOPRO_CHECK_EQ(fixture.state.find(host)->counters.at(6), std::uint16_t{4});

	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_EQUIP_CARD,
		PayloadBuilder().loc(0, proto::LOCATION_MZONE, 1, proto::POS_FACEUP_ATTACK).take()),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(update({query_field(proto::QUERY_EQUIP_CARD,
		PayloadBuilder().loc(0, proto::LOCATION_MZONE, 1, proto::POS_FACEUP_ATTACK).take()),
		query_field(proto::QUERY_END, {})}).status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(host)->equip_target_instance, target_a);
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(overlay_query_updates_prefix_without_changing_material_topology) {
	auto fixture = started(0, 0);
	CardInstanceId host = CardInstanceId::None;
	CardInstanceId first = CardInstanceId::None;
	CardInstanceId second = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &host));
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::Hand, 0, false, 0}, CardCode{2},
		CardPosition{proto::POS_FACEDOWN}, &first));
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::Hand, 0, false, 0}, CardCode{3},
		CardPosition{proto::POS_FACEDOWN}, &second));
	EDOPRO_CHECK(!fixture.state.move_card(first, {0, Zone::MonsterZone, 0, true, 0},
		CardPosition{proto::POS_FACEUP_ATTACK}));
	EDOPRO_CHECK(!fixture.state.move_card(second, {0, Zone::MonsterZone, 0, true, 1},
		CardPosition{proto::POS_FACEUP_ATTACK}));
	PayloadBuilder packet;
	packet.u8(0).u8(proto::LOCATION_MZONE).u8(0);
	for(const auto& field : std::vector<std::vector<std::uint8_t>>{
			query_field(proto::QUERY_OVERLAY_CARD, PayloadBuilder().u32(1).u32(99).take()),
			query_field(proto::QUERY_END, {})})
		for(const auto byte : field) packet.u8(byte);
	EDOPRO_CHECK_EQ(fixture.run(packet.packet(proto::MSG_UPDATE_CARD)).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(first)->code, CardCode{99});
	EDOPRO_CHECK_EQ(fixture.state.find(second)->code, CardCode{3});
	EDOPRO_CHECK_EQ(fixture.state.find(host)->materials.size(), std::size_t{2});
	EDOPRO_CHECK(fixture.state.check_invariants().empty());
}

EDOPRO_TEST(query_parser_rejects_truncated_and_unknown_modern_fields) {
	const auto truncated = modern_query({query_field(proto::QUERY_CODE, {1, 2})});
	const auto bad = parse_query_stream(truncated, false);
	EDOPRO_CHECK(!bad.valid);
	const auto unknown = modern_query({query_field(0x40000000u, {}), query_field(proto::QUERY_END, {})});
	const auto unsupported = parse_query_stream(unknown, false);
	EDOPRO_CHECK(!unsupported.valid);
	EDOPRO_CHECK(unsupported.unsupported);
	EDOPRO_CHECK(!unsupported.coverage.unknown_fields.empty());
}

EDOPRO_TEST(query_owner_uses_modern_u8_and_compat_u32_wire_widths) {
	const auto modern_record = [](std::vector<std::vector<std::uint8_t>> fields) {
		std::vector<std::uint8_t> record;
		for(const auto& field : fields)
			record.insert(record.end(), field.begin(), field.end());
		return record;
	};
	const auto modern = parse_query_record(
		modern_record({query_field(proto::QUERY_OWNER, {0}), query_field(proto::QUERY_END, {})}),
		false);
	EDOPRO_CHECK(modern.valid);
	EDOPRO_CHECK(modern.entries.front().patch.owner.has_value());
	EDOPRO_CHECK_EQ(modern.entries.front().patch.owner.value(), std::uint8_t{0});

	const auto modern_four_bytes = parse_query_record(
		modern_record({query_field(proto::QUERY_OWNER, query_u32(0x12345678u)),
			query_field(proto::QUERY_END, {})}), false);
	EDOPRO_CHECK(!modern_four_bytes.valid);

	PayloadBuilder compat_record;
	compat_record.u32(4 + 4 + 4).u32(proto::QUERY_OWNER).u32(0x12345678u);
	const auto compat = parse_query_record(compat_record.take(), true);
	EDOPRO_CHECK(compat.valid);
	EDOPRO_CHECK(compat.entries.front().patch.owner.has_value());
	EDOPRO_CHECK_EQ(compat.entries.front().patch.owner.value(), std::uint8_t{0x78});
}

EDOPRO_TEST(modern_and_compat_query_race_widths_are_independent) {
	const auto modern_record = [](std::vector<std::vector<std::uint8_t>> fields) {
		std::vector<std::uint8_t> record;
		for(const auto& field : fields)
			record.insert(record.end(), field.begin(), field.end());
		return record;
	};
	const auto modern_u64 = parse_query_record(
		modern_record({query_field(proto::QUERY_RACE, PayloadBuilder().u64(0x1122334455667788ull).take()),
			query_field(proto::QUERY_END, {})}), false, false);
	EDOPRO_CHECK(modern_u64.valid);
	EDOPRO_CHECK_EQ(modern_u64.entries.front().patch.race.value(), 0x1122334455667788ull);

	const auto modern_u32 = parse_query_record(
		modern_record({query_field(proto::QUERY_RACE, query_u32(0xa1b2c3d4u)),
			query_field(proto::QUERY_END, {})}), false, true);
	EDOPRO_CHECK(modern_u32.valid);
	EDOPRO_CHECK_EQ(modern_u32.entries.front().patch.race.value(), 0xa1b2c3d4ull);

	const auto modern_u64_with_legacy_flag = parse_query_record(
		modern_record({query_field(proto::QUERY_RACE, PayloadBuilder().u64(0x1122334455667788ull).take()),
			query_field(proto::QUERY_END, {})}), false, true);
	EDOPRO_CHECK(!modern_u64_with_legacy_flag.valid);

	for(const bool legacy_race_size : {false, true}) {
		PayloadBuilder record;
		record.u32(4 + 4 + 4).u32(proto::QUERY_RACE).u32(0xa1b2c3d4u);
		const auto compat = parse_query_record(record.take(), true, legacy_race_size);
		EDOPRO_CHECK(compat.valid);
		EDOPRO_CHECK_EQ(compat.entries.front().patch.race.value(), 0xa1b2c3d4ull);
	}
	EDOPRO_CHECK(!(ProtocolVariant{false, false} == ProtocolVariant{false, true}));

	// Exercise the same independently supplied variant bit through the live
	// decoder construction, not only through the parser helper.
	DuelState state;
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	PayloadBuilder packet;
	packet.u8(0).u8(proto::LOCATION_MZONE).u8(0);
	for(const auto& field : modern_record({
			query_field(proto::QUERY_RACE, query_u32(0xa1b2c3d4u)),
			query_field(proto::QUERY_END, {})}))
		packet.u8(field);
	ProtocolDecoder decoder{ProtocolVariant{false, true}};
	EDOPRO_CHECK(decoder.variant().legacy_race_size);
	EDOPRO_CHECK_EQ(decoder.decode(packet.packet(proto::MSG_UPDATE_CARD), state).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(state.find(id)->race.value(), 0xa1b2c3d4ull);
}

EDOPRO_TEST(win_names_a_winner_or_says_there_is_none) {
	auto fixture = started(0, 0);
	fixture.expect_decoded(PayloadBuilder().u8(1).u8(2).packet(proto::MSG_WIN));
	EDOPRO_CHECK(fixture.state.finished());
	EDOPRO_CHECK(fixture.state.winner().has_value());
	EDOPRO_CHECK_EQ(fixture.state.winner().value(), PlayerId{1});
	EDOPRO_CHECK_EQ(fixture.state.win_reason(), std::uint8_t{2});

	auto draw = started(0, 0);
	draw.expect_decoded(
		PayloadBuilder().u8(proto::PLAYER_NONE).u8(0).packet(proto::MSG_WIN));
	EDOPRO_CHECK(draw.state.finished());
	EDOPRO_CHECK(!draw.state.winner().has_value());
}

EDOPRO_TEST(supported_message_list_is_sorted_and_unique) {
	const auto& ids = ProtocolDecoder::supported_messages();
	EDOPRO_CHECK(std::is_sorted(ids.begin(), ids.end()));
	EDOPRO_CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
	EDOPRO_CHECK(!ids.empty());
}

EDOPRO_TEST(confirm_decktop_reveals_from_the_back_in_modern_and_compat_layouts) {
	for(const bool compat : {false, true}) {
		ProtocolDecoder decoder{ProtocolVariant{compat}};
		DuelState state;
		const auto start = variant_start_packet(compat, 3, 0);
		EDOPRO_CHECK_EQ(decoder.decode(start, state).status, DecodeStatus::Decoded);
		const auto deck = state.zone(0, Zone::Deck);
		const auto result = decoder.decode(confirm_decktop_packet(0, {101, 202}, compat), state);
		EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
		EDOPRO_CHECK_EQ(state.find(deck.back())->code, static_cast<CardCode>(101));
		EDOPRO_CHECK_EQ(state.find(deck[deck.size() - 2])->code, static_cast<CardCode>(202));
		EDOPRO_CHECK(state.check_invariants().empty());
	}
}

EDOPRO_TEST(confirm_decktop_truncated_second_entry_is_transactional) {
	auto fixture = started(2, 0);
	const auto before = fixture.state;
	PayloadBuilder packet;
	packet.u8(0).u32(2).u32(101).u8(0).u8(proto::LOCATION_DECK).u32(0).u32(0)
		.u32(202).u8(0);
	const auto result = fixture.run(packet.packet(proto::MSG_CONFIRM_DECKTOP));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
	EDOPRO_CHECK(fixture.state == before);
}

EDOPRO_TEST(confirm_cards_updates_tracked_cards_and_ignores_temporary_location_zero) {
	auto fixture = started(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode::None,
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	const auto result = fixture.run(confirm_cards_packet(1, {{1234, 0, proto::LOCATION_MZONE, 0},
		{5678, 0, 0, 0}}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.find(id)->code, static_cast<CardCode>(1234));
	EDOPRO_CHECK_EQ(fixture.state.cards().size(), std::size_t{1});

	ProtocolDecoder compat{ProtocolVariant{true}};
	DuelState compat_state;
	EDOPRO_CHECK_EQ(compat.decode(variant_start_packet(true, 0, 0), compat_state).status,
		DecodeStatus::Decoded);
	CardInstanceId compat_id = CardInstanceId::None;
	EDOPRO_CHECK(!compat_state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode::None,
		CardPosition{proto::POS_FACEUP_ATTACK}, &compat_id));
	EDOPRO_CHECK_EQ(compat.decode(confirm_cards_packet(0,
		{{4321, 0, proto::LOCATION_MZONE, 0}}, true), compat_state).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(compat_state.find(compat_id)->code, static_cast<CardCode>(4321));
}

EDOPRO_TEST(confirm_cards_bad_later_reference_rolls_back_earlier_reveal) {
	auto fixture = started(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode::None,
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	const auto before = fixture.state;
	const auto result = fixture.run(confirm_cards_packet(0, {{1234, 0, proto::LOCATION_MZONE, 0},
		{5678, 0, proto::LOCATION_MZONE, 4}}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(fixture.state == before);
}

EDOPRO_TEST(become_target_records_current_chain_targets_without_card_relationship_state) {
	auto fixture = started(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	ChainLink link;
	link.link = 1;
	EDOPRO_CHECK(!fixture.state.push_chain_link(link));
	const auto result = fixture.run(become_target_packet({{0, Zone::MonsterZone, 0, false, 0}}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.chain().back().targets, std::vector<CardInstanceId>{id});
	EDOPRO_CHECK(std::holds_alternative<CardsBecameTargets>(result.events.front()));
	EDOPRO_CHECK(fixture.state.find(id)->targets.empty());
}

EDOPRO_TEST(become_target_bad_later_reference_is_transactional) {
	auto fixture = started(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!fixture.state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
		CardPosition{proto::POS_FACEUP_ATTACK}, &id));
	ChainLink link;
	link.link = 1;
	EDOPRO_CHECK(!fixture.state.push_chain_link(link));
	const auto before = fixture.state;
	const auto result = fixture.run(become_target_packet({
		{0, Zone::MonsterZone, 0, false, 0}, {0, Zone::MonsterZone, 6, false, 0}}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(fixture.state == before);
}

EDOPRO_TEST(card_hint_replaces_latest_and_tracks_description_deltas_in_both_widths) {
	for(const bool compat : {false, true}) {
		ProtocolDecoder decoder{ProtocolVariant{compat}};
		DuelState state;
		EDOPRO_CHECK_EQ(decoder.decode(variant_start_packet(compat, 0, 0), state).status,
			DecodeStatus::Decoded);
		CardInstanceId id = CardInstanceId::None;
		EDOPRO_CHECK(!state.create_card({0, Zone::MonsterZone, 0, false, 0}, CardCode{1},
			CardPosition{proto::POS_FACEUP_ATTACK}, &id));
		EDOPRO_CHECK_EQ(decoder.decode(card_hint_packet({0, Zone::MonsterZone, 0, false, 0}, 3,
			0x1234, compat), state).status, DecodeStatus::Decoded);
		EDOPRO_CHECK_EQ(state.find(id)->hint->type, std::uint8_t{3});
		EDOPRO_CHECK_EQ(decoder.decode(card_hint_packet({0, Zone::MonsterZone, 0, false, 0}, 4,
			0x5678, compat), state).status, DecodeStatus::Decoded);
		EDOPRO_CHECK_EQ(state.find(id)->hint->value, std::uint64_t{0x5678});
		for(int i = 0; i < 2; ++i)
			EDOPRO_CHECK_EQ(decoder.decode(card_hint_packet({0, Zone::MonsterZone, 0, false, 0}, 6,
				225, compat), state).status, DecodeStatus::Decoded);
		EDOPRO_CHECK_EQ(state.find(id)->description_hints.at(225), std::uint32_t{2});
		EDOPRO_CHECK_EQ(decoder.decode(card_hint_packet({0, Zone::MonsterZone, 0, false, 0}, 7,
			225, compat), state).status, DecodeStatus::Decoded);
		EDOPRO_CHECK_EQ(state.find(id)->description_hints.at(225), std::uint32_t{1});
	}
}

EDOPRO_TEST(player_hints_are_protocol_absolute_and_reset_by_start) {
	auto fixture = started(0, 0);
	EDOPRO_CHECK_EQ(fixture.run(player_hint_packet(1, 6, 0x123456789ull)).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(fixture.state.player_description_hints(1).at(0x123456789ull),
		std::uint32_t{1});
	EDOPRO_CHECK_EQ(fixture.run(player_hint_packet(1, 7, 0x123456789ull)).status,
		DecodeStatus::Decoded);
	fixture.expect_decoded(start_packet(0, 0));
	EDOPRO_CHECK(fixture.state.player_description_hints(1).empty());

	ProtocolDecoder compat{ProtocolVariant{true}};
	DuelState compat_state;
	EDOPRO_CHECK_EQ(compat.decode(variant_start_packet(true, 0, 0), compat_state).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(compat.decode(player_hint_packet(0, 6, 0x12345678u, true), compat_state).status,
		DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(compat_state.player_description_hints(0).at(0x12345678u),
		std::uint32_t{1});
}

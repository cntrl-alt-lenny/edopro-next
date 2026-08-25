// The decoder: payload layouts, the four ways a packet can be refused, and
// the state each supported message produces.
#include "packet_builder.h"
#include "test_support.h"

#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/protocol_decoder.h"

#include <algorithm>

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

Packet draw_packet(std::uint8_t player, const std::vector<std::uint32_t>& codes) {
	PayloadBuilder builder;
	builder.u8(player).u32(static_cast<std::uint32_t>(codes.size()));
	for(const auto code : codes)
		builder.u32(code).u32(proto::POS_FACEDOWN_DEFENSE);
	return builder.packet(proto::MSG_DRAW);
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

EDOPRO_TEST(known_but_undecoded_ids_are_unsupported_not_malformed) {
	// The distinction matters: MSG_UPDATE_DATA payloads are perfectly well
	// formed, we simply do not read them yet, and a coverage report that
	// called them malformed would be a lie.
	Fixture fixture;
	const auto result = fixture.run(Packet{proto::MSG_UPDATE_DATA, {0, 1, 2, 3}});
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(!ProtocolDecoder::supports(proto::MSG_UPDATE_DATA));
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

// Hidden information is a first-class state, not a gap to be filled in.
//
// These tests pin the rule that decides most of the design: the model records
// what the client has been told, and never more. A card whose passcode has not
// been stated is unknown; a card whose passcode is taken away again becomes
// unknown; and nothing anywhere reconstructs an identity from somewhere else.
#include "packet_builder.h"
#include "test_support.h"

#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/protocol_decoder.h"

using namespace edopro_next::client;
using edopro_next::testing::PayloadBuilder;
namespace proto = edopro_next::client::protocol;

namespace {

struct Duel {
	ProtocolDecoder decoder;
	DuelState state;

	DecodeResult run(const Packet& packet) { return decoder.decode(packet, state); }

	void must_decode(const Packet& packet) {
		const auto result = run(packet);
		EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
		if(result.status != DecodeStatus::Decoded)
			edopro_next::testing::report_failure(__FILE__, __LINE__, result.detail);
	}
};

Duel opened(std::uint16_t main = 5, std::uint16_t extra = 1) {
	Duel duel;
	duel.must_decode(PayloadBuilder()
						 .u8(0)
						 .u32(8000)
						 .u32(8000)
						 .u16(main)
						 .u16(extra)
						 .u16(main)
						 .u16(extra)
						 .packet(proto::MSG_START));
	return duel;
}

Packet draw(std::uint8_t player, const std::vector<std::uint32_t>& codes) {
	PayloadBuilder builder;
	builder.u8(player).u32(static_cast<std::uint32_t>(codes.size()));
	for(const auto code : codes)
		builder.u32(code).u32(proto::POS_FACEDOWN_DEFENSE);
	return builder.packet(proto::MSG_DRAW);
}

std::size_t count_events_of_type(const DecodeResult& result, bool revealed) {
	std::size_t count = 0;
	for(const auto& event : result.events) {
		if(revealed && std::holds_alternative<CardIdentityRevealed>(event))
			++count;
		if(!revealed && std::holds_alternative<CardIdentityConcealed>(event))
			++count;
	}
	return count;
}

} // namespace

EDOPRO_TEST(a_fresh_deck_is_entirely_unknown) {
	const auto duel = opened(40, 15);
	for(const auto& card : duel.state.cards())
		EDOPRO_CHECK(!card.identity_known());
}

EDOPRO_TEST(a_draw_the_client_may_not_see_stays_unknown) {
	// The server sends a passcode of 0 for a card the recipient is not
	// entitled to see. The card still exists, is still tracked, and still has
	// no identity.
	auto duel = opened(5, 0);
	const auto result = duel.run(draw(1, {0}));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	const auto id = duel.state.zone(1, Zone::Hand).front();
	EDOPRO_CHECK(!duel.state.find(id)->identity_known());
	// Nothing changed, so no identity event was invented for it.
	EDOPRO_CHECK_EQ(count_events_of_type(result, true), std::size_t{0});
	EDOPRO_CHECK_EQ(count_events_of_type(result, false), std::size_t{0});
}

EDOPRO_TEST(identity_can_arrive_later) {
	auto duel = opened(5, 0);
	duel.must_decode(draw(0, {0}));
	const auto id = duel.state.zone(0, Zone::Hand).front();
	EDOPRO_CHECK(!duel.state.find(id)->identity_known());

	// A hand shuffle restates every card in hand; this one names it.
	const auto result = duel.run(
		PayloadBuilder().u8(0).u32(1).u32(5555).packet(proto::MSG_SHUFFLE_HAND));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK_EQ(duel.state.find(id)->code, static_cast<CardCode>(5555u));
	EDOPRO_CHECK_EQ(count_events_of_type(result, true), std::size_t{1});
}

EDOPRO_TEST(shuffling_a_deck_takes_identity_away_again) {
	auto duel = opened(3, 0);
	// Reveal the top card by moving it to the graveyard and back, which is the
	// only way this slice can name a deck card.
	const auto deck = duel.state.zone(0, Zone::Deck);
	const auto top = deck.back();
	duel.must_decode(PayloadBuilder()
						 .u32(1234)
						 .loc(0, proto::LOCATION_DECK, 2, proto::POS_FACEDOWN)
						 .loc(0, proto::LOCATION_DECK, 2, proto::POS_FACEUP)
						 .u32(proto::REASON_EFFECT)
						 .packet(proto::MSG_MOVE));
	EDOPRO_CHECK(duel.state.find(top)->identity_known());

	const auto result = duel.run(PayloadBuilder().u8(0).packet(proto::MSG_SHUFFLE_DECK));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK(!duel.state.find(top)->identity_known());
	EDOPRO_CHECK_EQ(count_events_of_type(result, false), std::size_t{1});
	// The instances survive; only the knowledge is gone.
	EDOPRO_CHECK_EQ(duel.state.zone(0, Zone::Deck).size(), std::size_t{3});
}

EDOPRO_TEST(a_move_that_states_no_code_does_not_erase_what_is_known) {
	auto duel = opened(5, 0);
	duel.must_decode(draw(0, {1111}));
	const auto id = duel.state.zone(0, Zone::Hand).front();

	// Moving with code 0 means "not stated here", not "forget it" - except
	// into the extra deck, which is genuinely a loss of visibility.
	duel.must_decode(PayloadBuilder()
						 .u32(0)
						 .loc(0, proto::LOCATION_HAND, 0, proto::POS_FACEDOWN)
						 .loc(0, proto::LOCATION_GRAVE, 0, proto::POS_FACEUP)
						 .u32(proto::REASON_DISCARD)
						 .packet(proto::MSG_MOVE));
	EDOPRO_CHECK_EQ(duel.state.find(id)->code, static_cast<CardCode>(1111u));
}

EDOPRO_TEST(returning_to_the_extra_deck_without_a_code_conceals) {
	auto duel = opened(5, 0);
	duel.must_decode(draw(0, {1111}));
	const auto id = duel.state.zone(0, Zone::Hand).front();

	const auto result = duel.run(PayloadBuilder()
									 .u32(0)
									 .loc(0, proto::LOCATION_HAND, 0, proto::POS_FACEDOWN)
									 .loc(0, proto::LOCATION_EXTRA, 0,
										  proto::POS_FACEDOWN_DEFENSE)
									 .u32(proto::REASON_RETURN)
									 .packet(proto::MSG_MOVE));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	EDOPRO_CHECK(!duel.state.find(id)->identity_known());
	EDOPRO_CHECK_EQ(count_events_of_type(result, false), std::size_t{1});
}

EDOPRO_TEST(a_face_down_card_can_be_known_and_a_face_up_card_unknown) {
	// Position and identity are independent. A replay stream is unfiltered, so
	// a face-down card may well have a known passcode; a live opponent's
	// face-up card is normally named, but nothing here requires it.
	auto duel = opened(0, 0);
	duel.must_decode(PayloadBuilder()
						 .u32(2222)
						 .loc(0, 0, 0, 0)
						 .loc(0, proto::LOCATION_SZONE, 0, proto::POS_FACEDOWN_DEFENSE)
						 .u32(0)
						 .packet(proto::MSG_MOVE));
	const auto set = duel.state.at(CardLocation{0, Zone::SpellZone, 0, false, 0});
	EDOPRO_CHECK(duel.state.find(set)->identity_known());
	EDOPRO_CHECK(duel.state.find(set)->position.face_down());

	duel.must_decode(PayloadBuilder()
						 .u32(0)
						 .loc(1, 0, 0, 0)
						 .loc(1, proto::LOCATION_MZONE, 0, proto::POS_FACEUP_ATTACK)
						 .u32(0)
						 .packet(proto::MSG_MOVE));
	const auto open = duel.state.at(CardLocation{1, Zone::MonsterZone, 0, false, 0});
	EDOPRO_CHECK(!duel.state.find(open)->identity_known());
	EDOPRO_CHECK(duel.state.find(open)->position.face_up());
}

EDOPRO_TEST(a_hand_shuffle_that_does_not_match_the_hand_is_refused) {
	auto duel = opened(5, 0);
	duel.must_decode(draw(0, {1111, 2222}));
	const auto before = duel.state.find(duel.state.zone(0, Zone::Hand).front())->code;

	const auto result =
		duel.run(PayloadBuilder().u8(0).u32(3).u32(1).u32(2).u32(3).packet(
			proto::MSG_SHUFFLE_HAND));
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK_EQ(duel.state.find(duel.state.zone(0, Zone::Hand).front())->code, before);
}

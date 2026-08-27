// The all-or-nothing guarantee: a refused packet must never leave DuelState
// half-mutated, no matter how far a handler got before it discovered it had
// to refuse.
//
// This was found to be FALSE for MSG_MOVE, MSG_POS_CHANGE, MSG_DRAW and
// MSG_BATTLE prior to this suite existing: each applies an identity, position
// or combat-stat change before a later step in the same handler that can
// still fail. ProtocolDecoder::decode() now decodes against a private working
// copy of the state and commits it back only on DecodeStatus::Decoded (see
// its implementation and the comment there), which makes the guarantee hold
// centrally rather than depending on every handler getting its own internal
// ordering right. These tests exercise that guarantee end to end, using
// DuelState's whole-value `operator==` rather than checking a few fields, so
// a mutation anywhere in the model - not just the field the reviewer happened
// to name - would be caught.
#include "packet_builder.h"
#include "test_support.h"

#include "edopro_next/client/protocol_constants.h"
#include "edopro_next/client/protocol_decoder.h"

using namespace edopro_next::client;
using edopro_next::testing::PayloadBuilder;
namespace proto = edopro_next::client::protocol;

namespace {

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

DuelState started(std::uint16_t main = 5, std::uint16_t extra = 1) {
	DuelState state;
	ProtocolDecoder decoder;
	const auto result = decoder.decode(start_packet(main, extra), state);
	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Decoded);
	return state;
}

CardLocation loc(PlayerId player, Zone zone, std::uint32_t sequence) {
	return CardLocation{player, zone, sequence, false, 0};
}

// Directly reaches past the public API to force a card into a state no
// legitimate sequence of packets produces: tracked, but already gone as far
// as move_card()/remove_card() are concerned. The same technique the existing
// suite already uses (test_duel_state.cpp,
// invariant_check_notices_a_corrupted_model) to provoke an internal-integrity
// failure on demand, here used to make a *later* step of a handler fail after
// an *earlier* step already mutated something - the exact shape of bug this
// file exists to rule out.
void force_untracked_in_place(DuelState& state, CardInstanceId id) {
	auto* card = state.find(id);
	EDOPRO_CHECK(card != nullptr);
	if(card != nullptr)
		card->tracked = false;
}

} // namespace

EDOPRO_TEST(whole_state_equality_actually_detects_a_difference) {
	// Guards the guard: every test below concludes "state == snapshot", so
	// operator== had better be capable of returning false for a real change.
	auto a = started(5, 0);
	auto b = a;
	EDOPRO_CHECK(a == b);

	ProtocolDecoder decoder;
	const auto moved = decoder.decode(
		PayloadBuilder()
			.u32(0)
			.loc(0, 0, 0, 0)
			.loc(0, proto::LOCATION_MZONE, 0, proto::POS_FACEUP_ATTACK)
			.u32(0)
			.packet(proto::MSG_MOVE),
		a);
	EDOPRO_CHECK_EQ(moved.status, DecodeStatus::Decoded);
	EDOPRO_CHECK(!(a == b));
}

EDOPRO_TEST(malformed_supported_packet_leaves_state_untouched) {
	auto state = started();
	const auto snapshot = state;

	ProtocolDecoder decoder;
	const auto result = decoder.decode(Packet{proto::MSG_NEW_TURN, {}}, state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Malformed);
	EDOPRO_CHECK(state == snapshot);
}

EDOPRO_TEST(unknown_message_leaves_state_untouched) {
	auto state = started();
	const auto snapshot = state;

	ProtocolDecoder decoder;
	// 9 falls in a gap of upstream's numbering (8 = MSG_REQUEST_DECK,
	// 10 = MSG_SELECT_BATTLECMD).
	const auto result = decoder.decode(Packet{9, {1, 2, 3}}, state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::UnknownMessage);
	EDOPRO_CHECK(state == snapshot);
}

EDOPRO_TEST(unsupported_message_leaves_state_untouched) {
	// Directly answers the question the MSG_POS_CHANGE strictness review
	// raised: does the undecoded query stream touch our model at all today?
	// No - decode() returns before a Decoding is even constructed for an
	// unsupported id, so there is no path through which it could.
	auto state = started();
	const auto snapshot = state;

	ProtocolDecoder decoder;
	const auto result =
		decoder.decode(Packet{proto::MSG_UPDATE_DATA, {0, 1, 2, 3, 4, 5, 6, 7}}, state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::UnsupportedMessage);
	EDOPRO_CHECK(state == snapshot);
}

EDOPRO_TEST(move_to_an_occupied_slot_rolls_back_the_identity_change) {
	// The review's own example: MSG_MOVE carries a non-zero code, which
	// apply_code() applies, before move_card() discovers the destination is
	// occupied and refuses. Prior to the trial-state fix, the mover's code
	// change survived a packet reported Inconsistent.
	auto state = started(0, 0);
	ProtocolDecoder decoder;

	CardInstanceId blocker = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card(loc(0, Zone::MonsterZone, 1), static_cast<CardCode>(1u),
									CardPosition{proto::POS_FACEUP_ATTACK}, &blocker)
					  .has_value());
	CardInstanceId mover = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card(loc(0, Zone::Hand, 0), static_cast<CardCode>(2u),
									CardPosition{proto::POS_FACEDOWN}, &mover)
					  .has_value());

	const auto snapshot = state;

	const auto result = decoder.decode(
		PayloadBuilder()
			.u32(9999) // a new, different code - this is the mutation under test
			.loc(0, proto::LOCATION_HAND, 0, proto::POS_FACEDOWN)
			.loc(0, proto::LOCATION_MZONE, 1, proto::POS_FACEUP_ATTACK)
			.u32(proto::REASON_SUMMON)
			.packet(proto::MSG_MOVE),
		state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(state == snapshot);
	// Named explicitly, so a failure here points straight at the identity
	// leak rather than requiring a diff of the whole-state assertion above.
	EDOPRO_CHECK_EQ(state.find(mover)->code, static_cast<CardCode>(2u));
	EDOPRO_CHECK_EQ(state.at(loc(0, Zone::MonsterZone, 1)), blocker);
}

EDOPRO_TEST(move_leaving_play_rolls_back_the_identity_change_when_removal_fails) {
	// The review's other MSG_MOVE example: leaving-play code applied via
	// apply_code(), then remove_card() fails. remove_card() cannot fail
	// through any legitimate sequence of packets (a card reachable via at()
	// is always in a state remove_card() can detach), so this forces the
	// specific internal guard - "already left play" - directly, the same way
	// test_duel_state.cpp already forces internal-integrity failures for its
	// own tests.
	auto state = started(0, 0);
	ProtocolDecoder decoder;

	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card(loc(0, Zone::MonsterZone, 0), static_cast<CardCode>(1u),
									CardPosition{proto::POS_FACEUP_ATTACK}, &id)
					  .has_value());
	force_untracked_in_place(state, id);
	const auto snapshot = state;

	const auto result = decoder.decode(
		PayloadBuilder()
			.u32(4242) // new code - the mutation under test
			.loc(0, proto::LOCATION_MZONE, 0, proto::POS_FACEUP_ATTACK)
			.loc(0, 0, 0, 0)
			.u32(proto::REASON_DESTROY)
			.packet(proto::MSG_MOVE),
		state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(state == snapshot);
	EDOPRO_CHECK_EQ(state.find(id)->code, static_cast<CardCode>(1u));
}

EDOPRO_TEST(pos_change_rolls_back_the_identity_change_when_the_flip_is_refused) {
	// A previously-unclaimed instance of the same bug class, found while
	// auditing the transactional claim: MSG_POS_CHANGE applies a non-zero
	// code before set_position(), and set_position() refuses a face-up/
	// face-down flip in the extra deck (that split is tracked by a counter
	// DuelState does not know how to renumber from here).
	auto state = started(0, 0);
	ProtocolDecoder decoder;

	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card(loc(0, Zone::ExtraDeck, 0), static_cast<CardCode>(1u),
									CardPosition{proto::POS_FACEDOWN_DEFENSE}, &id)
					  .has_value());
	const auto snapshot = state;

	const auto result = decoder.decode(
		PayloadBuilder()
			.u32(5555) // new code - the mutation under test
			.u8(0)
			.u8(proto::LOCATION_EXTRA)
			.u8(0)
			.u8(proto::POS_FACEDOWN_DEFENSE) // stated previous position: agrees with the model
			.u8(proto::POS_FACEUP_ATTACK)	  // new position: flips face-up, which is refused
			.packet(proto::MSG_POS_CHANGE),
		state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(state == snapshot);
	EDOPRO_CHECK_EQ(state.find(id)->code, static_cast<CardCode>(1u));
}

EDOPRO_TEST(battle_rolls_back_attacker_stats_when_the_defender_cannot_be_found) {
	// The review's MSG_BATTLE example: the attacker's ATK/DEF is written
	// before the defender is looked up, and the defender lookup can fail.
	auto state = started(0, 0);
	ProtocolDecoder decoder;

	CardInstanceId attacker = CardInstanceId::None;
	EDOPRO_CHECK(!state.create_card(loc(0, Zone::MonsterZone, 0), static_cast<CardCode>(1u),
									CardPosition{proto::POS_FACEUP_ATTACK}, &attacker)
					  .has_value());
	const auto snapshot = state;
	EDOPRO_CHECK(!state.find(attacker)->attack.has_value());

	const auto result = decoder.decode(
		PayloadBuilder()
			.loc(0, proto::LOCATION_MZONE, 0, 0)
			.u32(1800) // attacker ATK - the mutation under test
			.u32(1000) // attacker DEF - the mutation under test
			.u8(0)
			.loc(0, proto::LOCATION_MZONE, 3, 0) // empty slot: defender lookup fails
			.u32(0)
			.u32(0)
			.u8(0)
			.packet(proto::MSG_BATTLE),
		state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(state == snapshot);
	EDOPRO_CHECK(!state.find(attacker)->attack.has_value());
	EDOPRO_CHECK(!state.find(attacker)->defense.has_value());
}

EDOPRO_TEST(draw_rolls_back_the_entire_batch_when_one_card_cannot_be_moved) {
	// The most severe instance found: MSG_DRAW applies every drawn card's
	// identity change in one loop, then moves every drawn card in a second
	// loop. If the second card's move fails, a per-handler fix would still
	// have let the FIRST card's identity change *and* its successful move
	// through, because that mutation happened in an earlier, already-completed
	// step of the very same packet. Only a whole-packet, whole-state rollback
	// - which is what the trial-state design gives - catches this. Card B's
	// move is forced to fail via the same "already left play" technique used
	// above, while card A's move would succeed perfectly well on its own.
	auto state = started(2, 0);
	ProtocolDecoder decoder;

	const auto deck = state.zone(0, Zone::Deck);
	EDOPRO_CHECK_EQ(deck.size(), std::size_t{2});
	const auto card_a = deck[1]; // drawn first (top of deck = back of the pile)
	const auto card_b = deck[0]; // drawn second
	force_untracked_in_place(state, card_b);
	const auto snapshot = state;

	const auto result = decoder.decode(
		PayloadBuilder()
			.u8(0)
			.u32(2)
			.u32(1111) // card A's new code - would-be mutation #1
			.u32(proto::POS_FACEDOWN_DEFENSE)
			.u32(2222) // card B's new code - would-be mutation #2
			.u32(proto::POS_FACEDOWN_DEFENSE)
			.packet(proto::MSG_DRAW),
		state);

	EDOPRO_CHECK_EQ(result.status, DecodeStatus::Inconsistent);
	EDOPRO_CHECK(state == snapshot);
	// Named explicitly: card A's identity and location must be exactly as
	// they were, even though A's own move would have succeeded in isolation.
	EDOPRO_CHECK(!state.find(card_a)->identity_known());
	EDOPRO_CHECK_EQ(state.find(card_a)->location.zone, Zone::Deck);
	EDOPRO_CHECK(!state.find(card_b)->identity_known());
}

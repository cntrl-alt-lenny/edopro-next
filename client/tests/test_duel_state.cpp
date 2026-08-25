// DuelState in isolation: zone bookkeeping, identity, and the invariants the
// model promises to hold. Nothing here goes through the decoder.
#include "test_support.h"

#include "edopro_next/client/duel_state.h"
#include "edopro_next/client/protocol_constants.h"

using namespace edopro_next::client;
namespace proto = edopro_next::client::protocol;

namespace {

constexpr CardPosition kFaceDown{proto::POS_FACEDOWN_DEFENSE};
constexpr CardPosition kFaceUpAttack{proto::POS_FACEUP_ATTACK};

DuelState started_duel(std::uint16_t main = 5, std::uint16_t extra = 2) {
	DuelState state;
	state.start(0, {8000, 8000}, {DeckSizes{main, extra}, DeckSizes{main, extra}});
	return state;
}

CardLocation loc(PlayerId player, Zone zone, std::uint32_t sequence) {
	return CardLocation{player, zone, sequence, false, 0};
}

CardInstanceId create(DuelState& state, const CardLocation& where, CardPosition position) {
	CardInstanceId id = CardInstanceId::None;
	const auto error = state.create_card(where, CardCode::None, position, &id);
	EDOPRO_CHECK(!error.has_value());
	return id;
}

} // namespace

EDOPRO_TEST(default_state_has_addressable_field_zones) {
	const DuelState state;
	EDOPRO_CHECK_EQ(state.zone(0, Zone::MonsterZone).size(), std::size_t{7});
	EDOPRO_CHECK_EQ(state.zone(0, Zone::SpellZone).size(), std::size_t{8});
	EDOPRO_CHECK_EQ(state.zone(1, Zone::MonsterZone).size(), std::size_t{7});
	EDOPRO_CHECK(state.check_invariants().empty());
	EDOPRO_CHECK(!state.started());
}

EDOPRO_TEST(start_fills_both_decks_with_unknown_cards) {
	const auto state = started_duel(40, 15);
	EDOPRO_CHECK(state.started());
	EDOPRO_CHECK_EQ(state.zone(0, Zone::Deck).size(), std::size_t{40});
	EDOPRO_CHECK_EQ(state.zone(1, Zone::ExtraDeck).size(), std::size_t{15});
	EDOPRO_CHECK_EQ(state.cards().size(), std::size_t{110});
	for(const auto& card : state.cards()) {
		EDOPRO_CHECK(!card.identity_known());
		EDOPRO_CHECK(card.tracked);
	}
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(start_does_not_guess_the_turn_player) {
	// MSG_START's first byte describes the recipient, not turn order, so the
	// model must not pretend to know whose turn it is before MSG_NEW_TURN.
	const auto state = started_duel();
	EDOPRO_CHECK_EQ(state.turn(), std::uint32_t{0});
	EDOPRO_CHECK_EQ(state.phase(), Phase::Unknown);
}

EDOPRO_TEST(deck_sequence_zero_inserts_on_top) {
	// ClientField::AddCard treats deck sequence 0 as the top of the deck and
	// anything else as the bottom. Reproduced here, quirk included.
	auto state = started_duel(3, 0);
	const auto deck = state.zone(0, Zone::Deck);
	const auto top = deck.back();

	const auto moved = create(state, loc(0, Zone::Graveyard, 0), kFaceUpAttack);
	EDOPRO_CHECK(!state.move_card(moved, loc(0, Zone::Deck, 0), kFaceDown).has_value());
	EDOPRO_CHECK_EQ(state.zone(0, Zone::Deck).front(), moved);
	EDOPRO_CHECK_EQ(state.find(moved)->location.sequence, std::uint32_t{0});
	// The card that used to be at index 0 has shifted up.
	EDOPRO_CHECK_EQ(state.find(deck.front())->location.sequence, std::uint32_t{1});
	EDOPRO_CHECK_EQ(state.zone(0, Zone::Deck).back(), top);
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(piles_renumber_on_removal) {
	auto state = started_duel(0, 0);
	const auto a = create(state, loc(0, Zone::Hand, 0), kFaceDown);
	const auto b = create(state, loc(0, Zone::Hand, 0), kFaceDown);
	const auto c = create(state, loc(0, Zone::Hand, 0), kFaceDown);
	EDOPRO_CHECK_EQ(state.find(c)->location.sequence, std::uint32_t{2});

	EDOPRO_CHECK(!state.move_card(a, loc(0, Zone::Graveyard, 0), kFaceUpAttack).has_value());
	EDOPRO_CHECK_EQ(state.zone(0, Zone::Hand).size(), std::size_t{2});
	EDOPRO_CHECK_EQ(state.find(b)->location.sequence, std::uint32_t{0});
	EDOPRO_CHECK_EQ(state.find(c)->location.sequence, std::uint32_t{1});
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(extra_deck_keeps_face_up_cards_at_the_back) {
	auto state = started_duel(0, 0);
	const auto down_a = create(state, loc(0, Zone::ExtraDeck, 0), kFaceDown);
	const auto up = create(state, loc(0, Zone::ExtraDeck, 0), kFaceUpAttack);
	const auto down_b = create(state, loc(0, Zone::ExtraDeck, 0), kFaceDown);

	const auto& extra = state.zone(0, Zone::ExtraDeck);
	EDOPRO_CHECK_EQ(extra.size(), std::size_t{3});
	EDOPRO_CHECK_EQ(extra[0], down_a);
	EDOPRO_CHECK_EQ(extra[1], down_b);
	EDOPRO_CHECK_EQ(extra[2], up);
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(field_slots_are_addressable_and_exclusive) {
	auto state = started_duel(0, 0);
	const auto first = create(state, loc(0, Zone::MonsterZone, 3), kFaceUpAttack);
	EDOPRO_CHECK_EQ(state.at(loc(0, Zone::MonsterZone, 3)), first);
	EDOPRO_CHECK_EQ(state.at(loc(0, Zone::MonsterZone, 2)), CardInstanceId::None);

	CardInstanceId clash = CardInstanceId::None;
	const auto error = state.create_card(loc(0, Zone::MonsterZone, 3), CardCode::None,
										 kFaceUpAttack, &clash);
	EDOPRO_CHECK(error.has_value());
	EDOPRO_CHECK_EQ(clash, CardInstanceId::None);
	// A refused creation must not have allocated an instance.
	EDOPRO_CHECK_EQ(state.cards().size(), std::size_t{1});
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(refused_move_changes_nothing) {
	auto state = started_duel(0, 0);
	const auto blocker = create(state, loc(0, Zone::MonsterZone, 1), kFaceUpAttack);
	const auto mover = create(state, loc(0, Zone::Hand, 0), kFaceDown);

	const auto error = state.move_card(mover, loc(0, Zone::MonsterZone, 1), kFaceUpAttack);
	EDOPRO_CHECK(error.has_value());
	EDOPRO_CHECK_EQ(state.find(mover)->location, loc(0, Zone::Hand, 0));
	EDOPRO_CHECK_EQ(state.at(loc(0, Zone::MonsterZone, 1)), blocker);
	EDOPRO_CHECK_EQ(state.zone(0, Zone::Hand).size(), std::size_t{1});
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(out_of_range_field_sequence_is_refused) {
	auto state = started_duel(0, 0);
	CardInstanceId id = CardInstanceId::None;
	EDOPRO_CHECK(state.create_card(loc(0, Zone::MonsterZone, 9), CardCode::None, kFaceUpAttack,
								   &id)
					 .has_value());
	EDOPRO_CHECK(state.create_card(loc(0, Zone::Unknown, 0), CardCode::None, kFaceUpAttack, &id)
					 .has_value());
	EDOPRO_CHECK(state.create_card(loc(3, Zone::Hand, 0), CardCode::None, kFaceUpAttack, &id)
					 .has_value());
	EDOPRO_CHECK_EQ(state.cards().size(), std::size_t{0});
}

EDOPRO_TEST(material_attaches_and_reindexes) {
	auto state = started_duel(0, 0);
	const auto host = create(state, loc(0, Zone::MonsterZone, 0), kFaceUpAttack);
	const auto a = create(state, loc(0, Zone::Hand, 0), kFaceDown);
	const auto b = create(state, loc(0, Zone::Hand, 0), kFaceDown);

	const CardLocation attach{0, Zone::MonsterZone, 0, true, 0};
	EDOPRO_CHECK(!state.move_card(a, attach, std::nullopt).has_value());
	EDOPRO_CHECK(!state.move_card(b, attach, std::nullopt).has_value());

	EDOPRO_CHECK_EQ(state.find(host)->materials.size(), std::size_t{2});
	EDOPRO_CHECK_EQ(state.find(a)->location.overlay_index, std::uint32_t{0});
	EDOPRO_CHECK_EQ(state.find(b)->location.overlay_index, std::uint32_t{1});
	EDOPRO_CHECK_EQ(state.at(CardLocation{0, Zone::MonsterZone, 0, true, 1}), b);
	EDOPRO_CHECK(state.check_invariants().empty());

	// Detaching the first must renumber the second.
	EDOPRO_CHECK(!state.move_card(a, loc(0, Zone::Graveyard, 0), kFaceUpAttack).has_value());
	EDOPRO_CHECK_EQ(state.find(host)->materials.size(), std::size_t{1});
	EDOPRO_CHECK_EQ(state.find(b)->location.overlay_index, std::uint32_t{0});
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(removing_a_host_untracks_its_material) {
	auto state = started_duel(0, 0);
	const auto host = create(state, loc(0, Zone::MonsterZone, 0), kFaceUpAttack);
	const auto material = create(state, loc(0, Zone::Hand, 0), kFaceDown);
	EDOPRO_CHECK(!state.move_card(material, CardLocation{0, Zone::MonsterZone, 0, true, 0},
								  std::nullopt)
					  .has_value());

	EDOPRO_CHECK(!state.remove_card(host).has_value());
	EDOPRO_CHECK(!state.find(host)->tracked);
	EDOPRO_CHECK(!state.find(material)->tracked);
	EDOPRO_CHECK_EQ(state.at(loc(0, Zone::MonsterZone, 0)), CardInstanceId::None);
	EDOPRO_CHECK(state.check_invariants().empty());

	// A card that has left play cannot be moved again.
	EDOPRO_CHECK(state.move_card(host, loc(0, Zone::Graveyard, 0), kFaceUpAttack).has_value());
}

EDOPRO_TEST(life_points_clamp_at_zero) {
	auto state = started_duel(0, 0);
	EDOPRO_CHECK_EQ(state.set_life_points(0, 8000 - 9000), std::int64_t{0});
	EDOPRO_CHECK_EQ(state.life_points(0), std::int64_t{0});
	EDOPRO_CHECK_EQ(state.set_life_points(1, 12000), std::int64_t{12000});
	// Non-duelist indices are refused rather than writing out of bounds.
	EDOPRO_CHECK_EQ(state.set_life_points(proto::PLAYER_NONE, 500), std::int64_t{0});
}

EDOPRO_TEST(chain_links_must_arrive_in_order) {
	auto state = started_duel(0, 0);
	ChainLink first;
	first.link = 1;
	EDOPRO_CHECK(!state.push_chain_link(first).has_value());

	ChainLink skipped;
	skipped.link = 3;
	EDOPRO_CHECK(state.push_chain_link(skipped).has_value());
	EDOPRO_CHECK_EQ(state.chain().size(), std::size_t{1});

	ChainLink second;
	second.link = 2;
	EDOPRO_CHECK(!state.push_chain_link(second).has_value());
	EDOPRO_CHECK(!state.mark_chain_resolving(2).has_value());
	EDOPRO_CHECK(state.chain()[1].resolving);
	EDOPRO_CHECK(!state.mark_chain_resolved(2).has_value());
	EDOPRO_CHECK(state.chain()[1].resolved);
	EDOPRO_CHECK(!state.chain()[1].resolving);

	EDOPRO_CHECK(state.mark_chain_resolving(0).has_value());
	EDOPRO_CHECK(state.mark_chain_resolved(7).has_value());

	state.clear_chain();
	EDOPRO_CHECK(state.chain().empty());
	EDOPRO_CHECK(state.check_invariants().empty());
}

EDOPRO_TEST(begin_turn_counts_from_one) {
	auto state = started_duel(0, 0);
	state.begin_turn(0);
	EDOPRO_CHECK_EQ(state.turn(), std::uint32_t{1});
	EDOPRO_CHECK_EQ(state.turn_player(), PlayerId{0});
	state.begin_turn(1);
	EDOPRO_CHECK_EQ(state.turn(), std::uint32_t{2});
	EDOPRO_CHECK_EQ(state.turn_player(), PlayerId{1});
}

EDOPRO_TEST(invariant_check_notices_a_corrupted_model) {
	// Guards the guard: if check_invariants() cannot fail, its silence in the
	// other tests means nothing.
	auto state = started_duel(2, 0);
	const auto id = state.zone(0, Zone::Deck).front();
	const_cast<CardState*>(state.find(id))->location.sequence = 5;
	EDOPRO_CHECK(!state.check_invariants().empty());
}

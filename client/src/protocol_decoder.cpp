#include "edopro_next/client/protocol_decoder.h"

#include "edopro_next/client/protocol_constants.h"

#include <algorithm>
#include <array>

namespace edopro_next::client {
namespace {

namespace msg = protocol;

// Message ids this build decodes. The switch in decode_supported() must handle
// exactly these; a test asserts it, so the coverage report can never claim
// support this file does not actually provide.
constexpr std::array<std::uint8_t, 27> kSupported = {
	msg::MSG_START,
	msg::MSG_WIN,
	msg::MSG_SHUFFLE_DECK,
	msg::MSG_SHUFFLE_HAND,
	msg::MSG_NEW_TURN,
	msg::MSG_NEW_PHASE,
	msg::MSG_MOVE,
	msg::MSG_POS_CHANGE,
	msg::MSG_SET,
	msg::MSG_SUMMONING,
	msg::MSG_SUMMONED,
	msg::MSG_SPSUMMONING,
	msg::MSG_SPSUMMONED,
	msg::MSG_CHAINING,
	msg::MSG_CHAINED,
	msg::MSG_CHAIN_SOLVING,
	msg::MSG_CHAIN_SOLVED,
	msg::MSG_CHAIN_END,
	msg::MSG_DRAW,
	msg::MSG_DAMAGE,
	msg::MSG_RECOVER,
	msg::MSG_LPUPDATE,
	msg::MSG_PAY_LPCOST,
	msg::MSG_ATTACK,
	msg::MSG_BATTLE,
	msg::MSG_DAMAGE_STEP_START,
	msg::MSG_DAMAGE_STEP_END,
};

// The protocol's loc_info, read verbatim. Its four fields do not mean the same
// thing in every context: when LOCATION_OVERLAY is set, `position` is the index
// within the host card's material pile, not a card position. Keeping the raw
// form separate from CardLocation is what makes that reinterpretation explicit
// rather than a lurking bug.
struct LocInfo {
	std::uint8_t controller = 0;
	std::uint32_t location = 0;
	std::uint32_t sequence = 0;
	std::uint32_t position = 0;

	bool overlay() const { return (location & msg::LOCATION_OVERLAY) != 0; }
	bool nowhere() const { return location == 0; }
};

LocInfo read_loc_info(PacketReader& reader, bool compat) {
	LocInfo info;
	info.controller = reader.u8();
	info.location = reader.u8();
	info.sequence = reader.u8_or_u32(compat);
	info.position = reader.u8_or_u32(compat);
	return info;
}

CardLocation to_location(const LocInfo& info) {
	CardLocation location;
	location.controller = info.controller;
	location.zone = zone_from_protocol(info.location);
	location.sequence = info.sequence;
	location.overlay = info.overlay();
	location.overlay_index = info.overlay() ? info.position : 0;
	return location;
}

// Only meaningful when the loc_info is not an overlay reference.
CardPosition to_position(const LocInfo& info) {
	return CardPosition{static_cast<std::uint8_t>(info.position & 0xffu)};
}

CardCode to_code(std::uint32_t code) {
	return static_cast<CardCode>(code);
}

// Handlers read every field, then call this, then mutate. A payload that ran
// short, or that had bytes left over, therefore never produces a partial
// state change - it is reported as malformed instead.
bool fully_read(const PacketReader& reader) {
	return !reader.failed() && reader.exhausted();
}

// Guards a count read from the wire against the bytes actually present, so a
// corrupt length cannot drive a long loop before failing.
bool count_fits(PacketReader& reader, std::uint32_t count, std::size_t entry_size) {
	if(entry_size != 0 && count > reader.remaining() / entry_size) {
		reader.fail();
		return false;
	}
	return true;
}

class Decoding {
public:
	Decoding(const Packet& packet, DuelState& state, ProtocolVariant variant)
		: reader_(packet.payload), state_(state), compat_(variant.compat) {}

	PacketReader& reader() { return reader_; }
	DuelState& state() { return state_; }
	bool compat() const { return compat_; }

	void emit(DuelEvent event) { events_.push_back(std::move(event)); }
	std::vector<DuelEvent> take_events() { return std::move(events_); }

	// Records that the payload was understood but describes something the
	// current state cannot accommodate. The first such reason wins, and the
	// caller discards any events already queued.
	void inconsistent(std::string reason) {
		if(inconsistency_.empty())
			inconsistency_ = std::move(reason);
	}
	// Overloaded on a different name so that a call with a string literal
	// cannot be ambiguous between std::string and std::optional<std::string>.
	void inconsistent_if(const DuelState::Error& error) {
		if(error)
			inconsistent(*error);
	}
	bool is_inconsistent() const { return !inconsistency_.empty(); }
	const std::string& inconsistency() const { return inconsistency_; }

	// Applies a code the protocol just stated, emitting an event only when
	// the client's knowledge actually changed. A code of 0 means the client
	// is no longer being told what the card is.
	void apply_code(CardInstanceId id, CardCode code) {
		const auto* card = state_.find(id);
		if(card == nullptr || card->code == code)
			return;
		inconsistent_if(state_.set_code(id, code));
		if(is_known(code))
			emit(CardIdentityRevealed{id, code});
		else
			emit(CardIdentityConcealed{id});
	}

	bool require_duelist(PlayerId player) {
		if(is_duelist(player))
			return true;
		inconsistent("message names player " + player_name(player));
		return false;
	}

private:
	PacketReader reader_;
	DuelState& state_;
	bool compat_;
	std::vector<DuelEvent> events_;
	std::string inconsistency_;
};

// --- individual messages ------------------------------------------------
//
// Each handler follows the same shape: read every field, check fully_read(),
// then apply. Payload layouts are taken from the corresponding case in
// DuelClient::ClientAnalyze (gframe/duelclient.cpp) and were checked against
// the byte lengths in the committed replay fixtures.

void handle_start(Decoding& d) {
	auto& r = d.reader();
	const auto player_type = r.u8();
	if(d.compat())
		r.u8(); // duel_rule, read and discarded by the legacy client too
	std::array<std::int64_t, kPlayerCount> life{};
	life[0] = r.u32();
	life[1] = r.u32();
	std::array<DeckSizes, kPlayerCount> decks{};
	for(auto& deck : decks) {
		deck.main = r.u16();
		deck.extra = r.u16();
	}
	if(!fully_read(r))
		return;

	d.state().start(player_type, life, decks);
	d.emit(DuelStarted{player_type, life, decks});
}

void handle_win(Decoding& d) {
	auto& r = d.reader();
	const auto player = r.u8();
	const auto reason = r.u8();
	if(!fully_read(r))
		return;

	// The protocol uses PLAYER_NONE for a draw. Anything else that is not a
	// duelist is reported as "no winner" rather than guessed at.
	std::optional<PlayerId> winner;
	if(is_duelist(player))
		winner = player;
	d.state().finish(winner, reason);
	d.emit(DuelEnded{winner, reason});
}

void handle_shuffle_deck(Decoding& d) {
	auto& r = d.reader();
	const auto player = r.u8();
	if(!fully_read(r))
		return;
	if(!d.require_duelist(player))
		return;

	// Shuffling does not move anything the client can see. What it does is
	// destroy knowledge: every card in the deck becomes unidentified again.
	// The instance ids survive, but they no longer denote the same physical
	// cards - see docs/adr/0002-semantic-event-model.md.
	const auto deck = d.state().zone(player, Zone::Deck);
	d.emit(DeckShuffled{player, deck.size()});
	for(const auto id : deck)
		d.apply_code(id, CardCode::None);
}

void handle_shuffle_hand(Decoding& d) {
	auto& r = d.reader();
	const auto player = r.u8();
	const auto count = r.u8_or_u32(d.compat());
	if(!count_fits(r, count, sizeof(std::uint32_t)))
		return;
	std::vector<CardCode> codes;
	codes.reserve(count);
	for(std::uint32_t i = 0; i < count; ++i)
		codes.push_back(to_code(r.u32()));
	if(!fully_read(r))
		return;
	if(!d.require_duelist(player))
		return;

	const auto hand = d.state().zone(player, Zone::Hand);
	if(hand.size() != codes.size()) {
		d.inconsistent("hand shuffle lists " + std::to_string(codes.size()) +
					   " cards, hand holds " + std::to_string(hand.size()));
		return;
	}
	d.emit(HandShuffled{player, hand.size()});
	for(std::size_t i = 0; i < hand.size(); ++i)
		d.apply_code(hand[i], codes[i]);
}

void handle_new_turn(Decoding& d) {
	auto& r = d.reader();
	const auto player = r.u8();
	if(!fully_read(r))
		return;
	if(!d.require_duelist(player))
		return;

	d.state().begin_turn(player);
	d.emit(TurnStarted{d.state().turn(), player});
}

void handle_new_phase(Decoding& d) {
	auto& r = d.reader();
	const auto raw = r.u16();
	if(!fully_read(r))
		return;

	const auto phase = phase_from_protocol(raw);
	d.state().set_phase(phase);
	d.emit(PhaseChanged{phase});
}

void handle_move(Decoding& d) {
	auto& r = d.reader();
	const auto code = r.u32();
	const auto previous = read_loc_info(r, d.compat());
	const auto current = read_loc_info(r, d.compat());
	const auto reason = r.u32();
	if(!fully_read(r))
		return;

	const auto from = to_location(previous);
	const auto to = to_location(current);

	// A card appearing from nowhere: a token, or any card the client is only
	// now being told about.
	if(previous.nowhere()) {
		if(current.nowhere()) {
			d.inconsistent(std::string("move from nowhere to nowhere"));
			return;
		}
		CardInstanceId created = CardInstanceId::None;
		if(auto error = d.state().create_card(to, to_code(code),
											  current.overlay() ? CardPosition{}
																: to_position(current),
											  &created)) {
			d.inconsistent(*error);
			return;
		}
		const auto* card = d.state().find(created);
		d.emit(CardCreated{created, card->location, to_code(code)});
		return;
	}

	const auto id = d.state().at(from);
	if(id == CardInstanceId::None) {
		d.inconsistent("no card at " + to_string(from));
		return;
	}

	if(current.nowhere()) {
		// Leaving tracked play. The legacy client applies a non-zero code
		// first, which is how a card that was face-down can still be named as
		// it goes.
		if(code != 0)
			d.apply_code(id, to_code(code));
		if(auto error = d.state().remove_card(id)) {
			d.inconsistent(*error);
			return;
		}
		d.emit(CardRemoved{id, from, reason});
		return;
	}

	// Ordinary move. The rules for when a code is applied differ by whether
	// either end is a material pile, and are copied from the corresponding
	// branches of the MSG_MOVE handler in gframe/duelclient.cpp.
	if(!from.overlay && !to.overlay) {
		// Moving to the extra deck with code 0 is a genuine loss of
		// knowledge, so it is applied; elsewhere a zero code means "not
		// stated" and is ignored.
		if(code != 0 || to.zone == Zone::ExtraDeck)
			d.apply_code(id, to_code(code));
	} else if(!from.overlay) {
		if(code != 0)
			d.apply_code(id, to_code(code));
	}
	if(d.is_inconsistent())
		return;

	// Becoming material carries no position: the field is the material index.
	const std::optional<CardPosition> position =
		to.overlay ? std::nullopt : std::optional<CardPosition>{to_position(current)};
	if(auto error = d.state().move_card(id, to, position)) {
		d.inconsistent(*error);
		return;
	}
	d.emit(CardMoved{id, from, d.state().find(id)->location, reason});
}

void handle_pos_change(Decoding& d) {
	auto& r = d.reader();
	const auto code = r.u32();
	// Unlike loc_info, these three are single bytes in both protocol
	// revisions - see the MSG_POS_CHANGE case in gframe/duelclient.cpp.
	const auto controller = r.u8();
	const auto location = r.u8();
	const auto sequence = r.u8();
	const auto previous_position = CardPosition{r.u8()};
	const auto new_position = CardPosition{r.u8()};
	if(!fully_read(r))
		return;

	const CardLocation where{controller, zone_from_protocol(location), sequence, false, 0};
	const auto id = d.state().at(where);
	if(id == CardInstanceId::None) {
		d.inconsistent("no card at " + to_string(where));
		return;
	}

	// The message states the position it is changing *from*. If the model
	// disagrees, the model is wrong somewhere earlier, and saying so is worth
	// more than quietly overwriting it. This is stricter than upstream, which
	// applies the new position unconditionally (gframe/duelclient.cpp,
	// MSG_POS_CHANGE case) - a deliberate choice, not an oversight.
	//
	// The obvious worry: MSG_UPDATE_DATA/MSG_UPDATE_CARD are not decoded yet,
	// and legacy ClientCard::UpdateInfo does write `position` from a bare
	// QUERY_POSITION independently of MSG_POS_CHANGE (gframe/client_card.cpp).
	// If that ever let the *legacy* client's position drift ahead of what our
	// model can see, this check would misfire on a perfectly healthy stream.
	// Investigated and found not to hold for this slice: every real position
	// this model can observe arrives already through MSG_MOVE (which always
	// carries a destination position) or this very message: QUERY_POSITION
	// mirrors state already announced through one of those two decoded
	// channels rather than being an independent source of position changes.
	// Both committed fixtures corroborate this empirically - 773 and 799
	// MSG_UPDATE_DATA/CARD packets respectively, heavily interleaved with real
	// MSG_POS_CHANGE events, zero false-positive Inconsistent results in
	// either (tests/golden/*.semantic). Re-examine this the moment
	// MSG_UPDATE_DATA/MSG_UPDATE_CARD are actually decoded: this reasoning
	// covers the *current* 27-message slice, not a hypothetical future one.
	const auto held = d.state().find(id)->position;
	if(held != previous_position) {
		d.inconsistent("position change from " + to_string(previous_position) + " but " +
					   to_string(id) + " is " + to_string(held));
		return;
	}

	if(code != 0)
		d.apply_code(id, to_code(code));
	if(auto error = d.state().set_position(id, new_position)) {
		d.inconsistent(*error);
		return;
	}
	d.emit(PositionChanged{id, previous_position, new_position});
}

void handle_set(Decoding& d) {
	auto& r = d.reader();
	const auto code = r.u32();
	const auto info = read_loc_info(r, d.compat());
	if(!fully_read(r))
		return;

	// Announcement only. The card's actual placement arrives as its own
	// MSG_MOVE, which is where the state changes.
	d.emit(CardSetAnnounced{to_location(info), to_code(code)});
}

void handle_summoning(Decoding& d, SummonKind kind) {
	auto& r = d.reader();
	const auto code = r.u32();
	const auto info = read_loc_info(r, d.compat());
	if(!fully_read(r))
		return;

	d.emit(SummonAnnounced{kind, to_code(code), to_location(info)});
}

void handle_summoned(Decoding& d, SummonKind kind) {
	if(!fully_read(d.reader()))
		return;
	d.emit(SummonCompleted{kind});
}

void handle_chaining(Decoding& d) {
	auto& r = d.reader();
	const auto code = r.u32();
	const auto info = read_loc_info(r, d.compat());
	const auto triggering_controller = r.u8();
	const auto triggering_location = r.u8();
	const auto triggering_sequence = r.u8_or_u32(d.compat());
	const auto description = r.u32_or_u64(d.compat());
	const auto link = r.u8_or_u32(d.compat());
	if(!fully_read(r))
		return;

	ChainLink chain_link;
	chain_link.link = link;
	chain_link.code = to_code(code);
	chain_link.card_location = to_location(info);
	// The activating card may be somewhere this model does not track, in
	// which case the link still exists and simply names no instance.
	chain_link.card = d.state().at(chain_link.card_location);
	chain_link.triggering_controller = triggering_controller;
	chain_link.triggering_zone = zone_from_protocol(triggering_location);
	chain_link.triggering_sequence = triggering_sequence;
	chain_link.description = description;

	if(auto error = d.state().push_chain_link(chain_link)) {
		d.inconsistent(*error);
		return;
	}
	// Activating a card reveals it.
	if(code != 0 && chain_link.card != CardInstanceId::None)
		d.apply_code(chain_link.card, to_code(code));
	d.emit(ChainLinkAdded{chain_link});
}

void handle_chained(Decoding& d) {
	auto& r = d.reader();
	const auto link = r.u8();
	if(!fully_read(r))
		return;

	// The legacy client fills a pending link on MSG_CHAINING and commits it
	// here. This model commits on MSG_CHAINING, because that is the message
	// that carries the data, and uses MSG_CHAINED purely as a check that the
	// two agree on how long the chain now is.
	if(link != d.state().chain().size())
		d.inconsistent("chain confirmed at length " + std::to_string(link) +
					   ", model holds " + std::to_string(d.state().chain().size()));
}

void handle_chain_solving(Decoding& d) {
	auto& r = d.reader();
	const auto link = r.u8();
	if(!fully_read(r))
		return;
	if(auto error = d.state().mark_chain_resolving(link)) {
		d.inconsistent(*error);
		return;
	}
	d.emit(ChainLinkResolving{link});
}

void handle_chain_solved(Decoding& d) {
	auto& r = d.reader();
	const auto link = r.u8();
	if(!fully_read(r))
		return;
	if(auto error = d.state().mark_chain_resolved(link)) {
		d.inconsistent(*error);
		return;
	}
	d.emit(ChainLinkResolved{link});
}

void handle_chain_end(Decoding& d) {
	if(!fully_read(d.reader()))
		return;
	const auto links = static_cast<std::uint32_t>(d.state().chain().size());
	d.state().clear_chain();
	d.emit(ChainEnded{links});
}

void handle_draw(Decoding& d) {
	auto& r = d.reader();
	const auto player = r.u8();
	const auto count = r.u8_or_u32(d.compat());
	// Each drawn card is a code, plus a position in the modern protocol.
	const std::size_t entry = d.compat() ? sizeof(std::uint32_t) : 2 * sizeof(std::uint32_t);
	if(!count_fits(r, count, entry))
		return;
	std::vector<CardCode> codes;
	codes.reserve(count);
	for(std::uint32_t i = 0; i < count; ++i) {
		auto code = r.u32();
		if(d.compat())
			// The legacy client masks the top bit here; it carries the
			// reversed-deck flag, not part of the passcode.
			code &= 0x7fffffffu;
		else
			r.u32(); // position, unused by the legacy client too
		codes.push_back(to_code(code));
	}
	if(!fully_read(r))
		return;
	if(!d.require_duelist(player))
		return;

	const auto deck = d.state().zone(player, Zone::Deck);
	if(deck.size() < codes.size()) {
		d.inconsistent("draw of " + std::to_string(codes.size()) + " from a deck of " +
					   std::to_string(deck.size()));
		return;
	}

	// The top of the deck is the back of the pile, and the codes arrive in
	// draw order, so the first code belongs to the last card.
	std::vector<CardInstanceId> drawn;
	drawn.reserve(codes.size());
	for(std::size_t i = 0; i < codes.size(); ++i)
		drawn.push_back(deck[deck.size() - 1 - i]);

	for(std::size_t i = 0; i < drawn.size(); ++i)
		d.apply_code(drawn[i], codes[i]);
	for(const auto id : drawn) {
		if(auto error = d.state().move_card(id, CardLocation{player, Zone::Hand, 0, false, 0},
											std::nullopt)) {
			d.inconsistent(*error);
			return;
		}
	}
	d.emit(CardsDrawn{player, drawn});
}

void handle_life_change(Decoding& d, LifeChangeReason reason) {
	auto& r = d.reader();
	const auto player = r.u8();
	const auto amount = static_cast<std::int64_t>(r.u32());
	if(!fully_read(r))
		return;
	if(!d.require_duelist(player))
		return;

	const auto before = d.state().life_points(player);
	std::int64_t requested = before;
	switch(reason) {
	case LifeChangeReason::Damage:
	case LifeChangeReason::Cost:
		requested = before - amount;
		break;
	case LifeChangeReason::Recover:
		requested = before + amount;
		break;
	case LifeChangeReason::Update:
		requested = amount;
		break;
	}
	const auto after = d.state().set_life_points(player, requested);
	d.emit(LifePointsChanged{player, before, after, reason, amount});
}

void handle_attack(Decoding& d) {
	auto& r = d.reader();
	const auto attacker_info = read_loc_info(r, d.compat());
	const auto target_info = read_loc_info(r, d.compat());
	if(!fully_read(r))
		return;

	const auto attacker_location = to_location(attacker_info);
	const auto attacker = d.state().at(attacker_location);
	if(attacker == CardInstanceId::None) {
		d.inconsistent("no attacker at " + to_string(attacker_location));
		return;
	}

	const bool direct = target_info.nowhere();
	CardInstanceId target = CardInstanceId::None;
	if(!direct) {
		const auto target_location = to_location(target_info);
		target = d.state().at(target_location);
		if(target == CardInstanceId::None) {
			d.inconsistent("no attack target at " + to_string(target_location));
			return;
		}
	}

	d.state().set_attack(attacker, target);
	d.emit(AttackDeclared{attacker, target, direct});
}

void handle_battle(Decoding& d) {
	auto& r = d.reader();
	const auto attacker_info = read_loc_info(r, d.compat());
	const auto attacker_atk = r.i32();
	const auto attacker_def = r.i32();
	r.u8(); // "destroyed" flag, unused by the legacy client
	const auto target_info = read_loc_info(r, d.compat());
	const auto target_atk = r.i32();
	const auto target_def = r.i32();
	r.u8();
	if(!fully_read(r))
		return;

	const auto apply = [&d](const LocInfo& info, std::int32_t attack, std::int32_t defense) {
		const auto location = to_location(info);
		const auto id = d.state().at(location);
		if(id == CardInstanceId::None) {
			d.inconsistent("no combatant at " + to_string(location));
			return;
		}
		d.inconsistent_if(d.state().set_combat_stats(id, attack, defense));
		d.emit(CombatStatsRevealed{id, attack, defense});
	};

	apply(attacker_info, attacker_atk, attacker_def);
	// A direct attack has no defender, and says so with an empty location.
	if(!target_info.nowhere())
		apply(target_info, target_atk, target_def);
}

void handle_empty(Decoding& d, DuelEvent event) {
	if(!fully_read(d.reader()))
		return;
	d.emit(std::move(event));
}

// Dispatch. Every id in kSupported appears here exactly once.
void decode_supported(std::uint8_t message, Decoding& d) {
	switch(message) {
	case msg::MSG_START:
		return handle_start(d);
	case msg::MSG_WIN:
		return handle_win(d);
	case msg::MSG_SHUFFLE_DECK:
		return handle_shuffle_deck(d);
	case msg::MSG_SHUFFLE_HAND:
		return handle_shuffle_hand(d);
	case msg::MSG_NEW_TURN:
		return handle_new_turn(d);
	case msg::MSG_NEW_PHASE:
		return handle_new_phase(d);
	case msg::MSG_MOVE:
		return handle_move(d);
	case msg::MSG_POS_CHANGE:
		return handle_pos_change(d);
	case msg::MSG_SET:
		return handle_set(d);
	case msg::MSG_SUMMONING:
		return handle_summoning(d, SummonKind::Normal);
	case msg::MSG_SUMMONED:
		return handle_summoned(d, SummonKind::Normal);
	case msg::MSG_SPSUMMONING:
		return handle_summoning(d, SummonKind::Special);
	case msg::MSG_SPSUMMONED:
		return handle_summoned(d, SummonKind::Special);
	case msg::MSG_CHAINING:
		return handle_chaining(d);
	case msg::MSG_CHAINED:
		return handle_chained(d);
	case msg::MSG_CHAIN_SOLVING:
		return handle_chain_solving(d);
	case msg::MSG_CHAIN_SOLVED:
		return handle_chain_solved(d);
	case msg::MSG_CHAIN_END:
		return handle_chain_end(d);
	case msg::MSG_DRAW:
		return handle_draw(d);
	case msg::MSG_DAMAGE:
		return handle_life_change(d, LifeChangeReason::Damage);
	case msg::MSG_RECOVER:
		return handle_life_change(d, LifeChangeReason::Recover);
	case msg::MSG_LPUPDATE:
		return handle_life_change(d, LifeChangeReason::Update);
	case msg::MSG_PAY_LPCOST:
		return handle_life_change(d, LifeChangeReason::Cost);
	case msg::MSG_ATTACK:
		return handle_attack(d);
	case msg::MSG_BATTLE:
		return handle_battle(d);
	case msg::MSG_DAMAGE_STEP_START:
		return handle_empty(d, DamageStepStarted{});
	case msg::MSG_DAMAGE_STEP_END:
		return handle_empty(d, DamageStepEnded{});
	default:
		// Unreachable: supports() gates this switch, and a unit test asserts
		// the two agree.
		d.reader().fail();
		return;
	}
}

} // namespace

std::string_view decode_status_name(DecodeStatus status) noexcept {
	switch(status) {
	case DecodeStatus::Decoded:
		return "decoded";
	case DecodeStatus::UnsupportedMessage:
		return "unsupported";
	case DecodeStatus::UnknownMessage:
		return "unknown";
	case DecodeStatus::Malformed:
		return "malformed";
	case DecodeStatus::Inconsistent:
		return "inconsistent";
	}
	return "unknown";
}

const std::vector<std::uint8_t>& ProtocolDecoder::supported_messages() {
	static const std::vector<std::uint8_t> sorted = [] {
		std::vector<std::uint8_t> ids(kSupported.begin(), kSupported.end());
		std::sort(ids.begin(), ids.end());
		return ids;
	}();
	return sorted;
}

bool ProtocolDecoder::supports(std::uint8_t message) noexcept {
	return std::find(kSupported.begin(), kSupported.end(), message) != kSupported.end();
}

DecodeResult ProtocolDecoder::decode(const Packet& packet, DuelState& state) {
	DecodeResult result;
	result.message = packet.message;

	// The three refusals are deliberately distinct. An unknown id means the
	// stream or our generated table is wrong; an unsupported one means this
	// slice has not got to it yet; malformed means the bytes did not match a
	// layout we claim to know. Collapsing any pair would make the coverage
	// numbers dishonest.
	if(!protocol::is_known_message(packet.message)) {
		result.status = DecodeStatus::UnknownMessage;
		result.detail = "message id " + std::to_string(static_cast<unsigned>(packet.message)) +
						" is not defined by upstream";
		return result;
	}
	if(!supports(packet.message)) {
		result.status = DecodeStatus::UnsupportedMessage;
		result.detail = std::string(protocol::message_name(packet.message)) +
						" is known but not decoded by this build";
		return result;
	}

	// Decoding runs against a private working copy, never the caller's real
	// state, and that copy is committed back only once every check below has
	// passed. This is what makes "state is modified only on success" true
	// regardless of how far a handler got before it ran into trouble: a
	// handler is free to apply an identity change, a position, a combat stat
	// - anything - before discovering two steps later that the packet must be
	// refused, because whatever it touched lives in `trial` and `trial` is
	// simply discarded on any non-Decoded outcome. Reordering every handler to
	// front-load its checks would give the same guarantee far more fragilely
	// (one handler gets it wrong, one bug reappears); a single copy-and-commit
	// point gives it once, centrally, and it cannot regress handler by
	// handler. DuelState is cheap to copy (see its member list) and this path
	// is not hot, so the cost is a non-issue for a correctness-first
	// milestone. client/tests/test_transactional_decoding.cpp asserts this
	// end to end using DuelState::operator==, across MSG_MOVE, MSG_DRAW,
	// MSG_POS_CHANGE and MSG_BATTLE - the four handlers found to mutate ahead
	// of a later possible failure - plus the three non-Decoded statuses that
	// never reach a handler at all.
	DuelState trial = state;
	Decoding decoding(packet, trial, variant_);
	decode_supported(packet.message, decoding);

	auto& reader = decoding.reader();
	if(reader.failed()) {
		result.status = DecodeStatus::Malformed;
		result.detail = std::string(protocol::message_name(packet.message)) +
						" payload is " + std::to_string(packet.payload.size()) +
						" bytes, which does not satisfy its layout";
		return result;
	}
	if(!reader.exhausted()) {
		result.status = DecodeStatus::Malformed;
		result.detail = std::string(protocol::message_name(packet.message)) + " left " +
						std::to_string(reader.remaining()) + " of " +
						std::to_string(packet.payload.size()) + " bytes unread";
		return result;
	}
	if(decoding.is_inconsistent()) {
		result.status = DecodeStatus::Inconsistent;
		result.detail = decoding.inconsistency();
		return result;
	}

	result.status = DecodeStatus::Decoded;
	result.events = decoding.take_events();
	state = std::move(trial);
	return result;
}

} // namespace edopro_next::client

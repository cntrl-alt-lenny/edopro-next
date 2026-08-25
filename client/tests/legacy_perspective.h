// Test-only reference implementation of the legacy client's screen-relative
// player mapping, for the *future* legacy/model equivalence checker to build
// on. This header is not part of edopro_next_client and must never become
// part of it: the semantic model is deliberately protocol-absolute (see
// docs/architecture/semantic-model.md §4 and card_identity.h's PlayerId
// comment), and "which player sits at the bottom of the screen" is a
// presentation/session-adapter concern, not something DuelState should ever
// need to know.
//
// Formula verified against source, not assumed: Game::LocalPlayer
// (gframe/game.cpp) is exactly
//
//     uint8_t Game::LocalPlayer(uint8_t player) {
//         return dInfo.isFirst ? player : 1 - player;
//     }
//
// a pure function of one bit, dInfo.isFirst. Every duelclient.cpp MSG_*
// handler routes the protocol player id through it before touching
// dInfo.lp[] or ClientField's zone arrays (deck[2]/hand[2]/mzone[2]/etc,
// gframe/client_field.h); ClientField itself performs no remapping at all -
// it is the caller's job, done consistently at every one of those call
// sites. The same function is also used in the *other* direction: a
// locally-computed choice is mapped back through LocalPlayer before being
// written into an outgoing response buffer (e.g. MSG_SELECT_PLACE/
// MSG_SELECT_DISFIELD auto-pick, duelclient.cpp). That is safe only because
// the mapping is self-inverse - flipping 0/1 twice returns the original -
// which local_player() below both implements and, via its own test, proves.
//
// A future equivalence checker comparing legacy state against this model
// must apply this exact normalization at every boundary crossing, in both
// directions:
//
//     legacy.dField.<zone>[local_player(P, isFirst)] == model.<zone>[P]
//     legacy.dInfo.lp[local_player(P, isFirst)]       == model.lp[P]
//
// where P is the model's protocol-absolute player id and isFirst is whatever
// dInfo.isFirst held at the moment MSG_START set it for that session (or, for
// tag duels, whatever it was most recently derived to for the current player;
// LocalPlayer's own body never reads isTeam1/team1/team2 directly - see
// gframe/duelclient.cpp:897-899 for how those feed into isFirst upstream).
#ifndef EDOPRO_NEXT_CLIENT_TEST_LEGACY_PERSPECTIVE_H
#define EDOPRO_NEXT_CLIENT_TEST_LEGACY_PERSPECTIVE_H

#include "edopro_next/client/card_identity.h"

namespace edopro_next::testing {

// Mirrors Game::LocalPlayer(uint8_t) exactly. `protocol_player` must be 0 or
// 1 - LocalPlayer's own call sites never invoke it on PLAYER_NONE/PLAYER_ALL
// (duelclient.cpp guards with `player < 2` first, e.g. before MSG_WIN's call).
constexpr client::PlayerId local_player(client::PlayerId protocol_player,
										bool is_first) noexcept {
	return is_first ? protocol_player : static_cast<client::PlayerId>(1 - protocol_player);
}

} // namespace edopro_next::testing

#endif // EDOPRO_NEXT_CLIENT_TEST_LEGACY_PERSPECTIVE_H

// Pins the normalization rule a future legacy/model equivalence checker must
// use, against the two cases that actually occur: a duel where the local
// client moved first, and one where it didn't. See legacy_perspective.h for
// the source citation the formula is verified against.
#include "legacy_perspective.h"
#include "test_support.h"

using edopro_next::client::PlayerId;
using edopro_next::testing::local_player;

EDOPRO_TEST(is_first_maps_every_player_to_itself) {
	EDOPRO_CHECK_EQ(local_player(0, true), PlayerId{0});
	EDOPRO_CHECK_EQ(local_player(1, true), PlayerId{1});
}

EDOPRO_TEST(not_is_first_flips_every_player) {
	EDOPRO_CHECK_EQ(local_player(0, false), PlayerId{1});
	EDOPRO_CHECK_EQ(local_player(1, false), PlayerId{0});
}

EDOPRO_TEST(the_mapping_is_its_own_inverse) {
	// What makes it safe to use LocalPlayer both to read a protocol-ordered
	// field into local storage AND to write a locally-computed choice back
	// into a protocol-ordered response (both usages are real - see
	// legacy_perspective.h).
	for(const bool is_first : {true, false}) {
		for(const PlayerId protocol_player : {PlayerId{0}, PlayerId{1}}) {
			const auto local = local_player(protocol_player, is_first);
			EDOPRO_CHECK_EQ(local_player(local, is_first), protocol_player);
		}
	}
}

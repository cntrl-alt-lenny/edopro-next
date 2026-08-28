// A presentation-independent deck: three ordered lists of card codes. See
// docs/architecture/deck-model.md for the source-verified semantics this
// reproduces (gframe/deck.h, gframe/deck_manager.cpp) and
// docs/adr/0004-deck-model-ydk-codec.md for why this stores CardCode rather
// than card records or legacy pointers.
//
// This is a data boundary, not a rules boundary: it never checks deck size,
// never enforces a three-copy limit, never applies a banlist, and never
// decides whether a card belongs in the Extra Deck by its type. It is
// exactly what a `.ydk` file's own section markers say, nothing more - see
// ydk.h for the codec that reads/writes it.
#ifndef EDOPRO_NEXT_DATA_DECK_H
#define EDOPRO_NEXT_DATA_DECK_H

#include <vector>

#include "edopro_next/data/card_code.h"

namespace edopro_next::data {

// Order and multiplicity are both meaningful and both preserved: three
// entries for the same CardCode are three entries, not a count of three,
// and their position relative to each other survives a load/save round
// trip. A CardCode here need not be known to any loaded CardDatabase -
// see ydk.h's "unknown codes" policy.
//
// CardCode::None is not a valid entry in main/extra/side - it is this
// project's "not a real card" sentinel (card_code.h), and parse_ydk()
// never produces one (ydk.h's code-0 policy excludes it during parsing).
// These are public vectors, though, so nothing here stops a caller from
// constructing one anyway; if a caller does, ydk.h's round-trip guarantee
// does not apply to that Deck - see ydk.h's serialize_ydk() doc comment
// and docs/architecture/deck-model.md#round-trip-contract for the exact,
// deliberately un-special-cased behaviour this produces.
struct Deck {
	std::vector<CardCode> main;
	std::vector<CardCode> extra;
	std::vector<CardCode> side;

	void clear() {
		main.clear();
		extra.clear();
		side.clear();
	}

	bool empty() const noexcept { return main.empty() && extra.empty() && side.empty(); }

	friend bool operator==(const Deck&, const Deck&) = default;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_DECK_H

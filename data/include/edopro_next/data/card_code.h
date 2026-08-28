// Card identity for the card-data facade.
//
// This is deliberately a separate type from edopro_next::client::CardCode.
// The two represent the same real-world concept - a card's passcode - but
// this module and client/ are independent by design (see
// docs/adr/0003-card-database-facade.md): neither includes the other, so
// neither can share a type without creating the coupling the separation
// exists to avoid. A caller that uses both is expected to convert at its own
// boundary, which is one std::uint32_t round-trip - to_number() one way,
// an explicit static_cast<CardCode>(...) (or CardCode{n} direct-list-
// initialization, valid for a scoped enum's underlying type since C++17)
// the other - cheap, and the price of keeping the module graph a DAG with
// no edge between "duel protocol model" and "card database".
#ifndef EDOPRO_NEXT_DATA_CARD_CODE_H
#define EDOPRO_NEXT_DATA_CARD_CODE_H

#include <cstdint>

namespace edopro_next::data {

// A card passcode, as printed on the card and used as the primary key of the
// `datas`/`texts` tables. `None` means "no card" - the value `alias` uses to
// say "no alias" - and is never a valid loaded card code: it is upstream's
// own convention too (CardDataC::getRealCode(), DeckManager's dummy/unknown-
// card entries all use code 0 as a synthesized "not a real card" sentinel,
// never as a `.cdb` row - gframe/data_manager.h, gframe/deck_manager.cpp).
// CardDatabase::load_database() enforces this: a row with id 0 fails that
// load rather than being stored, so `None` never has to be disambiguated
// from "the actual card with code 0" anywhere in this API.
enum class CardCode : std::uint32_t { None = 0 };

constexpr std::uint32_t to_number(CardCode code) noexcept {
	return static_cast<std::uint32_t>(code);
}

constexpr bool is_known(CardCode code) noexcept {
	return code != CardCode::None;
}

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_CARD_CODE_H

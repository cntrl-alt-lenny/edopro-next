// One card's row from a Project Ignis `.cdb` database, decoded into a
// semantic record: no SQLite types, no UI formatting, no legality decisions.
//
// This is a faithful decode of the `datas`/`texts` tables actually read by
// upstream's DataManager::ParseDB (gframe/data_manager.cpp), not a redesign.
// Every field below, and every transformation applied to get it from the raw
// column value, is documented in docs/architecture/card-database.md and
// verified against that source. Where this record's shape diverges from
// upstream's CardDataC, the divergence is named there and is either an
// Irrlicht/global-state/legacy-plumbing detail this module has no reason to
// carry (the ocgcore C-array setcode terminator, the wchar_t/UTF-16 detour,
// the Irrlicht virtual filesystem hook) or a deliberately stronger guarantee
// this module chooses to provide instead (CardDatabase's whole-file load
// atomicity - see card_database.h). It is never a guess at what the schema
// "should" mean. (Active-locale text overlay is a CardDatabase concern, not
// a CardRecord one; see card_database.h for its exact, source-verified
// fallback rules - which are not uniform across name/text vs. the sixteen
// auxiliary strings.)
#ifndef EDOPRO_NEXT_DATA_CARD_RECORD_H
#define EDOPRO_NEXT_DATA_CARD_RECORD_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "edopro_next/data/card_code.h"

namespace edopro_next::data {

// A card's row from `datas` + `texts`, joined and decoded.
struct CardRecord {
	// `datas.id`. The card's own passcode.
	CardCode code = CardCode::None;

	// `datas.alias`. CardCode::None means this card has no alias. When set,
	// this card is an alternate-artwork printing (or errata) of another card;
	// which one is not this module's concern (see docs/architecture/
	// card-database.md#alias) - it is deck/banlist-matching logic, not card
	// data.
	CardCode alias = CardCode::None;

	// `datas.ot`. A bitmask of SCOPE_* values (OCG/TCG/anime/illegal/video
	// game/custom/speed/prerelease/rush/legend/hidden - see
	// gframe/data_manager.h for the bit assignments this module also uses).
	// Exposed as an opaque bitmask on purpose: interpreting it (is this card
	// legal to play?) is a deck/duel-format decision, not a database fact.
	std::uint32_t scope = 0;

	// `datas.setcode`, unpacked from its packed-uint64 wire form into the
	// list of non-zero 16-bit set codes it actually names, in ascending
	// packed-slot order. Empty means no set. Unlike upstream's CardDataC,
	// there is no trailing zero sentinel here: that exists only because
	// CardDataC::setcodes_p is handed to ocgcore's C API as a null-terminated
	// array, which is plumbing this module does not have.
	std::vector<std::uint16_t> setcodes;

	// `datas.type`. A bitmask of TYPE_* values (monster/spell/trap and every
	// sub-type). Opaque here for the same reason as `scope`.
	std::uint32_t type = 0;

	// `datas.atk`. Signed: -1 and -2 are real, displayed values ("?" ATK/DEF
	// on cards whose stats vary), not sentinels this module strips.
	std::int32_t attack = 0;

	// `datas.def`, with one schema-defined exception: for a card whose `type`
	// includes the Link bit, this column does not hold a defense value at
	// all - it holds the link-marker bitmask, and Link monsters have no
	// defense. This module resolves that at load time exactly as upstream
	// does (DataManager::ParseDB): `defense` is 0 and `link_marker` carries
	// the value instead. See link_marker below.
	std::int32_t defense = 0;

	// `datas.level`, the level/rank magnitude, sign-extended from the low
	// byte of the packed `level` column and then stored exactly as upstream
	// stores it: `CardData::level`/`CardDataC::level` (and the engine-facing
	// `OCG_CardData::level` they are memcpy'd into unchanged) are all
	// `uint32_t`, not `int32_t` - so a "negative" decoded level is a large
	// wrapped value here too (see docs/architecture/card-database.md#
	// level-and-pendulum-scale for the exact arithmetic and why this is not
	// a decode error). Deck search's own level filter
	// (`gframe/deck_con.cpp`) compares this field as `uint32_t`, so matching
	// that representation - not the mathematically "cleaner" signed one -
	// is what keeps this facade's data consistent with upstream's actual
	// observable filtering behaviour, not just its storage layout.
	std::uint32_t level = 0;

	// The packed `level` column's high two bytes: a Pendulum monster's left
	// and right scale. Both are 0 for a non-Pendulum card, which is
	// indistinguishable from "Pendulum scale 0" at this layer - upstream has
	// the same ambiguity, resolved by `type` at the caller, not here.
	std::uint32_t left_scale = 0;
	std::uint32_t right_scale = 0;

	// `datas.race`. A bitmask, and 64 bits wide in the schema itself (not an
	// upstream extension) - `sqlite3_column_int64` is what ParseDB reads it
	// with, so a value using bit 32 or above is real, current schema data,
	// not a modern addition this module bolted on.
	std::uint64_t race = 0;

	// `datas.attribute`. A bitmask (a card can be queried for "is this
	// LIGHT", but the column itself holds one attribute per real card - the
	// bitmask shape exists because the same field slot is reused for
	// multi-attribute search filters upstream, which is exactly the kind of
	// presentation/search concern this module leaves opaque).
	std::uint32_t attribute = 0;

	// `datas.category`. An opaque bitmask consumed by upstream's card-search
	// filters (gframe/menu_handler.h, gframe/deck_con.cpp); this module does
	// not interpret it, only carries it for whoever builds search later.
	std::uint32_t category = 0;

	// Derived from `datas.def` when `type` has the Link bit set (see
	// `defense` above). 0 for every non-Link card. The bit layout is
	// upstream's LINK_MARKER_* constants (gframe/common.h); this module does
	// not interpret individual marker bits, only carries the mask forward.
	std::uint32_t link_marker = 0;

	// `texts.name`, UTF-8.
	std::string name;

	// `texts.desc`. Named `text` here, not `desc`, because upstream's own
	// `desc` name collides with the sixteen `str1..str16` fields below, which
	// upstream also calls "desc[]" once decoded (CardString::desc). Keeping
	// one of them named `desc` in this record would make every access
	// ambiguous to a reader who has not memorized which is which.
	std::string text;

	// `texts.str1` through `texts.str16`, in that order (strings[0] ==
	// str1). Their individual meaning is assigned per-card by CardScripts,
	// not by the schema - most cards leave most of them empty. This module
	// exposes exactly what the column holds and does not attempt to
	// interpret which slot means what for a given card.
	std::array<std::string, 16> strings{};

	friend bool operator==(const CardRecord&, const CardRecord&) = default;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_CARD_RECORD_H

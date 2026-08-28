// The card-database facade: reads Project Ignis `.cdb` files into
// CardRecords. See docs/architecture/card-database.md for the schema this
// reads and the exact semantics of everything below, and
// docs/adr/0003-card-database-facade.md for why this module exists on its
// own rather than inside client/ or gframe/.
//
// This is a data boundary, not a rules boundary: it never decides legality,
// never resolves an alias to "the" canonical printing, never picks a locale.
// It reports what a `.cdb` row says and nothing more.
#ifndef EDOPRO_NEXT_DATA_CARD_DATABASE_H
#define EDOPRO_NEXT_DATA_CARD_DATABASE_H

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>

#include "edopro_next/data/card_code.h"
#include "edopro_next/data/card_record.h"

namespace edopro_next::data {

// The result of one load_database()/load_locale() call. `ok` is false
// exactly when `error` is non-empty; there is no third state.
struct LoadResult {
	bool ok = false;
	std::string error;
	// Cards this call added or overwrote. 0 on failure.
	std::size_t rows_loaded = 0;

	explicit operator bool() const noexcept { return ok; }
};

// Loaded card data, keyed by CardCode. Every load is read-only against the
// source file and atomic against this catalogue: a call that returns `ok ==
// false` leaves the catalogue exactly as it was before the call (see
// docs/architecture/card-database.md#load-atomicity for why that is a
// deliberately stronger guarantee than upstream's own DataManager::ParseDB
// gives).
//
// Copyable and movable with the obvious value semantics; there is no live
// SQLite handle to manage, so there is nothing default copy/move would get
// wrong.
class CardDatabase {
public:
	// Loads `datas` joined with `texts` from `path` and merges the result
	// into this catalogue. A card code already present is completely
	// replaced by the new row - the last database loaded for a given code
	// wins in full, matching DataManager::ParseDB's unconditional
	// overwrite of `cards[code]`. A card present in one table but not the
	// other is not loaded at all, matching the inner join upstream's own
	// SELECT performs.
	LoadResult load_database(const std::filesystem::path& path);

	// Loads a locale `texts` table from `path` and overlays its name/text/
	// strings onto cards already present in this catalogue, field by field:
	// a non-empty locale value replaces the base value for that one field: an
	// empty one leaves the base value in place. A locale row for a code not
	// already loaded by load_database() is ignored - see
	// docs/architecture/card-database.md#locale-overlay for why this
	// deliberately simplifies upstream's order-independent linking.
	LoadResult load_locale(const std::filesystem::path& path);

	const CardRecord* find(CardCode code) const noexcept;
	bool contains(CardCode code) const noexcept;
	std::size_t size() const noexcept { return cards_.size(); }
	bool empty() const noexcept { return cards_.empty(); }

	// Ascending by CardCode, and stable across repeated calls for the same
	// loaded state - this is a std::map, not a hash table, precisely so
	// enumeration order does not depend on load order or on the standard
	// library's hash implementation.
	using const_iterator = std::map<CardCode, CardRecord>::const_iterator;
	const_iterator begin() const noexcept { return cards_.begin(); }
	const_iterator end() const noexcept { return cards_.end(); }

private:
	std::map<CardCode, CardRecord> cards_;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_CARD_DATABASE_H

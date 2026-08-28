// The card-database facade: reads Project Ignis `.cdb` files into
// CardRecords. See docs/architecture/card-database.md for the schema this
// reads and the exact semantics of everything below, and
// docs/adr/0003-card-database-facade.md for why this module exists on its
// own rather than inside client/ or gframe/.
//
// This is a data boundary, not a rules boundary: it never decides legality,
// never resolves an alias to "the" canonical printing, never picks a locale.
// It reports what a `.cdb` row says and nothing more - including which
// locale's text is in effect, which is a decision this module implements the
// mechanics for but never makes itself (there is no locale name, language
// code, or file-discovery logic here; see load_locale()/clear_locale()).
#ifndef EDOPRO_NEXT_DATA_CARD_DATABASE_H
#define EDOPRO_NEXT_DATA_CARD_DATABASE_H

#include <array>
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

// Loaded card data, keyed by CardCode, with an independently-tracked active
// locale text overlay - see docs/architecture/card-database.md#locale for
// the source-verified upstream lifecycle this reproduces
// (DataManager::ParseDB/ParseLocaleDB/ClearLocaleTexts, and Game::
// ApplyLocale's clear-then-reload sequence).
//
// Every load is read-only against the source file and atomic against this
// catalogue: a call that returns `ok == false` leaves the catalogue exactly
// as it was before the call, for both load_database() (the base layer) and
// load_locale() (the active locale layer) independently - see
// docs/architecture/card-database.md#load-atomicity.
//
// Pointer/reference stability: find() returns a pointer into this object's
// own storage, never into a temporary. That pointer stays valid for the
// lifetime of this CardDatabase (or until moved-from) - no operation here
// ever erases a loaded card. It is NOT a snapshot, though: a later
// load_database(), load_locale(), or clear_locale() call may update the
// CardRecord the pointer refers to in place (a base field changing on
// reload, or name/text/strings changing because the active locale changed).
// A caller that needs a point-in-time value should copy *find(code), not
// hold the pointer across a call that can mutate this catalogue.
//
// Copyable and movable with the obvious value semantics; there is no live
// SQLite handle to manage, so there is nothing default copy/move would get
// wrong.
class CardDatabase {
public:
	// Loads `datas` joined with `texts` from `path` and merges the result
	// into the base layer. A card code already present is completely
	// replaced by the new row - the last database loaded for a given code
	// wins in full, matching DataManager::ParseDB's unconditional
	// overwrite of `cards[code]`. A card present in one table but not the
	// other is not loaded at all, matching the inner join upstream's own
	// SELECT performs.
	//
	// A card code of 0 is rejected as a load failure rather than stored:
	// throughout this module (and upstream's own code - CardDataC::
	// getRealCode(), DeckManager's dummy/unknown-card entries) 0 is the "no
	// real card" sentinel (CardCode::None), never a real `.cdb` row; a file
	// containing one violates that invariant. See docs/architecture/
	// card-database.md#card-code-0.
	//
	// If this card code already has an active locale overlay (from a prior
	// load_locale() not since cleared), that overlay continues to apply to
	// the newly (re)loaded base row, matching upstream's own re-link
	// behavior on a base reload while a locale is active.
	LoadResult load_database(const std::filesystem::path& path);

	// Loads a locale `texts` table from `path` and applies it to the active
	// locale layer, then recomputes every affected card's visible name/text/
	// strings. A locale row for a code not already loaded by
	// load_database() is ignored - see docs/architecture/card-database.md
	// #locale for why this deliberately simplifies upstream's
	// order-independent linking, which its own real call sites never
	// exercise.
	//
	// Multiple load_locale() calls accumulate into ONE active locale layer,
	// exactly like DataManager::ParseLocaleDB reusing `locales[code]` across
	// files: a later call's row for a code already touched by an earlier
	// call in the same active locale completely replaces it, field by
	// field, even with an empty value. This is deliberately the same
	// last-file-wins rule load_database() uses for the base layer, not a
	// merge.
	//
	// Within the active locale layer, once any load_locale() call has
	// mentioned a code at all, that code's `name` and `text` are taken from
	// the locale layer as a pair - even if both are empty - and no longer
	// fall back to the base value field-by-field. This matches
	// CardDataM::GetStrings() returning the whole linked CardString or the
	// whole base one, never a mix. The sixteen auxiliary strings are
	// different: each falls back to its own base value independently
	// whenever the locale layer's same slot is empty, matching
	// CardDataM::GetDesc(). See docs/architecture/card-database.md#locale
	// for the full account, including why this module does not "fix" the
	// name/text behavior into the auxiliary-string one.
	LoadResult load_locale(const std::filesystem::path& path);

	// Discards the active locale layer entirely and restores every loaded
	// card's name/text/strings to its base value - the data-layer half of
	// upstream's ClearLocaleTexts(), called unconditionally by
	// Game::ApplyLocale() before loading a newly selected locale (or before
	// loading nothing at all, to return to the base language). Performs no
	// filesystem or SQLite operation, so it has no LoadResult-style failure
	// state to report - unlike load_database()/load_locale() it always runs
	// to completion, including as a harmless no-op when no locale is active.
	// It is not declared noexcept: resolving text back to the base value
	// copies strings, which can allocate.
	void clear_locale();

	const CardRecord* find(CardCode code) const noexcept;
	bool contains(CardCode code) const noexcept;
	std::size_t size() const noexcept { return records_.size(); }
	bool empty() const noexcept { return records_.empty(); }

	// Ascending by CardCode, and stable across repeated calls for the same
	// loaded state - this is a std::map, not a hash table, precisely so
	// enumeration order does not depend on load order or on the standard
	// library's hash implementation.
	using const_iterator = std::map<CardCode, CardRecord>::const_iterator;
	const_iterator begin() const noexcept { return records_.begin(); }
	const_iterator end() const noexcept { return records_.end(); }

private:
	// name/text/strings only - everything a locale `texts` row can carry.
	// Used for both the base text copy and the active locale overlay; the
	// base copy is modified only by a successful load_database() call for
	// that code (never by a locale operation), and the overlay is
	// discarded whole by clear_locale().
	struct TextFields {
		std::string name;
		std::string text;
		std::array<std::string, 16> strings{};
	};

	void resolve_text(CardCode code);

	// The materialized, effective view: what find()/iteration expose. Every
	// entry here mirrors the corresponding base_text_ entry's non-text
	// fields exactly, and its name/text/strings reflect base_text_ or
	// locale_text_ per the rules documented on load_locale() above.
	std::map<CardCode, CardRecord> records_;
	// The base layer, exactly as loaded by load_database() - never touched
	// by a locale operation. Kept independently of records_ so clear_locale()
	// can restore base text without re-reading any file.
	std::map<CardCode, TextFields> base_text_;
	// The active locale layer. A code present here (regardless of whether
	// its fields are empty) is "linked", in upstream's terms - see
	// load_locale()'s doc comment. Empty means no locale is active.
	std::map<CardCode, TextFields> locale_text_;
};

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_CARD_DATABASE_H

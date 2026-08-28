// Exercises edopro_next::data::CardSearchIndex against tiny, synthetic
// SQLite databases (synthetic_cdb.h) - never a committed Project Ignis
// `.cdb` file and never real card text. Every behaviour asserted here is
// checked against the exact upstream sources cited in
// docs/architecture/card-search.md.
#include "edopro_next/data/card_search_index.h"

#include "edopro_next/data/card_database.h"
#include "edopro_next/data/search_query.h"
#include "edopro_next/data/search_result.h"
#include "edopro_next/data/text_normalize.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "synthetic_cdb.h"
#include "test_support.h"

using edopro_next::data::BitmaskFilter;
using edopro_next::data::BitmaskFilter64;
using edopro_next::data::CardCode;
using edopro_next::data::CardDatabase;
using edopro_next::data::CardSearchIndex;
using edopro_next::data::MatchKind;
using edopro_next::data::NumericComparison;
using edopro_next::data::NumericFilter;
using edopro_next::data::SearchQuery;
using edopro_next::data::SearchResult;
using edopro_next::data::TextScope;
using namespace edopro_next::data::testing;

namespace {

constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypeSpell = 0x2;
constexpr std::uint32_t kTypeLink = 0x4000000;
constexpr std::uint32_t kTypePendulum = 0x1000000;

std::vector<CardCode> codes(const std::vector<SearchResult>& results) {
	std::vector<CardCode> out;
	out.reserve(results.size());
	for(const auto& r : results)
		out.push_back(r.code);
	return out;
}

bool contains(const std::vector<SearchResult>& results, CardCode code) {
	return std::any_of(results.begin(), results.end(),
						[code](const SearchResult& r) { return r.code == code; });
}

std::optional<MatchKind> match_kind_of(const std::vector<SearchResult>& results, CardCode code) {
	for(const auto& r : results) {
		if(r.code == code)
			return r.match;
	}
	return std::nullopt;
}

} // namespace

// ---------------------------------------------------------------------
// A) empty catalogue
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(searching_an_empty_index_returns_no_hits_safely) {
	CardSearchIndex index;
	EDOPRO_DATA_CHECK(index.empty());
	SearchQuery query;
	query.text = "anything";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(results.empty());
}

// ---------------------------------------------------------------------
// B) empty query: deliberately means "every card matches" (subject to
// any static filters), ranked NoTextQuery, ordered only by the
// deterministic tie-break - see search_query.h/search_result.h.
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(empty_text_query_returns_every_card_as_no_text_query) {
	TempFile file("empty_query");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Alpha");
	insert_card(db, 2, "Beta");
	insert_card(db, 3, "Gamma");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	const auto results = index.search(SearchQuery{});
	EDOPRO_DATA_CHECK_EQ(results.size(), std::size_t{3});
	for(const auto& r : results)
		EDOPRO_DATA_CHECK(r.match == MatchKind::NoTextQuery);
	// Deterministic tie-break: normalized name ascending.
	EDOPRO_DATA_CHECK_EQ(codes(results),
						 (std::vector<CardCode>{CardCode{1}, CardCode{2}, CardCode{3}}));
}

// ---------------------------------------------------------------------
// C) exact name
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(exact_normalized_name_match_ranks_as_exact_name) {
	TempFile file("exact_name");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Synthetic Dragon");
	insert_card(db, 2, "Synthetic Dragon Knight");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Synthetic Dragon";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(results.size(), std::size_t{2});
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::ExactName);
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{2}) == MatchKind::NamePrefix);
	// Exact match ranks ahead of the prefix match.
	EDOPRO_DATA_CHECK_EQ(codes(results), (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
}

// ---------------------------------------------------------------------
// D) case insensitivity
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(mixed_case_query_matches_mixed_case_name) {
	TempFile file("case");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "MiXeD CaSe NaMe");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "mixed case name";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::ExactName);
}

// ---------------------------------------------------------------------
// E) accent normalization - pins the source-derived fold table
// (Utils::ToUpperChar, gframe/utils.h) via normalize_search_text directly.
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(accented_and_unaccented_synthetic_text_normalize_identically) {
	// "ÀÉÎÕÜñ" -> "AEIOUN", per the exact table cited in text_normalize.cpp.
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("\xC3\x80\xC3\x89\xC3\x8E\xC3"
																	"\x95\xC3\x9C\xC3\xB1"),
						 std::string("AEIOUN"));
	// Inverted punctuation folds to plain ASCII.
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("\xC2\xA1\xC2\xBF"),
						 std::string("!?"));
	// A Latin-1 character outside the explicit table (AE ligature, 0xC6)
	// passes through unchanged, per the empirically-verified "C" locale
	// fallback documented in text_normalize.cpp.
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("\xC3\x86"), std::string("\xC3\x86"));

	TempFile file("accent");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Caf\xC3\xA9 Dragon"); // "Café Dragon"
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "CAFE DRAGON"; // unaccented query finds the accented name
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::ExactName);
}

// A malformed byte's raw numeric value must never be run through the fold
// table: 0xC3 (195) is a valid lead byte for a two-byte sequence, but if
// truncated (nothing follows it) it is not a real codepoint - and 195
// happens to fall inside the fold table's 192-197 "A" range, so a naive
// "treat the raw byte as a codepoint" fallback would incorrectly turn a
// truncated sequence into "A". Found by external review, not by initial
// implementation.
EDOPRO_DATA_TEST(a_truncated_multibyte_lead_byte_is_not_folded_to_a_table_letter) {
	// "\xC3" alone: a valid two-byte lead byte with no continuation byte.
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("\xC3"), std::string("\xC3"));
	// A lone continuation byte (never a valid lead byte at all).
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("\x80"), std::string("\x80"));
	// The malformed byte does not disturb decoding of what surrounds it.
	EDOPRO_DATA_CHECK_EQ(edopro_next::data::normalize_search_text("ab\xC3zy"), std::string("AB\xC3ZY"));
}

// ---------------------------------------------------------------------
// F) Name-only scope: a text-only occurrence must not match
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(name_only_scope_does_not_match_an_occurrence_only_in_text) {
	TempFile file("name_only");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Unrelated Name", "Mentions Keyword only in the rules text.");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Keyword";
	query.text_scope = TextScope::Name;
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(!contains(results, CardCode{1}));
}

// ---------------------------------------------------------------------
// G) Text-only scope: a name-only occurrence must not match
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(text_only_scope_does_not_match_an_occurrence_only_in_name) {
	TempFile file("text_only");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Keyword Dragon", "Ordinary rules text with nothing special.");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Keyword";
	query.text_scope = TextScope::Text;
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(!contains(results, CardCode{1}));
}

// ---------------------------------------------------------------------
// H) Name-or-Text scope: either can match
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(name_or_text_scope_matches_either_field) {
	TempFile file("name_or_text");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	// "Keyword" appears in card 1's name but not as a prefix (it comes
	// after "Dragon "), so this is a NameMatch, not a NamePrefix.
	insert_card(db, 1, "Dragon Keyword", "Nothing special.");
	insert_card(db, 2, "Unrelated Name", "Mentions Keyword in the text.");
	insert_card(db, 3, "Nothing Here", "Nothing here either.");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Keyword";
	query.text_scope = TextScope::NameOrText;
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(contains(results, CardCode{1}));
	EDOPRO_DATA_CHECK(contains(results, CardCode{2}));
	EDOPRO_DATA_CHECK(!contains(results, CardCode{3}));
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::NameMatch);
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{2}) == MatchKind::TextMatch);
}

// ---------------------------------------------------------------------
// I) multiple tokens: order-independent AND semantics (deliberately
// departs from upstream's ordered Utils::ContainsSubstring - see
// card_search_index.cpp).
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(multiple_tokens_require_all_present_regardless_of_order) {
	TempFile file("tokens");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "White Dragon Blue Eyes"); // tokens present, reverse order vs. query
	insert_card(db, 2, "Blue Eyes White Dragon"); // tokens present, same order as query
	insert_card(db, 3, "Blue Eyes Only");         // missing "white"/"dragon"
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "blue white"; // neither is a substring of either card's name as one phrase
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(contains(results, CardCode{1}));
	EDOPRO_DATA_CHECK(contains(results, CardCode{2}));
	EDOPRO_DATA_CHECK(!contains(results, CardCode{3}));
}

// ---------------------------------------------------------------------
// J) deterministic ranking across all tiers, and stable tie-break
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(ranking_tiers_are_ordered_exact_then_prefix_then_name_then_text) {
	TempFile file("ranking");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Dragon", "Unrelated text.");                        // ExactName
	insert_card(db, 2, "Dragon Knight", "Unrelated text.");                 // NamePrefix
	insert_card(db, 3, "Winged Dragon Beast", "Unrelated text.");           // NameMatch
	insert_card(db, 4, "Unrelated Creature", "Summons a Dragon to fight."); // TextMatch
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(codes(results), (std::vector<CardCode>{CardCode{1}, CardCode{2},
																  CardCode{3}, CardCode{4}}));
}

EDOPRO_DATA_TEST(equal_rank_results_tie_break_by_normalized_name_then_code) {
	TempFile file("tiebreak");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 20, "Zeta Dragon");
	insert_card(db, 10, "Alpha Dragon");
	insert_card(db, 15, "Alpha Dragon"); // same normalized name, different code
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(codes(results),
						 (std::vector<CardCode>{CardCode{10}, CardCode{15}, CardCode{20}}));
}

// ---------------------------------------------------------------------
// K) one CardCode appears once even if both name and text match
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_card_matching_in_both_name_and_text_appears_only_once) {
	TempFile file("no_dup");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Dragon Knight", "This Dragon protects its master.");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(results.size(), std::size_t{1});
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::NamePrefix);
}

// ---------------------------------------------------------------------
// L) raw scope metadata is never treated as legality unless explicitly
// filtered by the caller
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(cards_with_unusual_scope_bits_are_returned_unless_explicitly_filtered) {
	TempFile file("scope");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow custom{};
	custom.id = 1;
	custom.ot = 0x800; // an arbitrary "not officially released" style bit - opaque here
	insert_data_row(db, custom);
	insert_text_row(db, TextRow{1, "Custom Card", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	// No scope filter at all: the card is found like any other - this
	// index never excludes anything based on `scope` unless asked to.
	SearchQuery unfiltered;
	unfiltered.text = "Custom Card";
	EDOPRO_DATA_CHECK(contains(index.search(unfiltered), CardCode{1}));

	// An explicit, caller-supplied raw scope filter can still exclude it -
	// that is ordinary data filtering, not this index deciding legality.
	SearchQuery filtered;
	filtered.text = "Custom Card";
	filtered.scope = BitmaskFilter{0x1}; // require a bit this card doesn't have
	EDOPRO_DATA_CHECK(!contains(index.search(filtered), CardCode{1}));
}

// ---------------------------------------------------------------------
// M) attack/defense "?" sentinel values (-1/-2)
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(question_mark_attack_participates_in_plain_numeric_comparison) {
	TempFile file("sentinel");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow q{};
	q.id = 1;
	q.type = kTypeMonster;
	q.atk = -2; // "?" ATK
	insert_data_row(db, q);
	insert_text_row(db, TextRow{1, "Question Mark Monster", ""});
	DataRow normal{};
	normal.id = 2;
	normal.type = kTypeMonster;
	normal.atk = 3000;
	insert_data_row(db, normal);
	insert_text_row(db, TextRow{2, "Normal Monster", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	// "At least 2500" naturally excludes -2 with no sentinel-aware special
	// casing, matching upstream's own observable filtering result for
	// this case.
	SearchQuery at_least;
	at_least.attack = NumericFilter{2500, NumericComparison::AtLeast};
	const auto at_least_results = index.search(at_least);
	EDOPRO_DATA_CHECK(!contains(at_least_results, CardCode{1}));
	EDOPRO_DATA_CHECK(contains(at_least_results, CardCode{2}));

	// A caller specifically wanting "?" cards uses EqualTo -2 directly.
	SearchQuery exactly_question_mark;
	exactly_question_mark.attack = NumericFilter{-2, NumericComparison::EqualTo};
	const auto qm_results = index.search(exactly_question_mark);
	EDOPRO_DATA_CHECK(contains(qm_results, CardCode{1}));
	EDOPRO_DATA_CHECK(!contains(qm_results, CardCode{2}));
}

// ---------------------------------------------------------------------
// N) Link defense/link-marker behaviour
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_defense_filter_never_matches_a_link_monster) {
	TempFile file("link_defense");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow link{};
	link.id = 1;
	link.type = kTypeMonster | kTypeLink;
	link.def = 0; // link_marker bits live here in the raw schema; CardRecord
				  // resolves this into link_marker/defense=0 (M3A) - the
				  // filter must exclude Link monsters regardless.
	insert_data_row(db, link);
	insert_text_row(db, TextRow{1, "Link Monster", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.defense = NumericFilter{0, NumericComparison::EqualTo};
	EDOPRO_DATA_CHECK(!contains(index.search(query), CardCode{1}));
}

EDOPRO_DATA_TEST(link_marker_bitmask_filter_matches_required_bits) {
	TempFile file("link_marker");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow link{};
	link.id = 1;
	link.type = kTypeMonster | kTypeLink;
	link.def = 0x3; // two link-marker bits set
	insert_data_row(db, link);
	insert_text_row(db, TextRow{1, "Link Monster", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery has_both;
	has_both.link_marker = BitmaskFilter{0x3};
	EDOPRO_DATA_CHECK(contains(index.search(has_both), CardCode{1}));

	SearchQuery missing_one;
	missing_one.link_marker = BitmaskFilter{0x7};
	EDOPRO_DATA_CHECK(!contains(index.search(missing_one), CardCode{1}));
}

// ---------------------------------------------------------------------
// O) 64-bit race
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(race_filter_handles_a_high_bit_beyond_32_bits) {
	TempFile file("race64");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow high_race{};
	high_race.id = 1;
	high_race.race = UINT64_C(1) << 40;
	insert_data_row(db, high_race);
	insert_text_row(db, TextRow{1, "High Race Monster", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery matching;
	matching.race = BitmaskFilter64{UINT64_C(1) << 40};
	EDOPRO_DATA_CHECK(contains(index.search(matching), CardCode{1}));

	SearchQuery non_matching;
	non_matching.race = BitmaskFilter64{UINT64_C(1) << 41};
	EDOPRO_DATA_CHECK(!contains(index.search(non_matching), CardCode{1}));
}

// ---------------------------------------------------------------------
// P) wrapped uint32 level (M3A source-fidelity)
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_negative_encoded_level_participates_as_a_huge_unsigned_value) {
	TempFile file("level");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow negative{};
	negative.id = 1;
	negative.level = -249; // decodes to level 4294967289u, per card-database.md
	insert_data_row(db, negative);
	insert_text_row(db, TextRow{1, "Negative Level Monster", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	const auto* record = catalogue.find(CardCode{1});
	EDOPRO_DATA_CHECK(record != nullptr);
	EDOPRO_DATA_CHECK_EQ(record->level, 4294967289u);

	CardSearchIndex index;
	index.rebuild(catalogue);

	// A plain "at least 1" filter naturally includes the huge wrapped
	// value, matching upstream's own uint32_t comparison behaviour - not
	// a special case this index adds.
	SearchQuery query;
	query.level = NumericFilter{1, NumericComparison::AtLeast};
	EDOPRO_DATA_CHECK(contains(index.search(query), CardCode{1}));
}

// ---------------------------------------------------------------------
// Q) setcodes, including alias resolution
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(setcode_filter_matches_any_of_the_requested_codes) {
	TempFile file("setcode");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow card{};
	card.id = 1;
	card.setcode = 0x1234ull | (0x5678ull << 16); // two setcode slots
	insert_data_row(db, card);
	insert_text_row(db, TextRow{1, "Archetype Card", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery matching;
	matching.setcodes = std::vector<std::uint16_t>{0x5678};
	EDOPRO_DATA_CHECK(contains(index.search(matching), CardCode{1}));

	SearchQuery non_matching;
	non_matching.setcodes = std::vector<std::uint16_t>{0x9999};
	EDOPRO_DATA_CHECK(!contains(index.search(non_matching), CardCode{1}));
}

EDOPRO_DATA_TEST(setcode_filter_on_an_aliased_card_uses_the_aliased_cards_setcodes) {
	TempFile file("setcode_alias");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	DataRow canonical{};
	canonical.id = 1;
	canonical.setcode = 0xABCDull;
	insert_data_row(db, canonical);
	insert_text_row(db, TextRow{1, "Canonical Printing", ""});
	DataRow alt_art{};
	alt_art.id = 2;
	alt_art.alias = 1;
	alt_art.setcode = 0; // the alternate printing's own row has no setcode
	insert_data_row(db, alt_art);
	insert_text_row(db, TextRow{2, "Alternate Art Printing", ""});
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.setcodes = std::vector<std::uint16_t>{0xABCD};
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(contains(results, CardCode{1}));
	EDOPRO_DATA_CHECK(contains(results, CardCode{2}));
}

// ---------------------------------------------------------------------
// R) locale rebuild lifecycle: base -> locale -> clear
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(search_reflects_the_active_locale_only_after_an_explicit_rebuild) {
	TempFile base_file("locale_base");
	sqlite3* base_db = open_writable(base_file.path());
	create_datas_texts_schema(base_db);
	insert_card(base_db, 1, "Base Name");
	sqlite3_close(base_db);

	TempFile locale_file("locale_overlay");
	sqlite3* locale_db = open_writable(locale_file.path());
	create_texts_only_schema(locale_db);
	insert_text_row(locale_db, TextRow{1, "Localized Name", ""});
	sqlite3_close(locale_db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(base_file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery base_query;
	base_query.text = "Base Name";
	EDOPRO_DATA_CHECK(contains(index.search(base_query), CardCode{1}));

	EDOPRO_DATA_CHECK(catalogue.load_locale(locale_file.path()).ok);
	// Stale index: still reflects the base name until rebuild() runs.
	EDOPRO_DATA_CHECK(contains(index.search(base_query), CardCode{1}));
	SearchQuery locale_query;
	locale_query.text = "Localized Name";
	EDOPRO_DATA_CHECK(!contains(index.search(locale_query), CardCode{1}));

	index.rebuild(catalogue);
	EDOPRO_DATA_CHECK(contains(index.search(locale_query), CardCode{1}));
	EDOPRO_DATA_CHECK(!contains(index.search(base_query), CardCode{1}));

	catalogue.clear_locale();
	index.rebuild(catalogue);
	EDOPRO_DATA_CHECK(contains(index.search(base_query), CardCode{1}));
	EDOPRO_DATA_CHECK(!contains(index.search(locale_query), CardCode{1}));
}

// ---------------------------------------------------------------------
// S) a later database override changes search results only after rebuild
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_database_override_is_invisible_to_search_until_rebuilt) {
	TempFile file("override");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Original Name");
	sqlite3_close(db);

	TempFile override_file("override2");
	sqlite3* override_db = open_writable(override_file.path());
	create_datas_texts_schema(override_db);
	insert_card(override_db, 1, "Replaced Name");
	sqlite3_close(override_db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery original_query;
	original_query.text = "Original Name";
	EDOPRO_DATA_CHECK(contains(index.search(original_query), CardCode{1}));

	EDOPRO_DATA_CHECK(catalogue.load_database(override_file.path()).ok);
	EDOPRO_DATA_CHECK(contains(index.search(original_query), CardCode{1})); // still stale

	index.rebuild(catalogue);
	EDOPRO_DATA_CHECK(!contains(index.search(original_query), CardCode{1}));
	SearchQuery replaced_query;
	replaced_query.text = "Replaced Name";
	EDOPRO_DATA_CHECK(contains(index.search(replaced_query), CardCode{1}));
}

// ---------------------------------------------------------------------
// T) stable, repeatable ordering
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(the_same_index_and_query_return_identical_ordered_results_every_time) {
	TempFile file("stable");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	for(std::uint32_t i = 1; i <= 20; ++i)
		insert_card(db, i, "Repeated Dragon Name " + std::to_string(i % 5));
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	const auto first = index.search(query);
	const auto second = index.search(query);
	EDOPRO_DATA_CHECK(first == second);
}

// ---------------------------------------------------------------------
// U) result limit truncates only after ranking
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(limit_truncates_the_already_ranked_results_deterministically) {
	TempFile file("limit");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Dragon");         // ExactName
	insert_card(db, 2, "Dragon Knight");  // NamePrefix
	insert_card(db, 3, "Winged Dragon");  // NameMatch
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	query.limit = 2;
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(results.size(), std::size_t{2});
	EDOPRO_DATA_CHECK_EQ(codes(results), (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
}

// ---------------------------------------------------------------------
// exact_code field
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(exact_code_restricts_to_that_one_code_and_ranks_it_first) {
	TempFile file("exact_code");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Zeta Dragon"); // would rank after the others by name
	insert_card(db, 2, "Alpha Dragon");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.text = "Dragon";
	query.exact_code = CardCode{1};
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK_EQ(codes(results), (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::ExactCode);
}

// exact_code boosts ranking within the unified result set - it is not an
// implicit filter (search_query.h's own doc comment: "still subject to
// every other filter below, `text` included"). With no text and no other
// filter active, every card still matches (the empty-query contract,
// item B above); exact_code naming a code absent from the catalogue just
// means nothing receives the ExactCode boost - it does not narrow the
// result set to nothing.
EDOPRO_DATA_TEST(exact_code_for_a_missing_code_does_not_narrow_an_otherwise_unconstrained_query) {
	TempFile file("exact_code_missing");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Only Card");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.exact_code = CardCode{999};
	const auto results = index.search(query);
	EDOPRO_DATA_CHECK(contains(results, CardCode{1}));
	EDOPRO_DATA_CHECK(match_kind_of(results, CardCode{1}) == MatchKind::NoTextQuery);
}

// The same missing code, but now with a text constraint the one real
// card does not satisfy - here exact_code naming nothing real correctly
// contributes no results, because the text filter itself excludes
// everything, not because exact_code acted as a filter.
EDOPRO_DATA_TEST(exact_code_for_a_missing_code_with_a_non_matching_text_query_matches_nothing) {
	TempFile file("exact_code_missing_text");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	insert_card(db, 1, "Only Card");
	sqlite3_close(db);

	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	SearchQuery query;
	query.exact_code = CardCode{999};
	query.text = "Nonexistent Keyword";
	EDOPRO_DATA_CHECK(index.search(query).empty());
}

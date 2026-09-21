// Exercises edopro_next::data::Deck and the .ydk codec (parse_ydk/
// serialize_ydk/load_ydk/save_ydk) against synthetic, entirely made-up card
// codes - never a real decklist, and never a CardDatabase: this whole
// executable links edopro_next_deck only (see data/CMakeLists.txt), which
// does not link SQLite or edopro_next_data, so a passing build of this file
// is itself evidence that reading/writing a .ydk never requires a card
// database (see item P in docs/architecture/deck-model.md#testing).
//
// Every parsing/writing fact asserted here is checked against
// gframe/deck_manager.cpp's LoadCardList/SaveDeck/MakeYdkEntryString - see
// docs/architecture/deck-model.md for the exact source citations behind
// each MATCH/DIVERGE decision this suite pins.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "edopro_next/data/ydk.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef small
#undef small
#endif
#endif

#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::Deck;
using edopro_next::data::YdkWriteOptions;

namespace {

std::filesystem::path unique_temp_path(const char* label) {
	static std::atomic<int> counter{0};
	const auto n = counter.fetch_add(1);
	return std::filesystem::temp_directory_path() /
		("edopro_next_deck_test_" + std::string(label) + "_" + std::to_string(n) + ".ydk");
}

class TempFile {
public:
	explicit TempFile(const char* label) : path_(unique_temp_path(label)) {
		std::filesystem::remove(path_);
	}
	~TempFile() { std::filesystem::remove(path_); }
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;

	const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};

#ifdef _WIN32
std::atomic<unsigned> named_pipe_counter{0};

class NamedPipe {
public:
	NamedPipe() = default;
	~NamedPipe() { close(); }
	NamedPipe(const NamedPipe&) = delete;
	NamedPipe& operator=(const NamedPipe&) = delete;

	bool create(bool occupy) {
		const auto number = named_pipe_counter.fetch_add(1);
		leaf_ = L"edopro_next_deck_test_" + std::to_wstring(GetCurrentProcessId()) +
			L"_" + std::to_wstring(number);
		name_ = L"\\\\.\\pipe\\" + leaf_;
		server_ = CreateNamedPipeW(
			name_.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
		if(server_ == INVALID_HANDLE_VALUE)
			return false;
		if(!occupy)
			return true;

		connect_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if(connect_event_ == nullptr)
			return false;
		OVERLAPPED overlapped{};
		overlapped.hEvent = connect_event_;
		bool already_connected = ConnectNamedPipe(server_, &overlapped) != FALSE;
		if(!already_connected) {
			const auto error = GetLastError();
			if(error == ERROR_PIPE_CONNECTED)
				already_connected = true;
			else if(error != ERROR_IO_PENDING)
				return false;
		}
		if(already_connected)
			return true;

		client_ = CreateFileW(name_.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if(client_ == INVALID_HANDLE_VALUE)
			return false;
		if(WaitForSingleObject(connect_event_, 5000) != WAIT_OBJECT_0)
			return false;
		DWORD transferred = 0;
		return GetOverlappedResult(server_, &overlapped, &transferred, FALSE) != FALSE;
	}

	void close() {
		if(client_ != INVALID_HANDLE_VALUE) {
			CloseHandle(client_);
			client_ = INVALID_HANDLE_VALUE;
		}
		if(server_ != INVALID_HANDLE_VALUE) {
			CancelIoEx(server_, nullptr);
			DisconnectNamedPipe(server_);
			CloseHandle(server_);
			server_ = INVALID_HANDLE_VALUE;
		}
		if(connect_event_ != nullptr) {
			CloseHandle(connect_event_);
			connect_event_ = nullptr;
		}
	}

	std::filesystem::path path() const { return std::filesystem::path(name_); }

	std::vector<std::filesystem::path> alternate_paths() const {
		return {
			std::filesystem::path(L"\\\\.\\GLOBALROOT\\Device\\NamedPipe\\" + leaf_),
			std::filesystem::path(L"\\localhost\\pipe\\" + leaf_),
			std::filesystem::path(L"\\127.0.0.1\\pipe\\" + leaf_),
			std::filesystem::path(L"\\\\?\\UNC\\localhost\\pipe\\" + leaf_),
		};
	}

private:
	std::wstring name_;
	std::wstring leaf_;
	HANDLE server_ = INVALID_HANDLE_VALUE;
	HANDLE client_ = INVALID_HANDLE_VALUE;
	HANDLE connect_event_ = nullptr;
};

edopro_next::data::YdkLoadResult load_named_pipe_with_timeout(
	const std::filesystem::path& path, NamedPipe& pipe) {
	std::packaged_task<edopro_next::data::YdkLoadResult()> task([path] {
		return edopro_next::data::load_ydk(path);
	});
	auto future = task.get_future();
	std::thread(std::move(task)).detach();
	if(future.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
		pipe.close();
		EDOPRO_DATA_CHECK(false);
		return {};
	}
	return future.get();
}
#endif

} // namespace

// ---------------------------------------------------------------------
// A) canonical three-section deck
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(canonical_three_section_deck_parses_into_matching_sections) {
	const auto parsed = edopro_next::data::parse_ydk("#created by TestPlayer\n"
													   "#main\n"
													   "11111111\n"
													   "22222222\n"
													   "#extra\n"
													   "33333333\n"
													   "!side\n"
													   "44444444\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main,
						 (std::vector<CardCode>{CardCode{11111111}, CardCode{22222222}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.extra, (std::vector<CardCode>{CardCode{33333333}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.side, (std::vector<CardCode>{CardCode{44444444}}));
	EDOPRO_DATA_CHECK(parsed.ignored.empty());
}

// ---------------------------------------------------------------------
// B) empty sections round-trip
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(deck_with_only_main_round_trips_with_extra_and_side_empty) {
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}};
	const auto text = edopro_next::data::serialize_ydk(deck);
	const auto parsed = edopro_next::data::parse_ydk(text);
	EDOPRO_DATA_CHECK(parsed.deck == deck);
	EDOPRO_DATA_CHECK(parsed.deck.extra.empty());
	EDOPRO_DATA_CHECK(parsed.deck.side.empty());
}

EDOPRO_DATA_TEST(completely_empty_deck_round_trips_to_itself) {
	const Deck deck;
	const auto text = edopro_next::data::serialize_ydk(deck);
	const auto parsed = edopro_next::data::parse_ydk(text);
	EDOPRO_DATA_CHECK(parsed.deck == deck);
	EDOPRO_DATA_CHECK(parsed.deck.empty());
}

// ---------------------------------------------------------------------
// C) order and duplicates survive exactly
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(duplicate_codes_are_preserved_as_separate_entries_in_file_order) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n"
													   "5\n"
													   "3\n"
													   "5\n"
													   "5\n"
													   "3\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{5}, CardCode{3},
																	CardCode{5}, CardCode{5},
																	CardCode{3}}));
}

// ---------------------------------------------------------------------
// D) unknown non-zero codes survive with no CardDatabase required
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(an_arbitrary_nonzero_code_unknown_to_any_catalogue_is_preserved_verbatim) {
	// This module has no CardDatabase type to even ask "is this known" -
	// the point of this test is that a syntactically valid code is stored
	// unconditionally, whether or not it corresponds to a real card.
	const auto parsed = edopro_next::data::parse_ydk("#main\n987654321\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{987654321}}));
}

// ---------------------------------------------------------------------
// E) comments and creator metadata never become cards
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(comment_lines_including_ones_containing_digits_never_become_cards) {
	const auto parsed = edopro_next::data::parse_ydk("#created by Someone42\n"
													   "# this comment mentions 12345678\n"
													   "#main\n"
													   "111\n"
													   "# another comment\n"
													   "222\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}, CardCode{222}}));
	EDOPRO_DATA_CHECK(parsed.ignored.empty());
}

// ---------------------------------------------------------------------
// F) blank lines and CRLF
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(blank_lines_between_entries_are_skipped) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n\n111\n\n\n222\n\n#extra\n\n!side\n\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}, CardCode{222}}));
	EDOPRO_DATA_CHECK(parsed.deck.extra.empty());
	EDOPRO_DATA_CHECK(parsed.deck.side.empty());
}

EDOPRO_DATA_TEST(crlf_line_endings_parse_identically_to_lf) {
	const auto parsed = edopro_next::data::parse_ydk("#main\r\n111\r\n222\r\n#extra\r\n333\r\n"
													   "!side\r\n444\r\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}, CardCode{222}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.extra, (std::vector<CardCode>{CardCode{333}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.side, (std::vector<CardCode>{CardCode{444}}));
}

EDOPRO_DATA_TEST(final_line_without_a_trailing_newline_is_still_parsed) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n111\n222");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}, CardCode{222}}));
}

// ---------------------------------------------------------------------
// G) section transitions, including the one-way-latch quirk
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_repeated_hash_extra_marker_is_harmless) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n1\n#extra\n2\n#extra\n3\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{1}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.extra, (std::vector<CardCode>{CardCode{2}, CardCode{3}}));
}

EDOPRO_DATA_TEST(any_bang_prefixed_line_not_only_literal_bang_side_starts_the_side_section) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n1\n!not the usual marker\n2\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{1}}));
	EDOPRO_DATA_CHECK_EQ(parsed.deck.side, (std::vector<CardCode>{CardCode{2}}));
}

// Mirrors LoadCardList exactly (gframe/deck_manager.cpp:292-294): once a
// '!' line is seen, is_side never resets, so a "#extra" marker that
// appears afterward is inert - a code following it still lands in side,
// not extra. A real .ydk (upstream's own writer) never produces this
// ordering; this pins the one-way-latch behaviour deliberately, not
// because it is expected in practice.
EDOPRO_DATA_TEST(a_hash_extra_marker_appearing_after_bang_side_does_not_reopen_extra) {
	const auto parsed = edopro_next::data::parse_ydk("!side\n1\n#extra\n2\n");
	EDOPRO_DATA_CHECK(parsed.deck.main.empty());
	EDOPRO_DATA_CHECK(parsed.deck.extra.empty());
	EDOPRO_DATA_CHECK_EQ(parsed.deck.side, (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
}

EDOPRO_DATA_TEST(cards_before_any_section_marker_default_to_main) {
	const auto parsed = edopro_next::data::parse_ydk("111\n222\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}, CardCode{222}}));
}

// ---------------------------------------------------------------------
// H) malformed lines, and exact std::stoul-relevant semantics
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_line_with_no_digit_at_all_is_skipped_without_being_reported_as_malformed) {
	const auto parsed = edopro_next::data::parse_ydk("#main\nhello\n111\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}}));
	EDOPRO_DATA_CHECK(parsed.ignored.empty());
}

EDOPRO_DATA_TEST(leading_numeral_followed_by_letters_parses_the_leading_number) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n123abc\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{123}}));
	EDOPRO_DATA_CHECK(parsed.ignored.empty());
}

EDOPRO_DATA_TEST(surrounding_whitespace_around_a_number_is_tolerated) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n  42  \n\t99\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{42}, CardCode{99}}));
}

EDOPRO_DATA_TEST(a_leading_plus_sign_is_accepted) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n+5abc\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{5}}));
}

// std::stoul("-1") does not throw: it parses the unsigned magnitude and
// negates modulo 2^(width of unsigned long) - see
// docs/architecture/deck-model.md for the empirical verification this
// depends on. -1 wraps to the all-ones pattern, which truncated to
// uint32_t is 0xFFFFFFFF, matching LoadCardList's own
// static_cast<uint32_t>(std::stoul(str)) exactly.
EDOPRO_DATA_TEST(a_leading_minus_sign_wraps_via_unsigned_arithmetic_matching_upstream) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n-1\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{0xFFFFFFFFu}}));
}

EDOPRO_DATA_TEST(a_decimal_point_stops_the_number_at_the_point) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n12.5\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{12}}));
}

// std::stoul defaults to base 10, so a leading "0x" is not hex: it parses
// "0" and stops at 'x' - which this codec's own code-0 policy then
// excludes as "not a real card", not as malformed.
EDOPRO_DATA_TEST(a_hex_looking_prefix_parses_as_decimal_zero_not_as_hexadecimal) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n0x1A\n");
	EDOPRO_DATA_CHECK(parsed.deck.main.empty());
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.size(), std::size_t{1});
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).reason, std::string("card code 0 is not a real card"));
}

EDOPRO_DATA_TEST(a_number_far_beyond_any_native_integer_width_is_reported_as_malformed) {
	const auto parsed =
		edopro_next::data::parse_ydk("#main\n99999999999999999999999999999999\n");
	EDOPRO_DATA_CHECK(parsed.deck.main.empty());
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.size(), std::size_t{1});
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).reason, std::string("malformed card code"));
}

// ---------------------------------------------------------------------
// I) card code 0
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(a_literal_zero_line_is_excluded_and_reported_not_stored_as_a_card) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n0\n111\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{111}}));
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.size(), std::size_t{1});
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).line_number, std::size_t{2});
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).reason, std::string("card code 0 is not a real card"));
}

EDOPRO_DATA_TEST(zero_is_excluded_the_same_way_in_extra_and_side_sections) {
	const auto parsed = edopro_next::data::parse_ydk("#extra\n0\n!side\n0\n");
	EDOPRO_DATA_CHECK(parsed.deck.extra.empty());
	EDOPRO_DATA_CHECK(parsed.deck.side.empty());
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.size(), std::size_t{2});
}

// ---------------------------------------------------------------------
// J) uint32 boundary values
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(the_maximum_valid_uint32_code_is_stored_normally) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n4294967295\n");
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{4294967295u}}));
	EDOPRO_DATA_CHECK(parsed.ignored.empty());
}

// Whether "4294967296" (2^32) parses successfully and then truncates to 0,
// or std::stoul throws std::out_of_range on the string itself, depends on
// unsigned long's own width - this codec calls std::stoul exactly as
// upstream does (deck-model.md#2.5), so it inherits whichever behaviour
// the build's C++ standard library actually has, deliberately not
// papered over with platform-independent parsing. 64-bit unsigned long
// (this project's Linux baseline, docs/BASELINE.md) parses then wraps;
// exactly-32-bit unsigned long (e.g. Windows/LLP64, per README.md's
// stated intent to eventually support it) overflows std::stoul itself.
// Both are genuine per-platform std::stoul behaviour, verified by
// std::numeric_limits rather than assumed - see the "ABI-dependent" note
// in docs/architecture/deck-model.md#2.5. Either way, no card is stored.
EDOPRO_DATA_TEST(two_to_the_32_is_excluded_one_way_or_another_depending_on_unsigned_long_width) {
	const auto parsed = edopro_next::data::parse_ydk("#main\n4294967296\n");
	EDOPRO_DATA_CHECK(parsed.deck.main.empty());
	EDOPRO_DATA_CHECK_EQ(parsed.ignored.size(), std::size_t{1});
	if constexpr(std::numeric_limits<unsigned long>::max() >
				 std::numeric_limits<std::uint32_t>::max()) {
		// unsigned long is wider than uint32_t: std::stoul("4294967296")
		// succeeds, static_cast<uint32_t> wraps it to 0, and this codec's
		// code-0 policy excludes it - matching upstream's own
		// static_cast<uint32_t>(std::stoul(...)) on the same ABI.
		EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).reason,
							 std::string("card code 0 is not a real card"));
	} else {
		// unsigned long is exactly 32 bits (the C++ standard guarantees
		// it is never narrower): std::stoul("4294967296") itself throws
		// std::out_of_range before any truncation happens, caught and
		// reported as malformed - matching upstream's own catch(...).
		EDOPRO_DATA_CHECK_EQ(parsed.ignored.at(0).reason, std::string("malformed card code"));
	}
}

// ---------------------------------------------------------------------
// K) writer exact output
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(serialize_ydk_without_creator_matches_the_exact_expected_bytes) {
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}, CardCode{3}};
	deck.extra = {CardCode{10001}};
	const auto text = edopro_next::data::serialize_ydk(deck);
	EDOPRO_DATA_CHECK_EQ(text, std::string("#main\n1\n2\n3\n#extra\n10001\n!side\n"));
}

EDOPRO_DATA_TEST(serialize_ydk_with_creator_emits_the_created_by_line_first) {
	Deck deck;
	deck.side = {CardCode{7}};
	YdkWriteOptions options;
	options.creator = "TestPlayer";
	const auto text = edopro_next::data::serialize_ydk(deck, options);
	EDOPRO_DATA_CHECK_EQ(text,
						 std::string("#created by TestPlayer\n#main\n#extra\n!side\n7\n"));
}

EDOPRO_DATA_TEST(serialize_ydk_writes_all_three_section_markers_even_when_empty) {
	const Deck deck;
	const auto text = edopro_next::data::serialize_ydk(deck);
	EDOPRO_DATA_CHECK_EQ(text, std::string("#main\n#extra\n!side\n"));
}

// A caller-supplied creator string is metadata, not deck content - an
// embedded newline must not be able to turn "one comment line" into
// "one comment line plus whatever the rest of the string looks like as
// file content". Found by external review, not by initial implementation.
EDOPRO_DATA_TEST(serialize_ydk_strips_embedded_newlines_from_creator) {
	Deck deck;
	deck.main = {CardCode{1}};
	YdkWriteOptions options;
	options.creator = "line one\nline two\r\nline three";
	const auto text = edopro_next::data::serialize_ydk(deck, options);
	EDOPRO_DATA_CHECK_EQ(text,
						 std::string("#created by line oneline twoline three\n#main\n1\n#extra\n!side\n"));
}

// The concrete failure scenario the previous test's byte-level check is
// meant to rule out: without sanitization, this exact creator string would
// place "999" on its own line before "#main", which the parser (nothing
// yet having set is_extra/is_side) would read as a Main Deck card - a
// caller-controlled string silently mutating deck semantics on the next
// load.
EDOPRO_DATA_TEST(a_creator_containing_a_newline_cannot_inject_a_card_line) {
	Deck deck;
	deck.main = {CardCode{1}};
	YdkWriteOptions options;
	options.creator = "x\n999";
	const auto text = edopro_next::data::serialize_ydk(deck, options);
	const auto parsed = edopro_next::data::parse_ydk(text);
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{1}}));
}

// ---------------------------------------------------------------------
// L) semantic round-trip
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(parse_then_serialize_then_parse_again_yields_an_identical_deck) {
	const auto first = edopro_next::data::parse_ydk("#main\n1\n1\n2\n#extra\n3\n4\n!side\n5\n");
	const auto text = edopro_next::data::serialize_ydk(first.deck);
	const auto second = edopro_next::data::parse_ydk(text);
	EDOPRO_DATA_CHECK(second.deck == first.deck);
}

// The round-trip contract above is scoped to a Deck containing only
// non-zero CardCode values - true of anything parse_ydk()/load_ydk()
// themselves produce, since neither ever stores CardCode::None (§I above).
// Deck::main/extra/side are public vectors, though (deck.h), so nothing
// stops a caller from constructing one directly. This pins the actual,
// documented (not accidental) result: serialize_ydk writes CardCode::None
// as a literal "0" line like any other code (no filtering - see
// serialize_ydk's own doc comment), which a later parse_ydk() then
// excludes under the same code-0 policy - so this specific Deck does not
// round-trip, and this test exists so that stays a known, intentional
// contract boundary rather than an untested gap.
EDOPRO_DATA_TEST(a_deck_containing_card_code_none_does_not_round_trip) {
	Deck deck;
	deck.main = {CardCode{1}, CardCode::None, CardCode{2}};
	const auto text = edopro_next::data::serialize_ydk(deck);
	EDOPRO_DATA_CHECK_EQ(text, std::string("#main\n1\n0\n2\n#extra\n!side\n"));
	const auto parsed = edopro_next::data::parse_ydk(text);
	EDOPRO_DATA_CHECK(parsed.deck != deck);
	EDOPRO_DATA_CHECK_EQ(parsed.deck.main, (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
}

// ---------------------------------------------------------------------
// M) file read/write via the filesystem wrapper
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(save_ydk_then_load_ydk_round_trips_through_a_real_file) {
	TempFile file("save_load");
	Deck deck;
	deck.main = {CardCode{111}, CardCode{222}};
	deck.extra = {CardCode{333}};
	deck.side = {CardCode{444}, CardCode{444}};
	const auto save_result = edopro_next::data::save_ydk(file.path(), deck);
	EDOPRO_DATA_CHECK(save_result.ok);
	const auto load_result = edopro_next::data::load_ydk(file.path());
	EDOPRO_DATA_CHECK(load_result.ok);
	EDOPRO_DATA_CHECK(load_result.deck == deck);
}

// ---------------------------------------------------------------------
// N) failed load is transactional - a caller that guards on `ok` before
// assigning never has an existing Deck partially overwritten.
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(loading_a_missing_file_fails_and_leaves_an_existing_deck_untouched) {
	const auto missing = std::filesystem::temp_directory_path() /
		"edopro_next_deck_test_definitely_missing_file.ydk";
	std::filesystem::remove(missing);

	Deck existing;
	existing.main = {CardCode{1}, CardCode{2}};

	const auto result = edopro_next::data::load_ydk(missing);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to open file: " + missing.string());
	EDOPRO_DATA_CHECK(result.deck.empty());

	// The caller's own guard - not this codec - is what makes a failed
	// load transactional: only apply `result.deck` when `result.ok`.
	if(result.ok)
		existing = result.deck;
	EDOPRO_DATA_CHECK_EQ(existing.main, (std::vector<CardCode>{CardCode{1}, CardCode{2}}));
}

EDOPRO_DATA_TEST(loading_a_directory_fails_cleanly) {
	const auto result = edopro_next::data::load_ydk(std::filesystem::temp_directory_path());
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK(result.deck.empty());
}

EDOPRO_DATA_TEST(a_stat_failure_defers_to_the_open_failure) {
	const auto directory = std::filesystem::temp_directory_path();
	const auto link = directory / "edopro_next_deck_test_dangling_link.ydk";
	const auto target = directory / "edopro_next_deck_test_missing_target.ydk";
	std::error_code cleanup_error;
	std::filesystem::remove(link, cleanup_error);
	std::filesystem::remove(target, cleanup_error);
	std::error_code create_error;
	std::filesystem::create_symlink(target, link, create_error);
	if(create_error) {
		std::cout << "  SKIP a_stat_failure_defers_to_the_open_failure: cannot create symlink: "
				  << create_error.message() << "\n";
		return;
	}

	const auto result = edopro_next::data::load_ydk(link);
	std::filesystem::remove(link, cleanup_error);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to open file: " + link.string());
}

EDOPRO_DATA_TEST(a_non_regular_file_is_rejected_before_opening) {
	const std::filesystem::path device = "/dev/zero";
	if(!std::filesystem::exists(device)) {
		std::cout << "  SKIP a_non_regular_file_is_rejected_before_opening: /dev/zero is unavailable\n";
		return;
	}
	const auto result = edopro_next::data::load_ydk(device);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to read file: " + device.string());
}

EDOPRO_DATA_TEST(loading_a_free_named_pipe_fails_before_consuming_an_instance) {
#ifndef _WIN32
	std::cout << "  SKIP loading_a_free_named_pipe_fails_before_consuming_an_instance: Windows named pipes are unavailable on this platform\n";
	return;
#else
	NamedPipe pipe;
	if(!pipe.create(false)) {
		EDOPRO_DATA_CHECK(false);
		return;
	}
	const auto result = load_named_pipe_with_timeout(pipe.path(), pipe);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to read file: " + pipe.path().string());
	EDOPRO_DATA_CHECK(result.deck.empty());
#endif
}

EDOPRO_DATA_TEST(loading_a_busy_named_pipe_fails_without_opening_an_instance) {
#ifndef _WIN32
	std::cout << "  SKIP loading_a_busy_named_pipe_fails_without_opening_an_instance: Windows named pipes are unavailable on this platform\n";
	return;
#else
	NamedPipe pipe;
	if(!pipe.create(true)) {
		EDOPRO_DATA_CHECK(false);
		return;
	}
	const auto result = load_named_pipe_with_timeout(pipe.path(), pipe);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to read file: " + pipe.path().string());
	EDOPRO_DATA_CHECK(result.deck.empty());
#endif
}

EDOPRO_DATA_TEST(loading_named_pipe_aliases_fails_without_hanging) {
#ifndef _WIN32
	std::cout << "  SKIP loading_named_pipe_aliases_fails_without_hanging: Windows named pipes are unavailable on this platform\n";
	return;
#else
	for(std::size_t index = 0; index < 4; ++index) {
		NamedPipe pipe;
		if(!pipe.create(false)) {
			EDOPRO_DATA_CHECK(false);
			return;
		}
		const auto path = pipe.alternate_paths().at(index);
		const auto result = load_named_pipe_with_timeout(path, pipe);
		std::cout << "    " << path.string() << " -> " << result.error << "\n";
		EDOPRO_DATA_CHECK(!result.ok);
		EDOPRO_DATA_CHECK_EQ(result.error, "failed to read file: " + path.string());
		EDOPRO_DATA_CHECK(result.deck.empty());
	}
#endif
}

EDOPRO_DATA_TEST(loading_a_share_locked_regular_file_reports_open_failure) {
#ifndef _WIN32
	std::cout << "  SKIP loading_a_share_locked_regular_file_reports_open_failure: Windows share locks are unavailable on this platform\n";
	return;
#else
	TempFile file("share_locked");
	{
		std::ofstream output(file.path(), std::ios::binary);
		output << "#main\n1\n";
	}
	const auto lock = CreateFileW(
		file.path().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	EDOPRO_DATA_CHECK(lock != INVALID_HANDLE_VALUE);
	if(lock == INVALID_HANDLE_VALUE)
		return;
	const auto result = edopro_next::data::load_ydk(file.path());
	CloseHandle(lock);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK_EQ(result.error, "failed to open file: " + file.path().string());
#endif
}

// ---------------------------------------------------------------------
// A failure the stream's own state does not surface for free, without an
// explicit flush()/a read through file.read() - see
// docs/architecture/deck-model.md's "Detecting a failure the stream's own
// state does not surface for free" for the empirical verification these
// two fixes are based on. Both use real Linux-only special files that do
// not exist on every platform this project might eventually build on
// (README.md's Windows/macOS builds are "not attempted" so far) - each
// test checks for its file first and does nothing if absent, rather than
// hard-failing a platform that cannot exercise this exact scenario. On
// this project's actual CI platform (ubuntu-latest) both files exist and
// both checks run for real.
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(saving_to_a_destination_that_only_fails_at_flush_is_reported_as_a_failure) {
	const std::filesystem::path full_device = "/dev/full";
	if(!std::filesystem::exists(full_device))
		return;
	Deck deck;
	deck.main = {CardCode{1}, CardCode{2}, CardCode{3}};
	const auto result = edopro_next::data::save_ydk(full_device, deck);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
}

EDOPRO_DATA_TEST(loading_a_file_that_fails_mid_read_is_reported_as_a_failure_not_an_empty_deck) {
	const std::filesystem::path unreadable = "/proc/self/mem";
	if(!std::filesystem::exists(unreadable))
		return;
	const auto result = edopro_next::data::load_ydk(unreadable);
	EDOPRO_DATA_CHECK(!result.ok);
	EDOPRO_DATA_CHECK(!result.error.empty());
	EDOPRO_DATA_CHECK(result.deck.empty());
}

// ---------------------------------------------------------------------
// O) deterministic repeated serialization
// ---------------------------------------------------------------------

EDOPRO_DATA_TEST(serializing_the_same_deck_twice_produces_byte_identical_output) {
	Deck deck;
	deck.main = {CardCode{9}, CardCode{8}, CardCode{9}};
	deck.extra = {CardCode{1}};
	deck.side = {CardCode{2}, CardCode{3}};
	YdkWriteOptions options;
	options.creator = "SamePlayer";
	const auto first = edopro_next::data::serialize_ydk(deck, options);
	const auto second = edopro_next::data::serialize_ydk(deck, options);
	EDOPRO_DATA_CHECK_EQ(first, second);
}

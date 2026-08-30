// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins parse_lflist()/load_lflist() against the exact grammar
// gframe/deck_manager.cpp's DeckManager::LoadLFListSingle implements - see
// docs/architecture/deck-legality.md#lflist-grammar for the full citation.
// Several tests verify hash behaviour algebraically (via XOR properties)
// rather than against a hand-computed magic constant, since the formula
// itself (gframe/deck_manager.cpp:80) is not something a reader should have
// to re-derive by hand to trust these tests.

#include "edopro_next/policy/lf_list.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>

#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::policy::LfList;
using edopro_next::policy::LfListLoadResult;
using edopro_next::policy::load_lflist;
using edopro_next::policy::parse_lflist;

namespace {

std::optional<LfList> only_list(const std::string& text) {
	auto result = parse_lflist(text);
	if(result.lists.size() != 1)
		return std::nullopt;
	return result.lists.front();
}

} // namespace

EDOPRO_POLICY_TEST(emptyInputProducesNoLists) {
	auto result = parse_lflist("");
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 0u);
	EDOPRO_POLICY_CHECK_EQ(result.ignored.size(), 0u);
}

EDOPRO_POLICY_TEST(textBeforeFirstBangIsDroppedWithoutEffect) {
	// A content-shaped line, a comment, and a $whitelist directive all
	// appear before any `!Name` header - none of them should survive, and
	// the section that follows must be identical to parsing it alone.
	auto withPreamble = only_list("999 1\n#comment\n$whitelist\n!L\n5 1\n");
	auto alone = only_list("!L\n5 1\n");
	EDOPRO_POLICY_CHECK(withPreamble.has_value());
	EDOPRO_POLICY_CHECK(alone.has_value());
	EDOPRO_POLICY_CHECK_EQ(*withPreamble, *alone);
	EDOPRO_POLICY_CHECK_EQ(withPreamble->whitelist, false);
}

EDOPRO_POLICY_TEST(oneListWithOneEntry) {
	auto list = only_list("!Test List\n4 1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->name, std::string("Test List"));
	EDOPRO_POLICY_CHECK_EQ(list->content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(list->content.at(CardCode{4}), 1);
	EDOPRO_POLICY_CHECK_EQ(list->whitelist, false);
	EDOPRO_POLICY_CHECK(list->hash != 0);
}

EDOPRO_POLICY_TEST(multipleListsInOneFile) {
	auto result = parse_lflist("!A\n1 0\n!B\n2 1\n!C\n3 2\n");
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 3u);
	EDOPRO_POLICY_CHECK_EQ(result.lists[0].name, std::string("A"));
	EDOPRO_POLICY_CHECK_EQ(result.lists[1].name, std::string("B"));
	EDOPRO_POLICY_CHECK_EQ(result.lists[2].name, std::string("C"));
	EDOPRO_POLICY_CHECK_EQ(result.lists[0].content.at(CardCode{1}), 0);
	EDOPRO_POLICY_CHECK_EQ(result.lists[1].content.at(CardCode{2}), 1);
	EDOPRO_POLICY_CHECK_EQ(result.lists[2].content.at(CardCode{3}), 2);
}

EDOPRO_POLICY_TEST(emptyNamedListStillEmitted) {
	// A `!Name` header immediately followed by another header (or EOF)
	// still produces a list - its hash is the bare seed, which is nonzero.
	auto result = parse_lflist("!First\n!Second\n1 0\n");
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 2u);
	EDOPRO_POLICY_CHECK_EQ(result.lists[0].name, std::string("First"));
	EDOPRO_POLICY_CHECK_EQ(result.lists[0].content.size(), 0u);
	EDOPRO_POLICY_CHECK(result.lists[0].hash != 0);

	auto trailingEmpty = parse_lflist("!Only\n");
	EDOPRO_POLICY_CHECK_EQ(trailingEmpty.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(trailingEmpty.lists[0].content.size(), 0u);
	EDOPRO_POLICY_CHECK(trailingEmpty.lists[0].hash != 0);
}

EDOPRO_POLICY_TEST(hashCommentLinesAreSkipped) {
	auto withComment = only_list("!L\n#a comment\n4 1\n# another\n");
	auto withoutComment = only_list("!L\n4 1\n");
	EDOPRO_POLICY_CHECK(withComment.has_value());
	EDOPRO_POLICY_CHECK(withoutComment.has_value());
	EDOPRO_POLICY_CHECK_EQ(*withComment, *withoutComment);
}

EDOPRO_POLICY_TEST(crlfAndEmbeddedCrTruncation) {
	// A trailing CR (CRLF line ending) truncates to the same result as no
	// CR at all...
	auto crlf = only_list("!L\r\n4 1\r\n");
	auto plain = only_list("!L\n4 1\n");
	EDOPRO_POLICY_CHECK(crlf.has_value());
	EDOPRO_POLICY_CHECK(plain.has_value());
	EDOPRO_POLICY_CHECK_EQ(*crlf, *plain);

	// ...and an EMBEDDED CR (not at the end of the line) truncates
	// everything from that point on, not just a trailing pair - so "4 1\r99"
	// behaves exactly like "4 1" (the "99" after the CR never existed).
	auto embedded = only_list("!L\n4 1\r99\n");
	EDOPRO_POLICY_CHECK(embedded.has_value());
	EDOPRO_POLICY_CHECK_EQ(*embedded, *plain);
}

EDOPRO_POLICY_TEST(whitelistDirectiveSetsFlag) {
	auto list = only_list("!L\n$whitelist\n4 1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->whitelist, true);
}

EDOPRO_POLICY_TEST(whitelistIsAPrefixMatchNotExactEquality) {
	// gframe/deck_manager.cpp:62 tests with rfind(key, 0, key.size()) == 0 -
	// a prefix match. "$whitelist" followed by anything still matches.
	auto list = only_list("!L\n$whitelistTRUE\n4 1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->whitelist, true);
}

EDOPRO_POLICY_TEST(whitelistBeforeAnyListHasNoObservableEffect) {
	// $whitelist before the first !Name sets the flag on the not-yet-open
	// section, but the very next !Name line unconditionally resets
	// whitelist to false - so this can never survive to be observed.
	auto withLeadingWhitelist = only_list("$whitelist\n!L\n4 1\n");
	auto without = only_list("!L\n4 1\n");
	EDOPRO_POLICY_CHECK(withLeadingWhitelist.has_value());
	EDOPRO_POLICY_CHECK(without.has_value());
	EDOPRO_POLICY_CHECK_EQ(*withLeadingWhitelist, *without);

	// And if no !Name line ever appears at all, nothing is ever emitted,
	// regardless of $whitelist.
	auto neverOpened = parse_lflist("$whitelist\n4 1\n");
	EDOPRO_POLICY_CHECK_EQ(neverOpened.lists.size(), 0u);
}

EDOPRO_POLICY_TEST(codeZeroIsSilentlySkipped) {
	auto list = only_list("!L\n0 1\n4 1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->content.size(), 1u);
	EDOPRO_POLICY_CHECK(list->content.find(CardCode{0}) == list->content.end());
	EDOPRO_POLICY_CHECK_EQ(list->content.at(CardCode{4}), 1);
	// Matches an equivalent file with the code-0 line simply absent.
	auto without = only_list("!L\n4 1\n");
	EDOPRO_POLICY_CHECK(without.has_value());
	EDOPRO_POLICY_CHECK_EQ(*list, *without);
}

EDOPRO_POLICY_TEST(malformedCodeDropsTheLine) {
	auto list = only_list("!L\nabc 1\n4 1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(list->content.at(CardCode{4}), 1);
	auto parsed = parse_lflist("!L\nabc 1\n4 1\n");
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored[0].line_number, 2u);
}

EDOPRO_POLICY_TEST(malformedCountDropsTheLine) {
	// "4 abc" - code 4 parses, but the count field (starting right after
	// the first space) contains no digit at all, so std::stol throws.
	auto parsed = parse_lflist("!L\n4 abc\n5 1\n");
	EDOPRO_POLICY_CHECK_EQ(parsed.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.lists[0].content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.lists[0].content.at(CardCode{5}), 1);
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored[0].line_number, 2u);
}

EDOPRO_POLICY_TEST(noSpaceDropsTheLine) {
	auto parsed = parse_lflist("!L\n12345\n5 1\n");
	EDOPRO_POLICY_CHECK_EQ(parsed.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.lists[0].content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored.size(), 1u);
}

EDOPRO_POLICY_TEST(multipleSpacesBetweenCodeAndCountDropsTheLine) {
	// gframe/deck_manager.cpp's own literal-space parsing: with TWO spaces
	// ("4  1"), the count field runs from the FIRST space up to the first
	// non-digit/non-dash character after it - which is the SECOND space
	// itself, one character later. That leaves only the first space
	// character to hand to std::stol, which throws. Two spaces is
	// therefore NOT equivalent to one - this is upstream's own literal
	// behaviour, reproduced exactly rather than made more lenient.
	auto parsed = parse_lflist("!L\n4  1\n5 1\n");
	EDOPRO_POLICY_CHECK_EQ(parsed.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(parsed.lists[0].content.size(), 1u);
	EDOPRO_POLICY_CHECK(parsed.lists[0].content.find(CardCode{4}) == parsed.lists[0].content.end());
	EDOPRO_POLICY_CHECK_EQ(parsed.lists[0].content.at(CardCode{5}), 1);
	EDOPRO_POLICY_CHECK_EQ(parsed.ignored.size(), 1u);
}

EDOPRO_POLICY_TEST(negativeCountParsesWithinSafeDomain) {
	auto list = only_list("!L\n4 -1\n");
	EDOPRO_POLICY_CHECK(list.has_value());
	EDOPRO_POLICY_CHECK_EQ(list->content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(list->content.at(CardCode{4}), -1);
}

EDOPRO_POLICY_TEST(trailingTextAfterAValidCountIsTolerated) {
	auto withComment = only_list("!L\n4 1 --Some Card\n");
	auto without = only_list("!L\n4 1\n");
	EDOPRO_POLICY_CHECK(withComment.has_value());
	EDOPRO_POLICY_CHECK(without.has_value());
	EDOPRO_POLICY_CHECK_EQ(*withComment, *without);
}

EDOPRO_POLICY_TEST(duplicateCodeOverwritesContentButHashAccumulatesBoth) {
	auto single1 = only_list("!L\n7 1\n");
	auto single2 = only_list("!L\n7 2\n");
	auto duplicated = only_list("!L\n7 1\n7 2\n");
	auto empty = only_list("!L\n");
	EDOPRO_POLICY_CHECK(single1.has_value() && single2.has_value() && duplicated.has_value() &&
						 empty.has_value());

	// Content: last write wins - the final value is 2, not 1.
	EDOPRO_POLICY_CHECK_EQ(duplicated->content.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(duplicated->content.at(CardCode{7}), 2);

	// Hash: XOR is commutative/associative and self-cancelling, so if the
	// duplicated-line hash reflects BOTH contributions (not just the last
	// one), then single1.hash ^ single2.hash ^ empty.hash reconstructs it
	// exactly: (seed^t1) ^ (seed^t2) ^ seed == seed^t1^t2 == duplicated's
	// hash. Verified algebraically rather than against a magic constant.
	const std::uint32_t reconstructed = single1->hash ^ single2->hash ^ empty->hash;
	EDOPRO_POLICY_CHECK_EQ(duplicated->hash, reconstructed);
	// And this must differ from single2's hash alone - proving the first
	// line's contribution was NOT overwritten in the hash the way it was
	// in content.
	EDOPRO_POLICY_CHECK(duplicated->hash != single2->hash);
}

EDOPRO_POLICY_TEST(hashIsIndependentOfListNameAndPriorSections) {
	auto namedA = only_list("!A\n4 1\n");
	auto namedB = only_list("!B\n4 1\n");
	EDOPRO_POLICY_CHECK(namedA.has_value() && namedB.has_value());
	EDOPRO_POLICY_CHECK_EQ(namedA->hash, namedB->hash);

	// A section's hash is unaffected by an unrelated section before it -
	// each `!` line resets to the same fixed seed.
	auto result = parse_lflist("!Other\n999 3\n!B\n4 1\n");
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 2u);
	EDOPRO_POLICY_CHECK_EQ(result.lists[1].hash, namedB->hash);
}

EDOPRO_POLICY_TEST(hashUnsafeCountDomainFailsClosed) {
	// Derived in docs/architecture/deck-legality.md#hash-domain: upstream's
	// own hash expression (gframe/deck_manager.cpp:80) is undefined
	// behaviour in C++ for any count outside [-26, 4] - both shift amounts
	// it computes (27+count, 5-count) must be in [0, 31]. This module
	// fails closed for that domain: the line is dropped entirely, from
	// both `content` and `hash`, exactly like a malformed line.
	auto empty = only_list("!L\n");
	EDOPRO_POLICY_CHECK(empty.has_value());

	// One past the safe domain on each side.
	auto tooHigh = parse_lflist("!L\n4 5\n");
	auto tooLow = parse_lflist("!L\n4 -27\n");
	EDOPRO_POLICY_CHECK_EQ(tooHigh.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(tooHigh.lists[0].content.size(), 0u);
	EDOPRO_POLICY_CHECK_EQ(tooHigh.lists[0].hash, empty->hash);
	EDOPRO_POLICY_CHECK_EQ(tooHigh.ignored.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(tooLow.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(tooLow.lists[0].content.size(), 0u);
	EDOPRO_POLICY_CHECK_EQ(tooLow.lists[0].hash, empty->hash);
	EDOPRO_POLICY_CHECK_EQ(tooLow.ignored.size(), 1u);

	// The exact boundary values ARE accepted (both ends of [-26, 4]).
	auto highBoundary = only_list("!L\n4 4\n");
	auto lowBoundary = only_list("!L\n4 -26\n");
	EDOPRO_POLICY_CHECK(highBoundary.has_value());
	EDOPRO_POLICY_CHECK_EQ(highBoundary->content.size(), 1u);
	EDOPRO_POLICY_CHECK(highBoundary->hash != empty->hash);
	EDOPRO_POLICY_CHECK(lowBoundary.has_value());
	EDOPRO_POLICY_CHECK_EQ(lowBoundary->content.size(), 1u);
	EDOPRO_POLICY_CHECK(lowBoundary->hash != empty->hash);

	// An extreme, wildly out-of-domain value is rejected the same way -
	// not a crash, not a differently-shaped result.
	auto extreme = parse_lflist("!L\n4 999999999\n");
	EDOPRO_POLICY_CHECK_EQ(extreme.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(extreme.lists[0].content.size(), 0u);
	EDOPRO_POLICY_CHECK_EQ(extreme.lists[0].hash, empty->hash);
}

EDOPRO_POLICY_TEST(loadLflistFileRoundTrips) {
	const auto path = std::filesystem::temp_directory_path() / "edopro_next_policy_test_lflist.conf";
	{
		std::ofstream file(path, std::ios::binary);
		file << "!Test\n4 1\n";
	}
	auto result = load_lflist(path);
	EDOPRO_POLICY_CHECK(result.ok);
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 1u);
	EDOPRO_POLICY_CHECK_EQ(result.lists[0].name, std::string("Test"));
	std::filesystem::remove(path);
}

EDOPRO_POLICY_TEST(loadLflistMissingFileFails) {
	auto result = load_lflist("/definitely/does/not/exist.conf");
	EDOPRO_POLICY_CHECK(!result.ok);
	EDOPRO_POLICY_CHECK(!result.error.empty());
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 0u);
}

EDOPRO_POLICY_TEST(loadLflistDirectoryPathFailsCleanly) {
	// External review: on Unix, opening a directory as an ifstream can
	// succeed (open() on a directory succeeds), while any actual read from
	// it fails - the exact streambuf-vs-istream distinction load_lflist()
	// itself cites (see its own doc comment, and data/src/ydk.cpp's
	// load_ydk(), where the same fix originates and was verified
	// empirically). Must fail cleanly (ok == false), never silently report
	// success with empty or truncated data.
	auto result = load_lflist(std::filesystem::temp_directory_path());
	EDOPRO_POLICY_CHECK(!result.ok);
	EDOPRO_POLICY_CHECK(!result.error.empty());
	EDOPRO_POLICY_CHECK_EQ(result.lists.size(), 0u);
}

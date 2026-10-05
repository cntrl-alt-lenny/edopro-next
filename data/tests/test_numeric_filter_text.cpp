// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pins parse_numeric_filter() (numeric_filter_text.h) plus CardSearchIndex's
// numeric comparisons against an independent transcription of upstream's
// deck search: parse_filter (gframe/deck_con.cpp:21-51), BufferIO::GetVal
// (gframe/bufferio.h:240-249), the ATK/DEF/Level/Scale comparisons in
// CheckCardProperties (gframe/deck_con.cpp:1202-1227), and the card decode
// those comparisons read (DataManager's loader, gframe/data_manager.cpp:
// 137-153). For every input form and every stored value below, the set of
// cards this project keeps must equal the set upstream keeps.
//
// Synthetic cards only (synthetic_cdb.h).
#include "edopro_next/data/numeric_filter_text.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "edopro_next/data/card_database.h"
#include "edopro_next/data/card_search_index.h"
#include "synthetic_cdb.h"
#include "test_support.h"

using edopro_next::data::CardCode;
using edopro_next::data::CardDatabase;
using edopro_next::data::CardSearchIndex;
using edopro_next::data::NumericComparison;
using edopro_next::data::NumericFilter;
using edopro_next::data::NumericFilterField;
using edopro_next::data::parse_numeric_filter;
using edopro_next::data::SearchQuery;
using namespace edopro_next::data::testing;

namespace {

// ocgcore/ocgapi_constants.h:33, :56, :58.
constexpr std::uint32_t kTypeMonster = 0x1;
constexpr std::uint32_t kTypePendulum = 0x1000000;
constexpr std::uint32_t kTypeLink = 0x4000000;

// ---- The transcription of upstream. Written from the quoted lines, not from
// numeric_filter_text.cpp. ----

// gframe/bufferio.h:240-249.
std::uint32_t upstream_get_val(const char* pstr) {
	std::uint32_t ret = 0;
	while(*pstr >= '0' && *pstr <= '9') {
		ret = ret * 10 + static_cast<std::uint32_t>(*pstr - '0');
		pstr++;
	}
	if(*pstr == 0)
		return ret;
	return 0;
}

// gframe/deck_con.cpp:21-51.
int upstream_parse_filter(const char* pstr, std::uint32_t& type) {
	if(*pstr == '=') {
		type = 1;
		return static_cast<int>(upstream_get_val(pstr + 1));
	}
	if(*pstr >= '0' && *pstr <= '9') {
		type = 1;
		return static_cast<int>(upstream_get_val(pstr));
	}
	if(*pstr == '>') {
		if(*(pstr + 1) == '=') {
			type = 2;
			return static_cast<int>(upstream_get_val(pstr + 2));
		}
		type = 3;
		return static_cast<int>(upstream_get_val(pstr + 1));
	}
	if(*pstr == '<') {
		if(*(pstr + 1) == '=') {
			type = 4;
			return static_cast<int>(upstream_get_val(pstr + 2));
		}
		type = 5;
		return static_cast<int>(upstream_get_val(pstr + 1));
	}
	if(*pstr == '?') {
		type = 6;
		return 0;
	}
	type = 0;
	return 0;
}

// The fields CheckCardProperties reads, decoded as gframe/data_manager.cpp:
// 137-153 does.
struct UpstreamCard {
	std::uint32_t type;
	std::int32_t attack;
	std::int32_t defense;
	std::uint32_t level;
	std::uint32_t lscale;
};

UpstreamCard upstream_decode(const DataRow& row) {
	UpstreamCard cd{};
	cd.type = row.type;
	cd.attack = row.atk;
	cd.defense = row.def;
	if(cd.type & kTypeLink)
		cd.defense = 0;
	const int level = row.level;
	if(level < 0)
		cd.level = static_cast<std::uint32_t>(-(level & 0xff));
	else
		cd.level = static_cast<std::uint32_t>(level & 0xff);
	cd.lscale = static_cast<std::uint32_t>((level >> 24) & 0xff);
	return cd;
}

// gframe/deck_con.cpp:1202-1227: true when the card is kept. filter_atk and
// filter_def are int32_t, filter_lv and filter_scl uint32_t
// (gframe/deck_con.h:115-122).
bool upstream_keeps(NumericFilterField field, const std::string& text, const UpstreamCard& data) {
	std::uint32_t t = 0;
	const int parsed = upstream_parse_filter(text.c_str(), t);
	switch(field) {
	case NumericFilterField::Attack: {
		const std::int32_t filter_atk = parsed;
		if(t) {
			if((t == 1 && data.attack != filter_atk) || (t == 2 && data.attack < filter_atk)
			   || (t == 3 && data.attack <= filter_atk) || (t == 4 && (data.attack > filter_atk || data.attack < 0))
			   || (t == 5 && (data.attack >= filter_atk || data.attack < 0)) || (t == 6 && data.attack != -2))
				return false;
		}
		return true;
	}
	case NumericFilterField::Defense: {
		const std::int32_t filter_def = parsed;
		if(t) {
			if((t == 1 && data.defense != filter_def) || (t == 2 && data.defense < filter_def)
			   || (t == 3 && data.defense <= filter_def) || (t == 4 && (data.defense > filter_def || data.defense < 0))
			   || (t == 5 && (data.defense >= filter_def || data.defense < 0)) || (t == 6 && data.defense != -2)
			   || (data.type & kTypeLink))
				return false;
		}
		return true;
	}
	case NumericFilterField::Level: {
		const std::uint32_t filter_lv = static_cast<std::uint32_t>(parsed);
		if(t) {
			if((t == 1 && data.level != filter_lv) || (t == 2 && data.level < filter_lv)
			   || (t == 3 && data.level <= filter_lv) || (t == 4 && data.level > filter_lv)
			   || (t == 5 && data.level >= filter_lv) || t == 6)
				return false;
		}
		return true;
	}
	case NumericFilterField::Scale: {
		const std::uint32_t filter_scl = static_cast<std::uint32_t>(parsed);
		if(t) {
			if((t == 1 && data.lscale != filter_scl) || (t == 2 && data.lscale < filter_scl)
			   || (t == 3 && data.lscale <= filter_scl) || (t == 4 && (data.lscale > filter_scl))
			   || (t == 5 && (data.lscale >= filter_scl)) || t == 6
			   || !(data.type & kTypePendulum))
				return false;
		}
		return true;
	}
	}
	return false;
}

// ---- The cards and the inputs. ----

std::vector<DataRow> synthetic_rows() {
	std::vector<DataRow> rows;
	std::uint32_t id = 1;
	const std::int32_t stats[] = {-2, -1, 0, 1, 999, 1000, 1001, 2500, 2147483647};
	for(auto atk : stats) {
		DataRow row{id++};
		row.type = kTypeMonster;
		row.atk = atk;
		rows.push_back(row);
	}
	for(auto def : stats) {
		DataRow row{id++};
		row.type = kTypeMonster;
		row.def = def;
		rows.push_back(row);
		// The same DEF column on a Link monster holds link markers.
		DataRow link{id++};
		link.type = kTypeMonster | kTypeLink;
		link.def = def < 0 ? 1 : def;
		rows.push_back(link);
	}
	// Level column values, including a negative one (decoded to a wrapped
	// uint32_t, card-database.md §2.3).
	const std::int32_t levels[] = {0, 1, 4, 5, 12, 13, -249};
	for(auto level : levels) {
		DataRow row{id++};
		row.type = kTypeMonster;
		row.level = level;
		rows.push_back(row);
	}
	// Pendulum scales packed into the level column's high bytes, on a
	// Pendulum and on a non-Pendulum monster.
	const std::int32_t scales[] = {0, 1, 4, 5, 13};
	for(auto scale : scales) {
		for(bool pendulum : {true, false}) {
			DataRow row{id++};
			row.type = kTypeMonster | (pendulum ? kTypePendulum : 0);
			row.level = (scale << 24) | (scale << 16) | 4;
			rows.push_back(row);
		}
	}
	return rows;
}

std::vector<std::string> inputs() {
	std::vector<std::string> out;
	const char* prefixes[] = {"", "=", ">", ">=", "<", "<=", "?", "=?", ">?"};
	const char* numbers[] = {"",           "0",          "1",          "4",        "5",
							 "12",         "13",         "999",        "1000",     "2500",
							 "4294967295", "4294967294", "4294967296", "2147483648", "2147483647",
							 "1000a",      "-1",         " 1",         "4294967043"};
	for(const char* prefix : prefixes)
		for(const char* number : numbers)
			out.push_back(std::string(prefix) + number);
	for(const char* extra : {"-5", " 5", "abc", "=>5", "<>5", "=<5", "0x10", "?5", "??", "\xd9\xa3", ">==5",
							 "<<5", "5 "})
		out.emplace_back(extra);
	return out;
}

} // namespace

EDOPRO_DATA_TEST(every_numeric_input_form_keeps_exactly_the_cards_upstream_keeps) {
	const auto rows = synthetic_rows();
	TempFile file("numeric_filter_text");
	sqlite3* db = open_writable(file.path());
	create_datas_texts_schema(db);
	for(const auto& row : rows) {
		insert_data_row(db, row);
		TextRow text;
		text.id = row.id;
		text.name = "card" + std::to_string(row.id);
		insert_text_row(db, text);
	}
	sqlite3_close(db);
	CardDatabase catalogue;
	EDOPRO_DATA_CHECK(catalogue.load_database(file.path()).ok);
	CardSearchIndex index;
	index.rebuild(catalogue);

	int compared = 0;
	int filtered = 0;
	for(auto field : {NumericFilterField::Attack, NumericFilterField::Defense, NumericFilterField::Level,
					  NumericFilterField::Scale}) {
		for(const auto& text : inputs()) {
			std::set<std::uint32_t> upstream;
			for(const auto& row : rows)
				if(upstream_keeps(field, text, upstream_decode(row)))
					upstream.insert(row.id);

			SearchQuery query;
			const auto filter = parse_numeric_filter(text, field);
			switch(field) {
			case NumericFilterField::Attack:
				query.attack = filter;
				break;
			case NumericFilterField::Defense:
				query.defense = filter;
				break;
			case NumericFilterField::Level:
				query.level = filter;
				break;
			case NumericFilterField::Scale:
				query.left_scale = filter;
				break;
			}
			std::set<std::uint32_t> ours;
			for(const auto& result : index.search(query))
				ours.insert(edopro_next::data::to_number(result.code));

			if(ours != upstream) {
				std::string diff;
				for(auto id : upstream)
					if(!ours.count(id))
						diff += " upstream-only:" + std::to_string(id);
				for(auto id : ours)
					if(!upstream.count(id))
						diff += " ours-only:" + std::to_string(id);
				report_failure(__FILE__, __LINE__,
							   "field " + std::to_string(static_cast<int>(field)) + " input \"" + text +
								   "\" differs:" + diff);
			}
			++compared;
			if(upstream.size() != rows.size())
				++filtered;
		}
	}
	EDOPRO_DATA_CHECK_EQ(compared, 4 * static_cast<int>(inputs().size()));
	// The inputs are not all "no filter": most of them remove cards.
	EDOPRO_DATA_CHECK(filtered > compared / 2);
}

// Hand-written expectations for the forms whose meaning surprises, so a
// reader does not have to run the comparison above to see them.
EDOPRO_DATA_TEST(the_surprising_input_forms_mean_what_upstream_makes_them_mean) {
	using F = NumericFilterField;
	// No recognised first character: no filter at all.
	EDOPRO_DATA_CHECK(!parse_numeric_filter("", F::Attack));
	EDOPRO_DATA_CHECK(!parse_numeric_filter(" 1500", F::Attack));
	EDOPRO_DATA_CHECK(!parse_numeric_filter("-1", F::Attack));
	EDOPRO_DATA_CHECK(!parse_numeric_filter("abc", F::Level));
	// Trailing characters make the number 0, they do not cancel the filter.
	const auto trailing = parse_numeric_filter("1500a", F::Attack);
	EDOPRO_DATA_CHECK(trailing && trailing->value == 0 && trailing->comparison == NumericComparison::EqualTo);
	const auto bare_equal = parse_numeric_filter("=", F::Attack);
	EDOPRO_DATA_CHECK(bare_equal && bare_equal->value == 0);
	// Strict comparisons.
	const auto greater = parse_numeric_filter(">1500", F::Attack);
	EDOPRO_DATA_CHECK(greater && greater->value == 1501 && greater->comparison == NumericComparison::AtLeast);
	const auto less = parse_numeric_filter("<1500", F::Defense);
	EDOPRO_DATA_CHECK(less && less->value == 1499 && less->comparison == NumericComparison::AtMost &&
					  less->excludes_negative);
	// "At most" drops "?" stats on ATK/DEF only.
	EDOPRO_DATA_CHECK(parse_numeric_filter("<=4", F::Attack)->excludes_negative);
	EDOPRO_DATA_CHECK(!parse_numeric_filter("<=4", F::Level)->excludes_negative);
	// "?" is -2 on ATK/DEF, and nothing at all on Level/Scale.
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("?", F::Attack)->value, std::int64_t{-2});
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("?1000", F::Defense)->value, std::int64_t{-2});
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("?", F::Level)->value, std::int64_t{-1});
	// 32-bit wrap: ATK reads the bits as signed, Level as unsigned.
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("4294967294", F::Attack)->value, std::int64_t{-2});
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("4294967294", F::Level)->value, std::int64_t{4294967294});
	EDOPRO_DATA_CHECK_EQ(parse_numeric_filter("4294967296", F::Level)->value, std::int64_t{0});
}

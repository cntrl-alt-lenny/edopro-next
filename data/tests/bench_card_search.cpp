// A non-gating measurement harness, not a CTest case (data/CMakeLists.txt
// does not register it with add_test) - see
// docs/architecture/card-search.md#performance for why. Builds a
// synthetic catalogue at a scale comparable to (with margin over) a real
// Yu-Gi-Oh card pool and reports build/query timings; nothing here
// asserts a threshold. Entirely synthetic names/text - never real card
// data.
#include "edopro_next/data/card_database.h"
#include "edopro_next/data/card_search_index.h"
#include "edopro_next/data/search_query.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "synthetic_cdb.h"

using namespace edopro_next::data;
using namespace edopro_next::data::testing;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::uint32_t kCardCount = 22000;
constexpr std::uint32_t kTypeMonster = 0x1;

// Deterministic pseudo-random synthetic content generator - a fixed seed,
// not std::random_device, so this benchmark's dataset (and therefore its
// timings) are reproducible across runs.
class SyntheticContent {
public:
	explicit SyntheticContent(std::uint64_t seed) : rng_(seed) {}

	std::string name(std::uint32_t id) {
		static const char* adjectives[] = {"Ancient", "Blazing",  "Crimson", "Dark",   "Elder",
											"Frozen",  "Golden",   "Hidden",  "Iron",   "Jade",
											"Lunar",   "Mystic",   "Noble",   "Obsidian", "Prime",
											"Radiant", "Shadow",   "Twin",    "Umbral",  "Wild"};
		static const char* nouns[] = {"Dragon",  "Knight",  "Beast",  "Warrior", "Serpent",
									   "Phoenix", "Golem",   "Spirit", "Guardian", "Sorcerer",
									   "Paladin", "Wyrm",    "Hunter", "Sentinel", "Colossus"};
		std::uniform_int_distribution<std::size_t> adj_dist(0, std::size(adjectives) - 1);
		std::uniform_int_distribution<std::size_t> noun_dist(0, std::size(nouns) - 1);
		return std::string(adjectives[adj_dist(rng_)]) + " " + nouns[noun_dist(rng_)] + " #" +
			std::to_string(id);
	}

	std::string text() {
		static const char* fragments[] = {
			"When this card is Normal Summoned, you can target 1 monster on the field; ",
			"During your Main Phase, you can activate this effect; ",
			"If this card is destroyed by battle or card effect, ",
			"You can banish this card from your Graveyard to ",
			"Once per turn, during either player's turn, you can ",
			"This card gains 500 ATK for each monster it points to. ",
			"Cannot be destroyed by battle. Once per turn, ",
			"Draw 1 card, then discard 1 card. "};
		std::uniform_int_distribution<int> frag_count(2, 5);
		std::uniform_int_distribution<std::size_t> frag_dist(0, std::size(fragments) - 1);
		std::string out;
		const int count = frag_count(rng_);
		for(int i = 0; i < count; ++i)
			out += fragments[frag_dist(rng_)];
		return out;
	}

private:
	std::mt19937_64 rng_;
};

void build_synthetic_database(const std::filesystem::path& path, std::uint32_t count) {
	sqlite3* db = open_writable(path);
	create_datas_texts_schema(db);
	run(db, "BEGIN TRANSACTION;");
	SyntheticContent content(0xC0FFEE);
	for(std::uint32_t id = 1; id <= count; ++id) {
		DataRow row{};
		row.id = id;
		row.type = kTypeMonster;
		row.atk = static_cast<std::int32_t>(id % 5000);
		row.def = static_cast<std::int32_t>((id * 7) % 5000);
		row.level = id % 12 + 1;
		row.race = UINT64_C(1) << (id % 25);
		row.attribute = UINT32_C(1) << (id % 7);
		insert_data_row(db, row);
		insert_text_row(db, TextRow{id, content.name(id), content.text()});
	}
	run(db, "COMMIT;");
	sqlite3_close(db);
}

template <typename Fn>
double time_ms(Fn&& fn) {
	const auto start = Clock::now();
	fn();
	const auto end = Clock::now();
	return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
	std::printf("edopro_next card search benchmark\n");
	std::printf("dataset: %u synthetic cards, realistic-length name/text\n", kCardCount);

	TempFile file("benchmark");
	build_synthetic_database(file.path(), kCardCount);

	CardDatabase catalogue;
	const double load_ms = time_ms([&] {
		const auto result = catalogue.load_database(file.path());
		if(!result.ok) {
			std::fprintf(stderr, "failed to load synthetic database: %s\n", result.error.c_str());
			std::exit(1);
		}
	});
	std::printf("CardDatabase::load_database: %.2f ms (%zu cards)\n", load_ms, catalogue.size());

	CardSearchIndex index;
	const double build_ms = time_ms([&] { index.rebuild(catalogue); });
	std::printf("CardSearchIndex::rebuild:    %.2f ms\n", build_ms);
	const double rebuild_ms = time_ms([&] { index.rebuild(catalogue); });
	std::printf("CardSearchIndex::rebuild (2nd, warm): %.2f ms\n", rebuild_ms);

	auto measure = [&](const char* label, const SearchQuery& query, int iterations = 50) {
		std::size_t total_hits = 0;
		const double elapsed = time_ms([&] {
			for(int i = 0; i < iterations; ++i)
				total_hits += index.search(query).size();
		});
		std::printf("%-45s %8.4f ms/query  (avg %zu hits)\n", label, elapsed / iterations,
					total_hits / static_cast<std::size_t>(iterations));
	};

	SearchQuery exact_name;
	exact_name.text = "Ancient Dragon #500";
	measure("exact-name query", exact_name);

	SearchQuery prefix;
	prefix.text = "Ancient";
	measure("name-prefix query", prefix);

	SearchQuery broad_text;
	broad_text.text = "Draw 1 card";
	broad_text.text_scope = TextScope::Text;
	measure("broad text-scan query", broad_text);

	SearchQuery filtered;
	filtered.type = BitmaskFilter{kTypeMonster};
	filtered.attack = NumericFilter{2000, NumericComparison::AtLeast};
	filtered.level = NumericFilter{5, NumericComparison::AtLeast};
	measure("filtered (type+atk+level) query, no text", filtered);

	SearchQuery ranked_broad;
	ranked_broad.text = "Dragon";
	measure("ranked broad name query", ranked_broad);

	std::printf("done\n");
	return 0;
}

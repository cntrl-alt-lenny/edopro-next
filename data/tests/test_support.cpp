#include "test_support.h"

#include <cstdio>
#include <iostream>

namespace edopro_next::data::testing {
namespace {

int g_failures = 0;
const char* g_current = "";

} // namespace

std::vector<TestCase>& registry() {
	static std::vector<TestCase> cases;
	return cases;
}

void report_failure(const char* file, int line, const std::string& message) {
	++g_failures;
	std::cout << "  FAIL " << g_current << "\n    " << file << ":" << line << "\n    "
			  << message << "\n";
}

} // namespace edopro_next::data::testing

int main() {
	using namespace edopro_next::data::testing;
	int failed_cases = 0;
	for(const auto& test : registry()) {
		g_current = test.name;
		const int before = g_failures;
		test.body();
		const bool ok = g_failures == before;
		failed_cases += ok ? 0 : 1;
		std::cout << (ok ? "  ok   " : "  bad  ") << test.name << "\n";
	}
	std::cout << registry().size() << " tests, " << failed_cases << " failed, " << g_failures
			  << " assertions failed\n";
	return failed_cases == 0 ? 0 : 1;
}

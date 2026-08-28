// A very small test harness - the same shape as client/tests/test_support.h
// and for the same reason (docs/adr/0002-semantic-event-model.md, Decision
// 4): this module exists to have as few dependencies as practical, and
// pulling GoogleTest into its test build would undercut that for the sake of
// assertions that fit on one page. This is a fresh copy, not a shared
// header, because data/ has no other reason to reference client/ at all -
// see data/CMakeLists.txt.
#ifndef EDOPRO_NEXT_DATA_TEST_SUPPORT_H
#define EDOPRO_NEXT_DATA_TEST_SUPPORT_H

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace edopro_next::data::testing {

struct TestCase {
	const char* name;
	void (*body)();
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& message);

struct Register {
	Register(const char* name, void (*body)()) { registry().push_back({name, body}); }
};

template <typename T>
std::string describe(const T& value) {
	if constexpr(requires(std::ostream& os, const T& v) { os << v; }) {
		std::ostringstream stream;
		stream << value;
		return stream.str();
	} else if constexpr(std::is_enum_v<T>) {
		return std::to_string(static_cast<long long>(value));
	} else {
		return "<value>";
	}
}

} // namespace edopro_next::data::testing

#define EDOPRO_DATA_TEST(name)                                                                   \
	static void name();                                                                          \
	static const ::edopro_next::data::testing::Register edopro_register_##name(#name, &name);    \
	static void name()

#define EDOPRO_DATA_CHECK(expr)                                                                  \
	do {                                                                                          \
		if(!(expr))                                                                              \
			::edopro_next::data::testing::report_failure(__FILE__, __LINE__, "expected: " #expr); \
	} while(false)

#define EDOPRO_DATA_CHECK_EQ(actual, expected)                                                   \
	do {                                                                                          \
		auto edopro_actual = (actual);                                                           \
		auto edopro_expected = (expected);                                                       \
		if(!(edopro_actual == edopro_expected))                                                  \
			::edopro_next::data::testing::report_failure(                                        \
				__FILE__, __LINE__,                                                              \
				std::string(#actual) + " == " + #expected + "\n    actual:   " +                 \
					::edopro_next::data::testing::describe(edopro_actual) + "\n    expected: " + \
					::edopro_next::data::testing::describe(edopro_expected));                    \
	} while(false)

#endif // EDOPRO_NEXT_DATA_TEST_SUPPORT_H

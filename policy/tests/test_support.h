// A very small test harness - the same shape as data/tests/test_support.h
// and client/tests/test_support.h, and for the same reason
// (docs/adr/0002-semantic-event-model.md, Decision 4): this module exists
// to have as few dependencies as practical, and pulling GoogleTest into its
// test build would undercut that for the sake of assertions that fit on
// one page. This is a fresh copy, not a shared header, because policy/ has
// no other reason to reference data/'s tests (or client/'s) at all - see
// policy/CMakeLists.txt.
#ifndef EDOPRO_NEXT_POLICY_TEST_SUPPORT_H
#define EDOPRO_NEXT_POLICY_TEST_SUPPORT_H

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace edopro_next::policy::testing {

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

} // namespace edopro_next::policy::testing

#define EDOPRO_POLICY_TEST(name)                                                                 \
	static void name();                                                                          \
	static const ::edopro_next::policy::testing::Register edopro_register_##name(#name, &name);  \
	static void name()

#define EDOPRO_POLICY_CHECK(expr)                                                                \
	do {                                                                                          \
		if(!(expr))                                                                              \
			::edopro_next::policy::testing::report_failure(__FILE__, __LINE__, "expected: " #expr); \
	} while(false)

#define EDOPRO_POLICY_CHECK_EQ(actual, expected)                                                 \
	do {                                                                                          \
		auto edopro_actual = (actual);                                                           \
		auto edopro_expected = (expected);                                                       \
		if(!(edopro_actual == edopro_expected))                                                  \
			::edopro_next::policy::testing::report_failure(                                      \
				__FILE__, __LINE__,                                                              \
				std::string(#actual) + " == " + #expected + "\n    actual:   " +                 \
					::edopro_next::policy::testing::describe(edopro_actual) + "\n    expected: " + \
					::edopro_next::policy::testing::describe(edopro_expected));                  \
	} while(false)

#endif // EDOPRO_NEXT_POLICY_TEST_SUPPORT_H

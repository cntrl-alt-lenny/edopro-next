// A very small test harness.
//
// The client library exists to have no dependencies. Pulling GoogleTest into
// its test build would undercut that for the sake of assertions that fit on
// one page, so this is what the suites use instead. It registers test cases,
// runs them all, prints failures with file and line, and exits non-zero if any
// failed - which is the whole of what CTest needs.
//
// EDOPRO_CHECK_EQ copies its operands rather than binding references to them.
// The values compared here are small, and a reference would dangle whenever an
// argument is a temporary - std::optional::value() on a returned optional, for
// instance.
#ifndef EDOPRO_NEXT_CLIENT_TEST_SUPPORT_H
#define EDOPRO_NEXT_CLIENT_TEST_SUPPORT_H

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace edopro_next::testing {

struct TestCase {
	const char* name;
	void (*body)();
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& message);

struct Register {
	Register(const char* name, void (*body)()) { registry().push_back({name, body}); }
};

// Renders a value for a failure message when it can be streamed, and says so
// plainly when it cannot, rather than refusing to compile.
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

} // namespace edopro_next::testing

#define EDOPRO_TEST(name)                                                                     \
	static void name();                                                                       \
	static const ::edopro_next::testing::Register edopro_register_##name(#name, &name);       \
	static void name()

#define EDOPRO_CHECK(expr)                                                                    \
	do {                                                                                      \
		if(!(expr))                                                                           \
			::edopro_next::testing::report_failure(__FILE__, __LINE__,                        \
												   "expected: " #expr);                       \
	} while(false)

#define EDOPRO_CHECK_EQ(actual, expected)                                                     \
	do {                                                                                      \
		auto edopro_actual = (actual);                                                        \
		auto edopro_expected = (expected);                                                    \
		if(!(edopro_actual == edopro_expected))                                               \
			::edopro_next::testing::report_failure(                                           \
				__FILE__, __LINE__,                                                           \
				std::string(#actual) + " == " + #expected + "\n    actual:   " +              \
					::edopro_next::testing::describe(edopro_actual) + "\n    expected: " +    \
					::edopro_next::testing::describe(edopro_expected));                       \
	} while(false)

#endif // EDOPRO_NEXT_CLIENT_TEST_SUPPORT_H

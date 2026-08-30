// Real upstream .ydk interoperability proof: hands a `.ydk` produced by this
// project's own edopro_next::data::save_ydk() to the real, unmodified
// DeckManager::LoadDeckFromFile() and compares the resulting ygo::Deck
// against independently-derived expected structures. See
// docs/architecture/ydk-interoperability.md for exactly what this does and
// does not prove, and docs/adr/0008-upstream-ydk-interop-harness.md for why
// it is built this way. Follows the same C++17-safe extern "C" boundary
// replay_verifier.h already establishes: gframe never sees a C++20 type from
// this header, only these two plain function declarations.
#ifndef EDOPRO_NEXT_YDK_INTEROP_H
#define EDOPRO_NEXT_YDK_INTEROP_H

extern "C" {
int edopro_next_verify_ydk_interop_cli() noexcept;
int edopro_next_verify_ydk_interop_fault_cli() noexcept;
}

#if defined(__cplusplus) && __cplusplus >= 202002L

namespace edopro_next::legacy_observer {

// Two fixed-fixture loads are performed - separated=false and
// separated=true (docs/architecture/ydk-interoperability.md#separated-modes)
// - each compared as a whole (Main+Extra+Side together) against its own
// independently-derived expected result.
struct YdkInteropStats {
	bool completed = false;
	int modes_compared = 0;
	int modes_matched = 0;

	bool equivalent() const noexcept {
		return completed && modes_compared > 0 && modes_matched == modes_compared;
	}
};

YdkInteropStats verify_ydk_interop(bool inject_fault = false);
int verify_ydk_interop_cli(bool inject_fault = false);

} // namespace edopro_next::legacy_observer

#endif // defined(__cplusplus) && __cplusplus >= 202002L

#endif // EDOPRO_NEXT_YDK_INTEROP_H

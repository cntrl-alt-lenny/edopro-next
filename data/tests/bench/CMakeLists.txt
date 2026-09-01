# bench_card_search lives in its own directory scope purely so its
# CMAKE_CXX_FLAGS_DEBUG can differ from the rest of data/tests/. See the
# add_subdirectory() call site in data/CMakeLists.txt for why: this is a
# non-gating measurement executable (docs/architecture/card-search.md#performance),
# deliberately built with /O2 even under a Debug configure, and MSVC's
# Debug-injected /RTC1 is a hard command-line error in combination with /O2 --
# not a warning, so /WX is irrelevant to it either way.
#
# The source file itself stays in data/tests/, next to the suite it measures;
# only the CMake target isolation needed a directory boundary.

if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

add_executable(bench_card_search ../bench_card_search.cpp)
target_link_libraries(bench_card_search PRIVATE edopro_next_search SQLite::SQLite3)
if(MSVC)
    target_compile_options(bench_card_search PRIVATE /W4 /O2)
else()
    target_compile_options(bench_card_search PRIVATE -Wall -Wextra -O2)
endif()

cmake_minimum_required(VERSION 3.21)

# Presentation-independent deck legality/policy foundation - a sibling of
# data/, not a part of it: data/'s own docs (card-database.md,
# card-search.md) repeatedly and explicitly describe legality as belonging
# in "an explicitly separate, explicitly reviewed layer above data/", not
# inside data/ itself - see docs/adr/0007-deck-legality-policy-module.md
# for the full boundary decision. Builds with a C++20 compiler and nothing
# this project doesn't already require - no Qt, no QML, no Irrlicht, no
# gframe, no ocgcore. See docs/architecture/deck-legality.md.
project(edopro_next_legality
    VERSION 0.1.0
    DESCRIPTION "edopro-next presentation-independent deck legality/policy foundation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EDOPRO_NEXT_LEGALITY_TESTS "Build the deck legality/policy tests" ON)
option(EDOPRO_NEXT_WERROR "Treat compiler warnings as errors" OFF)

# data/ is a sibling project, pulled in so `cmake -S policy -B policy/build`
# is self-sufficient on its own (matching data/'s own "builds standalone"
# philosophy) rather than requiring a separate umbrella build. This does
# mean CI configures/builds data/ a second time inside policy/build/_data
# when the "Card and deck data" job also builds data/ on its own first (see
# .github/workflows/edopro-next.yml) - accepted as a small, deliberate CI
# cost in exchange for policy/ never depending on being configured in a
# particular order relative to data/. data/'s own test suite is skipped
# here (EDOPRO_NEXT_DATA_TESTS OFF): it already runs, and is already
# reported on, via data/'s own standalone build.
if(NOT TARGET edopro_next_data)
    set(EDOPRO_NEXT_DATA_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../data ${CMAKE_CURRENT_BINARY_DIR}/_data)
endif()

add_library(edopro_next_legality STATIC
    src/lf_list.cpp
    src/deck_validation.cpp
)

target_include_directories(edopro_next_legality
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Explicit dependency on BOTH edopro_next_data (CardDatabase/CardRecord) and
# edopro_next_deck (Deck) - Deck is currently header-only (deck.h has no
# corresponding .cpp), so linking edopro_next_data alone would happen to
# compile today purely because both targets expose the same shared
# data/include tree as a PUBLIC include directory. That would hide a real
# conceptual dependency behind an accident of the current header/source
# split. Declaring both here means this target's dependency graph stays
# correct even if Deck ever grows a translation unit of its own, or if the
# header/library split changes - see docs/adr/0007.
target_link_libraries(edopro_next_legality PUBLIC edopro_next_data edopro_next_deck)

if(MSVC)
    target_compile_options(edopro_next_legality PRIVATE /W4 /permissive-)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_legality PRIVATE /WX)
    endif()
else()
    target_compile_options(edopro_next_legality PRIVATE -Wall -Wextra -Wpedantic)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_legality PRIVATE -Werror)
    endif()
endif()

if(EDOPRO_NEXT_LEGALITY_TESTS)
    enable_testing()

    # No test framework dependency, matching data/'s own tests - see
    # data/CMakeLists.txt's comment and data/tests/test_support.h. This is a
    # fresh copy, not a shared include, for the same reason data/'s own copy
    # is not shared with client/: a test-only include across a module
    # boundary is exactly the kind of accidental coupling these boundaries
    # exist to avoid.
    add_library(edopro_next_legality_test_main STATIC tests/test_support.cpp)
    target_include_directories(edopro_next_legality_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(edopro_next_legality_test_main PUBLIC edopro_next_legality)

    add_executable(test_lf_list tests/test_lf_list.cpp)
    target_link_libraries(test_lf_list PRIVATE edopro_next_legality_test_main)
    if(MSVC)
        target_compile_options(test_lf_list PRIVATE /W4)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_lf_list PRIVATE /WX)
        endif()
    else()
        target_compile_options(test_lf_list PRIVATE -Wall -Wextra)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_lf_list PRIVATE -Werror)
        endif()
    endif()
    add_test(NAME lf_list COMMAND test_lf_list)

    # Needs SQLite directly (not just through edopro_next_data) to build its
    # own synthetic .cdb fixtures via the raw sqlite3 C API - the same
    # pattern data/tests/synthetic_cdb.h and ui/tests both already use.
    # edopro_next_data's own SQLite dependency stays private to that
    # library; this is a second, independent, test-only use of SQLite.
    find_package(SQLite3 REQUIRED)

    add_executable(test_deck_validation tests/test_deck_validation.cpp)
    target_link_libraries(test_deck_validation PRIVATE edopro_next_legality_test_main SQLite::SQLite3)
    if(MSVC)
        target_compile_options(test_deck_validation PRIVATE /W4)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_deck_validation PRIVATE /WX)
        endif()
    else()
        target_compile_options(test_deck_validation PRIVATE -Wall -Wextra)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_deck_validation PRIVATE -Werror)
        endif()
    endif()
    add_test(NAME deck_validation COMMAND test_deck_validation)
endif()

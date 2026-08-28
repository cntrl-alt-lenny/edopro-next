cmake_minimum_required(VERSION 3.21)

# The card and deck data facades. Deliberately standalone, like client/:
# they build with a C++20 compiler and nothing else this project doesn't
# already require - no Qt, no QML, no Irrlicht, no ocgcore, no gframe, no
# legacy DataManager. The card database (edopro_next_data) additionally
# needs SQLite; the deck model and .ydk codec (edopro_next_deck) are two
# separate targets specifically so the latter needs neither SQLite nor the
# former - reading or writing a deck never requires a card database. If
# either target ever needs one of those excluded dependencies to compile,
# the separation this module exists to create has been broken. See
# docs/adr/0003-card-database-facade.md and
# docs/adr/0004-deck-model-ydk-codec.md.
project(edopro_next_data
    VERSION 0.1.0
    DESCRIPTION "edopro-next presentation-independent card and deck data facades"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EDOPRO_NEXT_DATA_TESTS "Build the card and deck data tests" ON)
option(EDOPRO_NEXT_WERROR "Treat compiler warnings as errors" OFF)

find_package(SQLite3 REQUIRED)

add_library(edopro_next_data STATIC
    src/card_database.cpp
)

target_include_directories(edopro_next_data
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# SQLite is linked but not exposed: no sqlite3.h include, no sqlite3*/
# sqlite3_stmt* type, ever appears in include/. A caller of this library
# cannot observe that it uses SQLite at all.
target_link_libraries(edopro_next_data PRIVATE SQLite::SQLite3)

# Warnings are part of the contract: this library parses attacker-influenced
# bytes (a downloaded .cdb is untrusted input), so a sign-conversion or
# truncation warning is worth reading.
if(MSVC)
    target_compile_options(edopro_next_data PRIVATE /W4 /permissive-)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_data PRIVATE /WX)
    endif()
else()
    target_compile_options(edopro_next_data PRIVATE -Wall -Wextra -Wpedantic)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_data PRIVATE -Werror)
    endif()
endif()

# The deck model and .ydk codec - see the module-level comment above for
# why this is a separate target rather than another source file in
# edopro_next_data: it must never link SQLite, and this is what proves
# that rather than merely documenting it.
add_library(edopro_next_deck STATIC
    src/ydk.cpp
)

target_include_directories(edopro_next_deck
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(edopro_next_deck PRIVATE /W4 /permissive-)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_deck PRIVATE /WX)
    endif()
else()
    target_compile_options(edopro_next_deck PRIVATE -Wall -Wextra -Wpedantic)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_deck PRIVATE -Werror)
    endif()
endif()

if(EDOPRO_NEXT_DATA_TESTS)
    enable_testing()

    # No test framework dependency, for the same reason client/tests has
    # none (docs/adr/0002-semantic-event-model.md, Decision 4): the
    # assertions this suite needs fit in one header. This is a fresh copy of
    # that header rather than an include of client/tests/test_support.h,
    # because this module has no other reason to reference client/ at all,
    # and a test-only include would be exactly the kind of accidental
    # coupling the module boundary exists to avoid.
    add_library(edopro_next_data_test_main STATIC tests/test_support.cpp)
    target_include_directories(edopro_next_data_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(edopro_next_data_test_main PUBLIC edopro_next_data)

    add_executable(test_card_database tests/test_card_database.cpp)
    target_link_libraries(test_card_database PRIVATE edopro_next_data_test_main SQLite::SQLite3)
    if(MSVC)
        target_compile_options(test_card_database PRIVATE /W4)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_card_database PRIVATE /WX)
        endif()
    else()
        target_compile_options(test_card_database PRIVATE -Wall -Wextra)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_card_database PRIVATE -Werror)
        endif()
    endif()
    add_test(NAME card_database COMMAND test_card_database)

    # A second, independent copy of the harness library (same source file
    # as edopro_next_data_test_main above), linked against edopro_next_deck
    # instead - so this suite's own link graph proves it never pulls in
    # SQLite or edopro_next_data, not just a comment claiming so.
    add_library(edopro_next_deck_test_main STATIC tests/test_support.cpp)
    target_include_directories(edopro_next_deck_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(edopro_next_deck_test_main PUBLIC edopro_next_deck)

    add_executable(test_deck_ydk tests/test_deck_ydk.cpp)
    target_link_libraries(test_deck_ydk PRIVATE edopro_next_deck_test_main)
    if(MSVC)
        target_compile_options(test_deck_ydk PRIVATE /W4)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_deck_ydk PRIVATE /WX)
        endif()
    else()
        target_compile_options(test_deck_ydk PRIVATE -Wall -Wextra)
        if(EDOPRO_NEXT_WERROR)
            target_compile_options(test_deck_ydk PRIVATE -Werror)
        endif()
    endif()
    add_test(NAME deck_ydk COMMAND test_deck_ydk)
endif()

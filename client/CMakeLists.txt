cmake_minimum_required(VERSION 3.21)

# The semantic client model. Deliberately standalone, like ui/: it builds with
# nothing but a C++20 compiler - no Qt, no QML, no Irrlicht, no ocgcore, no
# card database. If this ever needs one of those to compile, the separation
# this milestone exists to create has been broken.
project(edopro_next_client
    VERSION 0.1.0
    DESCRIPTION "edopro-next presentation-free semantic duel model"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EDOPRO_NEXT_CLIENT_TESTS "Build the semantic client tests" ON)
option(EDOPRO_NEXT_WERROR "Treat compiler warnings as errors" OFF)

add_library(edopro_next_client STATIC
    src/card_types.cpp
    src/duel_event.cpp
    src/duel_state.cpp
    src/packet.cpp
    src/protocol_decoder.cpp
    src/semantic_trace.cpp
)

add_library(edopro_next_legacy_observer_model STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/legacy/observer_model.cpp
)
target_include_directories(edopro_next_legacy_observer_model
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../integration/legacy
)
target_link_libraries(edopro_next_legacy_observer_model PUBLIC edopro_next_client)

target_include_directories(edopro_next_client
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings are part of the contract: this library parses attacker-influenced
# bytes, so a sign-conversion or unused-result warning is worth reading.
if(MSVC)
    target_compile_options(edopro_next_client PRIVATE /W4 /permissive-)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_client PRIVATE /WX)
    endif()
else()
    target_compile_options(edopro_next_client PRIVATE -Wall -Wextra -Wpedantic)
    if(EDOPRO_NEXT_WERROR)
        target_compile_options(edopro_next_client PRIVATE -Werror)
    endif()
endif()

add_executable(edopro_next_semantic_trace tools/semantic_trace_main.cpp)
target_link_libraries(edopro_next_semantic_trace PRIVATE edopro_next_client)

if(EDOPRO_NEXT_CLIENT_TESTS)
    enable_testing()

    # No test framework dependency. The assertions this suite needs fit in one
    # header, and adding GoogleTest to a library whose whole point is having no
    # dependencies would be a poor trade. See docs/adr/0002.
    add_library(edopro_next_client_test_main STATIC tests/test_support.cpp)
    target_include_directories(edopro_next_client_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(edopro_next_client_test_main PUBLIC edopro_next_client)

    foreach(suite duel_state protocol_decoder hidden_information semantic_trace
                  transactional_decoding legacy_perspective legacy_observer)
        add_executable(test_${suite} tests/test_${suite}.cpp)
        target_link_libraries(test_${suite} PRIVATE edopro_next_client_test_main)
        if(suite STREQUAL legacy_observer)
            target_link_libraries(test_${suite} PRIVATE edopro_next_legacy_observer_model)
        endif()
        if(MSVC)
            target_compile_options(test_${suite} PRIVATE /W4)
            if(EDOPRO_NEXT_WERROR)
                target_compile_options(test_${suite} PRIVATE /WX)
            endif()
        else()
            target_compile_options(test_${suite} PRIVATE -Wall -Wextra)
            if(EDOPRO_NEXT_WERROR)
                target_compile_options(test_${suite} PRIVATE -Werror)
            endif()
        endif()
        add_test(NAME ${suite} COMMAND test_${suite})
    endforeach()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

# Recompiles the same deckbuilder sources as edopro_next_shell itself,
# rather than linking a library extracted from that executable - see
# ../CMakeLists.txt's own comment: qt_add_qml_module's type registration
# scans the sources listed directly on the target it is invoked for, and
# this test target needs no QML registration at all (it never loads the
# QML engine, only constructs the C++ classes directly), so recompiling
# once here is the simpler, lower-risk choice over threading a second
# target through qt_add_qml_module.
add_executable(test_deckbuilder
    test_deckbuilder.cpp
    ${EDOPRO_NEXT_SHELL_DECKBUILDER_SOURCES}
)

target_include_directories(test_deckbuilder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/deckbuilder
)

target_link_libraries(test_deckbuilder PRIVATE
    Qt6::Test
    Qt6::Qml
    edopro_next_data
    edopro_next_deck
    edopro_next_search
    SQLite::SQLite3
)

add_test(NAME deckbuilder COMMAND test_deckbuilder)

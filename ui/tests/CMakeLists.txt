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

# ---- test_deckbuilder_screen: the real DeckBuilderScreen.qml, loaded
# through an actual QQmlEngine - not just the C++ adapter classes in
# isolation the way test_deckbuilder (above) does. Needs its own
# qt_add_qml_module under the same "EdoproNext" URI: Qt's qmltyperegistrar
# generates CardCatalog/DeckController/DeckSectionModel/SearchResultsModel
# registration per target, scoped to this executable's own compiled-in
# resources - it does not collide with edopro_next_shell's separate module
# of the same URI, since each binary resolves only its own resources.
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2)

add_executable(test_deckbuilder_screen
    test_deckbuilder_screen.cpp
    ${EDOPRO_NEXT_SHELL_DECKBUILDER_SOURCES}
)

target_include_directories(test_deckbuilder_screen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/deckbuilder
)

# Same requirement as ../CMakeLists.txt's own Theme.qml registration -
# pragma Singleton in the file is not enough on its own without also
# telling the module system at configure time.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# qt_add_qml_module's resource system cannot derive an implicit in-module
# path from a source file given as an absolute path outside this target's
# own source directory (unlike ../CMakeLists.txt's QML_FILES, which are
# all relative to ui/ itself) - each such file needs an explicit
# QT_RESOURCE_ALIAS naming its place in *this* module. A flat alias (just
# the bare filename) is enough: nothing in these files locates another by
# relative resource path - they resolve each other purely by QML type name
# through the shared "EdoproNext" import, exactly like the shell's own
# module does.
foreach(qml_file IN ITEMS Theme.qml components/SectionHeading.qml
                          components/DeckSectionList.qml components/CardPreview.qml
                          screens/DeckBuilderScreen.qml)
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file}
        PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

qt_add_qml_module(test_deckbuilder_screen
    URI EdoproNext
    VERSION 1.0
    QML_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/Theme.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/components/SectionHeading.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/components/DeckSectionList.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/components/CardPreview.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/screens/DeckBuilderScreen.qml
        TestHarness.qml
)

target_link_libraries(test_deckbuilder_screen PRIVATE
    Qt6::Test
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    edopro_next_data
    edopro_next_deck
    edopro_next_search
    SQLite::SQLite3
)

add_test(NAME deckbuilder_screen COMMAND test_deckbuilder_screen)
# This test creates a real QQuickWindow (TestHarness.qml) and must render
# it, unlike test_deckbuilder above (pure C++ object construction, no QML
# engine at all) - offscreen matches the platform the shell's own smoke
# test and visual-verification captures already use successfully in CI.
set_tests_properties(deckbuilder_screen PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

cmake_minimum_required(VERSION 3.21)

project(edopro_next_shell
    VERSION 0.0.1
    DESCRIPTION "edopro-next modern client shell"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# QtQuick.Dialogs (FileDialog, used by the deck builder's Open/Save As) is
# QuickDialogs2 at the CMake-component level, not a separate find_package.
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2)

qt_standard_project_setup(REQUIRES 6.5)

# The deck builder screen (docs/architecture/deck-builder-ui.md) needs the
# reviewed M3 data/deck/search facades - Qt-free by design (data/'s own
# CMakeLists.txt) and nested here as a sibling CMake project, not copied:
# add_subdirectory on a directory outside this tree needs an explicit binary
# dir, since it is not implied by the source layout the way an in-tree
# subdirectory's would be.
#
# EDOPRO_NEXT_DATA_TESTS is forced off here: data/'s own CTest suites
# (card_database, deck_ydk, card_search) are exercised on their own by the
# "Card and deck data" CI job against data/'s standalone build - building
# and linking three more test executables (plus SQLite twice, once per
# build tree) on every shell build would be pure waste with no additional
# coverage.
set(EDOPRO_NEXT_DATA_TESTS OFF CACHE BOOL "Data/ tests are exercised by their own CI job, not the shell build" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../data ${CMAKE_CURRENT_BINARY_DIR}/data)

# find_package's own SQLite3::SQLite3/SQLite::SQLite3 imported target is
# scoped to the directory that created it and that directory's
# subdirectories - data/'s own find_package(SQLite3) call (above) is
# therefore invisible outside data/'s own tree, including here. The
# edopro_next_data/edopro_next_search *library* targets remain visible
# everywhere regardless (ordinary add_library targets are global), so this
# second call only matters for code that names the SQLite import target
# directly - tests/CMakeLists.txt's synthetic-.cdb test fixture, which
# talks to sqlite3 directly the same way data/tests/ does.
find_package(SQLite3 REQUIRED)

# Build metadata is injected, never hardcoded in QML, so the shell can always
# report what it actually is rather than what someone once typed.
# Allow the SHA to be supplied (-DEDOPRO_NEXT_GIT_SHA=...) so builds from a
# tarball, an export, or a CI checkout without history still identify
# themselves. Only fall back to querying git when nothing was provided.
if(NOT EDOPRO_NEXT_GIT_SHA)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE EDOPRO_NEXT_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT EDOPRO_NEXT_GIT_SHA)
    set(EDOPRO_NEXT_GIT_SHA "unknown")
endif()

# Listed as a plain variable (not a separate library target) so that
# qt_add_qml_module's type registration - which scans the sources actually
# attached to *this* target - sees every QML_ELEMENT class directly. See
# tests/CMakeLists.txt's own comment for why the test executable recompiles
# these same files instead of linking a shared library built from them.
#
# Absolute paths, deliberately: a relative path in this variable would
# resolve against whichever directory *uses* it (tests/CMakeLists.txt's
# own CMAKE_CURRENT_SOURCE_DIR), not against where it was defined here.
set(EDOPRO_NEXT_SHELL_DECKBUILDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/card_entry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/card_entry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/card_catalog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/card_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/search_results_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/search_results_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/deck_section_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/deck_section_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/deck_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder/deck_controller.cpp
)

qt_add_executable(edopro_next_shell
    src/main.cpp
    src/appcontext.cpp
    src/appcontext.h
    ${EDOPRO_NEXT_SHELL_DECKBUILDER_SOURCES}
)

# The QML type registration Qt generates includes headers like
# "appcontext.h"/"card_catalog.h" by bare filename, so src/ (and its
# deckbuilder/ subdirectory) must be on the include path.
target_include_directories(edopro_next_shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deckbuilder
)

target_compile_definitions(edopro_next_shell PRIVATE
    EDOPRO_NEXT_GIT_SHA="${EDOPRO_NEXT_GIT_SHA}"
    EDOPRO_NEXT_VERSION="${PROJECT_VERSION}"
)

# pragma Singleton in the QML file is not sufficient on its own: the module
# system must also be told at configure time, or Theme resolves to undefined
# at runtime with no build-time error.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(edopro_next_shell
    URI EdoproNext
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/NavRail.qml
        qml/components/NavButton.qml
        qml/components/StatusRow.qml
        qml/components/SectionHeading.qml
        qml/components/DeckSectionList.qml
        qml/components/CardPreview.qml
        qml/screens/HomeScreen.qml
        qml/screens/NotImplementedScreen.qml
        qml/screens/DeckBuilderScreen.qml
)

target_link_libraries(edopro_next_shell PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    edopro_next_data
    edopro_next_deck
    edopro_next_search
)

if(EDOPRO_NEXT_UI_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

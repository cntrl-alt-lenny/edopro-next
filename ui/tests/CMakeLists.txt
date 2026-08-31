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

# qt_add_qml_module's resource system cannot derive an implicit in-module
# path from a source file given as an absolute path outside this target's
# own source directory (unlike ../CMakeLists.txt's QML_FILES, which are
# all relative to ui/ itself) - each such file needs an explicit
# QT_RESOURCE_ALIAS naming its place in *this* module. A flat alias (just
# the bare filename) is enough: nothing in these files locates another by
# relative resource path - they resolve each other purely by QML type name
# through the shared "EdoproNext" import, exactly like the shell's own
# module does.
#
# The alias fixes the *runtime resource path*, but qmlcachegen separately
# derives each file's *build-tree cache directory* from its path relative
# to this CMakeLists.txt, regardless of QT_RESOURCE_ALIAS - and that
# relative path is "../qml/...", because these files genuinely live one
# directory up. On Windows, qmlcachegen embeds that literal ".." into a
# directory it tries to create (e.g.
# "tests/.rcc/qmlcache/test_deckbuilder_screen_../qml"), and mkdir()
# rejects it outright (".." is not a legal path component to create, not
# just a resolution ambiguity).
#
# Two things were tried and rejected before NO_CACHEGEN below, both
# recorded so they are not tried again:
#   - Registering this module from ui/'s own directory instead (where
#     these files' real relative path has no ".."): makes qt_add_qml_module
#     see two calls for the same "EdoproNext" URI in one directory scope,
#     and edopro_next_shell's own Main.qml stopped resolving at runtime as
#     a result ("Module "EdoproNext" contains no type named "Main""),
#     confirmed reproducible and confirmed absent before that change -
#     trading a build-time failure in the test target for a runtime
#     failure in the shipped shell.
#   - Mirroring each file into a same-directory location (a symlink,
#     falling back to a plain copy via COPY_ON_ERROR where the host can't
#     create one) so qmlcachegen never saw the real, "..".-carrying path.
#     This built, but the copy-fallback path only refreshed at CMake
#     *configure* time: editing a QML file and running a plain
#     `cmake --build` left the mirror stale, so the test silently compiled
#     and passed against the *previous* version of the file (brief 006,
#     Correction B). CREATE_LINK SYMBOLIC succeeds on Linux, so CI's own
#     symlink path never exercised the bug - Windows-only and invisible to
#     the pipeline, discovered only by deliberately reproducing it on this
#     machine.
#
# NO_CACHEGEN removes the cause both fixes were working around, rather
# than working around it again: it tells qt_add_qml_module to skip
# qmlcachegen's ahead-of-time bytecode compilation for this module
# entirely, so the step that computes the offending ".."-carrying
# directory never runs, and QML_FILES can reference the real files at
# their real "../qml/..." path with no mirror, no alias-for-the-cache-path
# workaround, and no directory-scope change (this qt_add_qml_module call
# stays exactly where it always was, so the shell-breaking collision above
# cannot recur here either). The trade-off is scoped to this one test
# target: it loses ahead-of-time-compiled QML bytecode and falls back to
# Qt's ordinary JIT/interpreted QML loading, identical to how QML behaved
# before qmlcachegen existed - a real cost for a shipped, performance-
# sensitive binary, but not for a test executable, and edopro_next_shell's
# own module (../CMakeLists.txt) keeps its normal cache generation
# unchanged. Confirmed directly, not assumed: with this in place, editing
# ui/qml/screens/DeckBuilderScreen.qml and running only
# `cmake --build ui/build` (no reconfigure) shows rcc re-run and
# test_deckbuilder_screen.exe relinked, and `ninja -t query` on the
# generated qrc_test_deckbuilder_screen_raw_qml_0.cpp step lists the five
# real ui/qml/... files as its direct inputs - there is no intermediate
# copy left anywhere for a future edit to go stale against.
foreach(qml_file IN ITEMS Theme.qml components/SectionHeading.qml
                          components/DeckSectionList.qml components/CardPreview.qml
                          screens/DeckBuilderScreen.qml)
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file}
        PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

# Same requirement as ../CMakeLists.txt's own Theme.qml registration -
# pragma Singleton in the file is not enough on its own without also
# telling the module system at configure time.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(test_deckbuilder_screen
    URI EdoproNext
    VERSION 1.0
    NO_CACHEGEN
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

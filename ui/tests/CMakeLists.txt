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
# just a resolution ambiguity). This is Qt's own tooling gap - the alias
# property does not reach it, and there is no other qt_add_qml_module
# option that does either. Registering this module from ui/'s own
# directory instead (where these files' real relative path has no "..")
# was tried and rejected: it makes qt_add_qml_module see two calls for the
# same "EdoproNext" URI in one directory scope, and edopro_next_shell's
# own Main.qml stopped resolving at runtime as a result ("Module
# "EdoproNext" contains no type named "Main"", confirmed reproducible and
# confirmed absent before that change) - trading a build-time failure in
# the test target for a runtime failure in the shipped shell, which is
# worse. So the fix stays entirely in this file: mirror each file into a
# same-directory location via a symlink (falling back to a copy if the
# host can't create one - COPY_ON_ERROR), so the path qmlcachegen actually
# sees never contains "..". A symlink keeps the mirror always current with
# the real file with no extra step; the copy fallback exists only for a
# host where creating a symlink needs a privilege this build should not
# require, and is checked for staleness below rather than trusted blindly.
# Under the *source* directory, not the binary directory: qmlcachegen's
# relative-path computation is against CMAKE_CURRENT_SOURCE_DIR, and
# ui/build/tests (a typical out-of-tree binary dir) is no closer to
# ui/tests than ui/qml is - reaching it needs its own "..", trading one
# mkdir-rejects-".." failure for an identical one at a different path
# (confirmed: tried the binary dir first, got
# "mkdir(tests/.rcc/qmlcache/test_deckbuilder_screen_../build)" instead).
# Generated, not authored - see .gitignore.
set(_deckbuilder_screen_qml_mirror ${CMAKE_CURRENT_SOURCE_DIR}/.qml_mirror)
foreach(qml_file IN ITEMS Theme.qml components/SectionHeading.qml
                          components/DeckSectionList.qml components/CardPreview.qml
                          screens/DeckBuilderScreen.qml)
    set(_src ${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file})
    set(_mirrored ${_deckbuilder_screen_qml_mirror}/${qml_file})
    get_filename_component(_mirrored_dir "${_mirrored}" DIRECTORY)
    file(MAKE_DIRECTORY "${_mirrored_dir}")
    if(NOT EXISTS "${_mirrored}")
        file(CREATE_LINK "${_src}" "${_mirrored}" SYMBOLIC COPY_ON_ERROR)
    elseif(NOT IS_SYMLINK "${_mirrored}")
        # A COPY_ON_ERROR fallback from a previous configure: the real
        # file may have changed since. Re-copy so the mirror cannot go
        # silently stale (a symlink never needs this - it always reads
        # through to the current file).
        file(TIMESTAMP "${_src}" _src_stamp)
        file(TIMESTAMP "${_mirrored}" _mirrored_stamp)
        if(NOT _src_stamp STREQUAL _mirrored_stamp)
            file(COPY_FILE "${_src}" "${_mirrored}")
        endif()
        unset(_src_stamp)
        unset(_mirrored_stamp)
    endif()
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties(${_mirrored} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()
unset(_src)
unset(_mirrored)
unset(_mirrored_dir)

# Same requirement as ../CMakeLists.txt's own Theme.qml registration -
# pragma Singleton in the file is not enough on its own without also
# telling the module system at configure time. Set on the mirrored path,
# since that is what QML_FILES below actually names.
set_source_files_properties(${_deckbuilder_screen_qml_mirror}/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(test_deckbuilder_screen
    URI EdoproNext
    VERSION 1.0
    QML_FILES
        ${_deckbuilder_screen_qml_mirror}/Theme.qml
        ${_deckbuilder_screen_qml_mirror}/components/SectionHeading.qml
        ${_deckbuilder_screen_qml_mirror}/components/DeckSectionList.qml
        ${_deckbuilder_screen_qml_mirror}/components/CardPreview.qml
        ${_deckbuilder_screen_qml_mirror}/screens/DeckBuilderScreen.qml
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

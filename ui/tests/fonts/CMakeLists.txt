# Separate directory scope prevents two EdoproNext modules sharing registration.
qt_add_executable(test_platform_fonts
    test_platform_fonts.cpp
    ${EDOPRO_NEXT_FONT_SOURCES}
    ${EDOPRO_NEXT_SHELL_DECKBUILDER_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/appcontext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/appcontext.cpp
)
target_include_directories(test_platform_fonts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/deckbuilder)
set(font_test_qml)
foreach(qml_file IN ITEMS Main.qml Theme.qml
        components/NavRail.qml components/NavButton.qml components/StatusRow.qml
        components/SectionHeading.qml components/DeckSectionList.qml components/CardPreview.qml
        screens/HomeScreen.qml screens/NotImplementedScreen.qml screens/DeckBuilderScreen.qml)
    set(qml_path ${CMAKE_CURRENT_SOURCE_DIR}/../../qml/${qml_file})
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties(${qml_path} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
    list(APPEND font_test_qml ${qml_path})
endforeach()
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(test_platform_fonts
    URI EdoproNext VERSION 1.0 NO_CACHEGEN
    QML_FILES ${font_test_qml}
)
target_link_libraries(test_platform_fonts PRIVATE
    Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2
    edopro_next_data edopro_next_deck edopro_next_search edopro_next_legality SQLite::SQLite3)
add_test(NAME platform_fonts COMMAND test_platform_fonts)
set_tests_properties(platform_fonts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

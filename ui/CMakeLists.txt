cmake_minimum_required(VERSION 3.21)

project(edopro_next_shell
    VERSION 0.0.1
    DESCRIPTION "edopro-next modern client shell"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

# Build metadata is injected, never hardcoded in QML, so the shell can always
# report what it actually is rather than what someone once typed.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE EDOPRO_NEXT_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT EDOPRO_NEXT_GIT_SHA)
    set(EDOPRO_NEXT_GIT_SHA "unknown")
endif()

qt_add_executable(edopro_next_shell
    src/main.cpp
    src/appcontext.cpp
    src/appcontext.h
)

# The QML type registration Qt generates includes "appcontext.h" by bare
# filename, so src/ must be on the include path.
target_include_directories(edopro_next_shell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
        qml/screens/HomeScreen.qml
        qml/screens/NotImplementedScreen.qml
)

target_link_libraries(edopro_next_shell PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)

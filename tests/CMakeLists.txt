include(FetchContent)

set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_executable(cadence_core_tests
    core/alarm_test.cpp
    core/language_test.cpp
    core/model_test.cpp
    core/planner_test.cpp
    core/pomodoro_test.cpp
    core/progress_json_test.cpp
    core/settings_json_test.cpp
    core/summary_test.cpp
    core/template_json_test.cpp
    core/version_test.cpp)

target_link_libraries(cadence_core_tests PRIVATE
    cadence::core
    Catch2::Catch2WithMain)

target_compile_definitions(cadence_core_tests PRIVATE
    CADENCE_EXPECTED_VERSION="${PROJECT_VERSION}"
    CADENCE_RESOURCES_DIR="${PROJECT_SOURCE_DIR}/resources")

catch_discover_tests(cadence_core_tests)

add_executable(cadence_platform_tests
    platform/autostart_test.cpp)

target_link_libraries(cadence_platform_tests PRIVATE
    cadence::platform_files
    Catch2::Catch2WithMain)

catch_discover_tests(cadence_platform_tests)

add_executable(cadence_spotify_tests
    spotify/api_test.cpp
    spotify/pkce_test.cpp)

target_link_libraries(cadence_spotify_tests PRIVATE
    cadence::spotify_core
    Catch2::Catch2WithMain)

target_compile_definitions(cadence_spotify_tests PRIVATE
    CADENCE_SPOTIFY_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/spotify/fixtures")

catch_discover_tests(cadence_spotify_tests)

# QML component tests run on the offscreen platform so they need no display.
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickTest)

qt_add_executable(cadence_qml_tests qml/main.cpp)

target_link_libraries(cadence_qml_tests PRIVATE
    cadence_themeplugin
    Qt6::Quick
    Qt6::QuickTest)

target_compile_definitions(cadence_qml_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")

add_test(NAME cadence_qml_tests COMMAND cadence_qml_tests -import "${CMAKE_BINARY_DIR}/qml")
set_tests_properties(cadence_qml_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

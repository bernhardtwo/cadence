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
    core/model_test.cpp
    core/planner_test.cpp
    core/pomodoro_test.cpp
    core/progress_json_test.cpp
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

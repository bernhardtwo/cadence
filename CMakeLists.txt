cmake_minimum_required(VERSION 3.24)

project(Cadence
    VERSION 0.1.0
    DESCRIPTION "Structure your day into blocks with alarms, pomodoros and push-up logging"
    HOMEPAGE_URL "https://github.com/cadence-app/cadence"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CADENCE_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(CADENCE_WARNINGS_AS_ERRORS "Treat warnings in the core library as errors" ON)
option(CADENCE_BUILD_SPOTIFY_SPIKE "Build the Spotify troubleshooting tool under tools/" OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Every fetched dependency links statically. qtkeychain declares option(BUILD_SHARED_LIBS ON),
# which would otherwise leak into the cache and turn Catch2 and the rest into DLLs the test
# executables cannot find. Forced here, before the first FetchContent, so it cannot recur.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Fetched dependencies are linked statically" FORCE)

# JSON is fetched once here so the Qt free libraries under src/ share it.
include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src/core)
add_subdirectory(src/integrations/spotify)
add_subdirectory(src/platform)
add_subdirectory(src/app)

if(CADENCE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CADENCE_BUILD_SPOTIFY_SPIKE)
    add_subdirectory(tools/spotify-spike)
endif()

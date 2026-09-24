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

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/app)

if(CADENCE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CADENCE_BUILD_SPOTIFY_SPIKE)
    add_subdirectory(tools/spotify-spike)
endif()

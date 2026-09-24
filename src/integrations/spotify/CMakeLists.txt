# Qt free part: PKCE and response parsing, held to the same warning level as core.
add_library(cadence_spotify_core STATIC
    include/cadence/spotify/api.hpp
    include/cadence/spotify/pkce.hpp
    src/api.cpp
    src/pkce.cpp)
add_library(cadence::spotify_core ALIAS cadence_spotify_core)

target_include_directories(cadence_spotify_core PUBLIC include)
target_include_directories(cadence_spotify_core SYSTEM PRIVATE "${nlohmann_json_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(cadence_spotify_core PRIVATE /W4 /permissive-)
    if(CADENCE_WARNINGS_AS_ERRORS)
        target_compile_options(cadence_spotify_core PRIVATE /WX)
    endif()
else()
    target_compile_options(cadence_spotify_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    if(CADENCE_WARNINGS_AS_ERRORS)
        target_compile_options(cadence_spotify_core PRIVATE -Werror)
    endif()
endif()

# Qt part: the PKCE flow, the credential store and the Web API client.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network NetworkAuth)

set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
FetchContent_Declare(qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG 0.15.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(qtkeychain)

add_library(cadence_spotify STATIC
    include/cadence/spotify/auth.hpp
    include/cadence/spotify/client.hpp
    src/auth.cpp
    src/client.cpp)
add_library(cadence::spotify ALIAS cadence_spotify)
set_target_properties(cadence_spotify PROPERTIES AUTOMOC ON)

target_include_directories(cadence_spotify PUBLIC include)
target_include_directories(cadence_spotify SYSTEM PRIVATE "${qtkeychain_SOURCE_DIR}" "${qtkeychain_BINARY_DIR}")
target_link_libraries(cadence_spotify
    PUBLIC cadence::spotify_core Qt6::Core Qt6::Network Qt6::NetworkAuth
    PRIVATE qt6keychain Qt6::Gui)

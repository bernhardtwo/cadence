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

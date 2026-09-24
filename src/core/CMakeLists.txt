add_library(cadence_core STATIC
    include/cadence/core/activity.hpp
    include/cadence/core/alarm.hpp
    include/cadence/core/block.hpp
    include/cadence/core/clock.hpp
    include/cadence/core/phase.hpp
    include/cadence/core/planner.hpp
    include/cadence/core/pomodoro.hpp
    include/cadence/core/progress_json.hpp
    include/cadence/core/settings_json.hpp
    include/cadence/core/summary.hpp
    include/cadence/core/template_json.hpp
    include/cadence/core/time.hpp
    include/cadence/core/version.hpp
    src/alarm.cpp
    src/phase.cpp
    src/block.cpp
    src/planner.cpp
    src/pomodoro.cpp
    src/progress_json.cpp
    src/settings_json.cpp
    src/summary.cpp
    src/template_json.cpp
    src/time.cpp
    src/version.cpp)
add_library(cadence::core ALIAS cadence_core)

target_include_directories(cadence_core PUBLIC include)

include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# JSON stays an implementation detail of the core library, and third party headers are not held
# to the core warning level.
target_include_directories(cadence_core SYSTEM PRIVATE "${nlohmann_json_SOURCE_DIR}/include")

target_compile_definitions(cadence_core PRIVATE
    CADENCE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CADENCE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CADENCE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    CADENCE_VERSION_STRING="${PROJECT_VERSION}")

# The core library is Qt free and is held to the strictest warning level of the project.
if(MSVC)
    target_compile_options(cadence_core PRIVATE /W4 /permissive-)
    if(CADENCE_WARNINGS_AS_ERRORS)
        target_compile_options(cadence_core PRIVATE /WX)
    endif()
else()
    target_compile_options(cadence_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    if(CADENCE_WARNINGS_AS_ERRORS)
        target_compile_options(cadence_core PRIVATE -Werror)
    endif()
endif()

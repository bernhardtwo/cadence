add_library(cadence_core STATIC
    include/cadence/core/activity.hpp
    include/cadence/core/block.hpp
    include/cadence/core/clock.hpp
    include/cadence/core/planner.hpp
    include/cadence/core/time.hpp
    include/cadence/core/version.hpp
    src/block.cpp
    src/planner.cpp
    src/time.cpp
    src/version.cpp)
add_library(cadence::core ALIAS cadence_core)

target_include_directories(cadence_core PUBLIC include)

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

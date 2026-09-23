# The file builders are plain C++ so they can be unit tested without Qt; the OS side lives in a
# separate library that links Qt Core.
add_library(cadence_platform_files STATIC
    include/cadence/platform/autostart_files.hpp
    src/autostart_files.cpp)
add_library(cadence::platform_files ALIAS cadence_platform_files)
target_include_directories(cadence_platform_files PUBLIC include)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(cadence_platform STATIC
    include/cadence/platform/autostart.hpp
    src/autostart.cpp)
add_library(cadence::platform ALIAS cadence_platform)
target_include_directories(cadence_platform PUBLIC include)
target_link_libraries(cadence_platform PUBLIC cadence::platform_files Qt6::Core)

foreach(target cadence_platform_files cadence_platform)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(CADENCE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
        if(CADENCE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endforeach()

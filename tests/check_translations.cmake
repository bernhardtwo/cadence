# Extracts the strings of the app into scratch copies of the .ts files and fails when any copy
# ends up with work in it: an unfinished entry means a string was added or changed without a
# translation, a vanished or obsolete one means the source moved on and update_translations was
# not run. The tracked catalogs are never written. Run as a ctest with -DTS_DIR=<directory of
# the .ts files>, -DSOURCE_DIR=<repository root>, -DWORK_DIR=<scratch directory>,
# -DLUPDATE=<lupdate> and -DLCONVERT=<lconvert>.
foreach(var TS_DIR SOURCE_DIR WORK_DIR LUPDATE LCONVERT)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

file(GLOB ts_files "${TS_DIR}/*.ts")
if(NOT ts_files)
    message(FATAL_ERROR "no .ts files under ${TS_DIR}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(copies "")
foreach(ts_file IN LISTS ts_files)
    get_filename_component(name "${ts_file}" NAME)
    configure_file("${ts_file}" "${WORK_DIR}/${name}" COPYONLY)
    list(APPEND copies "${WORK_DIR}/${name}")
endforeach()

# The same sources as the update_translations target: the app and the Spotify library. Vanished
# entries are kept here, unlike in that target, so a removed string also fails the check.
file(GLOB sources
    "${SOURCE_DIR}/src/app/*.cpp"
    "${SOURCE_DIR}/src/app/*.hpp"
    "${SOURCE_DIR}/src/app/qml/*.qml"
    "${SOURCE_DIR}/src/integrations/spotify/src/*.cpp"
    "${SOURCE_DIR}/src/integrations/spotify/include/cadence/spotify/*.hpp")
execute_process(
    COMMAND "${LUPDATE}" -silent -locations none
            -I "${SOURCE_DIR}/src/app"
            -I "${SOURCE_DIR}/src/core/include"
            -I "${SOURCE_DIR}/src/platform/include"
            -I "${SOURCE_DIR}/src/integrations/spotify/include"
            ${sources} -ts ${copies}
    RESULT_VARIABLE lupdate_result
    OUTPUT_VARIABLE lupdate_output
    ERROR_VARIABLE lupdate_output)
if(NOT lupdate_result EQUAL 0)
    message(FATAL_ERROR "lupdate failed (${lupdate_result}):\n${lupdate_output}")
endif()
if(NOT lupdate_output STREQUAL "")
    message(STATUS "lupdate:\n${lupdate_output}")
endif()

# English is the source language: its catalog only carries plural forms, as in the build.
if(EXISTS "${WORK_DIR}/cadence_en.ts")
    execute_process(
        COMMAND "${LCONVERT}" -pluralonly -i "${WORK_DIR}/cadence_en.ts" -o "${WORK_DIR}/cadence_en.ts"
        RESULT_VARIABLE lconvert_result
        OUTPUT_VARIABLE lconvert_output
        ERROR_VARIABLE lconvert_output)
    if(NOT lconvert_result EQUAL 0)
        message(FATAL_ERROR "lconvert failed (${lconvert_result}):\n${lconvert_output}")
    endif()
endif()

set(problems "")
foreach(ts_file IN LISTS copies)
    file(READ "${ts_file}" content)
    get_filename_component(name "${ts_file}" NAME)
    string(REGEX MATCHALL "type=\"unfinished\"" unfinished "${content}")
    string(REGEX MATCHALL "type=\"(vanished|obsolete)\"" stale "${content}")
    list(LENGTH unfinished unfinished_count)
    list(LENGTH stale stale_count)
    if(unfinished_count GREATER 0)
        string(APPEND problems "${name}: ${unfinished_count} unfinished\n")
    endif()
    if(stale_count GREATER 0)
        string(APPEND problems "${name}: ${stale_count} vanished or obsolete\n")
    endif()
    message(STATUS "${name}: ${unfinished_count} unfinished, ${stale_count} stale")
endforeach()

if(NOT problems STREQUAL "")
    message(FATAL_ERROR "translations need attention (run the update_translations target and "
                        "translate):\n${problems}")
endif()

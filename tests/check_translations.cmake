# Fails when any translation file still has work in it: an unfinished entry means a string was
# never translated, a vanished or obsolete one means the source changed and lupdate was not
# followed by a cleanup. Run as a ctest with -DTS_DIR=<directory of the .ts files>.
if(NOT DEFINED TS_DIR)
    message(FATAL_ERROR "TS_DIR is required")
endif()

file(GLOB ts_files "${TS_DIR}/*.ts")
if(NOT ts_files)
    message(FATAL_ERROR "no .ts files under ${TS_DIR}")
endif()

set(problems "")
foreach(ts_file IN LISTS ts_files)
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
    message(FATAL_ERROR "translations need attention:\n${problems}")
endif()

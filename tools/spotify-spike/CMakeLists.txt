find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Network NetworkAuth)

# A console tool, not part of the app: exercises the Spotify Web API with a real login.
qt_add_executable(spotify_spike main.cpp)
set_target_properties(spotify_spike PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(spotify_spike PRIVATE Qt6::Gui Qt6::Network Qt6::NetworkAuth)

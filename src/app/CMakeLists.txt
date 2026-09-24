find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Qml Quick QuickControls2 Svg Multimedia)

qt_standard_project_setup(REQUIRES 6.5)
set(QT_QML_GENERATE_QMLLS_INI ON)

qt_add_executable(cadence
    main.cpp
    alarmsounds.cpp
    alarmsounds.hpp
    appinfo.cpp
    appinfo.hpp
    apppaths.cpp
    apppaths.hpp
    daycontroller.cpp
    daycontroller.hpp
    eventlog.cpp
    eventlog.hpp
    fileprogressstore.cpp
    fileprogressstore.hpp
    progressstore.hpp
    settings.cpp
    settings.hpp
    singleinstance.cpp
    singleinstance.hpp
    systemclock.cpp
    systemclock.hpp
    templateeditor.cpp
    templateeditor.hpp
    trayicon.cpp
    trayicon.hpp
    wakewatcher.cpp
    wakewatcher.hpp)

set_target_properties(cadence PROPERTIES
    OUTPUT_NAME cadence
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE)

set(CADENCE_QML_FILES
    qml/BlockBar.qml
    qml/FocusScreen.qml
    qml/Main.qml
    qml/NavPill.qml
    qml/Overlay.qml
    qml/PomodoroGrid.qml
    qml/SettingsScreen.qml
    qml/SignalButton.qml
    qml/SignalChoice.qml
    qml/SignalField.qml
    qml/SignalToggle.qml
    qml/StatsScreen.qml
    qml/TemplatesScreen.qml
    qml/TodayScreen.qml
    qml/TopBar.qml)

foreach(qml_file IN LISTS CADENCE_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

qt_add_qml_module(cadence
    URI Cadence
    VERSION 1.0
    QML_FILES ${CADENCE_QML_FILES}
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/Cadence"
    IMPORT_PATH "${CMAKE_BINARY_DIR}/qml")

add_subdirectory(qml/theme)

qt_add_resources(cadence "cadence_fonts"
    PREFIX "/fonts"
    BASE fonts
    FILES
        fonts/BigShouldersDisplay/BigShouldersDisplay-Variable.ttf
        fonts/Archivo/Archivo-Variable.ttf)

qt_add_resources(cadence "cadence_templates"
    PREFIX "/templates"
    BASE "${PROJECT_SOURCE_DIR}/resources/templates"
    FILES "${PROJECT_SOURCE_DIR}/resources/templates/default.json")

qt_add_resources(cadence "cadence_sounds"
    PREFIX "/sounds"
    BASE "${PROJECT_SOURCE_DIR}/resources/sounds"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/sounds/chime.wav"
        "${PROJECT_SOURCE_DIR}/resources/sounds/soft.wav")

qt_add_resources(cadence "cadence_icons"
    PREFIX "/icons"
    BASE icons
    FILES icons/tray.svg)

target_link_libraries(cadence PRIVATE
    cadence::core
    cadence::platform
    cadence_themeplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Multimedia)

# Copy the Qt runtime next to the executable so a local build starts by double-click.
if(WIN32)
    option(CADENCE_DEPLOY_QT_RUNTIME "Run windeployqt after building the app" ON)
    if(CADENCE_DEPLOY_QT_RUNTIME)
        add_custom_command(TARGET cadence POST_BUILD
            COMMAND Qt6::windeployqt
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:cadence>"
            COMMENT "Deploying Qt runtime next to cadence.exe"
            VERBATIM)
    endif()
endif()

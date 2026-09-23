set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(cadence_theme
    URI Cadence.Theme
    VERSION 1.0
    STATIC
    QML_FILES Theme.qml
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/Cadence/Theme")

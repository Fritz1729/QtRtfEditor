# QtRtfEditor — CMakeLists.txt (Root)
#
# Wiederverwendbare RTF-fähige QTextEdit-Subclass.
# GPLv3-only + kommerzielle Lizenz (Dual-Licensing, Qt-konform).

cmake_minimum_required(VERSION 3.16)

project(QtRtfEditor
    VERSION 0.1.0
    DESCRIPTION "Wiederverwendbare RTF-fähige QTextEdit-Subclass"
    HOMEPAGE_URL https://github.com/Fritz1729/QtRtfEditor
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# --- Bibliothek ---
add_subdirectory(src)

# --- Tests ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(tests)
endif()

# --- Beispiele ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(examples/demo)
endif()

# --- Installation (CMake-Package-Konfiguration) ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Header-Installation
    install(DIRECTORY include/RichTextEdit
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Config-Dateien für find_package()
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QtRtfEditorConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/QtRtfEditorConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtRtfEditor
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/QtRtfEditorConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/QtRtfEditorConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/QtRtfEditorConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtRtfEditor
    )

    install(TARGETS QtRtfEditor
        EXPORT QtRtfEditorTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(EXPORT QtRtfEditorTargets
        NAMESPACE QtRtfEditor::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtRtfEditor
    )
endif()

# QtRtfEditor — CMakeLists.txt (Root)
#
# QTextEdit subclass with partial RTF support.
# LGPL-3.0-only.

cmake_minimum_required(VERSION 3.27)

project(QtRtfEditor
    VERSION 0.1.4
    LANGUAGES CXX
    SPDX_LICENSE "LGPL-3.0-only"
    DESCRIPTION "QTextEdit subclass with partial RTF support"
    HOMEPAGE_URL https://github.com/Fritz1729/QtRtfEditor
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# --- Library ---
add_subdirectory(src)

# --- Tests ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(tests)
endif()

# --- Examples ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(demo)
endif()

# --- Licenses ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/licenses/LGPL-3.0-only.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/licenses/GPL-3.0-only.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}
    )
endif()


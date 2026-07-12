# QtRtfEditor — CMakeLists.txt (Root)
#
# Reusable RTF-capable QTextEdit subclass.
# GPLv3-only + commercial license (Dual-Licensing, Qt-compliant).

cmake_minimum_required(VERSION 3.16)

project(QtRtfEditor
    VERSION 0.1.1
    DESCRIPTION "Reusable RTF-capable QTextEdit subclass"
    HOMEPAGE_URL https://github.com/Fritz1729/QtRtfEditor
    LANGUAGES CXX
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
    add_subdirectory(examples/demo)
endif()


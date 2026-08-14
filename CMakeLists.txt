# QtRtfEditor — CMakeLists.txt (Root)
#
# QTextEdit subclass with partial RTF support.
# LGPL-3.0-only.

cmake_minimum_required(VERSION 3.21)

project(QtRtfEditor
    VERSION 0.1.4.2
    LANGUAGES CXX
    DESCRIPTION "QTextEdit subclass with partial RTF support"
    HOMEPAGE_URL https://github.com/Fritz1729/QtRtfEditor
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    if(BUILD_SHARED_LIBS)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wtype-limits -Wsign-conversion -Wshadow -Wswitch-enum -Wpedantic -Wformat=2 -Wimplicit-fallthrough=3 -Wrestrict)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Common output directories — keeps DLLs beside executables on Windows
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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


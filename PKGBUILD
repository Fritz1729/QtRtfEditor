# Maintainer: F1729

pkgname=qt-rtf-editor
_pkgbasename=QtRtfEditor
pkgver=0.1.0
pkgrel=1
pkgdesc='Reusable RTF-capable QTextEdit subclass'
arch=('x86_64' 'aarch64')
url='https://github.com/Fritz1729/QtRtfEditor'
license=('GPL-3.0-only' 'custom')
depends=('qt6-base')
makedepends=('cmake' 'ninja' 'qt6-base')
checkdepends=('qt6-base')
source=()
sha256sums=()

prepare() {
  local repo_root
  repo_root="$(dirname "${BASH_SOURCE[0]}")"

  # Create a clean source tarball from the repository root.
  tar -czf archive.tar.gz \
    --exclude='.git' \
    --exclude='3rdparty' \
    --exclude='tmp' \
    --exclude='Testing' \
    --exclude='pkg' \
    --exclude='build' \
    --exclude='*.tar.gz' \
    --exclude='*.pkg.tar.zst' \
    --exclude='.qtrtfeditor_recent.txt' \
    --exclude='.opencode' \
    --exclude='RTF_COVERAGE_PLAN.md' \
    -C "$repo_root" .

  # Extract into a directory outside the repo to avoid name collision with src/.
  mkdir -p "$srcdir"
  tar -xzf archive.tar.gz -C "$srcdir" --strip-components=1
  rm archive.tar.gz

  # Remove examples; keep tests/ because src/*.cpp includes RtfCompare.h.
  rm -rf "${srcdir:?}/examples"

  # Replace root CMakeLists.txt with a minimal build-only version.
  # Also replace src/CMakeLists.txt with the library-specific version
  # (the seed tarball contains the root project file there).
  cat > "${srcdir}/CMakeLists.txt" << 'CMAKE'
cmake_minimum_required(VERSION 3.16)
project(QtRtfEditor
    VERSION 0.1.0
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

add_subdirectory(src)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(tests)
endif()
CMAKE

  # Create cmake config file (needed by src/CMakeLists.txt).
  mkdir -p "${srcdir}/src/cmake"
  cat > "${srcdir}/src/cmake/QtRtfEditorConfig.cmake.in" << 'CMAKE'
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(Qt6 REQUIRED COMPONENTS Widgets)

include("${CMAKE_CURRENT_LIST_DIR}/QtRtfEditorTargets.cmake")

check_required_components(QtRtfEditor)
CMAKE

  # Library-specific CMakeLists.txt.
  cat > "${srcdir}/src/CMakeLists.txt" << 'CMAKE'
# QtRtfEditor — CMakeLists.txt (Library)

add_library(QtRtfEditor STATIC
    RichTextEdit.h
    RichTextEdit.cpp
    ProtectedRange.h
    ProtectedRange.cpp
    RtfExport.h
    RtfExport.cpp
    RtfTypes.h
    RtfParser.h
    RtfControl.cpp
    RtfParser.cpp
    RtfImport.h
    RtfImport.cpp
)

add_library(QtRtfEditor::QtRtfEditor ALIAS QtRtfEditor)

target_include_directories(QtRtfEditor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../tests>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(QtRtfEditor
    PUBLIC
        Qt6::Widgets
)

target_compile_definitions(QtRtfEditor
    PRIVATE RTE_BUILD_LIBRARY
)

set_target_properties(QtRtfEditor PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# --- Installation ---
include(GNUInstallDirs)

file(GLOB PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
install(FILES ${PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RichTextEdit
)

include(CMakePackageConfigHelpers)

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
)

install(EXPORT QtRtfEditorTargets
    NAMESPACE QtRtfEditor::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtRtfEditor
)
CMAKE
}

build() {
  rm -rf "$srcdir/build"
  cmake -B build -S "$srcdir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=ON \
    -G Ninja
  cmake --build build
}

check() {
  ctest --test-dir build --output-on-failure
}

package() {
  cmake --install build --prefix "${pkgdir}/usr"
}

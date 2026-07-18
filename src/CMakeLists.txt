# QtRtfEditor — CMakeLists.txt (Library)

add_library(QtRtfEditor STATIC
    RichTextEdit.h
    RichTextEdit.cpp
    RtfExport.h
    RtfExport.cpp
    RtfTypes.h
    RtfParser.h
    RtfCharset.h
    RtfCharset.cpp
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

# src/CMakeLists.txt

add_library(QtRtfEditor
    rich_text_edit.h
    rich_text_edit.cpp
    protected_range.h
    protected_range.cpp
    rtf_import.h
    rtf_import.cpp
    rtf_export.h
    rtf_export.cpp
)

add_library(QtRtfEditor::QtRtfEditor ALIAS QtRtfEditor)

target_include_directories(QtRtfEditor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(QtRtfEditor
    PUBLIC
        Qt6::Widgets
)

# Export-Symbol-Makro für DLL/Shared-Builds
target_compile_definitions(QtRtfEditor
    PRIVATE QRTFEDITOR_BUILD_LIBRARY
)

set_target_properties(QtRtfEditor PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# src/CMakeLists.txt

add_library(QtRtfEditor
    RichTextEdit.h
    RichTextEdit.cpp
    ProtectedRange.h
    ProtectedRange.cpp
    RtfExport.h
    RtfExport.cpp
)

add_library(QtRtfEditor::QtRtfEditor ALIAS QtRtfEditor)

target_include_directories(QtRtfEditor
    PUBLIC
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

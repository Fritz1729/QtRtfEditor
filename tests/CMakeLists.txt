# tests/CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

add_executable(test_protected_ranges
    test_protected_ranges.cpp
)

target_include_directories(test_protected_ranges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_protected_ranges
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_protected_ranges COMMAND test_protected_ranges)

# ---

add_executable(test_rtf_roundtrip
    test_rtf_roundtrip.cpp
)

target_include_directories(test_rtf_roundtrip PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_rtf_roundtrip
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_rtf_roundtrip COMMAND test_rtf_roundtrip)

# ---

add_executable(test_format_compatibility
    test_format_compatibility.cpp
)

target_include_directories(test_format_compatibility PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_format_compatibility
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_format_compatibility COMMAND test_format_compatibility)

# tests/CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# ── test_protected_ranges ──

add_executable(test_protected_ranges
    TestProtectedRanges.cpp
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

# ── test_rtf_structural: atomic unit tests for CompareRtf() ──

add_executable(test_rtf_structural
    TestSemanticComparison.cpp
    RtfCompare.cpp
    RtfCompare.h
)

target_include_directories(test_rtf_structural PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_rtf_structural
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_rtf_structural COMMAND test_rtf_structural)

# ── test_roundtrip: data-driven, iterates over TestData/*.rtf ──

add_executable(test_roundtrip
    TestRoundtrip.cpp
    RtfCompare.cpp
    RtfCompare.h
)

target_include_directories(test_roundtrip PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_roundtrip
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_roundtrip COMMAND test_roundtrip)

add_custom_command(TARGET test_roundtrip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/TestData
        ${CMAKE_CURRENT_BINARY_DIR}/testdata
)

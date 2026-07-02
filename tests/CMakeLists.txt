# tests/CMakeLists.txt

add_executable(test_rte
    test_protected_ranges.cpp
    test_rtf_roundtrip.cpp
    test_format_compatibility.cpp
)

target_link_libraries(test_rte
    PRIVATE
        QtRtfEditor::QtRtfEditor
        Qt6::Test
)

add_test(NAME test_rte COMMAND test_rte)

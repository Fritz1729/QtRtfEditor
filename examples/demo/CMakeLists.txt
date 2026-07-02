# examples/demo/CMakeLists.txt

add_executable(demo main.cpp)

target_include_directories(demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(demo
    PRIVATE
        QtRtfEditor::QtRtfEditor
)

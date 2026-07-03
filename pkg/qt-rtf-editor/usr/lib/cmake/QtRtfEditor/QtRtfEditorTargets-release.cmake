#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "QtRtfEditor::QtRtfEditor" for configuration "Release"
set_property(TARGET QtRtfEditor::QtRtfEditor APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(QtRtfEditor::QtRtfEditor PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libQtRtfEditor.a"
  )

list(APPEND _cmake_import_check_targets QtRtfEditor::QtRtfEditor )
list(APPEND _cmake_import_check_files_for_QtRtfEditor::QtRtfEditor "${_IMPORT_PREFIX}/lib/libQtRtfEditor.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)

# Usage Guide

This guide covers integrating QtRtfEditor into your project and common usage patterns.

## Integration in CMake Projects

### FetchContent

```cmake
# CMakeLists.txt of your project
include(FetchContent)
FetchContent_Declare(QtRtfEditor
    GIT_REPOSITORY https://github.com/Fritz1729/QtRtfEditor.git
     GIT_TAG        v0.1.4.2
)
FetchContent_MakeAvailable(QtRtfEditor)

# Link target
target_link_libraries(MyTarget
    PRIVATE QtRtfEditor::QtRtfEditor
)
```

### Installed Package

After `make install`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(QtRtfEditor REQUIRED)
target_link_libraries(MyTarget PRIVATE QtRtfEditor::QtRtfEditor)
```

## Example Code

```cpp
#include <RichTextEdit.h>

Rte::RichTextEdit editor;

// Load RTF
std::string rtf = R"({\rtf1\ansi{\b Bold}{\b0 normal}})";
editor.Load(rtf, Rte::FormatMode::Rtf);

// Set protection — cursor skips this range
editor.SetProtection(0, 4);

// Save RTF
std::string saved = editor.Save(Rte::FormatMode::Rtf);
```

## Signals

```cpp
Rte::RichTextEdit editor;

// React to clicks on protected text
QObject::connect(&editor, &Rte::RichTextEdit::protectedRegionClicked,
    [](std::size_t start, std::size_t end, const QString& text) {
        // User clicked protected text — handle navigation, tooltip, etc.
    });
```

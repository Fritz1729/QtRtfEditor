# QtRtfEditor

A reusable **RTF-capable QTextEdit subclass** for Qt6.

> **Early development.** The library is ready for testing but should not
> yet be trusted to preserve document data.

Designed for round-trip interoperability between Qt `QTextEdit` and
Delphi `TRichEdit` — both use RTF for clipboard and file I/O but
employ different formatting conventions. The library detects
TRichEdit-generated RTF and exports in the same style,
so Delphi applications can re-import the data correctly.

## Scope

- **RTF I/O**: Load and serialize RTF data, preserving
  Delphi TRichEdit formatting conventions.
- **Protected text ranges**: `\protect` character format — cursor
  skips over protected text, preventing any edit within it.
- **Subclassing**: All critical methods are virtual,
  enabling application-specific extensions.
- **Dual licensing**: LGPL-3.0-or-later or commercial license.

For detailed feature coverage, see [Documentation](documentation/README.md).

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run demo
./build/examples/demo/demo

# Tests
cd build
ctest --output-on-failure
```

## Integration in CMake Projects

```cmake
# CMakeLists.txt of your project
include(FetchContent)
FetchContent_Declare(QtRtfEditor
    GIT_REPOSITORY https://github.com/Fritz1729/QtRtfEditor.git
     GIT_TAG        v0.1.1
)
FetchContent_MakeAvailable(QtRtfEditor)

# Link target
target_link_libraries(MyTarget
    PRIVATE QtRtfEditor::QtRtfEditor
)
```

Alternatively, after `make install`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(QtRtfEditor REQUIRED)
target_link_libraries(MyTarget PRIVATE QtRtfEditor::QtRtfEditor)
```

## Example Code

```cpp
#include <RichTextEdit/RichTextEdit.h>

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

## Credits

Development relied heavily on:

- **Local LLM** usage (main model: Qwen 3.6 27B)
- **[OpenCode](https://opencode.ai)** as a harness

The following more mature projects provided valuable reference material:

- **[Ted](https://github.com/mdedoes/Ted)** — stable RTF standalone editor for Linux (written in C)
- **[Calligra RTF filter](https://github.com/KDE/calligra/tree/master/filters/words/rtf)** — parser used by KDE

## License

Dual licensing: **LGPL-3.0-or-later** or **commercial license**.
See [Qt Licensing](https://www.qt.io/licensing) for details.

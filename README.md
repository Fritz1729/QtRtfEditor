# QtRtfEditor

A reusable **RTF-capable QTextEdit subclass** for Qt6.

## Features

- **RTF I/O**: Load and serialize RTF data
  (Delphi/TRichEdit-compatible).
- **Protected text ranges**: Configurable protection of
  text blocks against deletion (None, Warn, Block).
- **Subclassing**: All critical methods are virtual,
  enabling application-specific extensions.
- **Dual licensing**: LGPL-3.0-or-later or commercial license.

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
    GIT_TAG        v0.1.0
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
editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);

// Load RTF
std::string rtf = R"({\rtf1\ansi{\b Bold}{\b0 normal})";
editor.load(rtf, Rte::FormatMode::Rtf);

// Set protection
editor.setProtection(0, 4, "lexicon", "entry:Example");

// Save RTF
std::string saved = editor.save(Rte::FormatMode::Rtf);
```

## Subclassing

```cpp
class MvEditor : public Rte::RichTextEdit
{
protected:
    void checkProtection(const QTextCursor &cursor, bool &allowed) override
    {
        // MV-specific logic: translate protection types,
        // show custom dialogs
        Rte::RichTextEdit::checkProtection(cursor, allowed);
    }
};
```

## License

Dual licensing: **LGPL-3.0-or-later** or **commercial license**.
See [Qt Licensing](https://www.qt.io/licensing) for details.

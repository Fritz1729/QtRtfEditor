# QtRtfEditor

A reusable **RTF-capable QTextEdit subclass** for Qt6.

Designed for round-trip interoperability between Qt `QTextEdit` and
Delphi `TRichEdit` — both use RTF for clipboard and file I/O but
employ different formatting conventions. The library detects
TRichEdit-generated RTF and exports in the same style to ensure
Delphi applications can re-import the data correctly.

## Features

- **RTF I/O**: Load and serialize RTF data, preserving
  Delphi TRichEdit formatting conventions.
- **Protected text ranges**: Configurable protection of
  text blocks against deletion (None, Warn, Block).
- **Subclassing**: All critical methods are virtual,
  enabling application-specific extensions.
- **Dual licensing**: LGPL-3.0-or-later or commercial license.

## Supported & Unsupported Features

### Supported

Basic character formatting: bold, italic, underline (solid), font selection, font size, text color.
Basic paragraph formatting: left/right/center alignment, left indent, first-line indent.
Superscript/subscript (toggle), color table, font table, Unicode escape (`\uN`).

### No Support Planned

#### Specific to Windows

- **OLE objects** (`\object`, `\objdata`, `\objalias`, `\objclass`) — Windows COM embedding
- **Metafiles** (EMF, WMF) — Windows-specific rasterizable formats, no Qt equivalent

#### Document Management

- **Document metadata** (`\info`, `\title`, `\author`, `\subject`, `\keywords`, `\versionN`) — handled by the embedding app
- **Track changes** (`\revtbl`, `\revN`, `\insrsidN`, `\delrsidN`, `\tridxN`) — version tracking, no Qt equivalent
- **Styles** (`\stylesheet`, `\sN`, `\snext`, `\sbasedonN`) — Word-specific style system
- **Fields** (`\field`, `\*\fldinst`, `\*\fldrslt`, `\date`, `\time`) — Word field codes
- **Index / TOC entries** (`\*{\index ...}`, `\*{\toc ...}`, `\*{\tc ...}`) — document generation, not editing
- **Document variables** (`\*\docvar`) — low-priority document metadata
- **User properties** (`\userprops`, `\propname`, `\staticval`) — custom document properties
- **Paragraph Group Properties** (`\*\pgptbl`, `\pgp`, `\ipgpN`) — Word 2002+ storage optimization
- **Style restrictions** (`\*\latentstyles`, `\lsd*`) — Word UI style visibility and locking
- **Password protection** (`\passwordhash`, `\password`) — document-level security

#### Support of Special Languages

- **Asian and East Asian fonts** (`\fscript`, `\fdecor`, `\stshfdbchN`, `\stshfhichN`, `\stshfbiN`, `\fcharset134/136/129`) — East Asian font binding and complex script support requires an external shaping library
- **Bidirectional text** (`\rtlch`, `\ltrch`, `\rtl`, `\ltrpar`, `\fbidis`) — BiDi layout requires a platform shaping library beyond Qt's basic RTL support
- **Complex script shaping** (Indic, Thai, Arabic) — requires HarfBuzz/UCDraw, no Qt equivalent

#### Page Layout & Document Structure

- **Section & page setup** (`\sectd`, `\sect`, `\sbk*`, `\pgwsxn`, `\pghsxn`, `\marg*`, `\cols*`, `\deftabN`, `\vertdoc`, `\horzdoc`) — QTextDocument is a flow model, not a page layout model
- **Comments / annotations** (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`) — no Qt equivalent
- **Compatibility & document properties** (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`) — no Qt equivalent

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run demo
./build/examples/demo/demo

# Tests
```bash
cd build
ctest --output-on-failure
```

Three test executables:

- **test_protected_ranges** — 13 tests for the protection API (`TestProtectedRanges.cpp`)
- **test_rtf_structural** — 12 atomic unit tests for the RTF comparison engine (`CompareRtf()`, `TestSemanticComparison.cpp`)
- **test_roundtrip** — data-driven test iterating `tests/TestData/*.rtf` (7 feature-family files)

Test data files in `tests/TestData/` are organized by feature family:
`alignment.rtf`, `bold.rtf`, `colors.rtf`, `complex.rtf`, `fonts.rtf`, `indents.rtf`, `superscript.rtf`.

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

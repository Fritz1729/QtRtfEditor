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

## Features

### RichEdit 2.0 is supported.

#### Topics

- **Character formatting** — bold, italic, underline (solid, dotted, dashed, thick), strikethrough, font family, font size, text color, background color, superscript/subscript, capitalization
- **Paragraph formatting** — alignment, left indent, first-line indent, right indent, spacing before/after, line height
- **Tab stops** — simple tab separator, tab stops with left/center/right alignment
- **Unicode** — escape sequences, hex escapes, non-breaking space, typographic characters
- **Document tables** — color table, font table
- **Embedded images** — BMP, PNG, JPEG via `\pict`

#### Not supported

- **Character formatting** — highlight (`\highlightN`), double underline (`\uldb`), line-spacing multiplier (`\slmultN`), positional superscript/subscript (`\upN`/`\dnN`)
- **Document metadata** (`\info`, `\title`, `\author`, `\subject`, `\keywords`, `\versionN`) — handled by the embedding app
- **OLE objects** (`\object`, `\objdata`, `\objalias`, `\objclass`) — Windows COM embedding
- **Metafiles** (EMF, WMF) — Windows-specific rasterizable formats
- **Section & page setup** (`\sectd`, `\sect`, `\sbk*`, `\pgwsxn`, `\pghsxn`, `\marg*`, `\cols*`, `\deftabN`, `\vertdoc`, `\horzdoc`)
- **Asian and East Asian fonts** (`\fscript`, `\fdecor`, `\stshfdbchN`, `\stshfhichN`, `\stshfbiN`, `\fcharset134/136/129`) — requires external shaping library
- **Bidirectional text** (`\rtlch`, `\ltrch`, `\rtl`, `\ltrpar`, `\fbidis`) — requires platform shaping library
- **Complex script shaping** (Indic, Thai, Arabic) — requires HarfBuzz/UCDraw

### No Support Planned

#### Word-specific extensions

- **Track changes** (`\revtbl`, `\revN`, `\insrsidN`, `\delrsidN`, `\tridxN`) — version tracking
- **Styles** (`\stylesheet`, `\sN`, `\snext`, `\sbasedonN`) — Word-specific style system
- **Fields** (`\field`, `\*\fldinst`, `\*\fldrslt`, `\date`, `\time`) — Word field codes
- **Index / TOC entries** (`\*{\index ...}`, `\*{\toc ...}`, `\*{\tc ...}`) — document generation, not editing
- **Document variables** (`\*\docvar`) — low-priority document metadata
- **User properties** (`\userprops`, `\propname`, `\staticval`) — custom document properties
- **Paragraph Group Properties** (`\*\pgptbl`, `\pgp`, `\ipgpN`) — Word 2002+ storage optimization
- **Style restrictions** (`\*\latentstyles`, `\lsd*`) — Word UI style visibility and locking
- **Password protection** (`\passwordhash`, `\password`) — document-level security
- **Comments / annotations** (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`) — Word 2002+
- **Compatibility flags** (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`) — Word-specific behavior flags

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
editor.SetProtectionPolicy(Rte::ProtectionPolicy::Warn);

// Load RTF
std::string rtf = R"({\rtf1\ansi{\b Bold}{\b0 normal})";
editor.Load(rtf, Rte::FormatMode::Rtf);

// Set protection
editor.SetProtection(0, 4, "lexicon", "entry:Example");

// Save RTF
std::string saved = editor.Save(Rte::FormatMode::Rtf);
```

## Subclassing

```cpp
class MvEditor : public Rte::RichTextEdit
{
protected:
    void CheckProtection(const QTextCursor& cursor, bool& allowed) override
    {
        // MV-specific logic: translate protection types,
        // show custom dialogs
        Rte::RichTextEdit::CheckProtection(cursor, allowed);
    }
};
```

## License

Dual licensing: **LGPL-3.0-or-later** or **commercial license**.
See [Qt Licensing](https://www.qt.io/licensing) for details.

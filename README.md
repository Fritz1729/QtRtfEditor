# QtRtfEditor

A reusable **RTF-capable QTextEdit subclass** for Qt6.

Designed for round-trip interoperability between Qt `QTextEdit` and
Delphi `TRichEdit` — both use RTF for clipboard and file I/O but
employ different formatting conventions. The library detects
TRichEdit-generated RTF and exports in the same style to ensure
Delphi applications can re-import the data correctly.

## Scope

- **RTF I/O**: Load and serialize RTF data, preserving
  Delphi TRichEdit formatting conventions.
- **Protected text ranges**: `\protect` character format — cursor
  skips over protected text, preventing any edit within it.
- **Subclassing**: All critical methods are virtual,
  enabling application-specific extensions.
- **Dual licensing**: LGPL-3.0-or-later or commercial license.

## Supported

Rich Edit 1.0–2.0 feature set (Delphi TRichEdit compatibility baseline).

### Character formatting

bold, italic, underline (solid, dotted, dashed, dash-dot, dash-dot-dot, thick),
strikethrough, font family, font size, text color, background color,
superscript/subscript, capitalization (all caps, small caps), character expansion
(`\expndN`, `\expndtwN`), kerning (`\kerning` / `\kerning0`)

### Paragraph formatting

alignment (left, center, right, justified), left indent, first-line indent, right indent,
spacing before/after, line height (fixed), tab stops (left, center, right),
lists (bullet, number, letter, roman)

### Unicode & symbols

escape sequences (`\uNNN`), hex escapes (`\'hh`), non-breaking space (`\~`),
typographic characters (bullet, emdash, endash, smart quotes, en/em space)

**Note:** Export always uses Unicode escapes (`\uNNN`) for characters above 0x7F.
Hex escapes (`\'hh`) are decoded on import using the active font's character set
(`\fcharsetN`) and the document code page (`\ansicpgN`, default 1252).
The export does not emit `\ansicpgN` — all text is Unicode-encoded.

### Document tables

color table (RGB), font table (family name, charset;
`\fprq` font pitch is recognized but not preserved)

### Layout tables

Rows, cells, column widths, vertical alignment, per-side borders (solid/dashed, width, color), cell padding, row padding, table alignment (left/center/right), row-level borders

### Layout tables (partial)

Cell shading (`\clshdn`) and merged cells (`\clmrg`) are parsed but not rendered — no Qt equivalent

### Embedded images

BMP, PNG, JPEG via `\pict`

## Not Supported

### Intentionally out of scope

- **Highlight** (`\highlightN`) — no reliable RGB mapping per RTF spec
- **Double underline** (`\uldb`) — Qt has no double-underline variant
- **Cell shading** (`\clshdn`) — Qt has no cell-level background API
- **Merged cells** (`\clmrg`) — out of scope
- **Line-spacing multiplier** (`\slmultN`) — Qt only supports fixed height
- **Positional superscript/subscript** (`\upN`/`\dnN`) — Qt only supports toggle
- **Underline color** (`\ulcN`) — Qt 6.11 has no `setFontUnderlineColor()`
- **Language ID** (`\langN`, `\chlangN`, `\langfeN`) — Qt 6.11 has no `setFontLanguageId()`

### Document metadata

`\info` section (`\title`, `\author`, `\subject`, `\keywords`, `\versionN`, `\revtbl`)
— handled by the embedding app

### Windows-specific

- **OLE objects** (`\object`, `\objdata`, `\objalias`, `\objclass`) — COM embedding
- **Metafiles** (EMF, WMF) — Windows rasterizable formats
- **Section & page setup** (`\sectd`, `\sect`, `\sbk*`, `\pgwsxn`, `\pghsxn`, `\marg*`, `\cols*`, `\vertdoc`, `\horzdoc`)
- **Headers/footers** (`{\header ...}`, `{\footer ...}`)
- **Footnotes/endnotes** (`{\footnote ...}`, `\ftnstart`, `\endnhere`)
- **Bookmarks** (`{\*\bkmkstart ...}`, `{\*\bkmkend ...}`) — no `QTextBookmark` in Qt6

### Complex text

- **Asian/East Asian fonts** (`\fscript`, `\fdecor`, `\stshfdbchN`, `\stshfhichN`, `\stshfbiN`, `\fcharset134/136/129`) — requires external shaping library
- **Bidirectional text** (`\rtlch`, `\ltrch`, `\rtl`, `\ltrpar`, `\fbidis`) — no platform BIDI library
- **Complex script shaping** (Indic, Thai, Arabic) — requires HarfBuzz/UCDraw

## No Support Planned

### Word-specific extensions

- **Track changes** (`\revtbl`, `\revN`, `\insrsidN`, `\delrsidN`, `\tridxN`) — version tracking
- **Styles** (`\stylesheet`, `\sN`, `\snext`, `\sbasedonN`) — Word style system
- **Fields** (`\field`, `\*\fldinst`, `\*\fldrslt`, `\date`, `\time`) — Word field codes
- **Index / TOC entries** (`\*{\index ...}`, `\*{\toc ...}`, `\*{\tc ...}`)
- **Document variables** (`\*\docvar`)
- **User properties** (`\userprops`, `\propname`, `\staticval`)
- **Paragraph Group Properties** (`\*\pgptbl`, `\pgp`, `\ipgpN`)
- **Style restrictions** (`\*\latentstyles`, `\lsd*`)
- **Password protection** (`\passwordhash`, `\password`)
- **Comments / annotations** (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`)
- **Compatibility flags** (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`)

### DeffN and DeftabN scope

The RTF 1.5 and 1.9.1 specifications define `\deffN` (default font index) and `\deftabN`
(default tab width) as **group-persistent** control words — pushed/popped on group entry/exit.

Our parser handles full group-persistence on import. On export, changed values are wrapped
in scoped groups to preserve semantics:

```rtf
{\rtf1\deff0 {\deff2\pard\plain Deep\par} {\deff1\pard\plain Mid\par} \pard\plain Outer\par}
```

This ensures that re-parsing produces the same document-level defaults and per-paragraph values.
For documents where `\deff` and `\deftab` appear only once at the document level (the common case),
no extra grouping is emitted.

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

// Load RTF
std::string rtf = R"({\rtf1\ansi{\b Bold}{\b0 normal})";
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

## License

Dual licensing: **LGPL-3.0-or-later** or **commercial license**.
See [Qt Licensing](https://www.qt.io/licensing) for details.

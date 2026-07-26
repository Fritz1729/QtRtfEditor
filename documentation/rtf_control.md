# Control Words (`RtfControl`)

**File:** [Source](../src/RtfControl.h) | [Previous](rtf_types.md) | [Next](rtf_charset.md)

`RtfControl` defines the RTF control word dispatch table -- a static array that maps each recognized control word to an action type and parameter. The parser uses this table to dispatch control words during tokenization.

## Role

`RtfControl` is the configuration layer. It defines *what* each RTF control word does without implementing *how* it is applied. The parser's `Dispatch()` function reads the `Action` and `Value` fields to route each control word to the appropriate handler.

## Data Structures

### RtfControl

Each entry maps a control word keyword to an action and parameter:

```cpp
RtfControl {
    keyword,                  // lowercase control word (e.g. "b", "fs", "par")
    action,                   // Action enum (what to do)
    value,                    // Value union (parameter specific to action type)
};
```

### Action Enum

The action type determines how the parser handles the control word:

```cpp
enum class Action : uint8_t {
    ToggleCharProp,     // Toggle boolean char property (\b, \i, \ul)
    SetCharProp,        // Set char property from numeric arg (\fN, \fsN)
    SetParaProp,        // Set paragraph property from numeric arg (\liN, \fiN)
    SetAlignment,       // Set paragraph alignment (\ql, \qc, \qr, \qj)
    SetUlStyle,         // Set underline style (\ul, \uld, \uldash)
    SetCapitalization,  // Set capitalization (\caps, \scaps)
    EmitParagraph,      // Emit current paragraph (\par)
    HeaderControl,      // RTF header control word (skip)
    HeaderMetadata,     // Header metadata (roundtrip preservation)
    TableControl,       // Table-related control word (colortbl, fonttbl, etc.)
    SetTabAlign,        // Set tab stop alignment (\tqc, \tqr, \tqd)
    TableControlWord,   // Table structure control (\trowd, \cell, \cellx)
    GroupPersistent,    // Group-persistent control (\deffN, \deftabN)
    Unknown,            // Unrecognized control word
};
```

### Value Union

The parameter carried by each control word entry:

```cpp
union Value {
    int raw;
    CharProp charProp;        // Bold, Italic, Underline, ...
    CharSetProp charSetProp;  // FontIndex, FontSize, ColorIndex, ...
    ParaProp paraProp;        // LeftIndent, FirstLineIndent, ...
    Align align;              // Left, Center, Right, Justified
    RtfUlStyle ulStyle;       // UlNone, UlSolid, UlDotted, ...
    RtfCaps caps;             // CapsNone, CapsAll, CapsSmall
    TabAlign tabAlign;        // Left, Center, Right, Decimal
    TableCtrlWord tableCtrlWord; // Trowd, Cellx, Cell, Row, ...
};
```

### Control Table

A `static_assert` in the .cpp file verifies the entry count matches `kRtfControlTableSize` in the header at compile time.

## Control Word Coverage

### Character Formatting (fully implemented: parser + import + export)

| Control Word | Action | Status |
|--------------|--------|--------|
| `\b` / `\b0` | ToggleCharProp::Bold | Full roundtrip |
| `\i` / `\i0` | ToggleCharProp::Italic | Full roundtrip |
| `\ul` / `\ul0` / `\ulnone` | SetUlStyle | Full roundtrip |
| `\uld` | SetUlStyle::Dotted | Full roundtrip |
| `\uldash` | SetUlStyle::Dashed | Full roundtrip |
| `\uldashd` | SetUlStyle::DashDot | Full roundtrip (renders as DashDotLine) |
| `\uldashdd` | SetUlStyle::DashDotDot | Full roundtrip (renders as DashDotDotLine) |
| `\uldb` | SetUlStyle::Double | Full roundtrip (renders as solid underline) |
| `\ulth` | SetUlStyle::Thick | Full roundtrip (renders as wave underline) |
| `\sub` / `\sub0` | ToggleCharProp::Subscript | Full roundtrip |
| `\super` / `\super0` | ToggleCharProp::Superscript | Full roundtrip |
| `\strike` / `\strike0` | ToggleCharProp::Strike | Full roundtrip |
| `\kerning` / `\kerning0` | ToggleCharProp::Kerning | Full roundtrip |
| `\protect` / `\protect0` | ToggleCharProp::Protect | Full roundtrip (cursor-skip) |
| `\fN` | SetCharProp::FontIndex | Full roundtrip |
| `\fsN` | SetCharProp::FontSize | Full roundtrip |
| `\cfN` | SetCharProp::ColorIndex | Full roundtrip |
| `\cbN` | SetCharProp::BgColorIndex | Full roundtrip |
| `\upN` | SetCharProp::UpOffset | Full roundtrip (stored as user property, Qt renders as boolean) |
| `\dnN` | SetCharProp::DnOffset | Full roundtrip (stored as user property, Qt renders as boolean) |
| `\expndN` / `\expndtwN` | SetCharProp::Expnd | Full roundtrip |
| `\caps` / `\caps0` | SetCapitalization::AllCaps | Full roundtrip |
| `\scaps` / `\scaps0` | SetCapitalization::SmallCaps | Full roundtrip |
| `\ulcN` | SetCharProp::UlColorIndex | Full roundtrip (stored as RGB user property, no Qt rendering API) |
| `\highlightN` | SetCharProp::HighlightIndex | Parser + export (stored as user property, no Qt rendering API) |
| `\langN` | SetCharProp::LangId | Parser + export (stored as user property, no Qt rendering API in 6.11) |

### Paragraph Formatting (fully implemented: parser + import + export)

| Control Word | Action | Status |
|--------------|--------|--------|
| `\ql` / `\qc` / `\qr` / `\qj` / `\qi` | SetAlignment | Full roundtrip |
| `\liN` | SetParaProp::LeftIndent | Full roundtrip |
| `\fiN` | SetParaProp::FirstLineIndent | Full roundtrip |
| `\riN` | SetParaProp::RightIndent | Full roundtrip |
| `\sbN` | SetParaProp::SpaceBefore | Full roundtrip |
| `\saN` | SetParaProp::SpaceAfter | Full roundtrip |
| `\slN` | SetParaProp::LineHeight | Full roundtrip |
| `\slmultN` | SetParaProp::SlMult | Full roundtrip (stored as block user property, no Qt rendering API) |
| `\txN` | SetParaProp::TabStop | Full roundtrip |
| `\par` | EmitParagraph | Full roundtrip |
| `\pard` | HeaderMetadata (reset) | Full roundtrip |
| `\plain` | HeaderMetadata (reset) | Full roundtrip |

### Tab Stops

| Control Word | Action | Status |
|--------------|--------|--------|
| `\tqc` | SetTabAlign::Center | Full roundtrip |
| `\tqr` | SetTabAlign::Right | Full roundtrip |
| `\tqd` | SetTabAlign::Decimal | Parser only (Qt renders as left-aligned) |

### Lists

| Control Word | Action | Status |
|--------------|--------|--------|
| `\listidN` | SetCharProp::ListId | Full roundtrip |
| `\listlevelN` | SetParaProp::ListLevel | Full roundtrip |
| `\listtable` | HeaderControl | Parser only (parsed, not exported) |

### Group-Persistent

| Control Word | Action | Status |
|--------------|--------|--------|
| `\deffN` | GroupPersistent | Full roundtrip (scoped groups on export) |
| `\deftabN` | GroupPersistent | Full roundtrip (scoped groups on export) |

### Document Header / Metadata

| Control Word | Action | Status |
|--------------|--------|--------|
| `\rtf1` | HeaderControl | Parsed (emitted by export) |
| `\ansi` | HeaderControl | Parsed (emitted by export) |
| `\deflangN` | HeaderMetadata | Full roundtrip (stored as document property) |
| `\viewkindN` | HeaderMetadata | Full roundtrip (stored as document property) |
| `\ucN` | HeaderMetadata | Full roundtrip (stored as document property) |
| `\ansicpgN` | HeaderMetadata | Full roundtrip (stored as document property) |

### Color Table

| Control Word | Action | Status |
|--------------|--------|--------|
| `\colortbl` | TableControl | Full roundtrip |
| `\redN` | TableControl | Full roundtrip |
| `\greenN` | TableControl | Full roundtrip |
| `\blueN` | TableControl | Full roundtrip |

### Font Table

| Control Word | Action | Status |
|--------------|--------|--------|
| `\fonttbl` | TableControl | Full roundtrip |
| `\fN` | TableControl (in fonttbl) | Full roundtrip |
| `\fcharsetN` | TableControl | Full roundtrip |
| `\froman` | TableControl | Full roundtrip |
| `\fswiss` | TableControl | Full roundtrip |
| `\fmodern` | TableControl | Full roundtrip |
| `\fnil` | TableControl | Full roundtrip |
| `\fprqN` | TableControl | Parser only (recognized, not preserved) |

### Images (\pict)

| Control Word | Action | Status |
|--------------|--------|--------|
| `\pict` | TableControl | Full roundtrip (byte-identical via hex preservation) |
| `\jpegblip` | TableControl | Full roundtrip |
| `\pngblip` | TableControl | Full roundtrip |
| `\dibitmap` | TableControl | Full roundtrip |
| `\picwN` | TableControl | Full roundtrip |
| `\pichN` | TableControl | Full roundtrip |
| `\picwgoalN` | TableControl | Full roundtrip |
| `\pichgoalN` | TableControl | Full roundtrip |
| `\picscalexN` | TableControl | Full roundtrip |
| `\picscaleyN` | TableControl | Full roundtrip |
| `\piccropl/r/t/bN` | TableControl | Parser only (not exported) |

### Tables

| Control Word | Action | Status |
|--------------|--------|--------|
| `\trowd` | TableCtrlWord::Trowd | Full roundtrip |
| `\cellxN` | TableCtrlWord::Cellx | Full roundtrip |
| `\cell` | TableCtrlWord::Cell | Full roundtrip |
| `\row` | TableCtrlWord::Row | Full roundtrip |
| `\intbl` | TableCtrlWord::Intbl | Full roundtrip |
| `\clshdnN` | TableCtrlWord::ClShading | Parser only (no Qt cell background API) |
| `\clvertalt/c/b` | TableCtrlWord | Full roundtrip |
| `\clbrdrl/t/r/b` | TableCtrlWord | Full roundtrip |
| `\brdrs` | TableCtrlWord::BrdrSolid | Full roundtrip |
| `\brdrn` | TableCtrlWord::BrdrNone | Full roundtrip |
| `\brdrd` | TableCtrlWord::BrdrDashed | Full roundtrip |
| `\brdrwN` | TableCtrlWord::BrdrWidth | Full roundtrip |
| `\brdrcfN` | TableCtrlWord::BrdrColor | Full roundtrip |
| `\clpadl/r/t/bN` | TableCtrlWord | Full roundtrip |
| `\trpaddl/r/t/bN` | TableCtrlWord | Full roundtrip |
| `\trql` / `\trqc` / `\trqr` | TableCtrlWord | Full roundtrip |
| `\trleftN` | TableCtrlWord | Full roundtrip |
| `\trwN` | TableCtrlWord | Full roundtrip |
| `\trbrdrl/t/r/b` | TableCtrlWord | Full roundtrip |
| `\clmrg` | TableCtrlWord::ClMerge | Parser only (no Qt merged cell support) |

### Special Characters

| Control Word | Status |
|--------------|--------|
| `\bullet` | Full roundtrip (U+2022) |
| `\emdash` | Full roundtrip (U+2014) |
| `\endash` | Full roundtrip (U+2013) |
| `\lquote` / `\rquote` | Full roundtrip (U+2018 / U+2019) |
| `\ldblquote` / `\rdblquote` | Full roundtrip (U+201C / U+201D) |
| `\tab` | Full roundtrip |
| `\~` | Full roundtrip (non-breaking space) |
| `\uNNN` | Full roundtrip (Unicode escape) |
| `\'hh` | Full roundtrip (hex escape, charset-aware) |

### Intentionally Not Supported (not in control table)

| Control Word | Reason |
|--------------|--------|
| `\v` | Cannot hide text while preserving layout in Qt |
| `\rtlch` / `\ltrch` | No BIDI library |
| `\sectd` | No Qt section/page layout API |
| `\field` | Word-specific field codes (except `HYPERLINK`) |
| `\stylesheet` | Word style system |
| `\object` | Windows COM embedding |
| `\password` | No password support |
| `\hyphauto` | No hyphenation in Qt |

## Key Files

| File | Purpose |
|------|---------|
| `RtfControl.h` | RtfControl struct, enum declarations, table size constant |
| `RtfControl.cpp` | Control word table, static_assert verifies count |

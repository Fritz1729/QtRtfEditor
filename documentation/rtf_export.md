# Export (`RtfExport`)

**File:** [Source](../src/RtfExport.h) | [Previous](rtf_import.md) | [Next](tests.md)

`RtfExport` provides `ExportRtf()` and `ExportHtml()` -- functions that serialize a `QTextDocument` to RTF or HTML. The RTF exporter uses manual generation to produce Delphi/TRichEdit-compatible output with mode-toggle style formatting.

## Role

`RtfExport` walks the `QTextDocument` tree, collecting fonts, colors, and lists in a first pass, then serializes content in a second pass. It carries persistent RTF format state across blocks (font, color) to minimize control word emission.

## Public API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ExportRtf` | `string(document)` | Serialize QTextDocument as RTF string |
| `ExportHtml` | `string(document)` | Serialize as HTML (delegates to `QTextDocument::toHtml()`) |

## Export Flow

```
ExportRtf()
  -> First pass: collect fonts, colors, lists from document
  -> Emit header: \rtf1, \deffN, \colortbl, \listtable, \fonttbl, metadata
  -> Second pass: walk root frame
    -> For paragraphs: BlockExportContext::ExportBlock()
    -> For tables: inline table serialization
  -> Emit closing }
```

## Data Structures

### BlockExportContext

Carries export state across paragraph blocks:

```cpp
BlockExportContext {
    out,                          // std::ostringstream
    document,                     // const QTextDocument&
    defaultFont,                  // default font
    fontMap,                      // family -> font index
    colorList, bgColorList,       // collected colors
    listMap,                      // QTextList* -> list ID
    defaultFontIdx,               // default font index
    carriedOverFormat,            // RTF format carried over from last block
    lastParaFmt,                  // last paragraph format (reset detection)
    lastParaFmtSet,               // whether last para format was emitted
    lastDeff,                     // last \deffN emitted
    lastDeftab,                   // last \deftabN emitted
    deffDeftabGroupDepth,         // nested group depth for deff/deftab
    firstBlock,                   // suppress initial empty paragraph
};
```

## First Pass: Collection

The first pass iterates all blocks and fragments to build:

| Collection | Source | Purpose |
|------------|--------|---------|
| `fontMap` | Fragment font families | Map family name to RTF font index |
| `colorList` | Fragment foreground colors | Build `\colortbl` |
| `bgColorList` | Fragment background colors | Build `\colortbl` |
| `listMap` | Block text lists | Map list pointer to `\listidN` |
| `listStyleMap` | List format styles | Map list to RTF style type |

## Header Generation

The exporter emits the RTF header in order:

| Component | Control Words | Condition |
|-----------|---------------|-----------|
| Document header | `{\rtf1\ansi\deffN\deftabN` | Always |
| Color table | `{\colortbl ;\redN\greenN\blueN;...}` | If colors exist |
| List table | `{\listtable\list\listidN\liststyletypeN...}` | If lists exist |
| Font table | `{\fonttbl{\fN\fnil\fcharset0 Family;}}` | If non-default fonts exist |
| Metadata | `\deflangN\viewkindN\ucN` | If present in document properties |

Font family type hints: Arial -> `\fswiss`, Times New Roman -> `\froman`, Courier New -> `\fmodern`, others -> `\fnil`.

## Paragraph Export

`BlockExportContext::ExportBlock()` serializes a single paragraph:

1. Emit `\pntext` group from block property (`UserPropBlockPntextRtf`) if present
2. Handle list group: `{\\listidN\\listlevel0 ...}`
3. Emit `\pard` if paragraph formatting changed from last block
4. Emit paragraph formatting: alignment, indents, spacing, line height, tab stops
5. Handle group-persistent `\deffN` / `\deftabN` with scoped groups
6. Scan fragments for anchor ranges, then emit format deltas with `\field` wrapping for hyperlinks
7. Emit `\par` (with group closers if deff/deftab or field groups were opened)

### Format Delta Emission

The exporter tracks `lastEmitted` format state. For each fragment, it emits only the controls that differ:

| Change | Emitted Control |
|--------|----------------|
| Font size changed | `\fsN` |
| Font changed | `\fN` |
| Foreground color changed | `\cfN` |
| Background color changed | `\cbN` |
| Bold turned on/off | `\b` / `\b0` |
| Italic turned on/off | `\i` / `\i0` |
| Strikethrough turned on/off | `\strike` / `\strike0` |
| Underline style changed | `\ul`, `\ul0`, `\uld`, etc. |
| Superscript turned on/off | `\super` / `\super0` |
| Subscript turned on/off | `\sub` / `\sub0` |
| Capitalization changed | `\caps`, `\scaps`, `\caps0`, `\scaps0` |
| Kerning turned on/off | `\kerning` / `\kerning0` |
| Expansion changed | `\expndN` |
| Protected turned on/off | `\protect` / `\protect0` |
| Up/dn offset changed | `\upN` / `\up0`, `\dnN` / `\dn0` |
| Language changed | `\langN` |
| Highlight changed | `\highlightN` |
| Underline color changed | `\ulcN` / `\ulc0` |

### Plain Reset

When a paragraph ends with active formatting, the exporter emits `\plain` to reset all character formatting. This carries over the font index to the next paragraph (`carriedOverFormat.fontIndex`).

## Hyperlink Export

Before iterating fragments, the exporter scans the block for anchor ranges — contiguous fragments with `isAnchor() == true` sharing the same URL. Adjacent fragments with the same href are merged into a single range. During fragment iteration, entering an anchor range emits `{\\field{\\*\\fldinst HYPERLINK "URL"}{\\*\\fldrslt`, and exiting the range closes both `fldrslt` and `field` groups. Bookmark targets (`#Name`) are serialized as-is.

## Table Export

Tables are serialized inline during the frame walk:

1. Compute cumulative `\cellx` positions from column width constraints
2. For each row, emit `{\trowd \cellxN \cellxN ...`
3. On first row only, emit table alignment (`\trql` / `\trqc` / `\trqr`)
4. Pre-read all cell borders to detect uniform sides
5. Emit row-level borders for uniform sides (`\trbrdrl`, `\trbrdrt`, etc.)
6. For each cell:
   - Emit cell padding (`\clpadl/r/t/bN`)
   - Emit non-uniform cell borders (`\clbrdrl/t/r/b`)
   - Emit `\intbl` and cell content
   - Emit `\cell`
7. Close row with `\row}`

Border optimization: if all cells in a row share the same border on one side, emit it as a row border (`\trbrdrl`) instead of per-cell borders.

## Image Export

`EmitImageAsPict()` exports images as `\pict` groups with two code paths:

| Path | Condition | Behavior |
|------|-----------|----------|
| Hex preservation | Document has stored `rtfPictHex` property | Emit original hex bytes (byte-identical roundtrip) |
| Re-encoding | Pasted/dropped image without stored hex | Re-encode from binary data (PNG/JPEG/BMP) |

Format detection uses `rtfImageFormat` document property. Blip tag mapping: jpg -> `\jpegblip`, bmp -> `\dibitmap`, others -> `\pngblip`.

## Text Escaping

`RtfEscape()` converts `QString` to RTF-escaped text:

| Character | RTF Escape |
|-----------|-----------|
| `\` | `\\` |
| `{` | `\{` |
| `}` | `\}` |
| Tab | `\tab` |
| Code > 127 | `\uNNN?` (Unicode escape with fallback) |
| Code <= 127 | Literal character |

## Key Files

| File | Purpose |
|------|---------|
| `RtfExport.h` | `ExportRtf()`, `ExportHtml()` declarations |
| `RtfExport.cpp` | Manual RTF generation, font/color collection, table/image export |

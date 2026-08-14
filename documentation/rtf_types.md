# Data Types (`RtfTypes`)

**File:** [Source](../src/RtfTypes.h) | [Previous](rich_text_edit.md) | [Next](rtf_control.md)

`RtfTypes` defines all shared data structures used by the parser, import, and export modules. It is the document model -- a structural representation of an RTF document.

## Role

`RtfTypes.h` is the central header for all shared types. It contains no implementation -- only declarations. The structures form a tree: `RtfDocument` contains a vector of `std::variant<RtfParagraph, RtfTableRowEntry, RtfImage>`, paragraphs contain runs, and table rows contain cells with runs.

## Data Structures

### RtfDocument

The top-level document container:

```cpp
RtfDocument {
    defaultFontIndex,         // \deffN (document-level default)
    defaultFontSize,          // \fs in header (half-points)
    defaultLangId,            // \deflangN (0 = not present)
    viewKind,                 // \viewkindN
    ucByteCount,              // \ucN (default 1)
    codePage,                 // \ansicpgN (default 1252)
    defaultTabStopTwips,      // \deftabN (default 180 = 1/8 inch)
    colors[],                 // RtfColorEntry array (\colortbl)
    fonts[],                  // RtfFontEntry array (\fonttbl)
    elements[],               // variant<RtfParagraph, RtfTableRowEntry, RtfImage>
    unknownTags[],            // unrecognized control words (preservation)
};
```

### RtfParagraph

A paragraph with formatting and content runs. Contains a `ParagraphFormat` member:

```cpp
RtfParagraph {
    format,                   // ParagraphFormat (paragraph-level formatting)
    runs[],                   // RtfRun array (text content)
    listId,                   // list ID (\listidN)
    listLevel,                // list level (\listlevelN)
    listStyle,                // ListStyle enum
    listIndent,               // indent for list items
    defaultFontIndex,         // \deffN (group-persistent, not paragraph formatting)
    defaultTabStopTwips,      // \deftabN (group-persistent, not paragraph formatting)
    pntextRtf,                // original \pntext RTF fragment for roundtrip preservation
};
```

### ParagraphFormat

Shared paragraph formatting:

```cpp
ParagraphFormat {
    alignment,                // 1=left, 128=center, 2=right, 4=justified
    leftIndent,               // \liN (twips)
    firstLineIndent,          // \fiN (twips)
    rightIndent,              // \riN (twips)
    spaceBefore,              // \sbN (twips)
    spaceAfter,               // \saN (twips)
    lineHeight,               // \slN (twips, fixed height)
    slMult,                   // \slmultN (line-spacing multiplier)
    tabStops[],               // TabStop array
};
```

### RtfRun

A homogeneous run of text with a single format:

```cpp
RtfRun {
    text,                     // UTF-8 encoded text content
    format,                   // RtfRunFormat
};
```

### RtfRunFormat

Character-level formatting:

```cpp
RtfRunFormat {
    bold, italic, underline,  // boolean toggles
    strikeOut, protected_,    // boolean toggles
    fontIndex, fontSize,      // \fN, \fsN
    colorIndex, bgColorIndex, // \cfN, \cbN (1-based color table index)
    superscript, subscript,   // boolean toggles
    kerning,                  // \kerning
    expnd,                    // \expndN (character expansion percentage)
    underlineStyle,           // RtfUnderlineStyle enum
    capitalization,           // QFont::Capitalization
    upOffset, dnOffset,       // \upN, \dnN (positional offset)
    ulColorIndex,             // \ulcN (underline color)
    langId,                   // \langN
    highlightIndex,           // \highlightN
    isAnchor,                 // hyperlink flag
    anchorHref,               // hyperlink URL string (std::string)
    inPntext,                 // inside \pntext group (list marker text)
};
```

### RtfTableRowEntry

A table row with cells and formatting:

```cpp
RtfTableRowEntry {
    cellxPositions[],         // \cellxN cumulative positions (twips)
    cells[],                  // pair<vector<RtfRun>, TableCellFormat>
    rowBorders,               // TableCellBorders
    rowLeft/Right/Top/BottomPadding,
    tableAlignment,           // 0=left, 1=center, 2=right
    tableLeftPosition,        // \trleftN
    tableWidth,               // \trwN
};
```

### RtfImage

An embedded image:

```cpp
RtfImage {
    data,                     // QByteArray (raw image bytes)
    format,                   // RtfImageFormat enum (Jpeg/Png/Bmp/Unknown)
    picw, pich,               // pixel dimensions (\picwN, \pichN)
    picwgoal, pichgoal,       // display dimensions in twips
    picscalex, picscaley,     // scaling percentage
    piccropl/r/t/b,           // crop offsets
    rtfPictHex,               // original hex-encoded binary (byte-identical roundtrip)
};
```

### RtfColorEntry / RtfFontEntry

```cpp
RtfColorEntry { red, green, blue };
RtfFontEntry  { family (string), fcharset (int) };
```

## Enums

| Enum | Values |
|------|--------|
| `RtfUnderlineStyle` | NoUnderline(0), Single(1), Dash(2), DotLine(3), DashDotLine(4), DashDotDotLine(5), Wave(6), SpellCheck(7), Double(8), Thick(9) |
| `RtfListStyle` | None(0), Disc(1), Circle(2), Square(3), Box(4), Check(5), Number(6), Letter(7), Roman(8) |
| `BorderStyle` | None, Solid, Dashed |
| `RtfImageFormat` | Jpeg, Png, Bmp, Unknown |
| `TableSide` | Side_Left, Side_Top, Side_Right, Side_Bottom, Side_Undefined |

## User Properties

Qt user property IDs used to store RTF metadata in `QTextDocument`:

| Constant | ID | Purpose |
|----------|-----|---------|
| `UserPropProtect` | 1000 | Protected text flag |
| `UserPropUpOffset` | 1004 | Positional superscript offset |
| `UserPropDnOffset` | 1005 | Positional subscript offset |
| `UserPropLangId` | 1006 | Language ID (\langN) |
| `UserPropParaDefaultFontIndex` | 1007 | Per-paragraph \deffN |
| `UserPropParaDefaultTabStopTwips` | 1008 | Per-paragraph \deftabN |
| `UserPropHighlightIndex` | 1009 | Highlight color index |
| `UserPropBlockPntextRtf` | 1010 | Original \pntext RTF fragment |
| `UserPropUlStyle` | 1011 | Original underline style enum |
| `UserPropUlColorIndex` | 1012 | Underline color (stored as "R,G,B" string) |
| `UserPropSlMult` | 1013 | Line-spacing multiplier (\slmultN) |

## Helper Functions

| Function | Purpose |
|----------|---------|
| `ResolveColorEntry(idx, colors)` | Look up color by index |
| `NormalizeCellBorders(cell, row)` | Merge row borders into cell borders |
| `EffectiveCellPadding(cellPad, rowPad)` | Compute max(cellPad, rowPad) |
| `TwipsToHalfPt(twips)` | Twips to half-points |
| `MarginTwipsToPoints(twips)` | Twips to points |
| `PointsToTwips(pts)` | Points to twips |
| `PointsToHalfPtTwips(pts)` | Points to half-point twips |
| `ByteArrayToVector(ba)` | QByteArray to std::vector<uint8_t> |
| `VectorToByteArray(v)` | std::vector<uint8_t> to QByteArray |
| `QtUlStyleFor(rtfStyle)` | RtfUnderlineStyle to QTextCharFormat::UnderlineStyle |
| `RtfUlStyleFor(qtStyle)` | QTextCharFormat::UnderlineStyle to RtfUnderlineStyle |
| `QtListStyleFor(rtfStyle)` | RtfListStyle to QTextListFormat::Style |
| `RtfListStyleFor(qtStyle)` | QTextListFormat::Style to RtfListStyle |

## Key Files

| File | Purpose |
|------|---------|
| `RtfTypes.h` | All type declarations, enums, user properties, conversion helpers |

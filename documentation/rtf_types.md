# Data Types (`RtfTypes`)

**File:** [Source](../src/RtfTypes.h) | [Previous](rich_text_edit.md) | [Next](rtf_control.md)

`RtfTypes` defines all shared data structures used by the parser, import, and export modules. It is the document model -- a structural representation of an RTF document.

## Role

`RtfTypes.h` is the only header that all other modules depend on. It contains no implementation -- only declarations. The structures form a tree: `RtfDocument` contains a vector of `std::variant<RtfParagraph, RtfTableRowEntry, RtfImage>`, paragraphs contain runs, and table rows contain cells with runs.

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

A paragraph with formatting and content runs. Inherits from `ParagraphFormatting`:

```cpp
RtfParagraph : ParagraphFormatting {
    runs[],                   // RtfRun array (text content)
    listId,                   // list ID (\listidN)
    listLevel,                // list level (\listlevelN)
    listStyle,                // ListStyle enum
    listIndent,               // indent for list items
};
```

### ParagraphFormatting

Shared paragraph formatting:

```cpp
ParagraphFormatting {
    alignment,                // 1=left, 128=center, 2=right, 4=justified
    leftIndent,               // \liN (twips)
    firstLineIndent,          // \fiN (twips)
    rightIndent,              // \riN (twips)
    spaceBefore,              // \sbN (twips)
    spaceAfter,               // \saN (twips)
    lineHeight,               // \slN (twips, fixed height)
    slMult,                   // \slmultN (line-spacing multiplier)
    tabStops[],               // TabStop array
    defaultFontIndex,         // \deffN (group-persistent)
    defaultTabStopTwips,      // \deftabN (group-persistent)
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
    underlineStyle,           // UnderlineStyle enum
    capitalization,           // Capitalization enum
    upOffset, dnOffset,       // \upN, \dnN (positional offset)
    ulColorIndex,             // \ulcN (underline color)
    langId,                   // \langN
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
| `UnderlineStyle` | None, Solid, Dotted, Dashed, DashDot, DashDotDot, Double, Thick |
| `Capitalization` | None, AllCaps, SmallCaps |
| `ListStyle` | None, Disc, Bullet, Box, Check, Number, Letter, Roman |
| `BorderStyle` | None, Solid, Dashed |
| `RtfImageFormat` | Jpeg, Png, Bmp, Unknown |

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
| `AlignmentToString(int)` | RTF alignment value to human-readable string |
| `RtfAlignmentToQt(int)` | RTF alignment to `Qt::Alignment` |
| `toUnderlineStyle(QTextCharFormat::UnderlineStyle)` | Qt to RTF underline style |
| `qtUnderlineStyleFor(UnderlineStyle)` | RTF to Qt underline style (lossy for Double/Thick) |
| `toCapitalization(QFont::Capitalization)` | Qt to RTF capitalization |
| `RtfListStyleToQt(ListStyle)` | RTF to Qt list style |
| `QtListStyleToRtf(QTextListFormat::Style)` | Qt to RTF list style |

## Key Files

| File | Purpose |
|------|---------|
| `RtfTypes.h` | All type declarations, enums, user properties, helper functions |

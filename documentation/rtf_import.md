# Import (`RtfImport`)

**File:** [Source](../src/RtfImport.h) | [Previous](rtf_parser.md) | [Next](rtf_export.md)

`RtfImport` provides `ImportRtf()` -- the function that converts an `RtfDocument` (produced by `ParseRtf()`) into a populated `QTextDocument`. It walks the document model tree and inserts content using Qt's text APIs.

## Role

`RtfImport` is the bridge between the internal `RtfDocument` model and Qt's `QTextDocument`. It translates RTF formatting concepts into their Qt equivalents: `QTextCharFormat` for character formatting, `QTextBlockFormat` for paragraphs, `QTextTable` for tables, and `QTextList` for lists.

## Public API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ImportRtf` | `void(document, rtf, codePage = 1252)` | Parse RTF and populate QTextDocument |
| `BuildDocument` | `void(document, rtfDoc)` | Convert RtfDocument to QTextDocument (internal) |

## Import Flow

```
ImportRtf()
  -> ParseRtf() -> RtfDocument
  -> BuildDocument(document, rtfDoc)
    -> Walk elements vector
    -> BuildParagraph(), FlushTableRows(), BuildImage()
  -> Store metadata as document properties
```

## Helper Functions

| Function | Purpose |
|----------|---------|
| `InsertRuns(cursor, runs, doc, font)` | Insert character-formatted runs into cursor |
| `BuildParagraph(cursor, para, doc, ...)` | Create block with formatting, insert runs, handle lists |
| `BuildImage(cursor, img, document)` | Insert image with resource registration |
| `FlushTableRows(cursor, rows, doc, font)` | Convert accumulated table rows to QTextTable |
| `ApplyBorderToCellFormat(cf, side, ...)` | Apply border to QTextTableCellFormat |
| `ResolveBorderColor(colorIdx, doc)` | Look up color from color table |
| `ComputeEffectivePadding(cellPad, rowPad)` | Cell padding takes precedence over row padding |

## Character Formatting

`InsertRuns()` maps `RtfRunFormat` fields to `QTextCharFormat`:

| RTF Property | Qt API | Notes |
|-------------|--------|-------|
| `fontSize` | `setFontPointSize(fs / 2.0)` | Half-points to points |
| `bold` | `setFontWeight(QFont::Bold)` | |
| `italic` | `setFontItalic(true)` | |
| `strikeOut` | `setFontStrikeOut(true)` | |
| `fontIndex` | `setFontFamilies(...)` | Look up from font table |
| `colorIndex` | `setForeground(QColor)` | Look up from color table |
| `bgColorIndex` | `setBackground(QBrush)` | Look up from color table |
| `underlineStyle` | `setUnderlineStyle(...)` | Map via `qtUnderlineStyleFor()` |
| `capitalization` | `setFontCapitalization(...)` | AllCaps / SmallCaps |
| `kerning` | `setFontKerning(true)` | |
| `expnd` | `setFontLetterSpacing(...)` | Percentage-based expansion |
| `superscript` | `setVerticalAlignment(AlignSuperScript)` | |
| `subscript` | `setVerticalAlignment(AlignSubScript)` | |
| `protected_` | `setProperty(UserPropProtect, true)` | Stored as user property |
| `upOffset` | `setProperty(UserPropUpOffset, N)` | Stored as user property |
| `dnOffset` | `setProperty(UserPropDnOffset, N)` | Stored as user property |
| `langId` | `setProperty(UserPropLangId, N)` | Stored as user property |

## Paragraph Formatting

`BuildParagraph()` maps paragraph formatting to `QTextBlockFormat`:

| RTF Property | Qt API | Notes |
|-------------|--------|-------|
| `alignment` | `setAlignment(...)` | Via `RtfAlignmentToQt()` |
| `leftIndent` | `setLeftMargin(indent / 2.0)` | Twips to points |
| `firstLineIndent` | `setIndent(indent / 2.0)` | Twips to points |
| `rightIndent` | `setRightMargin(indent / 2.0)` | Twips to points |
| `spaceBefore` | `setTopMargin(val / 2.0)` | Twips to points |
| `spaceAfter` | `setBottomMargin(val / 2.0)` | Twips to points |
| `lineHeight` | `setLineHeight(val / 2.0, FixedHeight)` | Twips to points |
| `tabStops` | `setTabPositions(...)` | Position only (alignment lossy) |

## List Handling

Lists are handled by tracking list state across paragraphs:

1. If paragraph has `listId > 0` and differs from previous, create new `QTextList`
2. If same `listId` as previous, add block to existing list
3. If `listId == 0`, exit list context

List style is mapped via `RtfListStyleToQt()` (Disc, Circle, Square, Decimal, LowerAlpha, LowerRoman).

## Table Handling

Tables are accumulated in a `tableRows` vector, then flushed when a non-table element is encountered:

1. Use first row's `cellxPositions` to compute column widths
2. Create `QTextTable` with constraints from `\cellx` positions
3. For each cell, apply `QTextTableCellFormat` with vertical alignment, padding, and borders
4. Cell borders take precedence; fall back to row borders if cell border is not set
5. Padding uses `max(cellPad, rowPad) / 2.0` (twips to points)

## Image Handling

`BuildImage()` inserts images as Qt resources:

1. Decode image dimensions from `QImageReader`
2. Apply `picscalex/y` scaling
3. Register image as `QTextDocument::ImageResource` with `rtfimage://N.ext` name
4. Store original RTF hex string and format as document properties for byte-identical roundtrip
5. Insert image via `QTextImageFormat` with width/height

## Metadata Storage

Document-level metadata is stored as `QTextDocument` properties for roundtrip preservation:

| RTF Property | Document Property |
|-------------|-------------------|
| `\deflangN` | `UserPropMetaDefaultLangId` |
| `\viewkindN` | `UserPropMetaViewKind` |
| `\ucN` | `UserPropMetaUcByteCount` |
| `\deftabN` | `UserPropMetaDefaultTabStopTwips` |
| `\deffN` | `UserPropParaDefaultFontIndex` (per-paragraph) |
| `\deftabN` | `UserPropParaDefaultTabStopTwips` (per-paragraph) |

## Key Files

| File | Purpose |
|------|---------|
| `RtfImport.h` | `ImportRtf()` declaration |
| `RtfImport.cpp` | `BuildDocument()`, `InsertRuns()`, `BuildParagraph()`, table/image handling |

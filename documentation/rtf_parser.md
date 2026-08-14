# Parser (`RtfParser`)

**File:** [Source](../src/RtfParser.h) | [Previous](rtf_charset.md) | [Next](rtf_import.md)

`RtfParser` provides `Parse()` -- the entry point for converting an RTF string into an `RtfDocument` structure. The parser is a recursive descent tokenizer with group-aware state management.

## Role

The parser is the bridge between raw RTF text and the structured `RtfDocument` model. It handles tokenization, control word dispatch, group nesting, and special group parsing (colortbl, fonttbl, pict, listtable).

## Public API

| Method | Signature | Purpose |
|--------|-----------|---------|
| `Parse` | `RtfDocument(rtf, codePage = 1252)` | Parse RTF string into document structure |

## Parser Architecture

The parser is the `RtfParser` class. It uses a single-pass, recursive descent approach:

### Core Methods

| Method | Purpose |
|--------|---------|
| `Parse()` | Main loop: dispatch `{`, `}`, `\`, and literal characters |
| `ParseGroup()` | Handle `{...}` groups with state push/pop |
| `ParseControl()` | Dispatch control symbols (`\N`, `\{`, `\}`, `\\`, `\~`, `\'hh`, `\uNNN`, `\t`, `\word`) |
| `ParseControlWord()` | Read control word + argument, dispatch via control table |
| `Dispatch(ctrl, arg)` | Route control word to handler based on Action type |
| `FinalizeRun()` | Flush accumulated literal text into current paragraph or cell |
| `FlushCurrentParagraph()` | Emit current paragraph to document elements |
| `RestoreState()` | Pop group state on `}` (format, paragraph, tab alignment, deff, deftab) |

### Group Handling

Known table groups are detected by prefix matching after `{`, along with special handling for `\field` and `\pntext` groups and star-prefixed destinations:

| Group | Method | Behavior |
|-------|--------|----------|
| `\field` | `ParseField()` | Parse `HYPERLINK` field with `fldinst`/`fldrslt` sub-groups |
| `\pntext` | Inline handler | Capture raw RTF fragment for roundtrip, parse content with `inPntext` flag |
| `\*` (star) | `SkipGroup()` | Unknown star-prefixed destinations silently skipped per RTF spec |

| `\colortbl` | `ParseColortbl()` | Parse `;\redN\greenN\blueN;` entries |
| `\fonttbl` | `ParseFonttbl()` | Parse `{\fN\fcharsetN Family;}` entries |
| `\pict` | `ParsePict()` | Extract hex-encoded image data and metadata |
| `\listtable` | `ParseListtable()` | Parse list definitions, map listid to style |
| Unknown | `Parse()` | Recurse into general parser |

### Control Word Dispatch

The parser uses `FindControl()` (O(1) lookup via static `std::unordered_map`) to look up each control word, then calls `Dispatch()` which switches on the `Action` enum:

| Action | Handler |
|--------|---------|
| `ToggleCharProp` | Finalize run, toggle boolean property |
| `SetCharProp` | Finalize run, set property from numeric arg |
| `SetParaProp` | Set paragraph property from numeric arg |
| `SetAlignment` | Set alignment value |
| `SetUlStyle` | Finalize run, set underline style |
| `SetCapitalization` | Finalize run, set capitalization |
| `EmitParagraph` | Handle `\par` (flush paragraph or table row) |
| `HeaderControl` | Skip (header words like `\rtf1`, `\ansi`) |
| `HeaderMetadata` | Handle `\pard`, `\plain`, `\uc`, `\deflang`, `\viewkind`, `\ansicpg` |
| `TableControlWord` | Handle table structure controls via `HandleTableControl()` |
| `GroupPersistent` | Handle `\deffN` and `\deftabN` with group stack |
| `Unknown` | Record in `unknownTags` for preservation |

### State Management

The parser maintains several stacks for group-persistent state:

| Stack | Purpose |
|-------|---------|
| `_formatStack` | Character format pushed/popped with groups |
| `_paraStateStack` | Paragraph formatting pushed/popped with groups |
| `_pendingTabAlignmentStack` | Tab alignment state |
| `_deffStack` | Group-persistent `\deffN` |
| `_deftabStack` | Group-persistent `\deftabN` |
| `_ucStack` | Group-persistent `\ucN` |

### Table Parsing

Table parsing uses a state machine with flags `_inTable`, `_inRow`, and `_inTableCell`:

| Control | Effect |
|---------|--------|
| `\trowd` | Start new row, reset cell state |
| `\cellxN` | Add column position to cellxPositions |
| `\intbl` | Mark cell content start |
| `\cell` | Close current cell, start next |
| `\row` | Close row, emit to document |

Border handling uses a pending border mechanism (`BeginBorderSide` -> accumulate style/width/color -> `ApplyPendingBorder`).

### Image Parsing

`ParsePict()` collects hex-encoded image data and metadata from `\pict` groups:

| Control | Field |
|---------|-------|
| `\jpegblip` / `\pngblip` / `\dibitmap` | `_pictFormat` |
| `\picwN` / `\pichN` | `_pictPicw`, `_pictPich` |
| `\picwgoalN` / `\pichgoalN` | display dimensions |
| `\picscalexN` / `\picscaleyN` | scaling |
| `\piccropl/r/t/bN` | crop offsets |

Hex data is collected as literal characters (0-9, A-F, a-f), then decoded via `QByteArray::fromHex()`.

### Unicode Escapes

`\uNNN?` is handled with UTF-16 surrogate pair support: if the first value is a high surrogate (0xD800-0xDBFF), the parser reads a second `\uNNN` as the low surrogate and combines them into a single codepoint above U+FFFF. After emitting the codepoint, the parser skips `\ucN` fallback bytes (alternate ANSI encoding for backward compatibility).

### Special Handling

| Feature | Implementation |
|---------|---------------|
| Whitespace after toggle-OFF | `_skipLeadingWsTrim` flag preserves content |
| Escaped backslash (`\\`) | Special handling for `\\{` and `\\}` sequences |
| Tab (`\t`) | Only treated as tab if not followed by word chars (`\trowd`, etc.) |
| Paragraph flush | Initial empty paragraphs are suppressed; subsequent empties are preserved |
| Trailing empty elements | `RemoveTrailingEmptyElements()` strips at end of parse |
| Iteration limit | `kMaxIter = 10'000'000` prevents infinite loops |
| Hyperlinks | `\field` groups with `HYPERLINK` in `fldinst`; extracts URL or `#Name` bookmark target |
| List markers | `\pntext` group parsed with `inPntext` flag; raw RTF fragment captured in `RtfParagraph::pntextRtf` |
| Star destinations | Unknown `{\*\word ...}` groups silently skipped via `SkipGroup()` |
| Negative arguments | Control word arguments parsed with sign handling for negative values |

## Key Files

| File | Purpose |
|------|---------|
| `RtfParser.h` | `RtfParser` class declaration |
| `RtfParser.cpp` | `RtfParser` implementation: tokenizer, group handling, table/image parsing |

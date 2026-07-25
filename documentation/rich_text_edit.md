# Public API (`RichTextEdit`)

**File:** [Source](../src/RichTextEdit.h) | [Previous](supported_features.md) | [Next](rtf_types.md)

`RichTextEdit` is the public API entry point. It is a `QTextEdit` subclass that adds RTF I/O with Delphi/TRichEdit compatibility and `\protect` cursor-skip support.

## Role

`RichTextEdit` is the only class the embedding application interacts with directly. It delegates all RTF parsing and serialization to the internal modules (`RtfImport`, `RtfExport`, `RtfParser`), and wraps `QTextEdit`'s native HTML I/O for `FormatMode::Html`.

## Data Structures

### FormatMode

The format selector for `Load()` and `Save()`:

```cpp
enum class FormatMode { Rtf, Html };
```

## API Methods

| Method | Purpose |
|--------|---------|
| `Load(blob, mode)` | Load RTF or HTML content; replaces all document content |
| `Save(mode)` | Serialize document as RTF or HTML string |
| `SetProtection(start, end)` | Apply `\protect` format to a text range |
| `ClearProtection()` | Remove `\protect` from all document text |
| `IsProtected(position)` | Check if a position is within protected text |
| `SetCodePage(codePage)` | Set default code page for ANSI hex escape decoding |
| `CodePage()` | Get default code page (default: 1252) |

## Overridden Methods

| Method | Purpose |
|--------|---------|
| `keyPressEvent()` | Clamps cursor out of protected regions after key navigation |
| `mousePressEvent()` | Detects single-click on protected text, emits signal |
| `insertFromMimeData()` | Handles paste from clipboard with format detection |

## Signals

| Signal | Parameters | Purpose |
|--------|------------|---------|
| `protectedRegionClicked` | `start`, `end`, `text` | Emitted when user clicks protected text |

## Protected Range Mechanism

Protected text uses the `UserPropProtect` user property (ID 1000) on `QTextCharFormat`. The cursor is clamped after every navigation event by `ClampCursor()`, which checks `RangeHasProtected()` and moves the cursor forward or backward past the protected run.

## Key Files

| File | Purpose |
|------|---------|
| `RichTextEdit.h` | Class declaration, API signatures |
| `RichTextEdit.cpp` | Implementation: Load/Save, cursor clamping, event handling |

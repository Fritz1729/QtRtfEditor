# Charset Mapping (`RtfCharset`)

**File:** [Source](../src/RtfCharset.h) | [Previous](rtf_control.md) | [Next](rtf_parser.md)

`RtfCharset` provides character set mapping functions for decoding RTF hex escapes (`\'hh`) to Unicode codepoints. It handles the Symbol font charset and Windows CP1252 encoding.

## Role

`RtfCharset` is called by the parser when processing `\'hh` hex escapes. The parser passes the active font's `\fcharsetN` value and document code page to determine the correct mapping. All output is UTF-8 encoded.

## Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `MapSymbolByte` | `int(byte)` | Map byte 0x80-0xFF in Symbol charset to Unicode |
| `MapCp1252Byte` | `int(byte)` | Map byte 0x80-0xFF in CP1252 to Unicode |
| `MapHexByteToCodepoint` | `int(byte, fcharset, codePage, fontFamily)` | Entry point: dispatch to correct mapping |

## Mapping Logic

`MapHexByteToCodepoint` is the entry point used by the parser:

1. If `fcharset == 2` and font family is "Symbol" (case-insensitive), use `MapSymbolByte`
2. If byte < 0x80, return as-is (ASCII)
3. If codePage == 1252, use `MapCp1252Byte` for bytes 0x80-0xFF
4. Otherwise, return byte as-is (pass-through)

## Symbol Charset Table

The Symbol font maps bytes 0x80-0xFF to mathematical, musical, and typographic symbols (e.g., 0xA0 -> U+20AC Euro, 0x96 -> U+2022 Bullet, 0xAA -> U+2205 Empty Set). Bytes 0x00-0x7F are unmapped (not used). The lookup table has 256 entries with 0 for unmapped bytes.

## CP1252 Table

CP1252 (Windows ANSI) maps bytes 0x80-0xFF to Latin-1 Supplement and Windows extensions (e.g., 0x80 -> U+20AC Euro, 0x93 -> U+201C Left Double Quote). The lookup table has 128 entries (offset 0x80). Bytes outside this range pass through.

## Key Files

| File | Purpose |
|------|---------|
| `RtfCharset.h` | Function declarations |
| `RtfCharset.cpp` | Symbol table (256 entries), CP1252 table (128 entries), mapping functions |

# Supported Features

**File:** [Previous](README.md) | [Next](rich_text_edit.md)

QtRtfEditor targets the Rich Edit 1.0–2.0 feature set, the Delphi TRichEdit compatibility baseline.

## Full Support

Character and paragraph formatting, Unicode, tables, and images are fully supported with roundtrip fidelity.

### Character Formatting

bold, italic, underline (solid, dotted, dashed, dash-dot, dash-dot-dot, thick),
double underline (`\uldb` renders as solid but roundtrips correctly),
strikethrough, font family, font size, text color, background color,
superscript/subscript, capitalization (all caps, small caps), character expansion
(`\expndN`, `\expndtwN`), kerning (`\kerning` / `\kerning0`)

### Paragraph Formatting

alignment (left, center, right, justified), left indent, first-line indent, right indent,
spacing before/after, line height (fixed), tab stops (left, center, right),
lists (bullet, number, letter, roman)

### Unicode & Symbols

escape sequences (`\uNNN`), hex escapes (`\'hh`), non-breaking space (`\~`),
typographic characters (bullet, emdash, endash, smart quotes, en/em space)

Export always uses Unicode escapes (`\uNNN`) for characters above 0x7F.
Hex escapes (`\'hh`) are decoded on import using the active font's character set
(`\fcharsetN`) and the document code page (`\ansicpgN`, default 1252).
Export does not emit `\ansicpgN` — all text is Unicode-encoded.

### Document Tables

color table (RGB), font table (family name, charset;
`\fprq` font pitch is recognized but not preserved)

### Layout Tables

Rows, cells, column widths, vertical alignment, per-side borders (solid/dashed, width, color),
cell padding, row padding, table alignment (left/center/right), row-level borders

### Embedded Images

BMP, PNG, JPEG via `\pict`

### Hyperlinks

URL hyperlinks via `\field` / `HYPERLINK` groups.
Parsed as `RtfRunFormat::isAnchor` / `anchorHref`, rendered as
`QTextCharFormat::isAnchor` / `anchorHref`, and serialized back to
`\field` groups on export. Internal bookmark references
(`HYPERLINK \bkmk3 Name`) are parsed as `#Name` targets.

## Partial Support

These features are parsed and preserved but cannot be rendered.

| Feature | RTF Tag | Reason |
|---------|---------|--------|
| Cell shading | `\clshdn` | No Qt cell background API |
| Merged cells | `\clmrg` | No Qt merged cell support |

## Rendered with Approximation

These features are rendered visually (using the closest Qt equivalent) and preserved through roundtrip with the original RTF tag intact.

| Feature | RTF Tag | Qt Approximation |
|---------|---------|------------------|
| Thick underline | `\ulth` | Wave underline |
| Double underline | `\uldb` | Solid underline |
| Positional superscript/subscript | `\upN`, `\dnN` | Boolean toggle |
| Underline color | `\ulcN` | No Qt API — color stored as user property for roundtrip |
| Line-spacing multiplier | `\slmultN` | No Qt API — multiplier stored as block property for roundtrip |

## Stored but Not Rendered

These features are parsed and preserved through roundtrip, but cannot be rendered because Qt lacks the corresponding API.

| Feature | RTF Tag | Reason |
|---------|---------|--------|
| Highlight | `\highlightN` | No reliable RGB mapping per RTF spec |
| Language ID | `\langN`, `\chlangN`, `\langfeN` | Qt 6.11 has no `setFontLanguageId()` |

## Out of Scope

### Document Metadata

`\info` section (`\title`, `\author`, `\subject`, `\keywords`, `\versionN`, `\revtbl`)
— handled by the embedding app

### Windows-Specific

- **OLE objects** (`\object`, `\objdata`, `\objalias`, `\objclass`) — COM embedding
- **Metafiles** (EMF, WMF) — Windows rasterizable formats
- **Section & page setup** (`\sectd`, `\sect`, `\sbk*`, `\pgwsxn`, `\pghsxn`, `\marg*`, `\cols*`, `\vertdoc`, `\horzdoc`)
- **Headers/footers** (`{\header ...}`, `{\footer ...}`)
- **Footnotes/endnotes** (`{\footnote ...}`, `\ftnstart`, `\endnhere`)

### Word-Specific Extensions

- **Track changes** (`\revtbl`, `\revN`, `\insrsidN`, `\delrsidN`, `\tridxN`)
- **Styles** (`\stylesheet`, `\sN`, `\snext`, `\sbasedonN`)
- **Fields** (`\field`, `\*\fldinst`, `\*\fldrslt`) — only `HYPERLINK` fields are supported; `\date`, `\time`, and other field types are not
- **Index / TOC entries** (`\*{\index ...}`, `\*{\toc ...}`, `\*{\tc ...}`)
- **Document variables** (`\*\docvar`)
- **User properties** (`\userprops`, `\propname`, `\staticval`)
- **Paragraph Group Properties** (`\*\pgptbl`, `\pgp`, `\ipgpN`)
- **Style restrictions** (`\*\latentstyles`, `\lsd*`)
- **Password protection** (`\passwordhash`, `\password`)
- **Comments / annotations** (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`)
- **Compatibility flags** (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`)

### Complex Text

- **Asian/East Asian fonts** (`\fscript`, `\fdecor`, `\stshfdbchN`, `\stshfhichN`, `\stshfbiN`, `\fcharset134/136/129`) — requires external shaping library
- **Bidirectional text** (`\rtlch`, `\ltrch`, `\rtl`, `\ltrpar`, `\fbidis`) — no platform BIDI library
- **Complex script shaping** (Indic, Thai, Arabic) — requires HarfBuzz/UCDraw

## DeffN and DeftabN Scope

The RTF 1.5 and 1.9.1 specifications define `\deffN` (default font index) and `\deftabN`
(default tab width) as **group-persistent** control words — pushed/popped on group entry/exit.

The parser handles full group-persistence on import. On export, changed values are wrapped
in scoped groups to preserve semantics:

```rtf
{\rtf1\deff0 {\deff2\pard\plain Deep\par} {\deff1\pard\plain Mid\par} \pard\plain Outer\par}
```

This ensures that re-parsing produces the same document-level defaults and per-paragraph values.
For documents where `\deff` and `\deftab` appear only once at the document level (the common case),
no extra grouping is emitted.

## Key Files

| File | Purpose |
|------|---------|
| `RtfControl.cpp` | Control word dispatch table |
| `RtfParser.cpp` | Recursive descent tokenizer and group handling |
| `RtfImport.cpp` | RtfDocument -> QTextDocument conversion |
| `RtfExport.cpp` | QTextDocument -> RTF serialization |

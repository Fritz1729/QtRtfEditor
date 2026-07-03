# RTF 1.5 Coverage Plan

## Overview

Phase-by-phase TDD approach to cover missing RTF 1.5 features. Each phase:
1. Write unit tests first (in `TestSemanticComparison.cpp`) to define expected behavior
2. Write roundtrip test documents (in `tests/TestData/`) for full load/save cycles
3. Implement parser, import, export, and compare changes to make tests pass

Current state: Plan created. No implementation has started yet.

## Qt6 Support Matrix (for reference)

**Trivial** — Direct Qt API, small code addition:
- `\tab` — `QChar::TableSeparator`
- `\'hh`, `\bullet`, `\emdash`, smart quotes — Unicode characters

**Moderate** — Qt API exists but needs structural changes:
- `\strike` — `setFontStrikeOut()`
- `\cbN` — `setTextBackgroundColor()`
- `\highlightN` — `setTextBackgroundColor()` with QColor palette mapping
- `\caps` / `\scaps` — `setFontCapitalization()`
- `\ul` variants (partial) — `QFont::UnderlineStyle` (most styles map, thick/wave/hairline do not)
- `\slN` / `\slmultN` — `setLineHeight()`
- `\sbN` / `\saN` — `setTopMargin()` / `setBottomMargin()`
- `\langN` / `\deflangN` — `setFontLanguageId()` / `QTextCodec::setDefaultCodec()`
- Lists (`QTextList`) — bullet and numbered styles
- Tables (`QTextTable`) — rows, cells, borders, padding
- Images (`insertImage()` + `addResource()`) — BMP/PNG/JPEG
- Tab stops — `QTextBlockFormat::setTabStops()` (positions only, alignment/leaders do not)

**Hard** — Workarounds required:
- Headers/footers → custom frame management
- Footnotes → no Qt equivalent
- Bookmarks → no `QTextBookmark` in Qt6
- Paragraph borders → wrap block in `QTextFrame`

**Not feasible in Qt** (preservation only via raw RTF passthrough):
- Field codes (`\field`, `\*\fldinst`, `\*\fldrslt`)
- Paragraph styles (`\sN`, `\stylesheet`)
- Track changes (`\revtbl`, `\insrsidN`, `\delrsidN`)
- Text shadow (`\shad`)
- Hidden text (`\v`) — cannot hide while preserving layout
- OLE objects (`\object`) — Windows COM embedding
- Metafiles (EMF, WMF) — Windows-specific rasterizable formats
- Document metadata (`\info`, `\title`, `\author`) — handled by embedding app
- Character effects (`\out`, `\embo`, `\impr`, `\sr`) — no Qt equivalent
- Complex script shaping — requires HarfBuzz/UCDraw, no Qt equivalent

## Rich Edit Version Mapping (compatibility priority)

Delphi TRichEdit uses Rich Edit 3.0 (MSFTEDIT.DLL in modern Delphi uses 4.1, but 3.0 is the compatibility baseline).
Features are prioritized by their Rich Edit version, not by implementation complexity.

### Rich Edit 1.0 (1996) — Partially Implemented (Baseline)
Implemented: solid underline, bold, italic, font selection, font size, text color, left/first-line indent,
left/center/right alignment, paragraph break, simple tabs, superscript/subscript (toggle).
Intentionally out of scope: strike-out (`\strike`), right indent (`\riN`), character background color (`\cbN`),
underline variants, space before/after, line spacing, tab stops, tab alignment, Unicode escapes,
small caps, all caps, hidden text, soft line breaks, keep/keep-next, widow/orphan control,
RTL paragraphs, page breaks, smart quotes, hyphenation flags.
These are covered in RE 2.0/3.0 phases below.

### Rich Edit 2.0 (1998) — Phase 1
Included: Unicode support (`\uN`), background color (`\cbN`), strikethrough (`\strike`),
more underline types (`\uld`, `\uldb`, `\uldash`, `\ulth`), small caps (`\caps`/`\scaps`),
space before/after (\sbN / \saN), Word line spacing (\slN / \slmultN),
tab stops (\txN), tab alignment (\tqr / \tqc),
superscript/subscript offset (\upN / \dnN), special typographic characters (\bullet, \emdash, \endash, smart quotes),
right indent (`\riN`).
Intentionally out of scope: bidirectional text (`\rtlch`/`\ltrch`/`\rtl`/`\ltrpar`),
complex script support, East Asian fonts, font binding, font pitch/quality (`\fprqN`),
language IDs (`\langN`), code pages (`\ansicpgN`), hidden text (`\v`),
soft line breaks (`\line`), paragraph protection (`\keep`/`\keepn`/`\widowctrl`),
page breaks (`\page`), hyphenation flags (`\hyph*`), character effects
(\out / \embo / \impr / \sr). These are covered in RE 3.0, RE 4.1, or TODO phases.

### Rich Edit 3.0 (2002) — Phase 2 (Target Compatibility Mode for Delphi TRichEdit)
Included: paragraph numbering (lists, `\listtable`), simple tables (`\trow`/`\cellx`/`\cl*`),
more underline types (`\uldot`, `\uldashd`), underline coloring (`\ulcN`),
advanced typography (center/right/decimal tabs, \tqd / \tqdl / \tqdb / \tqb),
full justification (`\qj`), language IDs (`\langN`), code pages (`\chcpN`),
font binding hints (`\fscript`, `\fdecor`), UTF-8 RTF, hyperlink fields (`HYPERLINK`).
Intentionally out of scope: bidirectional/complex script support (Indic/Thai/Arabic),
East Asian font binding, heading styles (`\sN`), paragraph borders (`\brdrt`/`\brdrb`),
headers/footers (`{\header ...}`), footnotes (`{\footnote ...}`), bookmarks,
section breaks (`\sectd`), page layout (`\pgwsxn`/`\marg*`/`\cols*`),
document comments (`\*\annot`), paragraph flow controls (`\keep`/`\keepn`/`\widowctrl`),
hidden text (`\v`), soft line breaks (`\line`), image embedding (`\pict`/`\picbmp`/`\jpg`/`\png`).
**This is the compatibility baseline for legacy Delphi applications.**

### Rich Edit 4.1 (2007) — Lower Priority (TODO)
Included: improved tables (cell wrapping), additional language support, soft hyphens (`\hyph-`).
Intentionally out of scope: hyphenation behavior (\hyphauto / \hyphhotzN / \hyphconsecN),
page rotation, Text Services Framework, additional IME support, section/page setup,
mathematics, comments, paragraph flow controls, compatibility flags.
These are covered in the TODO section below.

### Out of Scope
OLE objects, metafiles, East Asian font binding, VML/drawing objects, custom XML/SmartTags,
mail merge, theme data, password protection, document metadata, fields, styles, track changes,
comments, index/TOC, document variables, user properties, paragraph group properties,
style restrictions, section/page setup, math, compatibility flags.

## Data Model for Preservation (Phase 3+)

For infeasible features, raw RTF must survive round-trip unchanged:

```
RtfDocument {
    ... existing fields ...
    std::vector<RtfPreservedChunk> preservedChunks;
}

RtfPreservedChunk {
    std::string rawRtf;      // The exact raw RTF text
    std::size_t rtfPosition; // Position in original RTF for dedup
}
```

**Import flow**: Parser captures raw text of unknown groups → BuildDocument inserts visible markers as protected ranges → Raw RTF stored in ProtectedRangeInfo.target.

**Export flow**: Scan QTextDocument for markers → Replace each marker with preserved raw RTF → Emit normal RTF for everything else.

**Compare behavior**: Both docs have same preserved chunks → Identical. One missing → UnknownTag.

**User experience**: Unsupported features appear as visible markers (protected ranges). The embedding app can customize display via `SetUnsupportedFeatureDisplayCallback()`. Protected ranges gate editing.

---

## Phase 1 — Rich Edit 2.0 Features (High Priority)

Target: Unicode support, paragraph formatting beyond basic, character effects available in RE 2.0.

### 1.1 Unit Tests for `TestSemanticComparison.cpp`

Each feature needs these test slots:

| Test Name | Purpose |
|---|---|
| `different_strike` | StrikeOn vs NoStrike → StructuralDiff |
| `different_cb` | Different `\cbN` → StructuralDiff |
| `cb_semantic` | Different `\cbN` indices, same RGB → Identical (semantic) |
| `different_ri` | Different `\riN` → StructuralDiff |
| `different_sb` | Different `\sbN` → StructuralDiff |
| `different_sa` | Different `\saN` → StructuralDiff |
| `different_sl` | Different `\slN` → StructuralDiff |
| `sl_mult` | `\slmultN` different → StructuralDiff |
| `different_highlight` | Different `\highlightN` → StructuralDiff |
| `highlight_semantic` | Different indices, same color → Identical |
| `different_caps` | `\caps` vs no caps → StructuralDiff |
| `different_scaps` | `\scaps` vs no scaps → StructuralDiff |
| `different_ul_style` | `\ul` vs `\uldash` → StructuralDiff |
| `different_ulth` | `\ulth` vs `\ul` → StructuralDiff |
| `different_uldb` | `\uldb` vs `\ul` → StructuralDiff |
| `different_tab` | Tab present vs absent → StructuralDiff |
| `different_up` | `\upN` values differ → StructuralDiff |
| `different_dn` | `\dnN` values differ → StructuralDiff |

Also need to extend `RtfRunFormat` to include `strikeOut`, `rightIndent` (on paragraph level), `spaceBefore`, `spaceAfter`, `lineHeight`, `highlight`, `capitalization`, `underlineStyle`, `tabStops`, `upOffset`, `dnOffset`.

And extend `RtfParagraph` to include `rightIndent`, `spaceBefore`, `spaceAfter`, `lineHeight`.

### 1.2 Roundtrip Test Documents

| File | Features |
|---|---|
| `strikethrough.rtf` | `\strike` text, `\strike` + `\b`, `\strike` toggle off |
| `background-color.rtf` | `\cb1` through `\cb5` with `\colortbl` entries |
| `right-indent.rtf` | `\ri500`, `\ri0`, combined with `\li` |
| `spacing.rtf` | `\sb100\sa200`, `\sb0\sa0`, `\sl400` |
| `line-spacing.rtf` | `\sl200` (fixed), `\slmult2` (proportional) |
| `highlighting.rtf` | `\highlight1` through `\highlight5`, highlight + background color |
| `caps.rtf` | `\caps`, `\scaps`, `\scaps` with `\b` |
| `underline-styles.rtf` | `\ul`, `\uld`, `\uldash`, `\uldb`, `\ulth` |
| `tabs.rtf` | `\tab` characters, `\tx720`, `\tqr`, `\tqc` |
| `positional-supsub.rtf` | `\up6\dn6` (half-points) |
| `special-chars.rtf` | `\'e9` (e-acute), `\bullet`, `\emdash`, `\endash`, `\lquote`, `\rquote`, `\ldblquote`, `\rdblquote`, `\~` (nbspace) |

---

## Phase 2 — Rich Edit 3.0 Features (Target Compatibility)

Target: Paragraph numbering, simple tables, more underline types, advanced typography, font binding, UTF-8 RTF.

### 2.1 Unit Tests

| Test Name | Purpose |
|---|---|
| `different_list_style` | `ListDisc` vs `ListDecimal` → StructuralDiff |
| `different_list_indent` | Different list indent → StructuralDiff |
| `different_table_rows` | Different row count → StructuralDiff |
| `different_table_cols` | Different col count → StructuralDiff |
| `different_cell_border` | Cell borders differ → StructuralDiff |
| `different_cell_padding` | Cell padding differs → StructuralDiff |
| `different_image` | Different image reference → StructuralDiff |
| `different_tab_stops` | Different `\txN` positions → StructuralDiff |
| `different_ulc` | Different `\ulcN` → StructuralDiff |
| `different_lang` | Different `\langN` → StructuralDiff |

Requires extending `RtfDocument` with `tables` vector, `lists` vector, `images` vector.

### 2.2 Roundtrip Test Documents

| File | Features |
|---|---|
| `lists.rtf` | Bulleted list (`ListDisc`), numbered (`ListDecimal`), nested |
| `tables.rtf` | Simple table, merged cells (`\clmrg`), cell borders |
| `images.rtf` | Embedded BMP, PNG (via `\pict`) |
| `tab-stops.rtf` | `\tx720`, `\tqr`, `\tqc`, `\tldot` |
| `underline-color.rtf` | `\ulc1` through `\ulc15` (RE 3.0 underline colors) |
| `language-id.rtf` | `\lang1033`, `\lang1031`, `\lang1036` |
| `utf8-rtf.rtf` | UTF-8 encoded document with mixed character sets |

---

## Phase 3 — Hard Workarounds (Low Priority)

Requires building the preserved chunk system.

### 3.1 Unit Tests

| Test Name | Purpose |
|---|---|
| `preserved_chunk_roundtrip` | Raw RTF in chunk survives Load→Save unchanged |
| `compare_preserved_same` | Both docs have same preserved chunk → Identical |
| `compare_preserved_one_missing` | Only one doc has preserved chunk → UnknownTag |
| `preserved_chunk_display_callback` | Custom callback produces custom marker text |
| `editing_around_markers` | Protected range blocks editing at marker |
| `marker_positions_shift_on_edit` | Markers update when surrounding text changes |

### 3.2 Roundtrip Test Documents

| File | Features |
|---|---|
| `headers-footers.rtf` | `{\header ...}`, `{\footer ...}` |
| `footnotes.rtf` | `{\footnote ...}`, multiple footnotes |
| `bookmarks.rtf` | `{\*\bkmkstart ...}...{\*\bkmkend ...}` |
| `paragraph-borders.rtf` | `\brdrt`, `\brdrb`, `\brdrl`, `\brdrr`, `\box` |

---

## Phase 4 — Not Feasible in Qt (Preservation Only)

All via raw RTF preservation. Covers features from all Rich Edit versions that have no Qt equivalent.

### 4.1 Unit Tests

| Test Name | Purpose |
|---|---|
| `preserved_chunk_roundtrip` | Raw RTF in chunk survives Load→Save unchanged |
| `compare_preserved_same` | Both docs have same preserved chunk → Identical |
| `compare_preserved_one_missing` | Only one doc has preserved chunk → UnknownTag |
| `preserved_chunk_display_callback` | Custom callback produces custom marker text |
| `editing_around_markers` | Protected range blocks editing at marker |
| `marker_positions_shift_on_edit` | Markers update when surrounding text changes |
| `field_roundtrip` | `\field{\*\fldinst...}{\fldrslt...}` preserved |
| `style_roundtrip` | `\s246` style reference preserved |
| `section_roundtrip` | `\sectd\pgwsxn...` section preserved |
| `track_changes_roundtrip` | `\revtbl`, `\rev1` revision marks preserved |
| `hidden_text_roundtrip` | `\v0` hidden text preserved |

### 4.2 Roundtrip Test Documents

| File | Features |
|---|---|
| `fields.rtf` | `\field`, `\*\fldinst`, `\*\fldrslt`, `\date`, `\time` |
| `styles.rtf` | `\stylesheet`, `\sN` references |
| `sections.rtf` | `\sectd`, `\sbkpage`, `\cols2`, `\marglN` |
| `track-changes.rtf` | `\*\revtbl`, `\rev1`, `\deleted`, `\revised` |

---

## TODO — Lower Priority (RE 4.1 / Partial / Special Cases)

Features from Rich Edit 4.1+ or with partial Qt support, listed by feature family.

### Language & Code Page (RE 2.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\langN`, `\chlangN` | `setFontLanguageId()` | Direct |
| `\deflangN` | `QTextCodec::setDefaultCodec()` | Direct |
| `\chcpN` | `QTextCodec::codecForCp()` | Direct |

### Font Table Extensions (RE 2.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\fscript`, `\fdecor` | `QFont::FontFamily` hint | Handled as font family hint |
| `\fprqN` | — | Pitch/quality — hint only, Qt ignores |

### Hyperlinks (RE 2.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `HYPERLINK` field | `QTextCharFormat::setAnchorHref()` | Partial — extracts URL from field instruction, not full field round-trip |

### Paragraph Flow Controls (RE 2.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\line` | `QTextBlockFormat::setLineHeight()` with break | Soft line break within paragraph |
| `\keep`, `\keepn` | — | No Qt equivalent — raw RTF preservation |
| `\widowctrl` | — | No Qt equivalent — raw RTF preservation |
| `\page` | `QTextCursor::insertBlock()` with page break | Page break before paragraph — (partial) |

### Compatibility & Document Properties (RE 2.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\hyphhotzN`, `\hyphconsecN`, `\hyphauto`, `\hyphcaps` | — | Hyphenation has no Qt equivalent |
| `\lnongrid` | — | No Qt equivalent |
| `\donotembedsysfontN`, `\donotembeddingdataN` | — | Embedding flags, no Qt equivalent |
| `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN` | — | XML/VML compatibility flags |
| `\trackmovesN`, `\trackformattingN` | — | Tracking flags, no Qt equivalent |
| `\muser` | — | Word 97 compatibility flag |

### Mathematics (RE 4.1)

| Controls | Qt API | Notes |
|---|---|---|
| `\*\*`, `\q`, `\r`, `\sup`, `\sub`, `\f`, `\sqrt`, `\sum`, `\int` | — | No Qt equivalent — raw RTF preservation |

### Section & Page Setup (RE 3.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\sectd`, `\sect`, `\sbk*` | — | Section breaks, page/column breaks — raw RTF preservation |
| `\pgwsxn`, `\pghsxn` | — | Page width/height — raw RTF preservation |
| `\marglsxn`, `\margtsxn`, `\margbksxn`, `\margrsxn` | — | Page-level margins — raw RTF preservation |
| `\deftabN` | — | Default tab width — raw RTF preservation |
| `\cols*`, `\colwx`, `\colfc`, `\colno` | — | Multi-column layout — raw RTF preservation |
| `\vertdoc`, `\horzdoc` | — | Document layout direction — raw RTF preservation |

### Comments (RE 3.0+)

| Controls | Qt API | Notes |
|---|---|---|
| `\*\annot`, `\*\ftnacc`, `\*\annotrslt` | — | Document comments/annotations — raw RTF preservation |

---

## Files That Need Changes (Summary)

### Existing files to modify:

| File | Changes |
|---|---|
| `tests/RtfCompare.h` | Extend `RtfRunFormat` (strike, highlight, caps, ulStyle, etc.), `RtfParagraph` (rightIndent, spaceBefore, lineHeight, etc.), add `Preserved` to `RtfCompareResult` enum |
| `tests/RtfCompare.cpp` | Add comparison logic for new fields, handle preserved chunks |
| `src/RtfParser.cpp` | Parse `\strike`, `\cbN`, `\riN`, `\sbN`, `\saN`, `\slN`, `\highlightN`, `\caps`, `\scaps`, `\uld`, `\uldash`, `\uldb`, `\ulth`, `\tab`, `\rtlch`, `\ltrch`, `\upN`, `\dnN`, etc. Capture raw RTF for unknown groups |
| `src/RtfParser.h` | Add `RtfPreservedChunk` struct, `preservedChunks` to `RtfDocument` |
| `src/RtfImport.cpp` | Build document with new fields, insert markers for preserved chunks |
| `src/RtfExport.cpp` | Export new fields, replace markers with preserved raw RTF |
| `src/RichTextEdit.h` | Add `SetUnsupportedFeatureDisplayCallback()`, `UnsupportedFeatures()`, `GetUnsupportedFeatureRtf()` |
| `src/RichTextEdit.cpp` | Implement new methods |
| `tests/TestSemanticComparison.cpp` | Add test slots for all new features |
| `tests/TestRoundtrip.cpp` | Add `preserved` counter, updated reporting |

### New files to create:

| File | Purpose |
|---|---|
| `tests/TestData/strikethrough.rtf` | Phase 1 |
| `tests/TestData/background-color.rtf` | Phase 1 |
| `tests/TestData/right-indent.rtf` | Phase 1 |
| `tests/TestData/spacing.rtf` | Phase 1 |
| `tests/TestData/line-spacing.rtf` | Phase 1 |
| `tests/TestData/highlighting.rtf` | Phase 1 |
| `tests/TestData/caps.rtf` | Phase 1 |
| `tests/TestData/underline-styles.rtf` | Phase 1 |
| `tests/TestData/tabs.rtf` | Phase 1 |
| `tests/TestData/positional-supsub.rtf` | Phase 1 |
| `tests/TestData/special-chars.rtf` | Phase 1 |
| `tests/TestData/lists.rtf` | Phase 2 |
| `tests/TestData/tables.rtf` | Phase 2 |
| `tests/TestData/images.rtf` | Phase 2 |
| `tests/TestData/tab-stops.rtf` | Phase 2 |
| `tests/TestData/headers-footers.rtf` | Phase 3 |
| `tests/TestData/footnotes.rtf` | Phase 3 |
| `tests/TestData/bookmarks.rtf` | Phase 3 |
| `tests/TestData/paragraph-borders.rtf` | Phase 3 |
| `tests/TestData/fields.rtf` | Phase 4 |
| `tests/TestData/styles.rtf` | Phase 4 |
| `tests/TestData/sections.rtf` | Phase 4 |
| `tests/TestData/track-changes.rtf` | Phase 4 |
| `tests/TestData/underline-color.rtf` | Phase 2 |
| `tests/TestData/language-id.rtf` | Phase 2 |
| `tests/TestData/utf8-rtf.rtf` | Phase 2 |

---

## TDD Workflow per Feature

1. **Write unit test** in `TestSemanticComparison.cpp` — define expected `RtfCompareResult` and `reason`
2. **Write roundtrip document** — minimal RTF exercising the feature
3. **Run `ctest`** — tests should fail (parser doesn't recognize feature, or compare doesn't handle it)
4. **Implement** — add to parser, compare, export, import
5. **Run `ctest`** — tests should pass
6. **Repeat** for next feature

## Key Design Decisions

- `\strike` has no numeric parameter — just toggle on/off (`\strike` / `\strike0`), same pattern as `\b`, `\i`, `\ul`
- `\cbN` uses 1-based color table index (same as `\cfN`)
- `\riN`, `\sbN`, `\saN`, `\slN` use twips (RTF standard) → need conversion in export
- `\highlightN` uses hex values 1-16 in RTF → map to QColor palette on import, back on export
- `\ul` variants need a new field in `RtfRunFormat` — an enum or int for underline style
- `\rtlch` / `\ltrch` are character-level; `\rtlpar` / `\ltrpar` are paragraph-level
- `\upN` / `\dnN` use half-points — Qt only supports toggle via `QFont::VerticalAlignment`, not precise offsets — (partial)
- `\ulth`, `\uldblwave`, `\ulhwave` have no Qt equivalent → raw RTF preservation
- `\ulcN` (underline color) — Qt sets one underline color per font, cannot override per-run → raw RTF preservation
- `\out`, `\embo`, `\impr`, `\sr` (outline, emboss, imprint, shadow) — no Qt equivalent → raw RTF preservation
- `\v` (hidden text) — cannot hide text while preserving layout in Qt → raw RTF preservation
- `\txN` positions supported, `\tqx`/`\tqd`/`\tqdl`/`\tqdb`/`\tqb` (tab alignment/leaders) — no Qt equivalent → (partial)
- `\langN`, `\chlangN` map to `QTextCharFormat::setFontLanguageId()` — use Qt's `QLocale` constants
- Code pages (`\ansicpgN`) — use `QTextCodec::codecForCp()` with fallback to UTF-8 for unmappable pages
- `\fprqN` (pitch/quality) — hint only, Qt ignores; safe to record but not apply
- `\fscript`, `\fdecor` — font family hints, mapped to `QFont::FontFamily`
- `HYPERLINK` field — `QTextCharFormat::setAnchorHref()` extracts URL from field instruction
- `\line` (soft line break) — `QTextBlockFormat::setLineHeight()` with break; `\keep`, `\keepn`, `\widowctrl` — no Qt equivalent → raw RTF preservation
- Mathematics features — no Qt equivalent → raw RTF preservation
- Section & page setup (`\sectd`, `\sect`, `\cols*`, page dimensions, margins) — no Qt equivalent → raw RTF preservation
- Comments (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`) — no Qt equivalent → raw RTF preservation
- Compatibility flags (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`) — no Qt equivalent → raw RTF preservation
- Preserved chunks use the existing `ProtectedRange` system — type="unsupportedFeature", target=rawRTF

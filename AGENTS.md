# QtRtfEditor AGENTS.md

## Build
```bash
cmake -B build && cmake --build build
```
Out-of-source build into `build/`. Demo executable: `build/examples/demo/demo`.

## Structure
- **src/** — Core library (`QtRtfEditor` shared library):
  - `RichTextEdit.{h,cpp}` — `Rte::RichTextEdit`, `QTextEdit` subclass with RTF/HTML I/O and protected text ranges.
  - `ProtectedRange.{h,cpp}` — `ProtectedRange` class and `ProtectedRangeInfo` struct for managing protected text ranges.
  - `RtfExport.{h,cpp}` — Manual RTF generator exporting `QTextDocument` as Delphi/TRichEdit-compatible RTF with color table, font table, alignment, and indentation support. Also exports HTML.
- **include/RichTextEdit/** — Installable public header (`RichTextEdit.h`), re-exports the `Rte` namespace.
- **tests/** — Test suite with three test executables:
  - `test_protected_ranges` — 13 tests for protection policies, overlapping ranges, and key events (`TestProtectedRanges.cpp`).
  - `test_rtf_structural` — 12 atomic unit tests for `CompareRtf()` (shared `RtfCompare.{h,cpp}`).
   - `test_roundtrip` — One data-driven test method iterating over `tests/TestData/*.rtf` (`TestRoundtrip.cpp`).
- **tests/TestData/** — Feature-family RTF test files used by `test_roundtrip`.
- **examples/demo/** — Minimal GUI demo application (`demo` executable) exercising load, save, formatting, and protection features.
- **cmake/** — CMake package configuration files for `find_package()` support.

## Tests
```bash
cmake -B build && cmake --build build
ctest --output-on-failure --test-dir build
```

Each executable targets a different layer:

**test_protected_ranges** — unit tests for the `RichTextEdit` protection API (`TestProtectedRanges.cpp`).

**test_rtf_structural** — atomic tests for `CompareRtf()`. Each test exercises one specific comparison scenario (text, formatting, alignment, colors, fonts, edge cases). Failures document parser or comparison gaps; timeouts are tracked separately.

**test_roundtrip** — data-driven roundtrip test. One method iterates `tests/TestData/*.rtf`, loads each file via `RichTextEdit`, saves it back, and compares the extracted text. Files with unsupported features (parser iteration limit) are skipped. Five outcome counters are tracked: pass, fail, skip, timeout, exception.

## Version
`VERSION`-date embedded in `CMakeLists.txt` (`PROJECT_VERSION` → `QtRtfEditorConfigVersion.cmake`).

## Important Notes
- C++23, Qt6 (Widgets, Test).
- `build/` is gitignored.

### Out of Scope

Do NOT implement these features — they have no Qt equivalent and cannot be reasonably emulated:

#### Specific to Windows

- **OLE objects** (`\object`, `\objdata`, `\objalias`, `\objclass`) — Windows COM embedding
- **Metafiles** (EMF, WMF) — Windows-specific rasterizable formats, no Qt equivalent

#### Document Management

- **Document metadata** (`\info`, `\title`, `\author`, `\subject`, `\keywords`, `\versionN`) — handled by the embedding app
- **Track changes** (`\revtbl`, `\revN`, `\insrsidN`, `\delrsidN`, `\tridxN`) — version tracking, no Qt equivalent
- **Styles** (`\stylesheet`, `\sN`, `\snext`, `\sbasedonN`) — Word-specific style system
- **Fields** (`\field`, `\*\fldinst`, `\*\fldrslt`, `\date`, `\time`) — Word field codes
- **Index / TOC entries** (`\*{\index ...}`, `\*{\toc ...}`, `\*{\tc ...}`) — document generation, not editing
- **Document variables** (`\*\docvar`) — low-priority document metadata
- **User properties** (`\userprops`, `\propname`, `\staticval`) — custom document properties
- **Paragraph Group Properties** (`\*\pgptbl`, `\pgp`, `\ipgpN`) — Word 2002+ storage optimization
- **Style restrictions** (`\*\latentstyles`, `\lsd*`) — Word UI style visibility and locking
- **Password protection** (`\passwordhash`, `\password`) — document-level security

#### Support of Special Languages

- **Asian and East Asian fonts** (`\fscript`, `\fdecor`, `\stshfdbchN`, `\stshfhichN`, `\stshfbiN`, `\fcharset134/136/129`) — East Asian font binding and complex script support requires an external shaping library
- **Bidirectional text** (`\rtlch`, `\ltrch`, `\rtl`, `\ltrpar`, `\fbidis`) — BiDi layout requires a platform shaping library beyond Qt's basic RTL support
- **Complex script shaping** (Indic, Thai, Arabic) — requires HarfBuzz/UCDraw, no Qt equivalent

#### Page Layout & Document Structure

- **Section & page setup** (`\sectd`, `\sect`, `\sbk*`, `\pgwsxn`, `\pghsxn`, `\marg*`, `\cols*`, `\deftabN`, `\vertdoc`, `\horzdoc`) — QTextDocument is a flow model, not a page layout model
- **Comments / annotations** (`\*\annot`, `\*\ftnacc`, `\*\annotrslt`) — no Qt equivalent
- **Compatibility & document properties** (`\hyph*`, `\lnongrid`, `\donotembedsysfontN`, `\donotembeddingdataN`, `\relyonvmlN`, `\validatexmlN`, `\showxmlerrorsN`, `\trackmovesN`, `\trackformattingN`, `\muser`) — no Qt equivalent

All other RTF 1.9.1 features should be either implemented per `RTF_COVERAGE_PLAN.md` or preserved
as raw RTF via the protected-range preservation mechanism (Phase 3+).

## Code Style

**IMPORTANT:** Changes to coding rules are made only by the user, never by the agent.
The agent applies rules but never adds to the list itself.

### Redundancy
- Avoid code duplication.
- No separator lines or code repetitions in comments.
- Minimize dependencies and code in headers.

### Language Constructs
- Use `const` wherever possible; `const_cast` is allowed only to add const, never to remove it
- Avoid `auto`; exceptions: structured binding (e.g. `auto [a, b] = ...`) and iterator declarations (e.g. `auto it = container.begin()`)
- Raw pointers allowed only as non-owning views; ownership always in `unique_ptr`; avoid `shared_ptr`

### Formatting
- 4-space indentation, no tabs
- Each statement on its own line
- `const` always before the identifier
- `*` and `&` attached to the type, not the variable (e.g. `QWidget* _pWidget`, `const std::string& text`)

### Naming Conventions
- Identifiers in English where possible
- Classes/Structs: `PascalCase`
- Functions/Methods: `PascalCase`
- Member variables: `_camelCase` (leading underscore)
- Prefixes (after underscore in members)
  - Not used to show which type is used
  - pointer: 'p', e.g. 'pText'
  - unique_ptr: 'up'
  - non-const reference: 'r'

### Documentation
- Use `//` comments for inline notes and implementation context.
- Use `/** @brief */` Doxygen blocks on public API declarations where the signature alone doesn't convey the meaning — this enables IDE hover tooltips.
- Doxygen blocks include `@param`/`@return`/`@throws` when parameter meanings, return conventions, or exception behavior aren't obvious (e.g., exclusive end positions, out-parameters, handler return semantics, protected-range semantics).
- Doxygen blocks are omitted where the method name is self-evident (e.g. `clearProtection()`, `protectionPolicy()`, `keyPressEvent()`).
- Top-level file-path comment lines (e.g. `// src/foo.h`) are omitted.
- Information-less section headers (e.g. `// === I/O ===`) are omitted.

## Working Practices
- Commit: Never commit without the user's explicit approval. Present a
  concise summary of all changes and wait for the user to say "commit".
  Keep commit messages short. Mention AI assistance (e.g. "AI-assisted").
- Never change the user's design decisions without consultation.
- Report when design decisions hinder your work.

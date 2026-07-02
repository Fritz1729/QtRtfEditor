# QtRtfEditor AGENTS.md

## Build
```bash
cmake -B build && cmake --build build
```
Out-of-source build into `build/`. Demo executable: `build/examples/demo/demo`.

## Structure
- **src/** — Core library (`QtRtfEditor` shared library):
  - `rich_text_edit.{h,cpp}` — `Rte::RichTextEdit`, `QTextEdit` subclass with RTF/HTML I/O and protected text ranges.
  - `protected_range.{h,cpp}` — `ProtectedRange` class and `ProtectedRangeInfo` struct for managing protected text ranges.
  - `rtf_import.{h,cpp}` — `Rte::isDelphiCompatible()` — checks whether RTF data contains known Delphi/TRichEdit formatting tags.
  - `rtf_export.{h,cpp}` — Manual RTF generator exporting `QTextDocument` as Delphi/TRichEdit-compatible RTF with color table, font table, alignment, and indentation support. Also exports HTML.
- **include/RichTextEdit/** — Installable public header (`RichTextEdit.h`), re-exports the `Rte` namespace.
- **tests/** — Test suite with three test executables:
  - `test_protected_ranges` — 11 tests for protection policies, overlapping ranges, and key events.
  - `test_rtf_roundtrip` — 3 tests for RTF save/load preservation.
  - `test_format_compatibility` — 8 tests for Delphi-compatibility detection and RTF header handling.
- **examples/demo/** — Minimal GUI demo application (`demo` executable) exercising load, save, formatting, and protection features.
- **cmake/** — CMake package configuration files for `find_package()` support.

## Tests
```bash
cmake -B build && cmake --build build
ctest --output-on-failure --test-dir build
```

## Version
`VERSION`-date embedded in `CMakeLists.txt` (`PROJECT_VERSION` → `QtRtfEditorConfigVersion.cmake`).

## Important Notes
- C++23, Qt6 (Widgets, Test).
- `build/` is gitignored.

## Code Style

**IMPORTANT:** Changes to coding rules are made only by the user, never by the agent.
The agent applies rules but never adds to the list itself.

### Redundancy
- Avoid code duplication.
- No separator lines or code repetitions in comments.

### Language Constructs
- Use `const` wherever possible; never use `const_cast`
- Avoid `auto`; exception: structured binding (e.g. `auto [a, b] = ...`)
- Raw pointers allowed only as non-owning views; ownership always in `unique_ptr`; avoid `shared_ptr`

### Formatting
- 4-space indentation, no tabs
- Each statement on its own line
- `const` always before the identifier
- `*` and `&` immediately after the identifier they qualify

### Naming Conventions
- Identifiers in English where possible
- Classes/Structs: `PascalCase`
- Functions/Methods: `PascalCase`
- Member variables: `_camelCase` (leading underscore)
- Pointer/unique_ptr: prefix `p` / `up` in name (e.g. `_pRichTextEdit`, `_upDocument`)

## Working Practices
- Commit: Never commit without the user's review and approval. Keep messages short. Mention AI assistance (e.g. "AI-assisted").
- Never change the user's design decisions without consultation.
- Report when design decisions hinder your work.

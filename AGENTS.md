# QtRtfEditor AGENTS.md

## Build & Test
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --output-on-failure --test-dir build -C Release
```
Out-of-source into `build/`. Demo: `build/examples/demo/demo`.

**Windows/MSVC:** multi-config generator — always pass `--config Release` to `cmake --build` and `-C Release` to `ctest`, or tests won't find binaries.

## Structure
- **`src/`** — `QtRtfEditor` static library:
  - `RichTextEdit.{h,cpp}` — `Rte::RichTextEdit`, `QTextEdit` subclass, RTF/HTML I/O, `\protect` cursor-skip.
  - `RtfExport.{h,cpp}` — Manual RTF generator (Delphi/TRichEdit-compatible).
  - `RtfImport.{h,cpp}` — RTF parser.
  - `RtfParser.{h,cpp}` — Token-level RTF tokenizer.
  - `RtfControl.{h,cpp}` — Control word handling.
  - `RtfCharset.{h,cpp}` — Character set mapping (symbol, CP1252, hex byte to codepoint).
  - `RtfTypes.h` — Shared types and enums.
- **`tests/`** — Three executables:
  - `test_protected_ranges` — Protection API.
  - `test_rtf_structural` — Atomic tests for `CompareRtf()` (shared `RtfCompare.{h,cpp}`).
  - `test_roundtrip` — Data-driven test over `tests/TestData/*.rtf`.
- **`src/cmake/`** — CMake package config template for `find_package()` support.
- **`examples/demo/`** — Minimal GUI demo.

## Testing quirks
- **`test_rtf_structural`:** `ParseRtf()` can hang on certain control words (`\colortbl`, etc.). Tests use a 3-second timeout via detached threads.
- **`test_roundtrip`:** Test data in `tests/TestData/` is copied to `build/testdata/` via `POST_BUILD` custom command.

## Out of Scope
Do NOT implement these — they have no Qt equivalent and no reason to emulate:

| Category | RTF tags | Reason |
|---|---|---|
| Windows | `\object`, `\objdata`, `\objalias`, `\objclass`, EMF, WMF | Windows COM / metafiles |
| Document mgmt | `\info`, `\revtbl`, `\stylesheet`, `\field`, `\*{\toc}`, `\password` | App-level / Word-specific |
| Language | `\fscript`, `\rtl`, complex script shaping | Requires external shaping library |
| Page layout | `\sectd`, `\sbk*`, `\marg*`, `\cols*`, comments | QTextDocument is a flow model |

## Code Style
- **Coding rules are owned by the user.** The agent applies them but never adds to the list.
- **C++23**, Qt6 (Widgets, Test).

### Redundancy
- Avoid code duplication.
- No separator lines in comments.
- No comments that restate what the code already says (names, actions).
- Minimize dependencies and code in headers.

### Language Constructs
- Use `const` wherever possible; `const_cast` only to add const
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
- Class member variables: `_camelCase` (leading underscore) — applies only to class members, not to plain structs or POD data containers
- Prefixes (after underscore in members): avoid type-redundant names like `_iValue`
  - pointer: 'p', e.g. 'pText'
  - unique_ptr: 'up'
  - non-const reference: 'r'

### Documentation
- `//` for inline notes. `/** @brief */` Doxygen on public APIs where the signature alone is insufficient.
- Include `@param`/`@return`/`@throws` unless obvious (exclusive end positions, out-parameters, handler return semantics).
- Omit Doxygen where self-evident (`clearProtection()`, `protectionPolicy()`, `keyPressEvent()`).
- Omit file-path comment lines and information-less section headers.

## Commit messages
- One-liner unless the change clearly needs explanation.
- **AI-assisted commits** — add a footer: `Co-developed-with: opencode (${MODEL})`

## Working Practices
- **Never commit or push without explicit user approval.**
- Before committing, show `git diff --stat`, summarize changes, present the commit message, and ask for review. Never push without approval.
- Never change the user's design decisions without consultation.
- Report when design decisions hinder your work.

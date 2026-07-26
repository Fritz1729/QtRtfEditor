# QtRtfEditor AGENTS.md

## Build & Test
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
ctest --output-on-failure --test-dir build -C Release
```
Out-of-source into `build/`. Demo: `build/demo/demo`.

**Windows/MSVC:** multi-config generator — always pass `--config Release` to `cmake --build` and `-C Release` to `ctest`, or tests won't find binaries.

## Structure
- **`src/`** — `QtRtfEditor` static library. See [documentation](documentation/).
- **`tests/`** — Three executables: `test_protected_ranges`, `test_rtf_structural`, `test_roundtrip`. See [Tests](documentation/tests.md).
- **`demo/`** — Minimal GUI demo.
- **`documentation/`** — Architecture docs. Not packaged.

## References
- **Supported / unsupported features:** [Supported Features](documentation/supported_features.md) — do NOT implement "Out of Scope" items.
- **Testing timeouts:** `CompareRtf()` and roundtrip file processing use 1-second per-operation timeouts. CTest enforces a 60-second global timeout per test executable.

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
- **Do not commit or push without explicit user approval.**
- Before committing, show `git diff --stat`, summarize changes, present the commit message, and ask for review. Do not push without approval.
- Do not change the user's design decisions without consultation.
- Report when design decisions hinder your work.
- **Test-driven development:** before implementing a fix or feature, write the test first. Add [semantic comparison tests](documentation/tests.md#test-rtf-structural) to verify structural equivalence, and [roundtrip tests](documentation/tests.md#test-roundtrip) to verify that load-save cycles preserve document semantics.

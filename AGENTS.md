# QtRtfEditor AGENTS.md

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
ctest --output-on-failure --test-dir build -C Release
```

Out-of-source into `build/`. Convenience wrapper: `./build.sh`. `-Wall` always enabled.

**Windows/MSVC:** pass `--config Release` to `cmake --build` and `-C Release` to `ctest`.

**Headless testing:** `QT_QPA_PLATFORM=offscreen`.

## Code Style

- **Coding rules are owned by the user.** The agent applies them but never adds to the list.
- **C++23**, Qt6 (Widgets, Test).

### Redundancy

- Avoid code duplication.
- No separator lines in comments.
- No comments that restate what the code already says.
- Minimize dependencies and code in headers.

### Language Constructs

- Use `const` wherever possible; `const_cast` only to add const
- Avoid `auto`; exceptions: structured binding and iterator declarations
- Raw pointers only as non-owning views; ownership always in `unique_ptr`; avoid `shared_ptr`
- `using namespace std` is acceptable in `.cpp` files

### Formatting

- 4-space indentation, no tabs
- Each statement on its own line
- `const` before the identifier; `*` and `&` attached to the type

### Naming Conventions

- Classes/Structs, Functions/Methods: `PascalCase`
- Class member variables: `_camelCase` (leading underscore) — only class members, not plain structs/POD
- Member prefixes: `p` (pointer), `up` (unique_ptr), `r` (non-const ref); avoid type-redundant names

### Documentation

- `//` for inline notes. `/** @brief */` Doxygen on public APIs where the signature is insufficient.
- Include `@param`/`@return`/`@throws` unless obvious.
- Omit Doxygen where self-evident. Omit file-path comment lines.

## Commit messages

- One-liner unless the change clearly needs explanation.
- **AI-assisted commits** — add a footer: `Co-developed-with: opencode (${MODEL})`

## Working Practices

- **Do not commit or push without explicit user approval.**
- Before committing, show `git diff --stat`, summarize changes, present the commit message, and ask for review.
- Do not change the user's design decisions without consultation.
- **Test-driven development:** write the test first. See [Tests](documentation/tests.md).

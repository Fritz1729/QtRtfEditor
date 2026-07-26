# Tests

**File:** [Source](../tests/) | [Previous](rtf_export.md) | [Next](README.md)

The test suite consists of three executables built with Qt6::Test and run via CTest.

## Test Strategy

The test strategy is built on two complementary layers:

**Semantic comparison tests** (`test_rtf_structural`) verify that `RtfCompare` can detect semantic differences and treats equivalent formulations as identical. This establishes trust in the comparator.

**Roundtrip tests** (`test_roundtrip`) leverage that trusted comparator to verify that features of test RTF files are preserved when loading and saving. Each file is parsed, imported, exported back to RTF, re-parsed, and compared structurally against the original.

Together they ensure: if the roundtrip test passes, the document's semantics are preserved -- not because the strings match, but because the parsed structures are semantically equivalent.

## Role

The tests verify two properties:
1. **Structural equivalence**: Two RTF strings parse to semantically equivalent `RtfDocument` structures
2. **Roundtrip fidelity**: Load -> Save produces RTF that is structurally identical to the original

## Test Executables

### test_protected_ranges

Verifies the `\protect` cursor-skip mechanism.

| Source | Purpose |
|--------|---------|
| `TestProtectedRanges.cpp` | Protected range API tests |

Linked against: `QtRtfEditor::QtRtfEditor`, `Qt6::Test`.

### test_rtf_structural

Atomic unit tests for `CompareRtf()`. Each test constructs two `RtfDocument` structures (or RTF strings) and verifies the comparison result.

| Source | Purpose |
|--------|---------|
| `TestSemanticComparison.cpp` | Test slots for `CompareRtf()` |
| `RtfCompare.cpp` | Structural comparison implementation |
| `RtfCompare.h` | `CompareRtf()`, `CompareImage()` declarations |

Linked against: `QtRtfEditor::QtRtfEditor`, `Qt6::Test`.

**Note:** `CompareRtf()` uses a 1-second timeout via detached threads to catch parser hangs. CTest enforces a 60-second global timeout per test executable.

### test_roundtrip

Data-driven test that iterates over `tests/TestData/*.rtf` files. For each file:

1. Parse the RTF string
2. Import into `QTextDocument`
3. Export back to RTF
4. Parse the exported RTF
5. Compare the two parsed documents with `CompareRtf()`
6. Report differences (structural mismatch or unknown tags)

| Source | Purpose |
|--------|---------|
| `TestRoundtrip.cpp` | Data-driven roundtrip test |
| `RtfCompare.cpp` | Structural comparison implementation |
| `RtfCompare.h` | `CompareRtf()`, `CompareImage()` declarations |

Test data in `tests/TestData/` is copied to the test binary's directory under `testdata/` via a POST_BUILD custom command.

## RtfCompare

The comparison infrastructure shared between `test_rtf_structural` and `test_roundtrip`:

### API

| Function | Purpose |
|----------|---------|
| `CompareRtf(docA, docB, reason)` | Compare two `RtfDocument` structures |
| `CompareRtf(rtfA, rtfB, reason)` | Compare two raw RTF strings (parses then compares) |
| `CompareImage(idx, imgA, imgB, reason)` | Compare two images (dimensions, hex data) |

### RtfCompareResult

```cpp
enum class RtfCompareResult {
    Identical,      // Documents are structurally equivalent
    UnknownTag,     // One document has unknown tags the other doesn't
    StructuralDiff, // Semantic difference in formatting, content, or structure
};
```

### Comparison Behavior

The comparator compares semantic values (resolved color/font indices), not raw table entries:

- Color entries are compared by RGB values (index-independent)
- Font entries are compared by family name and charset
- Paragraph formatting is compared field by field
- Runs are compared text and format
- Tables are compared row by row, cell by cell
- Image comparison skips binary roundtrip (export re-encodes with different compression)

## Build & Run

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all tests
ctest --output-on-failure --test-dir build -C Release

# Run specific test
ctest --output-on-failure -R test_roundtrip --test-dir build -C Release
```

## Key Files

| File | Purpose |
|------|---------|
| `tests/CMakeLists.txt` | Test executable definitions, test data copy |
| `tests/RtfCompare.h` | Comparison API declarations |
| `tests/RtfCompare.cpp` | Comparison implementation |
| `tests/TestProtectedRanges.cpp` | Protection API tests |
| `tests/TestSemanticComparison.cpp` | Atomic comparison tests |
| `tests/TestRoundtrip.cpp` | Data-driven roundtrip tests |
| `tests/TestData/` | RTF test documents |

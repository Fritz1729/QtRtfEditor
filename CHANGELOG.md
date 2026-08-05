# Changelog

## v0.1.4.1

Windows shared library build fixes (RTE_EXPORT, MSVC CRT /MD vs /MT, output directories), test hang fixes (Qt on main thread, parser decoupled from widgets), `-Wall` clean, CI hardening.

## v0.1.4

Optional shared library builds via BUILD_SHARED_LIBS (static remains the CMake default). PKGBUILD defaults to shared.

## v0.1.3.1

PKGBUILD pkgver fix, license text files added, `examples/demo` renamed to `demo`.

## v0.1.3

Hyperlinks, `\highlight`, `\pntext`, parser hardening (`\ucN`, star groups), hex escape fix, O(1) control lookup, table deduplication, per-module documentation.

## v0.1.2

Refactor (RtfTypes.h split, PascalCase), symbol font fix, protected range improvements, empty paragraph preservation, code simplification.

## v0.1.1

Protected ranges, images, tables, lists, tab stops, character formatting (`\expnd`, `\kerning`), charset-aware hex escapes, semantic comparison, roundtrip tests.

## v0.1.0

Initial release: QTextEdit subclass with partial RTF support, character/paragraph formatting, roundtrip tests, CI.

# QtRtfEditor Documentation

**File:** [Previous](tests.md) | [Next](supported_features.md)

This documentation covers the architecture, API, and internals of QtRtfEditor.

## Overview

QtRtfEditor is a `QTextEdit` subclass with partial RTF support. It provides bidirectional RTF I/O with Delphi/TRichEdit compatibility, HTML support, and protected range functionality.

## Document Navigation

| Document | Topic |
|----------|-------|
| [Supported Features](supported_features.md) | Feature coverage, supported and unsupported RTF tags |
| [Public API](rich_text_edit.md) | `RichTextEdit` — the main class users interact with |
| [Types](rtf_types.md) | Shared types, enums, and data structures |
| [Control Words](rtf_control.md) | RTF control word definitions and dispatch |
| [Charset](rtf_charset.md) | Character set mapping (symbol, CP1252, hex) |
| [Parser](rtf_parser.md) | `RtfParser` — recursive descent RTF tokenizer |
| [Import](rtf_import.md) | `RtfImport` — converts parsed RTF to QTextDocument |
| [Export](rtf_export.md) | `RtfExport` — generates RTF from QTextDocument |
| [Tests](tests.md) | Test suite architecture and data-driven tests |
| [Usage Guide](usage.md) | CMake integration, example code, and signals |


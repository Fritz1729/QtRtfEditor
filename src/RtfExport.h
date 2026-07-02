#pragma once

// Serializes a QTextDocument as RTF string.
//
// The export produces RTF using Delphi/TRichEdit's tag conventions:
// formatting tags act as persistent mode toggles (e.g., \b turns
// bold on, \b0 turns it off). This ensures round-trip compatibility
// when data originates from Delphi applications.
// For future HTML support, the interface is extensible.

#include <string>

class QTextDocument;

namespace Rte {

/**
 * @brief Serialize QTextDocument as RTF string.
 * @param document  The QTextDocument to serialize.
 * @return          RTF string (UTF-8).
 *
 * The export uses manual RTF generation that produces tags understood
 * by Delphi/TRichEdit (mode-toggle style for bold, italic, etc.).
 * This ensures round-trip compatibility when data is shared with
 * Delphi applications.
 */
std::string exportRtf(const QTextDocument &document);

/**
 * @brief Serialize QTextDocument as HTML string.
 * @param document  The QTextDocument to serialize.
 * @return          HTML string (UTF-8, q:-namespaced).
 */
std::string exportHtml(const QTextDocument &document);

} // namespace Rte

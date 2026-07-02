#pragma once

// Serializes a QTextDocument as RTF string.
//
// The export is intentionally limited to Delphi/TRichEdit-compatible
// RTF tags. For future HTML support, the interface is extensible.

#include <string>

class QTextDocument;

namespace Rte {

/**
 * @brief Serialize QTextDocument as RTF string.
 * @param document  The QTextDocument to serialize.
 * @return          RTF string (UTF-8).
 *
 * The export uses manual RTF generation that only produces
 * tags understood by Delphi/TRichEdit.
 */
std::string exportRtf(const QTextDocument &document);

/**
 * @brief Serialize QTextDocument as HTML string.
 * @param document  The QTextDocument to serialize.
 * @return          HTML string (UTF-8, q:-namespaced).
 */
std::string exportHtml(const QTextDocument &document);

} // namespace Rte

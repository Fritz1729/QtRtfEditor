#pragma once

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
std::string ExportRtf(const QTextDocument& document);

/**
 * @brief Serialize QTextDocument as HTML string.
 * @param document  The QTextDocument to serialize.
 * @return          HTML string (UTF-8, q:-namespaced).
 */
std::string ExportHtml(const QTextDocument& document);

} // namespace Rte

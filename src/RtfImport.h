#pragma once

#include <string>

#include "RichTextEdit.h"

class QTextDocument;

namespace Rte {

/**
 * @brief Parse an RTF string and populate a QTextDocument.
 * @param document  The QTextDocument to populate.
 * @param rtf       RTF string (UTF-8).
 * @param codePage  Default code page for ANSI hex escapes (default 1252).
 */
RTE_EXPORT void ImportRtf(QTextDocument* document, const std::string& rtf, int codePage = 1252);

} // namespace Rte

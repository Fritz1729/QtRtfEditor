#pragma once

#include <string>

class QTextDocument;

namespace Rte {

/**
 * @brief Parse an RTF string and populate a QTextDocument.
 * @param document  The QTextDocument to populate.
 * @param rtf       RTF string (UTF-8).
 */
void ImportRtf(QTextDocument* document, const std::string& rtf);

} // namespace Rte

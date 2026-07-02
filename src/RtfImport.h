#pragma once

// Checks whether an RTF string contains Delphi/TRichEdit-compatible
// formatting elements.

#include <string>

namespace Rte {

/**
 * @brief Checks whether an RTF string contains Delphi/TRichEdit-compatible
 *        formatting elements.
 * @param rtf  RTF string.
 * @return     true if known tags (bold, italic, color,
 *             underline, superscript/subscript, alignment, indent)
 *             are present.
 */
bool isDelphiCompatible(const std::string &rtf);

} // namespace Rte

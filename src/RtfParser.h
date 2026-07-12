#pragma once

#include <string>

#include "RtfCompare.h"
#include "RtfControl.h"

namespace Rte {

/**
 * @brief RTF parser class.
 *
 * Parses raw RTF text into a structural representation (RtfDocument).
 * All parsing methods are private — use the free function ParseRtf().
 */
class RtfParser {
public:
    /**
     * @brief Parse an RTF string into a structural representation.
     *
     * @param rtf      RTF string (UTF-8).
     * @param codePage Default code page for ANSI hex escapes (default 1252).
     * @return         Parsed document.
     */
    RtfDocument parse(const std::string& rtf, int codePage = 1252);
    int codePage() const { return _codePage; }

private:
    int _codePage = 1252;
};

/**
 * @brief Parse an RTF string into a structural representation.
 * @param rtf      RTF string (UTF-8).
 * @param codePage Default code page for ANSI hex escapes (default 1252).
 */
RtfDocument ParseRtf(const std::string& rtf, int codePage = 1252);

} // namespace Rte

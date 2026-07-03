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
     * @param rtf  RTF string (UTF-8).
     * @return     Parsed document.
     */
    RtfDocument parse(const std::string& rtf);
};

/**
 * @brief Parse an RTF string into a structural representation.
 */
RtfDocument ParseRtf(const std::string& rtf);

} // namespace Rte

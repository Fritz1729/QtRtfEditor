#pragma once

#include <string>

#include "RtfCompare.h"

namespace Rte {

/**
 * @brief Parse an RTF string into a structural representation.
 * @param rtf      RTF string (UTF-8).
 * @param codePage Default code page for ANSI hex escapes (default 1252).
 */
RtfDocument ParseRtf(const std::string& rtf, int codePage = 1252);

} // namespace Rte

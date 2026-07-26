#pragma once

#include <string>
#include <vector>

#include "RtfTypes.h"

namespace Rte {

enum class RtfCompareResult {
    Identical,
    UnknownTag,
    StructuralDiff
};

/**
 * @brief Compare two raw RTF strings for structural equivalence.
 * Parses and compares with 1-second timeout. Reports the first difference found.
 * @param rtfA   First RTF string.
 * @param rtfB   Second RTF string.
 * @param reason Output parameter for the difference description.
 * @return       Identical, UnknownTag, or StructuralDiff.
 */
RtfCompareResult CompareRtf(const std::string& rtfA, const std::string& rtfB,
                                std::string& reason);

/**
 * @brief Compare two images for structural equivalence.
 * @param idx    Image index in the document (for error reporting).
 * @param a      First image.
 * @param b      Second image.
 * @param reason Output parameter for the difference description.
 * @return       true if the images are structurally equivalent.
 */
bool CompareImage(size_t idx, const RtfImage& a, const RtfImage& b,
                    std::string& reason);

} // namespace Rte

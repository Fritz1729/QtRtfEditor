#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "RtfTypes.h"

namespace Rte {

struct RtfColorEntry {
    int red = 0, green = 0, blue = 0;
    bool operator==(const RtfColorEntry &) const = default;
};

struct RtfFontEntry {
    std::string family;
    bool operator==(const RtfFontEntry &) const = default;
};

struct RtfDocument {
    int defaultFontIndex = 0;
    int defaultFontSize = 0;      // half-points (\fs in header)
    std::vector<RtfColorEntry> colors;
    std::vector<RtfFontEntry> fonts;
    std::vector<RtfParagraph> paragraphs;
    std::vector<std::string> unknownTags;
};

enum class RtfCompareResult {
    Identical,
    UnknownTag,
    StructuralDiff
};

/**
 * @brief Compare two RTF documents for structural equivalence.
 *
 * Compares semantic values (resolved color/ font indices), not raw
 * table entries. Reports the first difference found.
 *
 * @param a      First document.
 * @param b      Second document.
 * @param reason Output parameter for human-readable description
 *               of the difference (empty if Identical).
 * @return       Identical, UnknownTag, or StructuralDiff.
 */
RtfCompareResult CompareRtf(const RtfDocument& a, const RtfDocument& b,
                              std::string& reason);

/**
 * @brief Compare two raw RTF strings for structural equivalence.
 */
RtfCompareResult CompareRtf(const std::string& rtfA, const std::string& rtfB,
                              std::string& reason);

} // namespace Rte

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QByteArray>

#include "RtfTypes.h"

namespace Rte {

enum class RtfImageFormat : uint8_t {
    Jpeg,
    Png,
    Bmp,
    Unknown,
};

struct RtfImage {
    QByteArray data;
    RtfImageFormat format = RtfImageFormat::Unknown;
    int picw = 0;
    int pich = 0;
    int picwgoal = 0;
    int pichgoal = 0;
    int picscalex = 100;
    int picscaley = 100;
    int piccropl = 0;
    int piccropr = 0;
    int piccropt = 0;
    int piccropb = 0;

    // Original hex-encoded binary data from the RTF {\pict ...} group
    std::string rtfPictHex;

    bool operator==(const RtfImage &) const = default;
};

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
    std::vector<RtfImage> images;
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

/**
 * @brief Compare two images for structural equivalence.
 */
bool CompareImage(size_t idx, const RtfImage& a, const RtfImage& b,
                   std::string& reason);

} // namespace Rte

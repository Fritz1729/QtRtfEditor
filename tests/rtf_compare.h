#pragma once

#include <string>
#include <vector>

namespace Rte {

struct RtfColorEntry {
    int red = 0, green = 0, blue = 0;
    bool operator==(const RtfColorEntry &) const;
};

struct RtfFontEntry {
    std::string family;
    bool operator==(const RtfFontEntry &) const;
};

struct RtfRunFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int fontIndex = 0;
    int fontSize = 0;
    int colorIndex = -1;
    bool superscript = false;
    bool subscript = false;

    bool operator==(const RtfRunFormat &) const;
};

struct RtfRun {
    std::string text;
    RtfRunFormat format;

    RtfRun() = default;
    RtfRun(std::string t, RtfRunFormat f) : text(std::move(t)), format(std::move(f)) {}

    bool operator==(const RtfRun &) const;
};

struct RtfParagraph {
    int alignment = 1;            // Qt::AlignLeft = 1
    int leftIndent = 0;           // half-points
    int firstLineIndent = 0;      // half-points
    int rightIndent = 0;          // half-points
    std::vector<RtfRun> runs;

    RtfParagraph() = default;

    bool operator==(const RtfParagraph &) const;
};

struct RtfDocument {
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
 * @brief Parse an RTF string into a structural representation.
 *
 * Recognized control words:
 *   Paragraph: \ql, \qc, \qr, \li, \fi, \ri
 *   Character: \b, \b0, \i, \i0, \ul, \ul0, \f, \fs, \cf,
 *              \sub, \sub0, \super, \super0
 *   Tables:    {\colortbl ...}, {\fonttbl ...}
 *
 * Unknown tags are collected and reported via unknownTags.
 *
 * @param rtf  RTF string (UTF-8).
 * @return     Parsed document.
 */
RtfDocument parseRtf(const std::string &rtf);

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
RtfCompareResult compareRtf(const RtfDocument &a, const RtfDocument &b,
                            std::string &reason);

/**
 * @brief Compare two raw RTF strings for structural equivalence.
 */
RtfCompareResult compareRtf(const std::string &rtfA, const std::string &rtfB,
                            std::string &reason);

} // namespace Rte

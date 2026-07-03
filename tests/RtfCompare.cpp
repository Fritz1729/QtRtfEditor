#include "RtfCompare.h"
#include "RtfParser.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Rte {

static bool IsColorInBounds(int index, size_t tableSize) {
    return index >= 0 && index < static_cast<int>(tableSize);
}

static bool ResolveAndCompareColors(int paraIdx, int runIdx,
    const RtfRun& runA, const RtfRun& runB,
    const RtfDocument& a, const RtfDocument& b,
    std::string& reason) {
    // Resolve to actual RGB for semantic comparison
    RtfColorEntry resolvedA = {0, 0, 0};
    RtfColorEntry resolvedB = {0, 0, 0};
    bool hasA = IsColorInBounds(runA.format.colorIndex, a.colors.size());
    bool hasB = IsColorInBounds(runB.format.colorIndex, b.colors.size());
    if (hasA) resolvedA = a.colors[runA.format.colorIndex];
    if (hasB) resolvedB = b.colors[runB.format.colorIndex];

    std::string boundMsg;
    if (!hasA || !hasB) {
        boundMsg = " [out-of-bounds: A=" + std::to_string(runA.format.colorIndex) +
            "/" + std::to_string(a.colors.size()) +
            ", B=" + std::to_string(runB.format.colorIndex) +
            "/" + std::to_string(b.colors.size()) + "]";
    }

    if (hasA && hasB && resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        // Same resolved color despite different indices — not a diff
        return false;
    }
    if (resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        // Both resolve to the same color (e.g., both default/auto) — not a diff
        return false;
    }

    std::string rgbA = "(" + std::to_string(resolvedA.red) + "," +
        std::to_string(resolvedA.green) + "," +
        std::to_string(resolvedA.blue) + ")";
    std::string rgbB = "(" + std::to_string(resolvedB.red) + "," +
        std::to_string(resolvedB.green) + "," +
        std::to_string(resolvedB.blue) + ")";
    reason = "Paragraph " + std::to_string(paraIdx) +
        " run " + std::to_string(runIdx) +
        " colorIndex: " + std::to_string(runA.format.colorIndex) +
        " (" + rgbA + ") vs " + std::to_string(runB.format.colorIndex) +
        " (" + rgbB + ")" + boundMsg;
    return true;
}

static bool ResolveAndCompareHighlight(int paraIdx, int runIdx,
    const RtfRun& runA, const RtfRun& runB,
    const RtfDocument& a, const RtfDocument& b,
    std::string& reason) {
    // Highlight uses a fixed palette of 16 colors
    // Map highlight index (1-16) to known RTF highlight colors
    static const std::vector<RtfColorEntry> highlightPalette = {
        {0, 0, 0},        // 1 - black
        {255, 255, 255},  // 2 - white
        {255, 0, 0},      // 3 - red
        {0, 255, 0},      // 4 - green
        {255, 255, 0},    // 5 - yellow
        {0, 0, 255},      // 6 - blue
        {255, 0, 255},    // 7 - magenta
        {0, 255, 255},    // 8 - cyan
        {128, 0, 0},      // 9 - dark red
        {0, 128, 0},      // 10 - dark green
        {128, 128, 0},    // 11 - dark yellow
        {0, 0, 128},      // 12 - dark blue
        {128, 0, 128},    // 13 - dark magenta
        {0, 128, 128},    // 14 - dark cyan
        {192, 192, 192},  // 15 - light gray
        {128, 128, 128},  // 16 - dark gray
    };

    RtfColorEntry resolvedA = {0, 0, 0};
    RtfColorEntry resolvedB = {0, 0, 0};
    bool hasA = runA.format.highlight >= 1 &&
                runA.format.highlight <= 16 &&
                runA.format.highlight >= 0;
    bool hasB = runB.format.highlight >= 1 &&
                runB.format.highlight <= 16 &&
                runB.format.highlight >= 0;

    if (hasA) resolvedA = highlightPalette[runA.format.highlight - 1];
    if (hasB) resolvedB = highlightPalette[runB.format.highlight - 1];

    if (hasA && hasB && resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        return false;
    }
    if (resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        return false;
    }

    reason = "Paragraph " + std::to_string(paraIdx) +
        " run " + std::to_string(runIdx) +
        " highlight: " + std::to_string(runA.format.highlight) +
        " vs " + std::to_string(runB.format.highlight);
    return true;
}

RtfCompareResult CompareRtf(const RtfDocument& a, const RtfDocument& b,
                              std::string& reason) {
    // Check for unknown tags
    if (!a.unknownTags.empty()) {
        reason = "Unknown tag in input: " + a.unknownTags.back();
        return RtfCompareResult::UnknownTag;
    }
    if (!b.unknownTags.empty()) {
        reason = "Unknown tag in output: " + b.unknownTags.back();
        return RtfCompareResult::UnknownTag;
    }

    // Compare paragraph count
    if (a.paragraphs.size() != b.paragraphs.size()) {
        reason = "Paragraph count: " + std::to_string(a.paragraphs.size()) +
                 " vs " + std::to_string(b.paragraphs.size());
        return RtfCompareResult::StructuralDiff;
    }

    for (size_t i = 0; i < a.paragraphs.size(); ++i) {
        const auto& paraA = a.paragraphs[i];
        const auto& paraB = b.paragraphs[i];

        // Resolve alignment names
        std::string alignA, alignB;
        if (paraA.alignment == 1) alignA = "left";
        else if (paraA.alignment == 128) alignA = "center";
        else if (paraA.alignment == 2) alignA = "right";
        else alignA = "unknown(" + std::to_string(paraA.alignment) + ")";

        if (paraB.alignment == 1) alignB = "left";
        else if (paraB.alignment == 128) alignB = "center";
        else if (paraB.alignment == 2) alignB = "right";
        else alignB = "unknown(" + std::to_string(paraB.alignment) + ")";

        // Compare paragraph-level formatting
        if (paraA.alignment != paraB.alignment) {
            reason = "Paragraph " + std::to_string(i) +
                     " alignment: " + alignA + " vs " + alignB;
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.leftIndent != paraB.leftIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " leftIndent: " + std::to_string(paraA.leftIndent) +
                     " vs " + std::to_string(paraB.leftIndent);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.firstLineIndent != paraB.firstLineIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " firstLineIndent: " + std::to_string(paraA.firstLineIndent) +
                     " vs " + std::to_string(paraB.firstLineIndent);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.rightIndent != paraB.rightIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " rightIndent: " + std::to_string(paraA.rightIndent) +
                     " vs " + std::to_string(paraB.rightIndent);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.spaceBefore != paraB.spaceBefore) {
            reason = "Paragraph " + std::to_string(i) +
                     " spaceBefore: " + std::to_string(paraA.spaceBefore) +
                     " vs " + std::to_string(paraB.spaceBefore);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.spaceAfter != paraB.spaceAfter) {
            reason = "Paragraph " + std::to_string(i) +
                     " spaceAfter: " + std::to_string(paraA.spaceAfter) +
                     " vs " + std::to_string(paraB.spaceAfter);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.lineHeight != paraB.lineHeight) {
            reason = "Paragraph " + std::to_string(i) +
                     " lineHeight: " + std::to_string(paraA.lineHeight) +
                     " vs " + std::to_string(paraB.lineHeight);
            return RtfCompareResult::StructuralDiff;
        }
        // Compare runs
        if (paraA.runs.size() != paraB.runs.size()) {
            reason = "Paragraph " + std::to_string(i) +
                     " run count: " + std::to_string(paraA.runs.size()) +
                     " vs " + std::to_string(paraB.runs.size());
            return RtfCompareResult::StructuralDiff;
        }

        for (size_t j = 0; j < paraA.runs.size(); ++j) {
        const auto& runA = paraA.runs[j];
        const auto& runB = paraB.runs[j];

            // Compare text
            if (runA.text != runB.text) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " text: '" + runA.text + "' vs '" + runB.text + "'";
                return RtfCompareResult::StructuralDiff;
            }

            // Compare formatting
            if (runA.format.bold != runB.format.bold) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " bold: " + std::string(runA.format.bold ? "true" : "false") +
                         " vs " + std::string(runB.format.bold ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.italic != runB.format.italic) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " italic: " + std::string(runA.format.italic ? "true" : "false") +
                         " vs " + std::string(runB.format.italic ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.underline != runB.format.underline) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " underline: " + std::string(runA.format.underline ? "true" : "false") +
                         " vs " + std::string(runB.format.underline ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.fontIndex != runB.format.fontIndex) {
                // Resolve font family name for semantic comparison
                std::string familyA, familyB;
                bool hasFontA = runA.format.fontIndex >= 0 &&
                    runA.format.fontIndex < static_cast<int>(a.fonts.size());
                bool hasFontB = runB.format.fontIndex >= 0 &&
                    runB.format.fontIndex < static_cast<int>(b.fonts.size());
                if (hasFontA) familyA = a.fonts[runA.format.fontIndex].family;
                if (hasFontB) familyB = b.fonts[runB.format.fontIndex].family;
                if (hasFontA && hasFontB && familyA == familyB) {
                    // Same resolved font family — skip
                } else {
                    reason = "Paragraph " + std::to_string(i) +
                             " run " + std::to_string(j) +
                             " fontIndex: " + std::to_string(runA.format.fontIndex) +
                             " (" + familyA + ") vs " + std::to_string(runB.format.fontIndex) +
                             " (" + familyB + ")";
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.fontSize != runB.format.fontSize) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " fontSize: " + std::to_string(runA.format.fontSize) +
                         " vs " + std::to_string(runB.format.fontSize);
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.colorIndex != runB.format.colorIndex) {
                if (ResolveAndCompareColors(i, j, runA, runB, a, b, reason)) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.colorIndex >= 0 &&
                (!IsColorInBounds(runA.format.colorIndex, a.colors.size()) ||
                 !IsColorInBounds(runA.format.colorIndex, b.colors.size()))) {
                // If the color was already resolved and matched above, skip
                if (runA.format.colorIndex == runB.format.colorIndex ||
                    runB.format.colorIndex < 0) {
                    // Same index or B has no color — already handled by semantic comparison
                } else {
                    reason = "Paragraph " + std::to_string(i) +
                             " run " + std::to_string(j) +
                             " colorIndex " + std::to_string(runA.format.colorIndex) +
                             " out of bounds in color table (A: " +
                             std::to_string(a.colors.size()) +
                             ", B: " + std::to_string(b.colors.size()) + ")";
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.superscript != runB.format.superscript) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " superscript: " + std::string(runA.format.superscript ? "true" : "false") +
                         " vs " + std::string(runB.format.superscript ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.subscript != runB.format.subscript) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " subscript: " + std::string(runA.format.subscript ? "true" : "false") +
                         " vs " + std::string(runB.format.subscript ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.strikeOut != runB.format.strikeOut) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " strikeOut: " + std::string(runA.format.strikeOut ? "true" : "false") +
                         " vs " + std::string(runB.format.strikeOut ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.bgColorIndex != runB.format.bgColorIndex) {
                if (ResolveAndCompareColors(i, j, runA, runB, a, b, reason)) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.highlight != runB.format.highlight) {
                if (ResolveAndCompareHighlight(i, j, runA, runB, a, b, reason)) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.underlineStyle != runB.format.underlineStyle) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " underlineStyle: " + std::to_string(static_cast<int>(runA.format.underlineStyle)) +
                         " vs " + std::to_string(static_cast<int>(runB.format.underlineStyle));
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.capitalization != runB.format.capitalization) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " capitalization: " + std::to_string(static_cast<int>(runA.format.capitalization)) +
                         " vs " + std::to_string(static_cast<int>(runB.format.capitalization));
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.upOffset != runB.format.upOffset) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " upOffset: " + std::to_string(runA.format.upOffset) +
                         " vs " + std::to_string(runB.format.upOffset);
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.dnOffset != runB.format.dnOffset) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " dnOffset: " + std::to_string(runA.format.dnOffset) +
                         " vs " + std::to_string(runB.format.dnOffset);
                return RtfCompareResult::StructuralDiff;
            }
        }
    }

    return RtfCompareResult::Identical;
}

RtfCompareResult CompareRtf(const std::string& rtfA, const std::string& rtfB,
                              std::string& reason) {
    RtfDocument docA = ParseRtf(rtfA);
    RtfDocument docB = ParseRtf(rtfB);
    return CompareRtf(docA, docB, reason);
}

} // namespace Rte

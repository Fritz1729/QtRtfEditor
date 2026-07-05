#include "RtfCompare.h"
#include "RtfParser.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Rte {

static RtfColorEntry ResolveColorFromTable(int idx, const RtfDocument& doc, bool* hasValue) {
    *hasValue = idx >= 0 && idx < static_cast<int>(doc.colors.size());
    if (*hasValue) return doc.colors[idx];
    return {0, 0, 0};
}

static bool CompareResolvedColors(int paraIdx, int runIdx, int idxA, int idxB,
    const RtfDocument& docA, const RtfDocument& docB,
    RtfColorEntry(*resolveA)(int, const RtfDocument&, bool*),
    RtfColorEntry(*resolveB)(int, const RtfDocument&, bool*),
    std::string& reason, const char* fieldName) {
    bool hasA, hasB;
    RtfColorEntry resolvedA = resolveA(idxA, docA, &hasA);
    RtfColorEntry resolvedB = resolveB(idxB, docB, &hasB);

    if (hasA && hasB && resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        return false;
    }
    if (resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        return false;
    }

    std::string rgbA = "(" + std::to_string(resolvedA.red) + "," +
        std::to_string(resolvedA.green) + "," +
        std::to_string(resolvedA.blue) + ")";
    std::string rgbB = "(" + std::to_string(resolvedB.red) + "," +
        std::to_string(resolvedB.green) + "," +
        std::to_string(resolvedB.blue) + ")";

    std::string boundMsg;
    if (!hasA || !hasB) {
        boundMsg = " [out-of-bounds: A=" + std::to_string(idxA) + "/" +
            std::to_string(docA.colors.size()) +
            ", B=" + std::to_string(idxB) + "/" +
            std::to_string(docB.colors.size()) + "]";
    }

    reason = "Paragraph " + std::to_string(paraIdx) +
        " run " + std::to_string(runIdx) +
        " " + fieldName + ": " + std::to_string(idxA) +
        " (" + rgbA + ") vs " + std::to_string(idxB) +
        " (" + rgbB + ")" + boundMsg;
    return true;
}

template<typename T>
static bool CompareField(size_t paraIdx, size_t runIdx, const char* name,
                         const T& a, const T& b,
                         std::string& reason) {
    if (a != b) {
        reason = "Paragraph " + std::to_string(paraIdx) +
                 (runIdx == SIZE_MAX ? "" : " run " + std::to_string(runIdx) + " ") +
                 name + ": " + std::to_string(a) + " vs " + std::to_string(b);
        return true;
    }
    return false;
}

static bool CompareBoolField(size_t paraIdx, size_t runIdx, const char* name,
                             bool a, bool b, std::string& reason) {
    if (a != b) {
        reason = "Paragraph " + std::to_string(paraIdx) +
                 " run " + std::to_string(runIdx) + " " +
                 name + ": " + (a ? "true" : "false") + " vs " + (b ? "true" : "false");
        return true;
    }
    return false;
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

    // Compare images
    if (a.images.size() != b.images.size()) {
        reason = "Image count: " + std::to_string(a.images.size()) +
                 " vs " + std::to_string(b.images.size());
        return RtfCompareResult::StructuralDiff;
    }
    for (size_t i = 0; i < a.images.size(); ++i) {
        const auto& imgA = a.images[i];
        const auto& imgB = b.images[i];
        if (CompareImage(i, imgA, imgB, reason)) return RtfCompareResult::StructuralDiff;
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

        if (CompareField(i, SIZE_MAX, "alignment",
                         paraA.alignment, paraB.alignment, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "leftIndent",
                         paraA.leftIndent, paraB.leftIndent, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "firstLineIndent",
                         paraA.firstLineIndent, paraB.firstLineIndent, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "rightIndent",
                         paraA.rightIndent, paraB.rightIndent, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "spaceBefore",
                         paraA.spaceBefore, paraB.spaceBefore, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "spaceAfter",
                         paraA.spaceAfter, paraB.spaceAfter, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "lineHeight",
                         paraA.lineHeight, paraB.lineHeight, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "slMult",
                           paraA.slMult, paraB.slMult, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "listId",
                           paraA.listId, paraB.listId, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "listLevel",
                           paraA.listLevel, paraB.listLevel, reason)) return RtfCompareResult::StructuralDiff;
        if (CompareField(i, SIZE_MAX, "listStyle",
                           static_cast<int>(paraA.listStyle), static_cast<int>(paraB.listStyle), reason)) return RtfCompareResult::StructuralDiff;
        if (paraA.tabStops.size() != paraB.tabStops.size()) {
            reason = "Paragraph " + std::to_string(i) +
                     " tabStops count: " + std::to_string(paraA.tabStops.size()) +
                     " vs " + std::to_string(paraB.tabStops.size());
            return RtfCompareResult::StructuralDiff;
        }
        for (size_t t = 0; t < paraA.tabStops.size(); ++t) {
            if (paraA.tabStops[t] != paraB.tabStops[t]) {
                reason = "Paragraph " + std::to_string(i) +
                         " tabStops[" + std::to_string(t) +
                         "]: position(" + std::to_string(paraA.tabStops[t].position) +
                         "/" + std::to_string(paraB.tabStops[t].position) +
                         ") align(" + std::to_string(paraA.tabStops[t].alignment) +
                         "/" + std::to_string(paraB.tabStops[t].alignment) + ")";
                return RtfCompareResult::StructuralDiff;
            }
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
            if (CompareBoolField(i, j, "bold",
                                 runA.format.bold, runB.format.bold, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareBoolField(i, j, "italic",
                                 runA.format.italic, runB.format.italic, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareBoolField(i, j, "underline",
                                 runA.format.underline, runB.format.underline, reason)) return RtfCompareResult::StructuralDiff;
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
            if (CompareField(i, j, "fontSize",
                             runA.format.fontSize, runB.format.fontSize, reason)) return RtfCompareResult::StructuralDiff;
            if (runA.format.colorIndex != runB.format.colorIndex) {
                if (CompareResolvedColors(i, j, runA.format.colorIndex, runB.format.colorIndex,
                                           a, b,
                                           ResolveColorFromTable, ResolveColorFromTable,
                                           reason, "colorIndex")) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (CompareBoolField(i, j, "superscript",
                                 runA.format.superscript, runB.format.superscript, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareBoolField(i, j, "subscript",
                                 runA.format.subscript, runB.format.subscript, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareBoolField(i, j, "strikeOut",
                                 runA.format.strikeOut, runB.format.strikeOut, reason)) return RtfCompareResult::StructuralDiff;
            if (runA.format.bgColorIndex != runB.format.bgColorIndex) {
                if (CompareResolvedColors(i, j, runA.format.bgColorIndex, runB.format.bgColorIndex,
                                           a, b,
                                           ResolveColorFromTable, ResolveColorFromTable,
                                           reason, "bgColorIndex")) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (CompareField(i, j, "underlineStyle",
                             static_cast<int>(runA.format.underlineStyle), static_cast<int>(runB.format.underlineStyle), reason)) return RtfCompareResult::StructuralDiff;
            if (CompareField(i, j, "capitalization",
                             static_cast<int>(runA.format.capitalization), static_cast<int>(runB.format.capitalization), reason)) return RtfCompareResult::StructuralDiff;
            if (CompareField(i, j, "upOffset",
                              runA.format.upOffset, runB.format.upOffset, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareField(i, j, "dnOffset",
                              runA.format.dnOffset, runB.format.dnOffset, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareBoolField(i, j, "kerning",
                                  runA.format.kerning, runB.format.kerning, reason)) return RtfCompareResult::StructuralDiff;
            if (CompareField(i, j, "expnd",
                               runA.format.expnd, runB.format.expnd, reason)) return RtfCompareResult::StructuralDiff;
            if (runA.format.ulColorIndex != 0 || runB.format.ulColorIndex != 0) {
                if (CompareResolvedColors(i, j, runA.format.ulColorIndex, runB.format.ulColorIndex,
                                            a, b,
                                            ResolveColorFromTable, ResolveColorFromTable,
                                            reason, "ulColorIndex")) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (CompareField(i, j, "langId",
                               runA.format.langId, runB.format.langId, reason)) return RtfCompareResult::StructuralDiff;
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

static const char* ImageFormatName(RtfImageFormat fmt) {
    switch (fmt) {
        case RtfImageFormat::Jpeg:  return "jpeg";
        case RtfImageFormat::Png:   return "png";
        case RtfImageFormat::Bmp:   return "bmp";
        default:                    return "unknown";
    }
}

bool CompareImage(size_t idx, const RtfImage& a, const RtfImage& b,
                   std::string& reason) {
    if (a.format != b.format) {
        reason = "Image " + std::to_string(idx) +
                 " format: " + ImageFormatName(a.format) + " vs " + ImageFormatName(b.format);
        return true;
    }
    // Compare hex-encoded RTF data (semantic comparison — ignores dimension/scale changes)
    // This enables byte-identical roundtrip for the image binary content.
    if (a.rtfPictHex != b.rtfPictHex) {
        reason = "Image " + std::to_string(idx) + " hex data mismatch";
        return true;
    }
    // Dimensions and scale are intentionally NOT compared — they may change during
    // document editing (resize, paste/drop re-encode) but the image content is identical.
    return false;
}

} // namespace Rte

#include "RtfCompare.h"
#include "RtfParser.h"

#include <algorithm>
#include <QFont>
#include <string>
#include <vector>

namespace Rte {

static RtfColorEntry ResolveColorFromTable(int idx, const RtfDocument& doc, bool* hasValue) {
    *hasValue = idx >= 0 && idx < static_cast<int>(doc.colors.size());
    if (*hasValue) return doc.colors[idx];
    return {0, 0, 0};
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

static TableCellBorders NormalizeCellBorders(const TableCellBorders& cellBorders, const TableCellBorders& rowBorders) {
    TableCellBorders normalized = cellBorders;
    auto fillFromRow = [&](int& cellVal, int& cellColor, BorderStyle& cellStyle, int rowVal, int rowColor, BorderStyle rowStyle) {
        if (cellVal <= 0 && cellStyle == BorderStyle::None) {
            cellVal = rowVal;
            cellColor = rowColor;
            cellStyle = rowStyle;
        }
    };
    fillFromRow(normalized.leftWidth, normalized.leftColor, normalized.leftStyle,
                rowBorders.leftWidth, rowBorders.leftColor, rowBorders.leftStyle);
    fillFromRow(normalized.topWidth, normalized.topColor, normalized.topStyle,
                rowBorders.topWidth, rowBorders.topColor, rowBorders.topStyle);
    fillFromRow(normalized.rightWidth, normalized.rightColor, normalized.rightStyle,
                rowBorders.rightWidth, rowBorders.rightColor, rowBorders.rightStyle);
    fillFromRow(normalized.bottomWidth, normalized.bottomColor, normalized.bottomStyle,
                rowBorders.bottomWidth, rowBorders.bottomColor, rowBorders.bottomStyle);
    return normalized;
}

static int EffectiveCellPadding(int cellPad, int rowPad) {
    return (cellPad > 0 || rowPad > 0) ? std::max(cellPad, rowPad) : 0;
}

static std::string BuildRunLocation(const std::string& kind, size_t locIdx,
                                     size_t charPos, size_t runA, size_t runB) {
    return kind + " " + std::to_string(locIdx) +
           " char " + std::to_string(charPos) +
           " (runA " + std::to_string(runA) +
           ", runB " + std::to_string(runB) + ")";
}

static bool CompareFormatSemantic(const RtfRunFormat& fmtA, const RtfRunFormat& fmtB,
    const RtfDocument& docA, const RtfDocument& docB,
    const std::string& loc, int& effFsA, int& effFsB, std::string& reason) {
    const auto reportBool = [&](const char* name, bool a, bool b) {
        if (a != b) {
            reason = loc + " " + name + ": " + (a ? "true" : "false") + " vs " + (b ? "true" : "false");
            return true;
        }
        return false;
    };
    const auto reportInt = [&](const char* name, int a, int b) {
        if (a != b) {
            reason = loc + " " + name + ": " + std::to_string(a) + " vs " + std::to_string(b);
            return true;
        }
        return false;
    };
    const auto reportResolvedColor = [&](const char* name, int idxA, int idxB) {
        bool hasA, hasB;
        RtfColorEntry resolvedA = ResolveColorFromTable(idxA, docA, &hasA);
        RtfColorEntry resolvedB = ResolveColorFromTable(idxB, docB, &hasB);
        if (resolvedA.red == resolvedB.red && resolvedA.green == resolvedB.green &&
            resolvedA.blue == resolvedB.blue) {
            return false;
        }
        std::string rgbA = "(" + std::to_string(resolvedA.red) + "," +
            std::to_string(resolvedA.green) + "," + std::to_string(resolvedA.blue) + ")";
        std::string rgbB = "(" + std::to_string(resolvedB.red) + "," +
            std::to_string(resolvedB.green) + "," + std::to_string(resolvedB.blue) + ")";
        std::string boundMsg;
        if (!hasA || !hasB) {
            boundMsg = " [out-of-bounds: A=" + std::to_string(idxA) + "/" +
                std::to_string(docA.colors.size()) + ", B=" + std::to_string(idxB) + "/" +
                std::to_string(docB.colors.size()) + "]";
        }
        reason = loc + " " + name + ": " + std::to_string(idxA) +
            " (" + rgbA + ") vs " + std::to_string(idxB) + " (" + rgbB + ")" + boundMsg;
        return true;
    };

    if (reportBool("bold", fmtA.bold, fmtB.bold)) return false;
    if (reportBool("italic", fmtA.italic, fmtB.italic)) return false;
    if (reportBool("underline", fmtA.underline, fmtB.underline)) return false;

    // Font — resolve by family; missing \fonttbl ≡ Qt default font
    static const std::string defaultFamily = QFont().family().toStdString();
    std::string familyA, familyB;
    bool hasFontA = fmtA.fontIndex >= 0 && fmtA.fontIndex < static_cast<int>(docA.fonts.size());
    bool hasFontB = fmtB.fontIndex >= 0 && fmtB.fontIndex < static_cast<int>(docB.fonts.size());
    if (hasFontA) familyA = docA.fonts[fmtA.fontIndex].family;
    if (hasFontB) familyB = docB.fonts[fmtB.fontIndex].family;
    if (familyA.empty()) familyA = defaultFamily;
    if (familyB.empty()) familyB = defaultFamily;
    if ((hasFontA || hasFontB) && familyA != familyB) {
        reason = loc + " fontIndex: " + std::to_string(fmtA.fontIndex) +
            " (" + familyA + ") vs " + std::to_string(fmtB.fontIndex) + " (" + familyB + ")";
        return false;
    }

    int effA = fmtA.fontSize > 0 ? fmtA.fontSize : effFsA;
    int effB = fmtB.fontSize > 0 ? fmtB.fontSize : effFsB;
    if (reportInt("fontSize", effA, effB)) return false;
    effFsA = effA;
    effFsB = effB;

    if (fmtA.colorIndex != fmtB.colorIndex) {
        if (reportResolvedColor("colorIndex", fmtA.colorIndex, fmtB.colorIndex)) return false;
    }
    if (reportBool("superscript", fmtA.superscript, fmtB.superscript)) return false;
    if (reportBool("subscript", fmtA.subscript, fmtB.subscript)) return false;
    if (reportBool("strikeOut", fmtA.strikeOut, fmtB.strikeOut)) return false;
    if (fmtA.bgColorIndex != fmtB.bgColorIndex) {
        if (reportResolvedColor("bgColorIndex", fmtA.bgColorIndex, fmtB.bgColorIndex)) return false;
    }
    if (reportInt("underlineStyle", static_cast<int>(fmtA.underlineStyle),
                                  static_cast<int>(fmtB.underlineStyle))) return false;
    if (reportInt("capitalization", static_cast<int>(fmtA.capitalization),
                                   static_cast<int>(fmtB.capitalization))) return false;
    if (reportInt("upOffset", fmtA.upOffset, fmtB.upOffset)) return false;
    if (reportInt("dnOffset", fmtA.dnOffset, fmtB.dnOffset)) return false;
    if (reportBool("kerning", fmtA.kerning, fmtB.kerning)) return false;
    if (reportInt("expnd", fmtA.expnd, fmtB.expnd)) return false;
    if (fmtA.ulColorIndex != 0 || fmtB.ulColorIndex != 0) {
        if (reportResolvedColor("ulColorIndex", fmtA.ulColorIndex, fmtB.ulColorIndex)) return false;
    }
    if (reportInt("langId", fmtA.langId, fmtB.langId)) return false;
    if (reportBool("protected", fmtA.protected_, fmtB.protected_)) return false;
    return true;
}

static RtfCompareResult CompareRunsSemantic(const std::vector<RtfRun>& runsA,
    const std::vector<RtfRun>& runsB, const RtfDocument& docA, const RtfDocument& docB,
    const std::string& kind, size_t locIdx, int& effFsA, int& effFsB, std::string& reason) {
    // Compare total text
    std::string textA, textB;
    for (const auto& run : runsA) textA += run.text;
    for (const auto& run : runsB) textB += run.text;
    if (textA != textB) {
        reason = kind + " " + std::to_string(locIdx) +
            " text: '" + textA + "' vs '" + textB + "'";
        return RtfCompareResult::StructuralDiff;
    }

    // Walk both run lists, compare formats at each boundary.
    // endA/endB track the absolute end position of each current run.
    size_t idxA = 0, idxB = 0;
    size_t charPos = 0;
    size_t endA = runsA.empty() ? 0 : runsA[0].text.length();
    size_t endB = runsB.empty() ? 0 : runsB[0].text.length();
    while (idxA < runsA.size() || idxB < runsB.size()) {
        size_t nextPos;
        if (idxA >= runsA.size()) nextPos = endB;
        else if (idxB >= runsB.size()) nextPos = endA;
        else nextPos = std::min(endA, endB);

        if (idxA < runsA.size() && idxB < runsB.size()) {
            std::string loc = BuildRunLocation(kind, locIdx, charPos, idxA, idxB);
            if (!CompareFormatSemantic(runsA[idxA].format, runsB[idxB].format,
                    docA, docB, loc, effFsA, effFsB, reason)) {
                return RtfCompareResult::StructuralDiff;
            }
        }

        if (endA == nextPos) {
            idxA++;
            if (idxA < runsA.size()) endA += runsA[idxA].text.length();
        }
        if (endB == nextPos) {
            idxB++;
            if (idxB < runsB.size()) endB += runsB[idxB].text.length();
        }
        charPos = nextPos;
    }
    return RtfCompareResult::Identical;
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

    if (a.defaultLangId != b.defaultLangId) {
        reason = "defaultLangId: " + std::to_string(a.defaultLangId) + " vs " + std::to_string(b.defaultLangId);
        return RtfCompareResult::StructuralDiff;
    }
    if (a.viewKind != b.viewKind) {
        reason = "viewKind: " + std::to_string(a.viewKind) + " vs " + std::to_string(b.viewKind);
        return RtfCompareResult::StructuralDiff;
    }
    if (a.ucByteCount != b.ucByteCount) {
        reason = "ucByteCount: " + std::to_string(a.ucByteCount) + " vs " + std::to_string(b.ucByteCount);
        return RtfCompareResult::StructuralDiff;
    }

    struct TableGroup {
        size_t startIdx;
        size_t rowCount;
        const std::vector<int>& cellxPositions;
    };

    // Group consecutive table rows into tables for comparison
    auto groupTables = [](const RtfDocument& d) {
        std::vector<TableGroup> tables;
        for (size_t i = 0; i < d.elements.size(); ) {
            if (std::holds_alternative<RtfTableRowEntry>(d.elements[i])) {
                size_t start = i;
                const auto& firstRow = std::get<RtfTableRowEntry>(d.elements[i]);
                size_t count = 1;
                while (i + count < d.elements.size() &&
                       std::holds_alternative<RtfTableRowEntry>(d.elements[i + count])) {
                    count++;
                }
                tables.push_back({start, count, firstRow.cellxPositions});
                i += count;
            } else {
                i++;
            }
        }
        return tables;
    };

    auto tablesA = groupTables(a);
    auto tablesB = groupTables(b);

    // Count logical elements from table groups: tables + (totalElements - tableRowElements)
    auto countLogical = [](const RtfDocument& d, const std::vector<TableGroup>& tables) {
        size_t tableRows = 0;
        for (const auto& t : tables) tableRows += t.rowCount;
        return tables.size() + d.elements.size() - tableRows;
    };

    if (countLogical(a, tablesA) != countLogical(b, tablesB)) {
        reason = "Total logical element count: " + std::to_string(countLogical(a, tablesA)) +
                 " vs " + std::to_string(countLogical(b, tablesB));
        return RtfCompareResult::StructuralDiff;
    }

    // Compare element by element
    size_t elemIdxA = 0, elemIdxB = 0;
    size_t tableIdxA = 0, tableIdxB = 0;
    size_t paraIdx = 0, imgIdx = 0;
    int effFontSizeA = 0, effFontSizeB = 0;

    while (elemIdxA < a.elements.size() || elemIdxB < b.elements.size()) {
        // Skip to next logical element (table, paragraph, or image)
        if (tableIdxA < tablesA.size() && tablesA[tableIdxA].startIdx == elemIdxA) {
            if (tableIdxB >= tablesB.size() || tablesB[tableIdxB].startIdx != elemIdxB) {
                reason = "Element type mismatch at position " + std::to_string(tableIdxA) +
                         ": table vs non-table";
                return RtfCompareResult::StructuralDiff;
            }

            // Compare table structure
            const auto& cellxA = tablesA[tableIdxA].cellxPositions;
            const auto& cellxB = tablesB[tableIdxB].cellxPositions;
            size_t rowCountA = tablesA[tableIdxA].rowCount;
            size_t rowCountB = tablesB[tableIdxB].rowCount;

            if (cellxA.size() != cellxB.size()) {
                reason = "Table " + std::to_string(tableIdxA) +
                         " column count: " + std::to_string(cellxA.size()) +
                         " vs " + std::to_string(cellxB.size());
                return RtfCompareResult::StructuralDiff;
            }
            for (size_t ci = 0; ci < cellxA.size(); ++ci) {
                if (cellxA[ci] != cellxB[ci]) {
                    reason = "Table " + std::to_string(tableIdxA) +
                             " cellx[" + std::to_string(ci) + "]: " +
                             std::to_string(cellxA[ci]) +
                             " vs " + std::to_string(cellxB[ci]);
                    return RtfCompareResult::StructuralDiff;
                }
            }

            if (rowCountA != rowCountB) {
                reason = "Table " + std::to_string(tableIdxA) +
                         " row count: " + std::to_string(rowCountA) +
                         " vs " + std::to_string(rowCountB);
                return RtfCompareResult::StructuralDiff;
            }

            // Compare table alignment
            {
                int alignA = rowCountA > 0 ?
                    std::get<RtfTableRowEntry>(a.elements[tablesA[tableIdxA].startIdx]).tableAlignment : 0;
                int alignB = rowCountB > 0 ?
                    std::get<RtfTableRowEntry>(b.elements[tablesB[tableIdxB].startIdx]).tableAlignment : 0;
                if (alignA != alignB) {
                    reason = "Table " + std::to_string(tableIdxA) +
                             " alignment: " + std::to_string(alignA) +
                             " vs " + std::to_string(alignB);
                    return RtfCompareResult::StructuralDiff;
                }
            }

            // Compare rows
            for (size_t ri = 0; ri < rowCountA; ++ri) {
                const auto& rowA = std::get<RtfTableRowEntry>(a.elements[tablesA[tableIdxA].startIdx + ri]);
                const auto& rowB = std::get<RtfTableRowEntry>(b.elements[tablesB[tableIdxB].startIdx + ri]);

                if (rowA.cells.size() != rowB.cells.size()) {
                    reason = "Table " + std::to_string(tableIdxA) +
                             " row " + std::to_string(ri) +
                             " cell count: " + std::to_string(rowA.cells.size()) +
                             " vs " + std::to_string(rowB.cells.size());
                    return RtfCompareResult::StructuralDiff;
                }
                for (size_t ci = 0; ci < rowA.cells.size(); ++ci) {
                    const auto& [runsA, fmtA] = rowA.cells[ci];
                    const auto& [runsB, fmtB] = rowB.cells[ci];

                    std::string cellKind = "Table " + std::to_string(tableIdxA) +
                        " row " + std::to_string(ri) + " cell " + std::to_string(ci);
                    int cellEffFsA = 0, cellEffFsB = 0;
                    if (CompareRunsSemantic(runsA, runsB, a, b, cellKind, 0,
                            cellEffFsA, cellEffFsB, reason) != RtfCompareResult::Identical) {
                        return RtfCompareResult::StructuralDiff;
                    }

                    if (fmtA.vertAlign != fmtB.vertAlign) {
                        reason = "Table " + std::to_string(tableIdxA) +
                                 " row " + std::to_string(ri) +
                                 " cell " + std::to_string(ci) +
                                 " vertAlign: " + std::to_string(fmtA.vertAlign) +
                                 " vs " + std::to_string(fmtB.vertAlign);
                        return RtfCompareResult::StructuralDiff;
                    }

                    // Normalize borders: merge row borders into cell borders for comparison
                    {
                        TableCellBorders normA = NormalizeCellBorders(fmtA.borders, rowA.rowBorders);
                        TableCellBorders normB = NormalizeCellBorders(fmtB.borders, rowB.rowBorders);
                        if (normA != normB) {
                            reason = "Table " + std::to_string(tableIdxA) +
                                     " row " + std::to_string(ri) +
                                     " cell " + std::to_string(ci) + " borders differ";
                            return RtfCompareResult::StructuralDiff;
                        }
                    }

                    // Compare effective padding
                    {
                        auto effPad = [&]() {
                            struct Pad { int top, left, right, bottom; };
                            return Pad{
                                EffectiveCellPadding(fmtA.topPadding, rowA.rowTopPadding),
                                EffectiveCellPadding(fmtA.leftPadding, rowA.rowLeftPadding),
                                EffectiveCellPadding(fmtA.rightPadding, rowA.rowRightPadding),
                                EffectiveCellPadding(fmtA.bottomPadding, rowA.rowBottomPadding)
                            };
                        };
                        auto padA = effPad();
                        auto effPadB = [&]() {
                            struct Pad { int top, left, right, bottom; };
                            return Pad{
                                EffectiveCellPadding(fmtB.topPadding, rowB.rowTopPadding),
                                EffectiveCellPadding(fmtB.leftPadding, rowB.rowLeftPadding),
                                EffectiveCellPadding(fmtB.rightPadding, rowB.rowRightPadding),
                                EffectiveCellPadding(fmtB.bottomPadding, rowB.rowBottomPadding)
                            };
                        };
                        auto padB = effPadB();
                        if (padA.top != padB.top || padA.left != padB.left ||
                            padA.right != padB.right || padA.bottom != padB.bottom) {
                            reason = "Table " + std::to_string(tableIdxA) +
                                     " row " + std::to_string(ri) +
                                     " cell " + std::to_string(ci) + " padding differs";
                            return RtfCompareResult::StructuralDiff;
                        }
                    }

                    if (fmtA.shadingColor != fmtB.shadingColor) {
                        reason = "Table " + std::to_string(tableIdxA) +
                                 " row " + std::to_string(ri) +
                                 " cell " + std::to_string(ci) +
                                 " shadingColor: " + std::to_string(fmtA.shadingColor) +
                                 " vs " + std::to_string(fmtB.shadingColor);
                        return RtfCompareResult::StructuralDiff;
                    }
                }
            }

            tableIdxA++;
            tableIdxB++;
            elemIdxA += rowCountA;
            elemIdxB += rowCountB;
        } else {
            // Non-table element
            bool isParaA = elemIdxA < a.elements.size() && std::holds_alternative<RtfParagraph>(a.elements[elemIdxA]);
            bool isParaB = elemIdxB < b.elements.size() && std::holds_alternative<RtfParagraph>(b.elements[elemIdxB]);
            bool isImgA = elemIdxA < a.elements.size() && std::holds_alternative<RtfImage>(a.elements[elemIdxA]);
            bool isImgB = elemIdxB < b.elements.size() && std::holds_alternative<RtfImage>(b.elements[elemIdxB]);

            if (isParaA != isParaB || isImgA != isImgB) {
                reason = "Element type mismatch at position " + std::to_string(elemIdxA);
                return RtfCompareResult::StructuralDiff;
            }

            if (isImgA && isImgB) {
                const auto& imgA = std::get<RtfImage>(a.elements[elemIdxA]);
                const auto& imgB = std::get<RtfImage>(b.elements[elemIdxB]);
                if (CompareImage(imgIdx, imgA, imgB, reason)) return RtfCompareResult::StructuralDiff;
                imgIdx++;
                elemIdxA++;
                elemIdxB++;
            } else if (isParaA && isParaB) {
                const auto& paraA = std::get<RtfParagraph>(a.elements[elemIdxA]);
                const auto& paraB = std::get<RtfParagraph>(b.elements[elemIdxB]);

                if (CompareField(paraIdx, SIZE_MAX, "alignment",
                                 paraA.alignment, paraB.alignment, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "leftIndent",
                                 paraA.leftIndent, paraB.leftIndent, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "firstLineIndent",
                                 paraA.firstLineIndent, paraB.firstLineIndent, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "rightIndent",
                                 paraA.rightIndent, paraB.rightIndent, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "spaceBefore",
                                 paraA.spaceBefore, paraB.spaceBefore, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "spaceAfter",
                                 paraA.spaceAfter, paraB.spaceAfter, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "lineHeight",
                                 paraA.lineHeight, paraB.lineHeight, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "slMult",
                                  paraA.slMult, paraB.slMult, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "listId",
                                  paraA.listId, paraB.listId, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "listLevel",
                                  paraA.listLevel, paraB.listLevel, reason)) return RtfCompareResult::StructuralDiff;
                if (CompareField(paraIdx, SIZE_MAX, "listStyle",
                                  static_cast<int>(paraA.listStyle), static_cast<int>(paraB.listStyle), reason)) return RtfCompareResult::StructuralDiff;
                if (paraA.tabStops.size() != paraB.tabStops.size()) {
                    reason = "Paragraph " + std::to_string(paraIdx) +
                             " tabStops count: " + std::to_string(paraA.tabStops.size()) +
                             " vs " + std::to_string(paraB.tabStops.size());
                    return RtfCompareResult::StructuralDiff;
                }
                for (size_t t = 0; t < paraA.tabStops.size(); ++t) {
                    if (paraA.tabStops[t] != paraB.tabStops[t]) {
                        reason = "Paragraph " + std::to_string(paraIdx) +
                                 " tabStops[" + std::to_string(t) +
                                 "]: position(" + std::to_string(paraA.tabStops[t].position) +
                                 "/" + std::to_string(paraB.tabStops[t].position) +
                                 ") align(" + std::to_string(paraA.tabStops[t].alignment) +
                                 "/" + std::to_string(paraB.tabStops[t].alignment) + ")";
                        return RtfCompareResult::StructuralDiff;
                    }
                }

                if (CompareRunsSemantic(paraA.runs, paraB.runs, a, b,
                        "Paragraph", paraIdx, effFontSizeA, effFontSizeB,
                        reason) != RtfCompareResult::Identical) {
                    return RtfCompareResult::StructuralDiff;
                }

                paraIdx++;
                elemIdxA++;
                elemIdxB++;
            } else {
                break;
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

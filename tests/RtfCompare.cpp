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

                    if (runsA.size() != runsB.size()) {
                        reason = "Table " + std::to_string(tableIdxA) +
                                 " row " + std::to_string(ri) +
                                 " cell " + std::to_string(ci) +
                                 " run count: " + std::to_string(runsA.size()) +
                                 " vs " + std::to_string(runsB.size());
                        return RtfCompareResult::StructuralDiff;
                    }
                    for (size_t j = 0; j < runsA.size(); ++j) {
                        if (runsA[j].text != runsB[j].text) {
                            reason = "Table " + std::to_string(tableIdxA) +
                                     " row " + std::to_string(ri) +
                                     " cell " + std::to_string(ci) +
                                     " run " + std::to_string(j) +
                                     " text: '" + runsA[j].text + "' vs '" + runsB[j].text + "'";
                            return RtfCompareResult::StructuralDiff;
                        }
                        if (CompareBoolField(tableIdxA, j, "cell bold",
                                             runsA[j].format.bold, runsB[j].format.bold, reason))
                            return RtfCompareResult::StructuralDiff;
                        if (CompareBoolField(tableIdxA, j, "cell italic",
                                             runsA[j].format.italic, runsB[j].format.italic, reason))
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

                if (paraA.runs.size() != paraB.runs.size()) {
                    reason = "Paragraph " + std::to_string(paraIdx) +
                             " run count: " + std::to_string(paraA.runs.size()) +
                             " vs " + std::to_string(paraB.runs.size());
                    return RtfCompareResult::StructuralDiff;
                }

                for (size_t j = 0; j < paraA.runs.size(); ++j) {
                    const auto& runA = paraA.runs[j];
                    const auto& runB = paraB.runs[j];

                    if (runA.text != runB.text) {
                        reason = "Paragraph " + std::to_string(paraIdx) +
                                 " run " + std::to_string(j) +
                                 " text: '" + runA.text + "' vs '" + runB.text + "'";
                        return RtfCompareResult::StructuralDiff;
                    }

                    if (CompareBoolField(paraIdx, j, "bold",
                                         runA.format.bold, runB.format.bold, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "italic",
                                         runA.format.italic, runB.format.italic, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "underline",
                                         runA.format.underline, runB.format.underline, reason)) return RtfCompareResult::StructuralDiff;
                    if (runA.format.fontIndex != runB.format.fontIndex) {
                        std::string familyA, familyB;
                        bool hasFontA = runA.format.fontIndex >= 0 &&
                            runA.format.fontIndex < static_cast<int>(a.fonts.size());
                        bool hasFontB = runB.format.fontIndex >= 0 &&
                            runB.format.fontIndex < static_cast<int>(b.fonts.size());
                        if (hasFontA) familyA = a.fonts[runA.format.fontIndex].family;
                        if (hasFontB) familyB = b.fonts[runB.format.fontIndex].family;
                        if (hasFontA && hasFontB && familyA == familyB) {
                        } else {
                            reason = "Paragraph " + std::to_string(paraIdx) +
                                     " run " + std::to_string(j) +
                                     " fontIndex: " + std::to_string(runA.format.fontIndex) +
                                     " (" + familyA + ") vs " + std::to_string(runB.format.fontIndex) +
                                     " (" + familyB + ")";
                            return RtfCompareResult::StructuralDiff;
                        }
                    }
                    if (CompareField(paraIdx, j, "fontSize",
                                      runA.format.fontSize > 0 ? runA.format.fontSize : effFontSizeA,
                                      runB.format.fontSize > 0 ? runB.format.fontSize : effFontSizeB,
                                      reason)) return RtfCompareResult::StructuralDiff;
                    effFontSizeA = runA.format.fontSize > 0 ? runA.format.fontSize : effFontSizeA;
                    effFontSizeB = runB.format.fontSize > 0 ? runB.format.fontSize : effFontSizeB;
                    if (runA.format.colorIndex != runB.format.colorIndex) {
                        if (CompareResolvedColors(paraIdx, j, runA.format.colorIndex, runB.format.colorIndex,
                                                   a, b,
                                                   ResolveColorFromTable, ResolveColorFromTable,
                                                   reason, "colorIndex")) {
                            return RtfCompareResult::StructuralDiff;
                        }
                    }
                    if (CompareBoolField(paraIdx, j, "superscript",
                                         runA.format.superscript, runB.format.superscript, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "subscript",
                                         runA.format.subscript, runB.format.subscript, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "strikeOut",
                                         runA.format.strikeOut, runB.format.strikeOut, reason)) return RtfCompareResult::StructuralDiff;
                    if (runA.format.bgColorIndex != runB.format.bgColorIndex) {
                        if (CompareResolvedColors(paraIdx, j, runA.format.bgColorIndex, runB.format.bgColorIndex,
                                                   a, b,
                                                   ResolveColorFromTable, ResolveColorFromTable,
                                                   reason, "bgColorIndex")) {
                            return RtfCompareResult::StructuralDiff;
                        }
                    }
                    if (CompareField(paraIdx, j, "underlineStyle",
                                     static_cast<int>(runA.format.underlineStyle), static_cast<int>(runB.format.underlineStyle), reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareField(paraIdx, j, "capitalization",
                                     static_cast<int>(runA.format.capitalization), static_cast<int>(runB.format.capitalization), reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareField(paraIdx, j, "upOffset",
                                      runA.format.upOffset, runB.format.upOffset, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareField(paraIdx, j, "dnOffset",
                                      runA.format.dnOffset, runB.format.dnOffset, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "kerning",
                                          runA.format.kerning, runB.format.kerning, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareField(paraIdx, j, "expnd",
                                       runA.format.expnd, runB.format.expnd, reason)) return RtfCompareResult::StructuralDiff;
                    if (runA.format.ulColorIndex != 0 || runB.format.ulColorIndex != 0) {
                        if (CompareResolvedColors(paraIdx, j, runA.format.ulColorIndex, runB.format.ulColorIndex,
                                                    a, b,
                                                    ResolveColorFromTable, ResolveColorFromTable,
                                                    reason, "ulColorIndex")) {
                            return RtfCompareResult::StructuralDiff;
                        }
                    }
                    if (CompareField(paraIdx, j, "langId",
                                        runA.format.langId, runB.format.langId, reason)) return RtfCompareResult::StructuralDiff;
                    if (CompareBoolField(paraIdx, j, "protected",
                                          runA.format.protected_, runB.format.protected_, reason)) return RtfCompareResult::StructuralDiff;
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

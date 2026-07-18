#include "RtfImport.h"

#include "RtfParser.h"
#include "RtfTypes.h"

#include <algorithm>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextOption>
#include <QTextListFormat>
#include <QTextList>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextTableCell>
#include <QTextTableCellFormat>
#include <QFont>
#include <QColor>
#include <QImageReader>
#include <QBuffer>

namespace Rte {

namespace {

static void InsertRuns(QTextCursor& cursor, const std::vector<RtfRun>& runs,
                       const RtfDocument& doc, const QFont& defaultFont) {
    for (const RtfRun& run : runs) {
        QTextCharFormat charFmt;

        if (run.format.fontSize > 0) {
            charFmt.setFontPointSize(run.format.fontSize / 2.0);
        }
        if (run.format.bold) {
            charFmt.setFontWeight(QFont::Bold);
        }
        if (run.format.italic) {
            charFmt.setFontItalic(true);
        }
        if (run.format.strikeOut) {
            charFmt.setFontStrikeOut(true);
        }

        if (run.format.fontIndex >= 0 &&
            run.format.fontIndex < static_cast<int>(doc.fonts.size())) {
            const RtfFontEntry& fontEntry = doc.fonts[run.format.fontIndex];
            QString fam = QString::fromStdString(fontEntry.family);
            if (!fam.isEmpty()) {
                QStringList families = {fam};
                // Symbol font is a Windows-only legacy font; on non-Windows OS it's
                // almost certainly missing, so add fallbacks that cover its glyph range.
                if (fam.toLower() == "symbol") {
                    families << "Segoe UI Symbol" << "Noto Sans Symbols2" << "Libertinus Math"
                             << "Standard Symbols PS" << "DejaVu Sans" << "Noto Sans Math";
                }
                charFmt.setFontFamilies(families);
            }
        }

        if (run.format.colorIndex >= 0 &&
            run.format.colorIndex < static_cast<int>(doc.colors.size())) {
            const auto& col = doc.colors[run.format.colorIndex];
            charFmt.setForeground(QColor(col.red, col.green, col.blue));
        }

        if (run.format.bgColorIndex >= 0 &&
            run.format.bgColorIndex < static_cast<int>(doc.colors.size())) {
            const auto& col = doc.colors[run.format.bgColorIndex];
            charFmt.setBackground(QBrush(QColor(col.red, col.green, col.blue)));
        }

        if (run.format.underline) {
            if (run.format.underlineStyle == UnderlineStyle::Solid ||
                run.format.underlineStyle == UnderlineStyle::None) {
                charFmt.setFontUnderline(true);
            } else {
                charFmt.setUnderlineStyle(qtUnderlineStyleFor(run.format.underlineStyle));
            }
        }

        if (run.format.capitalization == Capitalization::AllCaps) {
            charFmt.setFontCapitalization(QFont::AllUppercase);
        } else if (run.format.capitalization == Capitalization::SmallCaps) {
            charFmt.setFontCapitalization(QFont::SmallCaps);
        }

        if (run.format.kerning) {
            charFmt.setFontKerning(true);
        }
        if (run.format.expnd != 0) {
            double ptSize = run.format.fontSize > 0 ? run.format.fontSize / 2.0 : defaultFont.pointSizeF();
            charFmt.setFontLetterSpacing(run.format.expnd / 20.0 * ptSize);
        }

        if (run.format.superscript) {
            charFmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
        } else if (run.format.subscript) {
            charFmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
        }

        if (run.format.protected_) {
            charFmt.setProperty(UserPropProtect, true);
        }
        if (run.format.upOffset != 0) {
            charFmt.setProperty(UserPropUpOffset, run.format.upOffset);
        }
        if (run.format.dnOffset != 0) {
            charFmt.setProperty(UserPropDnOffset, run.format.dnOffset);
        }
        if (run.format.langId != 0) {
            charFmt.setProperty(UserPropLangId, run.format.langId);
        }

        cursor.insertText(QString::fromUtf8(run.text.data(),
                                              static_cast<int>(run.text.size())),
                           charFmt);
    }
}

static void BuildParagraph(QTextCursor& cursor, const RtfParagraph& para,
                            const RtfDocument& doc, const QFont& defaultFont,
                            int& prevListId, int& prevListLevel,
                            bool& inList, QTextList*& currentList) {
    QTextBlockFormat blockFmt;
    blockFmt.setAlignment(RtfAlignmentToQt(para.alignment));
    if (para.leftIndent > 0 || para.firstLineIndent > 0) {
        blockFmt.setLeftMargin(static_cast<double>(para.leftIndent) / 2.0);
        blockFmt.setIndent(para.firstLineIndent > 0 ? static_cast<int>(para.firstLineIndent / 2.0) : 0);
    }
    if (para.rightIndent > 0) {
        blockFmt.setRightMargin(static_cast<double>(para.rightIndent) / 2.0);
    }
    if (para.spaceBefore > 0) {
        blockFmt.setTopMargin(static_cast<double>(para.spaceBefore) / 2.0);
    }
    if (para.spaceAfter > 0) {
        blockFmt.setBottomMargin(static_cast<double>(para.spaceAfter) / 2.0);
    }
    if (para.lineHeight > 0) {
        blockFmt.setLineHeight(static_cast<double>(para.lineHeight) / 2.0,
                                QTextBlockFormat::FixedHeight);
    }
    if (!para.tabStops.empty()) {
        QList<QTextOption::Tab> tabs;
        for (const TabStop& ts : para.tabStops) {
            QTextOption::TabType type;
            switch (ts.alignment) {
            case 1: type = QTextOption::LeftTab; break;
            case 128: type = QTextOption::CenterTab; break;
            case 2: type = QTextOption::RightTab; break;
            case 3: type = QTextOption::LeftTab; break;
            default: type = QTextOption::LeftTab; break;
            }
            tabs << QTextOption::Tab(static_cast<double>(ts.position) / 2.0, type);
        }
        blockFmt.setTabPositions(tabs);
    }

    // Handle list insertion
    if (para.listId > 0 && (!inList || para.listId != prevListId)) {
        QTextListFormat listFmt;
        listFmt.setStyle(RtfListStyleToQt(para.listStyle));
        if (para.listIndent > 0) {
            listFmt.setIndent(static_cast<int>(para.listIndent / 2.0));
        }
        currentList = cursor.insertList(listFmt);
        inList = true;
    } else if (para.listId > 0 && inList && para.listId == prevListId) {
        cursor.insertBlock();
        cursor.setBlockFormat(blockFmt);
        currentList->add(cursor.block());
    } else {
        inList = false;
        currentList = nullptr;
        cursor.insertBlock(blockFmt);
    }

    // Store per-paragraph group-persistent values as block properties
    {
        QTextBlockFormat curFmt = cursor.blockFormat();
        curFmt.setProperty(UserPropParaDefaultFontIndex, para.defaultFontIndex);
        curFmt.setProperty(UserPropParaDefaultTabStopTwips, para.defaultTabStopTwips);
        cursor.setBlockFormat(curFmt);
    }
    prevListId = para.listId;
    prevListLevel = para.listLevel;

    InsertRuns(cursor, para.runs, doc, defaultFont);
}

static void BuildImage(QTextCursor& cursor, const RtfImage& img,
                        QTextDocument* document) {
    static int imgCounter = 0;
    cursor.insertBlock();

    // Determine image size in pixels
    qreal widthPx = 0, heightPx = 0;
    {
        QByteArray imageData = img.data;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        QSize size = reader.size();
        if (size.isValid() && size.width() > 0 && size.height() > 0) {
            widthPx = size.width() * 96.0 / 72.0;
            heightPx = size.height() * 96.0 / 72.0;
        }
    }

    if (widthPx <= 0 || heightPx <= 0) {
        widthPx = 100;
        heightPx = 100;
    }

    // Apply scaling
    widthPx *= img.picscalex / 100.0;
    heightPx *= img.picscaley / 100.0;

    // Register image as resource
    imgCounter++;
    const char* ext = "";
    switch (img.format) {
        case Rte::RtfImageFormat::Jpeg: ext = "jpg"; break;
        case Rte::RtfImageFormat::Png:  ext = "png"; break;
        case Rte::RtfImageFormat::Bmp:  ext = "bmp"; break;
        default:                        ext = "png"; break;
    }
    QString name = QString("rtfimage://%1.%2").arg(imgCounter).arg(ext);
    document->addResource(QTextDocument::ImageResource, QUrl(name), QVariant::fromValue(img.data));

    // Store original RTF hex string and format for byte-identical roundtrip
    if (!img.rtfPictHex.empty()) {
        QString propName = QString("rtfPictHex://img%1").arg(imgCounter);
        document->setProperty(qPrintable(propName),
                               QVariant::fromValue(QString::fromLatin1(img.rtfPictHex.c_str())));
        QString fmtPropName = QString("rtfImageFormat://img%1").arg(imgCounter);
        document->setProperty(qPrintable(fmtPropName),
                               QVariant::fromValue(QString(ext)));
    }

    QTextImageFormat imgFmt;
    imgFmt.setName(name);
    imgFmt.setWidth(widthPx);
    imgFmt.setHeight(heightPx);
    cursor.insertImage(imgFmt);
}

static QColor ResolveBorderColor(int colorIdx, const RtfDocument& doc) {
    if (colorIdx > 0 && colorIdx < static_cast<int>(doc.colors.size())) {
        const RtfColorEntry& col = doc.colors[colorIdx];
        return QColor(col.red, col.green, col.blue);
    }
    return QColor();
}

static void ApplyBorderToCellFormat(QTextTableCellFormat& cellFmt, int side,
                                     int width, BorderStyle style, int colorIdx, const RtfDocument& doc) {
    if (width <= 0) return;
    double borderPt = width / 2.0;
    QTextFrameFormat::BorderStyle qtStyle = QTextFrameFormat::BorderStyle_None;
    switch (style) {
        case BorderStyle::Dashed: qtStyle = QTextFrameFormat::BorderStyle_Dashed; break;
        case BorderStyle::Solid: qtStyle = QTextFrameFormat::BorderStyle_Solid; break;
        default: return;
    }
    QColor color = ResolveBorderColor(colorIdx, doc);

    switch (side) {
        case 0:
            cellFmt.setLeftBorder(borderPt);
            cellFmt.setLeftBorderStyle(qtStyle);
            if (color.isValid()) cellFmt.setLeftBorderBrush(color);
            break;
        case 1:
            cellFmt.setTopBorder(borderPt);
            cellFmt.setTopBorderStyle(qtStyle);
            if (color.isValid()) cellFmt.setTopBorderBrush(color);
            break;
        case 2:
            cellFmt.setRightBorder(borderPt);
            cellFmt.setRightBorderStyle(qtStyle);
            if (color.isValid()) cellFmt.setRightBorderBrush(color);
            break;
        case 3:
            cellFmt.setBottomBorder(borderPt);
            cellFmt.setBottomBorderStyle(qtStyle);
            if (color.isValid()) cellFmt.setBottomBorderBrush(color);
            break;
    }
}

static double ComputeEffectivePadding(int cellPad, int rowPad) {
    return (cellPad > 0 || rowPad > 0) ? std::max(cellPad, rowPad) / 2.0 : 0.0;
}

static void FlushTableRows(QTextCursor& cursor, std::vector<const RtfTableRowEntry*>& tableRows,
                             const RtfDocument& doc, const QFont& defaultFont) {
    if (tableRows.empty()) return;

    // Use first row's cellxPositions for column widths
    int colCount = static_cast<int>(tableRows[0]->cellxPositions.size());
    int rowCount = static_cast<int>(tableRows.size());
    if (colCount == 0 || rowCount == 0) {
        tableRows.clear();
        return;
    }

    QTextTableFormat tableFmt;
    tableFmt.setBorderStyle(QTextFrameFormat::BorderStyle_None);
    tableFmt.setCellSpacing(0);

    // Table alignment from first row
    {
        int align = tableRows[0]->tableAlignment;
        if (align == 1) tableFmt.setAlignment(Qt::AlignHCenter);
        else if (align == 2) tableFmt.setAlignment(Qt::AlignRight);
        else tableFmt.setAlignment(Qt::AlignLeft);
    }

    // Set column widths from \cellx positions
    QVector<QTextLength> constraints;
    for (int c = 0; c < colCount; ++c) {
        int width = (c == 0) ? tableRows[0]->cellxPositions[0] :
            tableRows[0]->cellxPositions[c] - tableRows[0]->cellxPositions[c - 1];
        constraints.append(QTextLength(QTextLength::FixedLength,
            static_cast<double>(width) / 20.0));
    }
    tableFmt.setColumnWidthConstraints(constraints);

    QTextTable* qtTable = cursor.insertTable(rowCount, colCount, tableFmt);

    for (int r = 0; r < rowCount; ++r) {
        const auto& rowEntry = tableRows[r];
        for (int c = 0; c < colCount; ++c) {
            QTextTableCell cell = qtTable->cellAt(r, c);
            QTextCursor cellCursor = cell.firstCursorPosition();

            QTextTableCellFormat cellFmt;
            if (c < static_cast<int>(rowEntry->cells.size())) {
                const auto& [runs, cellData] = rowEntry->cells[c];
                switch (cellData.vertAlign) {
                    case 1: cellFmt.setVerticalAlignment(QTextCharFormat::AlignMiddle); break;
                    case 2: cellFmt.setVerticalAlignment(QTextCharFormat::AlignBottom); break;
                    default: cellFmt.setVerticalAlignment(QTextCharFormat::AlignTop); break;
                }

                // Apply padding — cell padding takes precedence, fall back to row padding
                if (cellData.leftPadding > 0 || rowEntry->rowLeftPadding > 0)
                    cellFmt.setLeftPadding(ComputeEffectivePadding(cellData.leftPadding, rowEntry->rowLeftPadding));
                if (cellData.rightPadding > 0 || rowEntry->rowRightPadding > 0)
                    cellFmt.setRightPadding(ComputeEffectivePadding(cellData.rightPadding, rowEntry->rowRightPadding));
                if (cellData.topPadding > 0 || rowEntry->rowTopPadding > 0)
                    cellFmt.setTopPadding(ComputeEffectivePadding(cellData.topPadding, rowEntry->rowTopPadding));
                if (cellData.bottomPadding > 0 || rowEntry->rowBottomPadding > 0)
                    cellFmt.setBottomPadding(ComputeEffectivePadding(cellData.bottomPadding, rowEntry->rowBottomPadding));

                // Apply cell borders
                const auto& borders = cellData.borders;
                // For each side, use cell border; if not set, inherit from row borders
                for (int side = 0; side < 4; ++side) {
                    int w, ci;
                    BorderStyle s;
                    switch (side) {
                        case 0: w = borders.leftWidth; ci = borders.leftColor; s = borders.leftStyle; break;
                        case 1: w = borders.topWidth; ci = borders.topColor; s = borders.topStyle; break;
                        case 2: w = borders.rightWidth; ci = borders.rightColor; s = borders.rightStyle; break;
                        default: w = borders.bottomWidth; ci = borders.bottomColor; s = borders.bottomStyle; break;
                    }
                    if (w <= 0 && s == BorderStyle::None) {
                        // No cell border — try row border
                        const TableCellBorders& rb = rowEntry->rowBorders;
                        switch (side) {
                            case 0: ApplyBorderToCellFormat(cellFmt, side, rb.leftWidth, rb.leftStyle, rb.leftColor, doc); break;
                            case 1: ApplyBorderToCellFormat(cellFmt, side, rb.topWidth, rb.topStyle, rb.topColor, doc); break;
                            case 2: ApplyBorderToCellFormat(cellFmt, side, rb.rightWidth, rb.rightStyle, rb.rightColor, doc); break;
                            default: ApplyBorderToCellFormat(cellFmt, side, rb.bottomWidth, rb.bottomStyle, rb.bottomColor, doc); break;
                        }
                    } else {
                        ApplyBorderToCellFormat(cellFmt, side, w, s, ci, doc);
                    }
                }

                InsertRuns(cellCursor, runs, doc, defaultFont);
            } else {
                // Empty cell — apply row borders
                const TableCellBorders& rb = rowEntry->rowBorders;
                ApplyBorderToCellFormat(cellFmt, 0, rb.leftWidth, rb.leftStyle, rb.leftColor, doc);
                ApplyBorderToCellFormat(cellFmt, 1, rb.topWidth, rb.topStyle, rb.topColor, doc);
                ApplyBorderToCellFormat(cellFmt, 2, rb.rightWidth, rb.rightStyle, rb.rightColor, doc);
                ApplyBorderToCellFormat(cellFmt, 3, rb.bottomWidth, rb.bottomStyle, rb.bottomColor, doc);
            }
            cell.setFormat(cellFmt);
        }
    }

    // Move cursor after the table frame
    cursor.movePosition(QTextCursor::End);

    tableRows.clear();
}

void BuildDocument(QTextDocument* document, const RtfDocument& doc) {
    document->clear();

    QFont defaultFont;
    if (!doc.fonts.empty()) {
        int fi = doc.defaultFontIndex;
        if (fi < 0 || fi >= static_cast<int>(doc.fonts.size())) fi = 0;
        defaultFont.setFamily(QString::fromStdString(doc.fonts[fi].family));
    }
    if (doc.defaultFontSize > 0) {
        defaultFont.setPointSizeF(doc.defaultFontSize / 2.0);
    } else {
        defaultFont.setPointSizeF(12);
    }
    document->setDefaultFont(defaultFont);

    QTextCursor cursor(document);
    int prevListId = 0;
    int prevListLevel = -1;
    bool inList = false;
    QTextList* currentList = nullptr;
    std::vector<const RtfTableRowEntry*> tableRows;

    for (const std::variant<RtfParagraph, RtfTableRowEntry, RtfImage>& elem : doc.elements) {
        std::visit([&](const auto& element) {
            using T = std::decay_t<decltype(element)>;
            if constexpr (std::is_same_v<T, RtfParagraph>) {
                FlushTableRows(cursor, tableRows, doc, defaultFont);
                BuildParagraph(cursor, element, doc, defaultFont,
                               prevListId, prevListLevel, inList, currentList);
            } else if constexpr (std::is_same_v<T, RtfTableRowEntry>) {
                tableRows.push_back(&element);
            } else if constexpr (std::is_same_v<T, RtfImage>) {
                FlushTableRows(cursor, tableRows, doc, defaultFont);
                BuildImage(cursor, element, document);
            }
        }, elem);
    }

    FlushTableRows(cursor, tableRows, doc, defaultFont);
}

} // namespace

void ImportRtf(QTextDocument* document, const std::string& rtf, int codePage) {
    RtfDocument doc = ParseRtf(rtf, codePage);
    BuildDocument(document, doc);

    // Store metadata for roundtrip
    if (doc.defaultLangId != 0)
        document->setProperty(UserPropMetaDefaultLangId, doc.defaultLangId);
    if (doc.viewKind != 0)
        document->setProperty(UserPropMetaViewKind, doc.viewKind);
    if (doc.ucByteCount != 1)
        document->setProperty(UserPropMetaUcByteCount, doc.ucByteCount);
    if (doc.defaultTabStopTwips != 180)  // 180 = RTF spec default
        document->setProperty(UserPropMetaDefaultTabStopTwips, doc.defaultTabStopTwips);
}

} // namespace Rte

#include "RtfImport.h"

#include "RtfParser.h"
#include "RtfQtConversions.h"

#include <algorithm>
#include <array>
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

const char* ImageFormatExtension(RtfImageFormat fmt) {
    switch (fmt) {
        case RtfImageFormat::Jpeg: return "jpg";
        case RtfImageFormat::Png:  return "png";
        case RtfImageFormat::Bmp:  return "bmp";
        case RtfImageFormat::Unknown:
        default:
            throw std::runtime_error("unknown image format");
    }
}

BorderValues GetBorderValues(const TableCellBorders& b, TableSide side) {
    const auto& m = kBorderMembers[side];
    return { b.*(m.width), b.*(m.style), b.*(m.color) };
}

double TwipsToHalfPt(double twips) { return twips / 20.0; }
double MarginTwipsToPoints(double twips) { return twips / 2.0; }

QColor ResolveColor(int idx, const RtfDocument& doc) {
    const RtfColorEntry* col = ResolveColorEntry(idx, doc.colors);
    return col ? QColor(col->red, col->green, col->blue) : QColor();
}

void InsertRuns(QTextCursor& cursor, const std::vector<RtfRun>& runs,
                       const RtfDocument& doc, const QFont& defaultFont) {
    for (const RtfRun& run : runs) {
        // Skip \pntext runs — content is structural list marker, not paragraph body
        if (run.format.inPntext) continue;
        QTextCharFormat charFmt;

        if (run.format.fontSize > 0) {
            charFmt.setFontPointSize(run.format.fontSize / kHalfPtToPoint);
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
            const RtfFontEntry& fontEntry =
                doc.fonts[static_cast<std::size_t>(run.format.fontIndex)];
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

        QColor fgColor = ResolveColor(run.format.colorIndex, doc);
        if (fgColor.isValid()) charFmt.setForeground(fgColor);

        QColor bgColor = ResolveColor(run.format.bgColorIndex, doc);
        if (bgColor.isValid()) charFmt.setBackground(QBrush(bgColor));

        if (run.format.underline) {
            auto qtUlStyle = QtUlStyleFor(run.format.underlineStyle);
            if (qtUlStyle == QTextCharFormat::SingleUnderline ||
                qtUlStyle == QTextCharFormat::NoUnderline) {
                charFmt.setFontUnderline(true);
            } else {
                charFmt.setUnderlineStyle(qtUlStyle);
            }
            int ulRaw = static_cast<int>(run.format.underlineStyle);
            if (ulRaw > static_cast<int>(RtfUnderlineStyle::SpellCheck))
                charFmt.setProperty(UserPropUlStyle, ulRaw);
        }
        if (run.format.ulColorIndex > 0) {
            QColor ulColor = ResolveColor(run.format.ulColorIndex, doc);
            if (ulColor.isValid()) {
                charFmt.setProperty(UserPropUlColorIndex,
                                    QString("%1,%2,%3").arg(ulColor.red()).arg(ulColor.green()).arg(ulColor.blue()));
            }
        }

        if (run.format.capitalization == QFont::AllUppercase) {
            charFmt.setFontCapitalization(QFont::AllUppercase);
        } else if (run.format.capitalization == QFont::SmallCaps) {
            charFmt.setFontCapitalization(QFont::SmallCaps);
        }

        if (run.format.kerning) {
            charFmt.setFontKerning(true);
        }
        if (run.format.expnd != 0) {
            double ptSize = run.format.fontSize > 0 ? run.format.fontSize / kHalfPtToPoint : defaultFont.pointSizeF();
            charFmt.setFontLetterSpacing(run.format.expnd / kExpndToEm * ptSize);
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
        if (run.format.highlightIndex != 0) {
            charFmt.setProperty(UserPropHighlightIndex, run.format.highlightIndex);
        }
        if (run.format.isAnchor) {
            charFmt.setAnchor(true);
            charFmt.setAnchorHref(QString::fromStdString(run.format.anchorHref));
        }

        cursor.insertText(QString::fromUtf8(run.text.data(),
                                              static_cast<int>(run.text.size())),
                           charFmt);
    }
}

void BuildParagraph(QTextCursor& cursor, const RtfParagraph& para,
                             const RtfDocument& doc, const QFont& defaultFont,
                             int& prevListId, int& prevListLevel,
                             bool& inList, QTextList*& currentList) {
    QTextBlockFormat blockFmt;
    blockFmt.setAlignment(para.format.alignment);
    if (para.format.leftIndent != 0)
        blockFmt.setLeftMargin(MarginTwipsToPoints(para.format.leftIndent));
    if (para.format.firstLineIndent != 0)
        blockFmt.setIndent(static_cast<int>(MarginTwipsToPoints(para.format.firstLineIndent)));
    if (para.format.rightIndent != 0)
        blockFmt.setRightMargin(MarginTwipsToPoints(para.format.rightIndent));
    if (para.format.spaceBefore != 0)
        blockFmt.setTopMargin(MarginTwipsToPoints(para.format.spaceBefore));
    if (para.format.spaceAfter != 0)
        blockFmt.setBottomMargin(MarginTwipsToPoints(para.format.spaceAfter));
    if (para.format.lineHeight > 0) {
        blockFmt.setLineHeight(MarginTwipsToPoints(para.format.lineHeight),
                                QTextBlockFormat::FixedHeight);
    }
    if (!para.format.tabStops.empty()) {
        QList<QTextOption::Tab> tabs;
        for (const TabStop& ts : para.format.tabStops) {
            QTextOption::TabType type;
            switch (ts.alignment) {
            case Qt::AlignLeft:   type = QTextOption::LeftTab; break;
            case Qt::AlignHCenter: type = QTextOption::CenterTab; break;
            case Qt::AlignRight:  type = QTextOption::RightTab; break;
            case Qt::AlignJustify: type = QTextOption::LeftTab; break;
            default:              type = QTextOption::LeftTab; break;
            }
            tabs << QTextOption::Tab(MarginTwipsToPoints(ts.position), type);
        }
        blockFmt.setTabPositions(tabs);
    }

    // Handle list insertion
    if (para.listId > 0 && (!inList || para.listId != prevListId)) {
        QTextListFormat listFmt;
        listFmt.setStyle(QtListStyleFor(para.listStyle));
        if (para.listIndent > 0) {
            listFmt.setIndent(static_cast<int>(MarginTwipsToPoints(para.listIndent)));
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
    QTextBlockFormat curFmt = cursor.blockFormat();
    curFmt.setProperty(UserPropParaDefaultFontIndex, para.defaultFontIndex);
    curFmt.setProperty(UserPropParaDefaultTabStopTwips, para.defaultTabStopTwips);
    if (!para.pntextRtf.empty()) {
        curFmt.setProperty(UserPropBlockPntextRtf, QString::fromLatin1(para.pntextRtf.c_str()));
        for (const RtfRun& run : para.runs) {
            if (run.format.inPntext && run.format.fontIndex >= 0 &&
                run.format.fontIndex < static_cast<int>(doc.fonts.size())) {
                const RtfFontEntry& fe = doc.fonts[static_cast<size_t>(run.format.fontIndex)];
                curFmt.setProperty(UserPropBlockPntextFontFamily, QString::fromStdString(fe.family));
                break;
            }
        }
    }
    if (para.format.slMult != 1) {
        curFmt.setProperty(UserPropSlMult, para.format.slMult);
    }
    cursor.setBlockFormat(curFmt);
    prevListId = para.listId;
    prevListLevel = para.listLevel;

    InsertRuns(cursor, para.runs, doc, defaultFont);
}

void BuildImage(QTextCursor& cursor, const RtfImage& img,
                        QTextDocument* document, int& imgCounter) {
    cursor.insertBlock();

    // Determine image size in pixels
    qreal widthPx = 0, heightPx = 0;
    QByteArray imageData = VectorToByteArray(img.data);
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    QSize size = reader.size();
    if (size.isValid() && size.width() > 0 && size.height() > 0) {
        widthPx = size.width() * kDpiToPoints;
        heightPx = size.height() * kDpiToPoints;
    }

    if (widthPx <= 0 || heightPx <= 0) {
        widthPx = 100;
        heightPx = 100;
    }

    // Apply scaling
    widthPx *= img.picscalex / 100.0;
    heightPx *= img.picscaley / 100.0;

    // Register image as resource
    ++imgCounter;
    const char* ext = ImageFormatExtension(img.format);
    QString name = QString("rtfimage://%1.%2").arg(imgCounter).arg(ext);
    document->addResource(QTextDocument::ImageResource, QUrl(name), QVariant::fromValue(VectorToByteArray(img.data)));

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

using PadSetter = void (QTextTableCellFormat::*)(double);
using BorderSetter = void (QTextTableCellFormat::*)(double);
using BorderStyleSetter = void (QTextTableCellFormat::*)(QTextFrameFormat::BorderStyle);
using BorderBrushSetter = void (QTextTableCellFormat::*)(const QBrush&);

constexpr std::array<PadSetter, 4> kPadSetters = {{
    &QTextTableCellFormat::setLeftPadding, &QTextTableCellFormat::setTopPadding,
    &QTextTableCellFormat::setRightPadding, &QTextTableCellFormat::setBottomPadding,
}};

constexpr std::array<BorderSetter, 4> kBorderSetters = {{
    &QTextTableCellFormat::setLeftBorder, &QTextTableCellFormat::setTopBorder,
    &QTextTableCellFormat::setRightBorder, &QTextTableCellFormat::setBottomBorder,
}};

constexpr std::array<BorderStyleSetter, 4> kBorderStyleSetters = {{
    &QTextTableCellFormat::setLeftBorderStyle, &QTextTableCellFormat::setTopBorderStyle,
    &QTextTableCellFormat::setRightBorderStyle, &QTextTableCellFormat::setBottomBorderStyle,
}};

constexpr std::array<BorderBrushSetter, 4> kBorderBrushSetters = {{
    &QTextTableCellFormat::setLeftBorderBrush, &QTextTableCellFormat::setTopBorderBrush,
    &QTextTableCellFormat::setRightBorderBrush, &QTextTableCellFormat::setBottomBorderBrush,
}};

QColor ResolveBorderColor(int colorIdx, const RtfDocument& doc) {
    const RtfColorEntry* col = ResolveColorEntry(colorIdx, doc.colors);
    return col && colorIdx > 0 ? QColor(col->red, col->green, col->blue) : QColor();
}

void ApplyBorderToCellFormat(QTextTableCellFormat& cellFmt, TableSide side,
                                      int width, BorderStyle style, int colorIdx, const RtfDocument& doc) {
    if (width <= 0) return;
    QTextFrameFormat::BorderStyle qtStyle = QTextFrameFormat::BorderStyle_None;
    switch (style) {
        case BorderStyle::Dashed: qtStyle = QTextFrameFormat::BorderStyle_Dashed; break;
        case BorderStyle::Solid:  qtStyle = QTextFrameFormat::BorderStyle_Solid; break;
        case BorderStyle::None:
        default: return;
    }
    double borderPt = MarginTwipsToPoints(width);
    QColor color = ResolveBorderColor(colorIdx, doc);
    (cellFmt.*(kBorderSetters.at(side)))(borderPt);
    (cellFmt.*(kBorderStyleSetters.at(side)))(qtStyle);
    if (color.isValid()) (cellFmt.*(kBorderBrushSetters.at(side)))(QBrush(color));
}

double ComputeEffectivePadding(int cellPad, int rowPad) {
    int eff = EffectiveCellPadding(cellPad, rowPad);
    return eff > 0 ? MarginTwipsToPoints(eff) : 0.0;
}

void FlushTableRows(QTextCursor& cursor, std::vector<const RtfTableRowEntry*>& tableRows,
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
    Qt::Alignment align = tableRows[0]->tableAlignment;
    tableFmt.setAlignment(align);

    // Set column widths from \cellx positions
    QVector<QTextLength> constraints;
    for (int c = 0; c < colCount; ++c) {
        int width = (c == 0) ? tableRows[0]->cellxPositions[0] :
            tableRows[0]->cellxPositions[static_cast<std::size_t>(c)] -
            tableRows[0]->cellxPositions[static_cast<std::size_t>(c - 1)];
        constraints.append(QTextLength(QTextLength::FixedLength,
            TwipsToHalfPt(width)));
    }
    tableFmt.setColumnWidthConstraints(constraints);

    QTextTable* qtTable = cursor.insertTable(rowCount, colCount, tableFmt);

    for (int r = 0; r < rowCount; ++r) {
        const RtfTableRowEntry* rowEntry = tableRows[static_cast<std::size_t>(r)];
        for (int c = 0; c < colCount; ++c) {
            QTextTableCell cell = qtTable->cellAt(r, c);
            QTextCursor cellCursor = cell.firstCursorPosition();

            QTextTableCellFormat cellFmt;
            if (c < static_cast<int>(rowEntry->cells.size())) {
                const auto& [runs, cellData] =
                    rowEntry->cells[static_cast<std::size_t>(c)];
                switch (cellData.vertAlign) {
                    case 1: cellFmt.setVerticalAlignment(QTextCharFormat::AlignMiddle); break;
                    case 2: cellFmt.setVerticalAlignment(QTextCharFormat::AlignBottom); break;
                    default: cellFmt.setVerticalAlignment(QTextCharFormat::AlignTop); break;
                }

                // Cell shading — Qt has no cell background API; store for roundtrip
                if (cellData.shadingColor >= 0) {
                    QColor shading = ResolveColor(cellData.shadingColor, doc);
                    if (shading.isValid()) {
                        cellFmt.setProperty(UserPropCellShading,
                            QString("%1,%2,%3").arg(shading.red()).arg(shading.green()).arg(shading.blue()));
                    }
                }

                // Apply padding — cell padding takes precedence, fall back to row padding
                for (TableSide side : kTableSides) {
                    double effPad = ComputeEffectivePadding(cellData.padding[side], rowEntry->rowPadding[side]);
                    if (effPad > 0) (cellFmt.*(kPadSetters[side]))(effPad);
                }

                // Apply cell borders
                TableCellBorders nb = NormalizeCellBorders(cellData.borders, rowEntry->rowBorders);
                IterateTableSides([&](TableSide side) {
                    auto [w, s, col] = GetBorderValues(nb, side);
                    ApplyBorderToCellFormat(cellFmt, side, w, s, col, doc);
                });

                InsertRuns(cellCursor, runs, doc, defaultFont);
            } else {
                // Empty cell — apply row borders
                IterateTableSides([&](TableSide side) {
                    auto [w, s, col] = GetBorderValues(rowEntry->rowBorders, side);
                    ApplyBorderToCellFormat(cellFmt, side, w, s, col, doc);
                });
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
        defaultFont.setFamily(
            QString::fromStdString(doc.fonts[static_cast<std::size_t>(fi)].family));
    }
    if (doc.defaultFontSize > 0) {
        defaultFont.setPointSizeF(doc.defaultFontSize / kHalfPtToPoint);
    } else {
        defaultFont.setPointSizeF(12);
    }
    document->setDefaultFont(defaultFont);

    QTextCursor cursor(document);
    int imgCounter = 0;
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
                BuildImage(cursor, element, document, imgCounter);
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
    if (doc.defaultTabStopTwips != kDefaultTabStopTwips)
        document->setProperty(UserPropMetaDefaultTabStopTwips, doc.defaultTabStopTwips);
}

} // namespace Rte

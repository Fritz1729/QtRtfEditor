#include "RtfExport.h"
#include "RtfQtConversions.h"
#include "RtfTypes.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextFrame>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextTableCell>
#include <QFont>
#include <QTextBlockFormat>
#include <QTextListFormat>
#include <QTextList>
#include <QTextOption>
#include <QString>
#include <QByteArray>
#include <QImage>
#include <QBuffer>
#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <map>
#include <sstream>

using namespace std;

namespace Rte {

namespace {

constexpr array<const char*, 4> kRowBorderSideTags = {{
    "\\trbrdrl", "\\trbrdrt", "\\trbrdrr", "\\trbrdrb"
}};

constexpr array<const char*, 4> kCellBorderSideTags = {{
    "\\clbrdrl", "\\clbrdrt", "\\clbrdrr", "\\clbrdrb"
}};

using BorderWidthGetter = double (QTextTableCellFormat::*)() const;
using BorderStyleGetter = QTextFrameFormat::BorderStyle (QTextTableCellFormat::*)() const;
using BorderBrushGetter = QBrush (QTextTableCellFormat::*)() const;

constexpr array<BorderWidthGetter, 4> kBorderWidthGetters = {{
    &QTextTableCellFormat::leftBorder, &QTextTableCellFormat::topBorder,
    &QTextTableCellFormat::rightBorder, &QTextTableCellFormat::bottomBorder,
}};

constexpr array<BorderStyleGetter, 4> kBorderStyleGetters = {{
    &QTextTableCellFormat::leftBorderStyle, &QTextTableCellFormat::topBorderStyle,
    &QTextTableCellFormat::rightBorderStyle, &QTextTableCellFormat::bottomBorderStyle,
}};

constexpr array<BorderBrushGetter, 4> kBorderBrushGetters = {{
    &QTextTableCellFormat::leftBorderBrush, &QTextTableCellFormat::topBorderBrush,
    &QTextTableCellFormat::rightBorderBrush, &QTextTableCellFormat::bottomBorderBrush,
}};

using PaddingGetter = double (QTextTableCellFormat::*)() const;

constexpr array<PaddingGetter, 4> kPaddingGetters = {{
    &QTextTableCellFormat::leftPadding, &QTextTableCellFormat::topPadding,
    &QTextTableCellFormat::rightPadding, &QTextTableCellFormat::bottomPadding,
}};

constexpr array<const char*, 4> kCellPaddingTags = {{
    "clpadl", "clpadt", "clpadr", "clpadb"
}};

static const char* UnderlineStyleTag(UnderlineStyle style) {
    switch (style) {
        case UnderlineStyle::None:       return "";
        case UnderlineStyle::Solid:      return "\\ul";
        case UnderlineStyle::Dotted:     return "\\uld";
        case UnderlineStyle::Dashed:     return "\\uldash";
        case UnderlineStyle::DashDot:    return "\\uldashd";
        case UnderlineStyle::DashDotDot: return "\\uldashdd";
        case UnderlineStyle::Double:     return "\\uldb";
        case UnderlineStyle::Thick:      return "\\ulth";
    }
    return "";
}

static void WriteConditionalFormatOff(ostringstream& out, const RtfRunFormat& cur, const RtfRunFormat& lastEmitted, bool trailingSpace) {
    const char* space = trailingSpace ? " " : "";
    if (!cur.bold && lastEmitted.bold) out << "\\b0" << space;
    if (!cur.italic && lastEmitted.italic) out << "\\i0" << space;
    if (!cur.superscript && lastEmitted.superscript) out << "\\super0" << space;
    if (!cur.subscript && lastEmitted.subscript) out << "\\sub0" << space;
    if (cur.colorIndex == 0 && lastEmitted.colorIndex != 0) out << "\\cf0" << space;
    if (cur.bgColorIndex == 0 && lastEmitted.bgColorIndex != 0) out << "\\cb0" << space;
    if (cur.underlineStyle == UnderlineStyle::None && lastEmitted.underlineStyle != UnderlineStyle::None)
        out << "\\ul0" << space;
    if (!cur.strikeOut && lastEmitted.strikeOut) out << "\\strike0" << space;
    if (cur.capitalization == Capitalization::None && lastEmitted.capitalization == Capitalization::AllCaps)
        out << "\\caps0" << space;
    if (cur.capitalization == Capitalization::None && lastEmitted.capitalization == Capitalization::SmallCaps)
        out << "\\scaps0" << space;
    if (!cur.kerning && lastEmitted.kerning) out << "\\kerning0" << space;
    if (!cur.protected_ && lastEmitted.protected_) out << "\\protect0" << space;
    if (cur.upOffset == 0 && lastEmitted.upOffset != 0) out << "\\up0" << space;
    if (cur.dnOffset == 0 && lastEmitted.dnOffset != 0) out << "\\dn0" << space;
    if (cur.ulColorIndex == 0 && lastEmitted.ulColorIndex != 0) out << "\\ulc0" << space;
}

static bool IsFormatActive(const RtfRunFormat& fmt) {
    return fmt.bold || fmt.italic || fmt.superscript || fmt.subscript ||
        fmt.colorIndex > 0 || fmt.bgColorIndex > 0 ||
        fmt.underlineStyle != UnderlineStyle::None || fmt.strikeOut ||
        fmt.capitalization != Capitalization::None || fmt.kerning || fmt.protected_ ||
        fmt.upOffset != 0 || fmt.dnOffset != 0 || fmt.langId != 0 ||
        fmt.highlightIndex != 0 || fmt.ulColorIndex != 0;
}

static bool WritePlainTextOff(ostringstream& out, const RtfRunFormat& fmt) {
    if (IsFormatActive(fmt)) {
        out << "\\plain ";
        return true;
    }
    return false;
}

static UnderlineStyle EffectiveUnderlineStyle(const QTextCharFormat& fmt) {
    if (fmt.property(UserPropUlStyle).isValid()) {
        return static_cast<UnderlineStyle>(fmt.property(UserPropUlStyle).toInt());
    }
    UnderlineStyle style = toUnderlineStyle(fmt.underlineStyle());
    if (style != UnderlineStyle::None) return style;
    if (fmt.fontUnderline()) return UnderlineStyle::Solid;
    return UnderlineStyle::None;
}

static int FindColorIndex(const vector<QColor>& colorList, const QColor& color) {
    for (int i = 0; i < static_cast<int>(colorList.size()); ++i) {
        if (colorList[static_cast<size_t>(i)] == color) return i;
    }
    return -1;
}

static string RtfEscape(const QString& text) {
    string result;
    result.reserve(static_cast<size_t>(text.size()) * 2);

    for (const QChar& ch : text) {
        ushort code = ch.unicode();
        if (code == '\\') {
            result += "\\\\";
        } else if (code == '{') {
            result += "\\{";
        } else if (code == '}') {
            result += "\\}";
        } else if (code == '\t') {
            result += "\\tab ";
        } else if (code > 127) {
            int val = static_cast<int>(code);
            result += QString("\\u%1%2").arg(val).arg('?').toStdString();
        } else {
            result += static_cast<char>(code);
        }
    }
    return result;
}

static string AlignmentToRtf(Qt::Alignment alignment) {
    if (alignment & Qt::AlignRight) return "\\qr";
    if (alignment & Qt::AlignHCenter) return "\\qc";
    if (alignment & Qt::AlignJustify) return "\\qj";
    return "";
}

static void EmitPictHeader(ostringstream& out, const QString& blipTag, qreal width, qreal height) {
    out << "{\\pict\\" << blipTag.toStdString() << " ";
    int picwgoal = static_cast<int>(width * 2.0);
    int pichgoal = static_cast<int>(height * 2.0);
    if (picwgoal > 0) out << "\\picwgoal" << picwgoal;
    if (pichgoal > 0) out << "\\pichgoal" << pichgoal;
    out << ' ';
}

static string EmitImageAsPict(const QTextDocument& doc, const QString& name,
                                      qreal width, qreal height) {
    // Extract counter from name (e.g., "rtfimage://1.png" -> "1")
    int counter = 0;
    int lastSlash = name.lastIndexOf('/');
    if (lastSlash >= 0) {
        QString mid = name.mid(lastSlash + 1);
        mid = mid.split('.').first();
        counter = mid.toInt();
    }

    // Look up stored format (jpg, png, bmp)
    QString fmtPropName = QString("rtfImageFormat://img%1").arg(counter);
    QVariant fmtVariant = doc.property(qPrintable(fmtPropName));
    QString fmt = "png";
    if (fmtVariant.canConvert<QString>()) {
        fmt = fmtVariant.toString();
    }

    // Map format string to blip tag
    QString blipTag;
    if (fmt == "jpg") blipTag = "jpegblip";
    else if (fmt == "bmp") blipTag = "dibitmap";
    else blipTag = "pngblip";

    // Path 1: Check if we have a stored hex string (byte-identical roundtrip)
    QString propName = QString("rtfPictHex://img%1").arg(counter);
    QVariant hexVariant = doc.property(qPrintable(propName));
    if (hexVariant.canConvert<QString>()) {
        QString hexStr = hexVariant.toString();
        ostringstream out;
        EmitPictHeader(out, blipTag, width, height);
        out << hexStr.toStdString();
        out << '}';
        return out.str();
    }

    // Path 2: Re-encode from binary data (e.g., pasted/dropped images)
    QImage image;
    QByteArray raw = doc.resource(QTextDocument::ImageResource, QUrl(name)).toByteArray();
    image.loadFromData(raw);
    if (image.isNull()) return "";

    QByteArray encodedData;
    QString encFormat = "PNG";
    if (fmt == "jpg") {
        encFormat = "JPEG";
    } else if (fmt == "bmp") {
        encFormat = "BMP";
    }
    QBuffer buffer(&encodedData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, qPrintable(encFormat));

    ostringstream out;
    EmitPictHeader(out, blipTag, width, height);
    out << encodedData.toHex().data();
    out << "}";
    return out.str();
}

static void EmitTwipsTag(ostringstream& out, double val, const char* tag) {
    if (val > 0) {
        int twips = PointsToHalfPtTwips(val);
        out << "\\" << tag << twips;
    }
}

static void EmitParaFormatting(ostringstream& out, const QTextBlockFormat& blockFmt) {
    out << AlignmentToRtf(blockFmt.alignment());
    EmitTwipsTag(out, blockFmt.leftMargin(), "li");
    EmitTwipsTag(out, static_cast<double>(blockFmt.indent()), "fi");
    EmitTwipsTag(out, blockFmt.rightMargin(), "ri");
    EmitTwipsTag(out, blockFmt.topMargin(), "sb");
    EmitTwipsTag(out, blockFmt.bottomMargin(), "sa");
    int lhType = blockFmt.lineHeightType();
    if (lhType == QTextBlockFormat::FixedHeight) {
        double lhVal = blockFmt.lineHeight();
        int lhTwips = PointsToHalfPtTwips(lhVal);
        if (lhTwips > 0) {
            int slMult = blockFmt.property(UserPropSlMult).toInt();
            if (slMult <= 0) slMult = 1;
            out << "\\sl" << lhTwips;
            out << "\\slmult" << slMult;
        }
    }
    const QList<QTextOption::Tab> tabs = blockFmt.tabPositions();
    for (const QTextOption::Tab& tab : tabs) {
        const int twips = PointsToHalfPtTwips(tab.position);
        switch (tab.type) {
        case QTextOption::LeftTab:
            out << "\\tx" << twips;
            break;
        case QTextOption::RightTab:
            out << "\\tqr\\tx" << twips;
            break;
        case QTextOption::CenterTab:
            out << "\\tqc\\tx" << twips;
            break;
        case QTextOption::DelimiterTab:
        default:
            out << "\\tx" << twips;
            break;
        }
    }
}

static bool NeedsParaReset(const QTextBlockFormat& last, const QTextBlockFormat& blockFmt) {
    return blockFmt.alignment() != last.alignment() ||
        blockFmt.leftMargin() != last.leftMargin() ||
        blockFmt.rightMargin() != last.rightMargin() ||
        blockFmt.topMargin() != last.topMargin() ||
        blockFmt.bottomMargin() != last.bottomMargin() ||
        blockFmt.indent() != last.indent() ||
        blockFmt.lineHeightType() != last.lineHeightType() ||
        blockFmt.lineHeight() != last.lineHeight();
}

static void EmitParaFormattingIfNeeded(ostringstream& out, const QTextBlockFormat& blockFmt,
    QTextBlockFormat& lastParaFmt, bool& lastParaFmtSet) {
    bool hasParaFormatting = blockFmt.alignment() != Qt::AlignLeft ||
        blockFmt.leftMargin() > 0 || blockFmt.indent() > 0 ||
        blockFmt.rightMargin() > 0 || blockFmt.topMargin() > 0 ||
        blockFmt.bottomMargin() > 0 ||
        blockFmt.lineHeightType() == QTextBlockFormat::FixedHeight ||
        !blockFmt.tabPositions().isEmpty();
    bool reset = lastParaFmtSet && NeedsParaReset(lastParaFmt, blockFmt);
    if (hasParaFormatting || reset) {
        out << "\\pard ";
        if (hasParaFormatting) EmitParaFormatting(out, blockFmt);
    }
    lastParaFmt = blockFmt;
    lastParaFmtSet = true;
}

struct CellBorderInfo {
    double width = 0;
    QTextFrameFormat::BorderStyle style = QTextFrameFormat::BorderStyle_None;
    int colorIdx = 0;
    bool operator==(const CellBorderInfo& other) const {
        return width == other.width && style == other.style && colorIdx == other.colorIdx;
    }
    bool operator!=(const CellBorderInfo& other) const {
        return !(*this == other);
    }
};

static void CollectColor(const QColor& col, vector<QColor>& list) {
    if (col.isValid() && col.alpha() == 255 && FindColorIndex(list, col) < 0)
        list.push_back(col);
}

static int LookupColorIndex(const QColor& col, const vector<QColor>& list) {
    if (!col.isValid() || col.alpha() != 255) return 0;
    return FindColorIndex(list, col) + 1;
}

static QColor ParseUlColor(const QTextCharFormat& fmt) {
    QString s = fmt.property(UserPropUlColorIndex).toString();
    if (s.isEmpty()) return {};
    QStringList p = s.split(',');
    if (p.size() == 3)
        return QColor(p[0].toInt(), p[1].toInt(), p[2].toInt());
    return {};
}

static void CollectBorderColor(const QBrush& brush, vector<QColor>& colorList) {
    if (brush.style() != Qt::NoBrush && brush.color().isValid()) {
        QColor col = brush.color();
        if (col.alpha() == 255 &&
            !(col.red() == 0 && col.green() == 0 && col.blue() == 0))
            CollectColor(col, colorList);
    }
}

static CellBorderInfo ReadCellBorder(const QTextTableCellFormat& cf, const vector<QColor>& colorList, TableSide side) {
    CellBorderInfo bi{};
    bi.width = (cf.*(kBorderWidthGetters.at(side)))();
    bi.style = (cf.*(kBorderStyleGetters.at(side)))();
    const QBrush& brush = (cf.*(kBorderBrushGetters.at(side)))();
    if (brush.style() != Qt::NoBrush)
        bi.colorIdx = FindColorIndex(colorList, brush.color()) + 1;
    return bi;
}

static void EmitBorderSpec(ostringstream& out, double width, QTextFrameFormat::BorderStyle style, int colorIdx) {
    if (width <= 0.0) return;
    int halfPts = PointsToHalfPtTwips(width);
    switch (style) {
        case QTextFrameFormat::BorderStyle_Solid:  out << "\\brdrs"; break;
        case QTextFrameFormat::BorderStyle_Dashed:  out << "\\brdrd"; break;
        case QTextFrameFormat::BorderStyle_None:
        case QTextFrameFormat::BorderStyle_Dotted:
        case QTextFrameFormat::BorderStyle_Double:
        case QTextFrameFormat::BorderStyle_DotDash:
        case QTextFrameFormat::BorderStyle_DotDotDash:
        case QTextFrameFormat::BorderStyle_Groove:
        case QTextFrameFormat::BorderStyle_Ridge:
        case QTextFrameFormat::BorderStyle_Inset:
        case QTextFrameFormat::BorderStyle_Outset:
        default: return;
    }
    out << "\\brdrw" << halfPts;
    if (colorIdx > 0) out << "\\brdrcf" << colorIdx;
}

static void EmitBorderSide(ostringstream& out, const char* sideTag, double width, QTextFrameFormat::BorderStyle style, int colorIdx) {
    if (width <= 0.0) return;
    out << sideTag;
    EmitBorderSpec(out, width, style, colorIdx);
}

struct BlockExportContext {
    ostringstream& out;
    const QTextDocument& document;
    const QFont& defaultFont;
    const map<string, int>& fontMap;
    const vector<QColor>& colorList;
    const vector<QColor>& bgColorList;
    const map<const QTextList*, int>& listMap;
    int defaultFontIdx;
    RtfRunFormat carriedOverFormat{};
    QTextBlockFormat lastParaFmt{};
    bool lastParaFmtSet = false;
    int lastDeff = 0;
    int lastDeftab = 180;
    int deffDeftabGroupDepth = 0;
    bool firstBlock = true;

    void ExportBlock(const QTextBlock& block, bool isTableCell, bool justAfterTable = false);
};

void BlockExportContext::ExportBlock(const QTextBlock& block, bool isTableCell, bool justAfterTable) {
    if (block.text().isEmpty()) {
        if (firstBlock || justAfterTable) {
            firstBlock = false;
            return;
        }
        firstBlock = false;
        QTextBlockFormat blockFmt = block.blockFormat();
        EmitParaFormattingIfNeeded(out, blockFmt, lastParaFmt, lastParaFmtSet);
        out << "\\par\n";
        return;
    }
    firstBlock = false;

    QTextBlockFormat blockFmt = block.blockFormat();
    bool inListGroup = false;
    if (!isTableCell && block.textList()) {
        const QTextList* list = block.textList();
        auto listIt = listMap.find(list);
        if (listIt != listMap.end()) {
            out << "{\\listid" << listIt->second << "\\listlevel0";
            inListGroup = true;
        }
    }

    EmitParaFormattingIfNeeded(out, blockFmt, lastParaFmt, lastParaFmtSet);

    // Emit \pntext group if the block has pntext content stored
    QString pntext = blockFmt.property(UserPropBlockPntextRtf).toString();
    if (!pntext.isEmpty()) {
        QString pntextFam = blockFmt.property(UserPropBlockPntextFontFamily).toString();
        string pntextRtf = pntext.toLatin1().toStdString();
        if (!pntextFam.isEmpty() && !fontMap.empty()) {
            string famStr = pntextFam.toStdString();
            auto it = fontMap.find(famStr);
            if (it != fontMap.end()) {
                string newRef = "\\f" + to_string(it->second);
                size_t pos = 0;
                while ((pos = pntextRtf.find("\\f", pos)) != string::npos) {
                    size_t endNum = pos + 2;
                    while (endNum < pntextRtf.size() && isdigit(static_cast<unsigned char>(pntextRtf[endNum])))
                        endNum++;
                    if (endNum > pos + 2) {
                        pntextRtf.replace(pos, endNum - pos, newRef);
                        pos = pos + newRef.size();
                    } else {
                        pos = endNum;
                    }
                }
            }
        }
        out << "{\\pntext" << pntextRtf << "}";
    }

    {
        int paraDeff = blockFmt.property(UserPropParaDefaultFontIndex).toInt();
        int paraDeftab = blockFmt.property(UserPropParaDefaultTabStopTwips).toInt();
        bool deffChanged = (paraDeff != lastDeff);
        bool deftabChanged = (paraDeftab != lastDeftab);
        if (deffChanged || deftabChanged) {
            out << "{";
            deffDeftabGroupDepth++;
            if (deffChanged) {
                out << "\\deff" << paraDeff;
                lastDeff = paraDeff;
            }
            if (deftabChanged) {
                out << "\\deftab" << paraDeftab;
                lastDeftab = paraDeftab;
            }
        }
    }

    out << "\n";

    bool hasImage = false;
    for (QTextBlock::iterator itImg = block.begin(); itImg != block.end(); ++itImg) {
        QTextFragment frag = itImg.fragment();
        if (frag.isValid() && frag.charFormat().isImageFormat()) {
            hasImage = true;
            break;
        }
    }

    if (hasImage) {
        QTextBlock::iterator itImg = block.begin();
        while (itImg != block.end()) {
            QTextFragment frag = itImg.fragment();
            if (frag.isValid() && frag.charFormat().isImageFormat()) {
                QTextImageFormat imgFmt = frag.charFormat().toImageFormat();
                QString name = imgFmt.name();
                qreal w = imgFmt.width();
                qreal h = imgFmt.height();
                if (w > 0 && h > 0) {
                    string pict = EmitImageAsPict(document, name, w, h);
                    if (!pict.empty()) {
                        out << pict << "\n";
                    }
                }
            }
            itImg++;
        }
        if (inListGroup) out << '}';
        out << "\\plain\\par\n";
        carriedOverFormat.fontIndex = defaultFontIdx;
    } else {
        // Pre-scan fragments to find anchor ranges
        struct AnchorRange {
            size_t fragIdxStart;
            size_t fragIdxEnd; // inclusive
            string url;
        };
        vector<AnchorRange> anchorRanges;
        QTextBlock::iterator scanIt = block.begin();
        size_t scanFragIdx = 0;
        while (scanIt != block.end()) {
            QTextFragment frag = scanIt.fragment();
            if (frag.isValid() && frag.length() > 0 && frag.charFormat().isAnchor()) {
                QString href = frag.charFormat().anchorHref();
                if (!href.isEmpty()) {
                    bool canExtend = !anchorRanges.empty() &&
                        anchorRanges.back().url == href.toStdString() &&
                        anchorRanges.back().fragIdxEnd + 1 == scanFragIdx;
                    if (canExtend) {
                        anchorRanges.back().fragIdxEnd = scanFragIdx;
                    } else {
                        anchorRanges.push_back({scanFragIdx, scanFragIdx, href.toStdString()});
                    }
                }
            }
            scanFragIdx++;
            scanIt++;
        }
        map<size_t, size_t> fragToAnchorRange;
        for (size_t r = 0; r < anchorRanges.size(); ++r) {
            for (size_t i = anchorRanges[r].fragIdxStart; i <= anchorRanges[r].fragIdxEnd; ++i) {
                fragToAnchorRange[i] = r;
            }
        }

        RtfRunFormat prev;
        RtfRunFormat lastEmitted{};
        lastEmitted.colorIndex = 0;
        lastEmitted.bgColorIndex = 0;
        lastEmitted.fontIndex = carriedOverFormat.fontIndex;
        bool firstRun = true;

        // Anchor tracking
        size_t currentAnchorRange = SIZE_MAX;
        bool inAnchor = false;

        QTextBlock::iterator it = block.begin();
        size_t fragIdx = 0;
        while (it != block.end()) {
            QTextFragment frag = it.fragment();
            if (frag.isValid() && frag.length() > 0) {
                size_t anchorRange =
                    fragToAnchorRange.count(fragIdx) ? fragToAnchorRange[fragIdx] : SIZE_MAX;

                // Emit field opening if entering an anchor range
                if (anchorRange != SIZE_MAX && !inAnchor &&
                    anchorRanges[anchorRange].fragIdxStart == fragIdx) {
                    out << "{\\field{\\*\\fldinst HYPERLINK \""
                        << anchorRanges[anchorRange].url << "\"}{\\*\\fldrslt";
                    inAnchor = true;
                    currentAnchorRange = anchorRange;
                }
            }
            if (!frag.isValid() || frag.length() == 0) { fragIdx++; it++; continue; }

            QTextCharFormat charFmt = frag.charFormat();

            RtfRunFormat cur;
            qreal ptSize = charFmt.fontPointSize();
            if (ptSize <= 0) ptSize = defaultFont.pointSizeF();
            cur.fontSize = static_cast<int>(ptSize * 2);

            QString fam;
            QStringList fFams = charFmt.fontFamilies().toStringList();
            fam = fFams.isEmpty() ? QString() : fFams.first();
            if (fam.isEmpty()) fam = defaultFont.family();
            auto fIt = fontMap.find(fam.toStdString());
            cur.fontIndex = (fIt != fontMap.end()) ? fIt->second : defaultFontIdx;

            cur.colorIndex = LookupColorIndex(charFmt.foreground().color(), colorList);
            if (charFmt.background().style() != Qt::NoBrush)
                cur.bgColorIndex = LookupColorIndex(charFmt.background().color(), bgColorList);
            else
                cur.bgColorIndex = 0;

            cur.bold = charFmt.fontWeight() >= 700;
            cur.italic = charFmt.fontItalic();
            cur.strikeOut = charFmt.fontStrikeOut();
            cur.superscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSuperScript;
            cur.subscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSubScript;
            cur.underlineStyle = EffectiveUnderlineStyle(charFmt);
            cur.capitalization = toCapitalization(charFmt.fontCapitalization());
            cur.kerning = charFmt.fontKerning();
            cur.protected_ = charFmt.property(UserPropProtect).toBool();
            cur.upOffset = charFmt.property(UserPropUpOffset).toInt();
            cur.dnOffset = charFmt.property(UserPropDnOffset).toInt();
            cur.langId = charFmt.property(UserPropLangId).toInt();
            cur.highlightIndex = charFmt.property(UserPropHighlightIndex).toInt();
            QColor ulCol = ParseUlColor(charFmt);
            if (ulCol.isValid())
                cur.ulColorIndex = LookupColorIndex(ulCol, colorList);
            qreal spacing = charFmt.fontLetterSpacing();
            if (spacing > 0) {
                cur.expnd = lround(spacing * 20.0 / ptSize);
            }

            if (firstRun || cur != prev) {
                if (!firstRun) {
                    WriteConditionalFormatOff(out, cur, lastEmitted, true);
                }

                if (firstRun || cur.fontSize != lastEmitted.fontSize)
                    out << "\\fs" << cur.fontSize << ' ';
                if (cur.fontIndex != lastEmitted.fontIndex)
                    out << "\\f" << cur.fontIndex << ' ';
                if (cur.colorIndex != lastEmitted.colorIndex)
                    out << "\\cf" << cur.colorIndex << ' ';
                if (cur.bgColorIndex != lastEmitted.bgColorIndex)
                    out << "\\cb" << cur.bgColorIndex << ' ';
                if (cur.bold && !lastEmitted.bold) out << "\\b ";
                if (cur.italic && !lastEmitted.italic) out << "\\i ";
                if (cur.strikeOut && !lastEmitted.strikeOut) out << "\\strike ";
                if (cur.underlineStyle != UnderlineStyle::None && cur.underlineStyle != lastEmitted.underlineStyle)
                    out << UnderlineStyleTag(cur.underlineStyle) << ' ';
                if (cur.superscript && !lastEmitted.superscript) out << "\\super ";
                if (cur.subscript && !lastEmitted.subscript) out << "\\sub ";
                if (cur.capitalization != Capitalization::None && cur.capitalization != lastEmitted.capitalization) {
                    if (cur.capitalization == Capitalization::AllCaps) out << "\\caps ";
                    if (cur.capitalization == Capitalization::SmallCaps) out << "\\scaps ";
                }
                if (cur.kerning && !lastEmitted.kerning) out << "\\kerning ";
                if (cur.expnd != lastEmitted.expnd) out << "\\expnd" << cur.expnd << ' ';
                if (cur.protected_ && !lastEmitted.protected_) out << "\\protect ";
                if (cur.upOffset != lastEmitted.upOffset) out << "\\up" << cur.upOffset << ' ';
                if (cur.dnOffset != lastEmitted.dnOffset) out << "\\dn" << cur.dnOffset << ' ';
                if (cur.langId != lastEmitted.langId) out << "\\lang" << cur.langId << ' ';
                if (cur.highlightIndex != lastEmitted.highlightIndex) out << "\\highlight" << cur.highlightIndex << ' ';
                if (cur.ulColorIndex != lastEmitted.ulColorIndex) out << "\\ulc" << cur.ulColorIndex << ' ';

                lastEmitted = cur;
            }

            out << RtfEscape(frag.text());
            prev = cur;
            firstRun = false;

            if (inAnchor && currentAnchorRange != SIZE_MAX &&
                fragIdx == anchorRanges[currentAnchorRange].fragIdxEnd) {
                out << "}}";
                inAnchor = false;
                currentAnchorRange = SIZE_MAX;
            }

            fragIdx++;
            it++;
        }

        if (!firstRun) {
            bool plainEmitted = WritePlainTextOff(out, lastEmitted);
            if (plainEmitted) {
                carriedOverFormat.fontIndex = defaultFontIdx;
            } else {
                carriedOverFormat.fontIndex = lastEmitted.fontIndex;
            }
        }
        if (inListGroup) out << '}';
        out << "\\par";
        for (int i = 0; i < deffDeftabGroupDepth; i++)
            out << '}';
        deffDeftabGroupDepth = 0;
        out << "\n";
    }
}

} // namespace

string ExportRtf(const QTextDocument& document) {
    ostringstream out;

    qDebug() << "[export] ExportRtf start, collecting fonts";
    QFont defaultFont = document.defaultFont();

    // Read default tab stop twips (stored during import)
    int defaultTabStopTwips = document.property(UserPropMetaDefaultTabStopTwips).toInt();
    if (defaultTabStopTwips <= 0) defaultTabStopTwips = 180;  // RTF spec default

    map<string, int> fontMap;
    vector<QColor> colorList;
    vector<QColor> bgColorList;
    map<const QTextList*, int> listMap;
    map<const QTextList*, ListStyle> listStyleMap;
    int defaultFontIdx = 0;
    int listIdCounter = 1;
    int idx = 0;
    string defFamily = defaultFont.family().toStdString();
    fontMap[defFamily] = idx;
    defaultFontIdx = idx;
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            // Collect lists
            if (block.textList()) {
                const QTextList* list = block.textList();
                if (listMap.find(list) == listMap.end()) {
                    listMap[list] = listIdCounter++;
                    listStyleMap[list] = QtListStyleToRtf(list->format().style());
                }
            }

            QTextBlock::iterator it = block.begin();
            while (it != block.end()) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid()) { it++; continue; }
                QStringList fFams = frag.charFormat().fontFamilies().toStringList();
                QString fam = fFams.isEmpty() ? QString() : fFams.first();
                if (!fam.isEmpty() && fontMap.find(fam.toStdString()) == fontMap.end()) {
                    fontMap[fam.toStdString()] = ++idx;
                }
                QColor fg = frag.charFormat().foreground().color();
                if (!(fg.red() == 0 && fg.green() == 0 && fg.blue() == 0))
                    CollectColor(fg, colorList);
                QBrush bgBrush = frag.charFormat().background();
                if (bgBrush.style() != Qt::NoBrush)
                    CollectColor(bgBrush.color(), bgColorList);
                QColor ulCol = ParseUlColor(frag.charFormat());
                if (ulCol.isValid())
                    CollectColor(ulCol, colorList);
                it++;
            }

            // Collect fonts from pntext content (not in document fragments)
            QString pntextFam = block.blockFormat().property(UserPropBlockPntextFontFamily).toString();
            if (!pntextFam.isEmpty() && fontMap.find(pntextFam.toStdString()) == fontMap.end()) {
                fontMap[pntextFam.toStdString()] = ++idx;
            }
        }
    qDebug() << "[export] Font collection done, count=" << fontMap.size();

    out << "{\\rtf1\\ansi\\deff" << defaultFontIdx;
    if (defaultTabStopTwips != 180)
        out << "\\deftab" << defaultTabStopTwips;

    if (!colorList.empty() || !bgColorList.empty()) {
        out << "{\\colortbl ;";
        for (const QColor& color : colorList) {
            out << "\\red" << color.red()
                << "\\green" << color.green()
                << "\\blue" << color.blue() << ";";
        }
        for (const QColor& color : bgColorList) {
            out << "\\red" << color.red()
                << "\\green" << color.green()
                << "\\blue" << color.blue() << ";";
        }
        out << "}";
    }

    if (!listMap.empty()) {
        out << "{\\listtable";
        for (const auto& [list, id] : listMap) {
            out << "\\list\\listid" << id
                << "\\liststylebulletsimple\\liststyletype"
                << static_cast<int>(listStyleMap[list] == ListStyle::Number ? 3 :
                    listStyleMap[list] == ListStyle::Letter ? 6 :
                    listStyleMap[list] == ListStyle::Roman ? 4 : 1);
        }
        out << "}";
    }

    // Build fonttbl into a separate buffer; emit only if non-default fonts exist
    ostringstream fonttblOut;
    fonttblOut << "{\\fonttbl";
    map<string, string> typeHints = {
        {"arial", "\\fswiss"},
        {"times new roman", "\\froman"},
        {"courier new", "\\fmodern"},
    };
    for (int i = 0; i < static_cast<int>(fontMap.size()); ++i) {
        auto fontIt = find_if(fontMap.begin(), fontMap.end(),
            [i](const auto& p) { return p.second == i; });
        if (fontIt == fontMap.end()) continue;
        const string& family = fontIt->first;
        int fontIdx = fontIt->second;
        fonttblOut << "{\\f" << fontIdx;
        string lower = family;
        transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return tolower(c); });
        auto it = typeHints.find(lower);
        if (it != typeHints.end()) {
            fonttblOut << it->second;
        } else {
            fonttblOut << "\\fnil";
        }
        fonttblOut << "\\fcharset" << (lower == "symbol" ? 2 : 0) << " " << family << ";}";
    }
    fonttblOut << "}";

    // Emit fonttbl only if it contains fonts other than Qt's default
    qDebug() << "[export] Before QFont().family()";
    string defaultFamily = QFont().family().toStdString();
    qDebug() << "[export] QFont().family() returned:" << defaultFamily.c_str();
    bool hasNonDefaultFont = false;
    for (const auto& [family, _] : fontMap) {
        if (family != defaultFamily) {
            hasNonDefaultFont = true;
            break;
        }
    }
    if (hasNonDefaultFont) {
        out << fonttblOut.str();
    }

    // Emit metadata (only if present in imported document)
    QVariant langIdVar = document.property(UserPropMetaDefaultLangId);
    if (langIdVar.isValid()) {
        out << "\\deflang" << langIdVar.toInt();
    }
    QVariant viewKindVar = document.property(UserPropMetaViewKind);
    if (viewKindVar.isValid()) {
        out << "\\viewkind" << viewKindVar.toInt();
    }
    QVariant ucVar = document.property(UserPropMetaUcByteCount);
    if (ucVar.isValid()) {
        int ucVal = ucVar.toInt();
        out << "\\uc" << ucVal;
    }

    // Content export — iterate root frame to handle tables and paragraphs
    // Carry over persistent RTF format state (font, color, bgColor) across blocks.
    // RTF formatting is stream-global — \par does not reset it.
    // GCC's IPA produces false-positive -Wmaybe-uninitialized for the nested
    // string member in carriedOverFormat — suppress around this declaration.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    BlockExportContext exportCtx{
        out, document, defaultFont, fontMap, colorList, bgColorList, listMap,
        defaultFontIdx, {}, QTextBlockFormat{}, false, defaultFontIdx, defaultTabStopTwips, 0, true
    };
#pragma GCC diagnostic pop

    QTextFrame* rootFrame = document.rootFrame();
    bool justFinishedTable = false;
    for (QTextFrame::iterator frameIt = rootFrame->begin(); frameIt != rootFrame->end(); ++frameIt) {
        QTextFrame* currentFrame = frameIt.currentFrame();
        QTextTable* table = qobject_cast<QTextTable*>(currentFrame);
        if (table) {
            // === TABLE EXPORT ===
            QTextTableFormat tableFmt = table->format();
            QVector<QTextLength> constraints = tableFmt.columnWidthConstraints();
            int colCount = table->columns();
            int rowCount = table->rows();

            // Compute cumulative \cellx positions
            vector<int> cellxPositions;
            int cumulative = 0;
            for (int c = 0; c < colCount; ++c) {
                double width = constraints[c].value(QTextLength::FixedLength);
                int widthTwips = PointsToTwips(width);
                cumulative += widthTwips;
                cellxPositions.push_back(cumulative);
            }

    // Collect border colors from all cells
    for (int r = 0; r < rowCount; ++r) {
        for (int c = 0; c < colCount; ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            if (!cell.isValid()) continue;
            QTextTableCellFormat cf(cell.format().toTableCellFormat());
            CollectBorderColor(cf.leftBorderBrush(), colorList);
            CollectBorderColor(cf.topBorderBrush(), colorList);
            CollectBorderColor(cf.rightBorderBrush(), colorList);
            CollectBorderColor(cf.bottomBorderBrush(), colorList);
        }
    }

            for (int r = 0; r < rowCount; ++r) {
                out << "{\\trowd ";
                for (int pos : cellxPositions) {
                    out << "\\cellx" << pos;
                }

                // Table alignment (only on first row)
                if (r == 0) {
                    Qt::Alignment tAlign = tableFmt.alignment();
                    if (tAlign & Qt::AlignHCenter) out << "\\trqc";
                    else if (tAlign & Qt::AlignRight) out << "\\trqr";
                    else out << "\\trql";
                }

                out << "\n";

                // Pre-read all cell border info for this row to detect uniform sides
                vector<array<CellBorderInfo, 4>> rowCellBorders(
                    static_cast<size_t>(colCount));

                for (int c = 0; c < colCount; ++c) {
                    QTextTableCell cell = table->cellAt(r, c);
                    if (cell.isValid()) {
                        QTextTableCellFormat cf(cell.format().toTableCellFormat());
                        for (TableSide side : kTableSides) {
                            rowCellBorders[static_cast<size_t>(c)][side] =
                                ReadCellBorder(cf, colorList, side);
                        }
                    }
                }

                // Determine which sides have uniform borders across all cells
                bool uniformSide[4] = {true, true, true, true};
                CellBorderInfo uniformBorder[4]{};
                for (TableSide side : kTableSides) {
                    uniformBorder[side] = rowCellBorders[0][side];
                    for (int c = 1; c < colCount; ++c) {
                        if (rowCellBorders[static_cast<size_t>(c)][side] !=
                            uniformBorder[side]) {
                            uniformSide[side] = false;
                            break;
                        }
                    }
                    // Not uniform if no border (we don't emit \trbrdrl\brdrn)
                    if (uniformBorder[side].width <= 0.0) uniformSide[side] = false;
                }

                // Emit row-level borders for uniform sides
                for (TableSide side : kTableSides) {
                    if (uniformSide[side]) EmitBorderSide(out, kRowBorderSideTags.at(side), uniformBorder[side].width, uniformBorder[side].style, uniformBorder[side].colorIdx);
                }

                for (int c = 0; c < colCount; ++c) {
                    QTextTableCell cell = table->cellAt(r, c);
                    if (!cell.isValid()) {
                        out << "\\intbl \\cell";
                        continue;
                    }

                    QTextTableCellFormat cf(cell.format().toTableCellFormat());

                    // Emit cell padding
                    for (TableSide side : kTableSides) {
                        EmitTwipsTag(out, (cf.*(kPaddingGetters[side]))(), kCellPaddingTags[side]);
                    }

                    // Emit cell borders (skip sides that are emitted as row borders)
                    for (TableSide side : kTableSides) {
                        if (uniformSide[side]) continue;
                        CellBorderInfo& bi = rowCellBorders[static_cast<size_t>(c)][side];
                        EmitBorderSide(out, kCellBorderSideTags.at(side), bi.width, bi.style, bi.colorIdx);
                    }

                    // Iterate over blocks within the cell
                    QTextBlock cellBlock = cell.firstCursorPosition().block();
                    int cellEndPos = cell.lastPosition();
                    bool first = true;
                    while (cellBlock.isValid() && cellBlock.position() < cellEndPos) {
                        if (!cellBlock.text().isEmpty()) {
                            if (first) {
                                out << "\\intbl";
                                first = false;
                            }
                              exportCtx.ExportBlock(cellBlock, true);
                        }
                        cellBlock = cellBlock.next();
                    }
                    if (first) out << "\\intbl";
                    out << "\\cell";
                }
                out << "\\row}";
            }
            justFinishedTable = true;
        } else {
            QTextBlock block = frameIt.currentBlock();
            if (block.isValid()) {
                exportCtx.ExportBlock(block, false, justFinishedTable);
            }
            justFinishedTable = false;
        }
    }

    out << "}";
    return out.str();
}

string ExportHtml(const QTextDocument& document) {
    QString html = document.toHtml();
    return html.toStdString();
}

} // namespace Rte

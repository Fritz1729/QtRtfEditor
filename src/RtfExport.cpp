#include "RtfExport.h"
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
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <sstream>

namespace Rte {

namespace {

static const char* UnderlineStyleTag(UnderlineStyle style) {
    switch (style) {
        case UnderlineStyle::None:       return "";
        case UnderlineStyle::Solid:      return "\\ul";
        case UnderlineStyle::Dotted:     return "\\uld";
        case UnderlineStyle::Dashed:     return "\\uldash";
        case UnderlineStyle::Double:     return "\\uldb";
        case UnderlineStyle::Thick:      return "\\ulth";
    }
    return "";
}

static void WriteConditionalFormatOff(std::ostringstream& out, const RtfRunFormat& cur, const RtfRunFormat& lastEmitted, bool trailingSpace) {
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
}

static void WriteFormatOff(std::ostringstream& out, const RtfRunFormat& fmt, const RtfRunFormat& lastEmitted, bool trailingSpace) {
    const char* space = trailingSpace ? " " : "";
    if (fmt.underlineStyle != UnderlineStyle::None && lastEmitted.underlineStyle == UnderlineStyle::None)
        out << "\\ul0" << space;
    if (fmt.strikeOut && !lastEmitted.strikeOut) out << "\\strike0" << space;
    if (fmt.capitalization == Capitalization::AllCaps && lastEmitted.capitalization != Capitalization::AllCaps)
        out << "\\caps0" << space;
    if (fmt.capitalization == Capitalization::SmallCaps && lastEmitted.capitalization != Capitalization::SmallCaps)
        out << "\\scaps0" << space;
}

static bool IsFormatActive(const RtfRunFormat& fmt) {
    return fmt.bold || fmt.italic || fmt.superscript || fmt.subscript ||
        fmt.colorIndex > 0 || fmt.bgColorIndex > 0 ||
        fmt.underlineStyle != UnderlineStyle::None || fmt.strikeOut ||
        fmt.capitalization != Capitalization::None || fmt.kerning || fmt.protected_ ||
        fmt.upOffset != 0 || fmt.dnOffset != 0;
}

static void WritePlainTextOff(std::ostringstream& out, const RtfRunFormat& fmt) {
    if (IsFormatActive(fmt)) out << "\\plain ";
}

static UnderlineStyle EffectiveUnderlineStyle(const QTextCharFormat& fmt) {
    UnderlineStyle style = toUnderlineStyle(fmt.underlineStyle());
    if (style != UnderlineStyle::None) return style;
    if (fmt.fontUnderline()) return UnderlineStyle::Solid;
    return UnderlineStyle::None;
}

int FindColorIndex(const std::vector<QColor>& colorList, const QColor& color) {
    for (int i = 0; i < static_cast<int>(colorList.size()); ++i) {
        if (colorList[i] == color) return i;
    }
    return -1;
}

std::string RtfEscape(const QString& text) {
    std::string result;
    result.reserve(text.size() * 2);

    for (QChar ch : text) {
        ushort code = ch.unicode();
        switch (static_cast<char>(code)) {
            case '\\':  result += "\\\\"; break;
            case '{':   result += "\\{"; break;
            case '}':   result += "\\}"; break;
            case '\t':  result += "\\t"; break;
            default:
                if (code > 127) {
                    int val = static_cast<int>(code);
                    char fallback = (code < 128) ? static_cast<char>(code) : '?';
                    result += QString("\\u%1%2").arg(val).arg(fallback).toStdString();
                } else {
                    result += static_cast<char>(code);
                }
                break;
        }
    }
    return result;
}

std::string AlignmentToRtf(Qt::Alignment alignment) {
    if (alignment & Qt::AlignRight) return "\\qr";
    if (alignment & Qt::AlignHCenter) return "\\qc";
    if (alignment & Qt::AlignJustify) return "\\qj";
    return "";
}

static std::string EmitImageAsPict(const QTextDocument& doc, const QString& name,
                                      qreal width, qreal height) {
    // Extract counter from name (e.g., "rtfimage://1.png" -> "1")
    int counter = 0;
    {
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) {
            QString mid = name.mid(lastSlash + 1);
            mid = mid.split('.').first();
            counter = mid.toInt();
        }
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
        std::ostringstream out;
        out << "{\\pict\\" << blipTag.toStdString() << " ";
        int picwgoal = static_cast<int>(width * 2.0);
        int pichgoal = static_cast<int>(height * 2.0);
        if (picwgoal > 0) out << "\\picwgoal" << picwgoal;
        if (pichgoal > 0) out << "\\pichgoal" << pichgoal;
        out << ' ';
        out << hexStr.toStdString();
        out << '}';
        return out.str();
    }

    // Path 2: Re-encode from binary data (e.g., pasted/dropped images)
    QImage image;
    {
        QByteArray raw = doc.resource(QTextDocument::ImageResource, QUrl(name)).toByteArray();
        image.loadFromData(raw);
    }
    if (image.isNull()) return "";

    QByteArray encodedData;
    QString encFormat = "PNG";
    if (fmt == "jpg") {
        encFormat = "JPEG";
    } else if (fmt == "bmp") {
        encFormat = "BMP";
    }
    {
        QBuffer buffer(&encodedData);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, qPrintable(encFormat));
    }

    std::ostringstream out;
    out << "{\\pict\\" << blipTag.toStdString() << " ";

    int picwgoal = static_cast<int>(width * 2.0);
    int pichgoal = static_cast<int>(height * 2.0);
    if (picwgoal > 0) out << "\\picwgoal" << picwgoal;
    if (pichgoal > 0) out << "\\pichgoal" << pichgoal;

    out << encodedData.toHex().data();
    out << "}";
    return out.str();
}

} // namespace

std::string ExportRtf(const QTextDocument& document) {
    std::ostringstream out;

    QFont defaultFont = document.defaultFont();

    std::map<std::string, int> fontMap;
    std::vector<QColor> colorList;
    std::vector<QColor> bgColorList;
    std::map<const QTextList*, int> listMap;
    std::map<const QTextList*, ListStyle> listStyleMap;
    int defaultFontIdx = 0;
    int listIdCounter = 1;
    {
        int idx = 0;
        std::string defFamily = defaultFont.family().toStdString();
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
                auto collectColor = [&](const QColor& col, std::vector<QColor>& list) {
                    if (col.isValid() && col.alpha() == 255 && FindColorIndex(list, col) < 0)
                        list.push_back(col);
                };
                {
                    QColor fg = frag.charFormat().foreground().color();
                    if (!(fg.red() == 0 && fg.green() == 0 && fg.blue() == 0))
                        collectColor(fg, colorList);
                }
                {
                    QBrush bgBrush = frag.charFormat().background();
                    if (bgBrush.style() != Qt::NoBrush)
                        collectColor(bgBrush.color(), bgColorList);
                }
                it++;
            }
        }
    }

    out << "{\\rtf1\\ansi\\deff" << defaultFontIdx;

    if (!colorList.empty() || !bgColorList.empty()) {
        out << "{\\colortbl ;";
        for (const auto& color : colorList) {
            out << "\\red" << color.red()
                << "\\green" << color.green()
                << "\\blue" << color.blue() << ";";
        }
        for (const auto& color : bgColorList) {
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

    out << "{\\fonttbl";
    std::map<std::string, std::string> typeHints = {
        {"arial", "\\fswiss"},
        {"times new roman", "\\froman"},
        {"courier new", "\\fmodern"},
    };
    for (int i = 0; i < static_cast<int>(fontMap.size()); ++i) {
        auto fontIt = std::find_if(fontMap.begin(), fontMap.end(),
            [i](const auto& p) { return p.second == i; });
        if (fontIt == fontMap.end()) continue;
        const std::string& family = fontIt->first;
        int idx = fontIt->second;
        out << "{\\f" << idx;
        std::string lower = family;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        auto it = typeHints.find(lower);
        if (it != typeHints.end()) {
            out << it->second;
        } else {
            out << "\\fnil";
        }
        out << "\\fcharset0 " << family << ";}";
    }
    out << "}";

    // Emit metadata (only if present in imported document)
    {
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
    }

    // Content export — iterate root frame to handle tables and paragraphs
    auto exportBlock = [&](const QTextBlock& block, bool isTableCell) {
        QString text = block.text();
        bool hasText = !text.trimmed().isEmpty();
        if (!hasText) {
            return;
        }

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

        // Emit \pard if any paragraph formatting is non-default
        {
            bool hasParaFormatting = blockFmt.alignment() != Qt::AlignLeft ||
                blockFmt.leftMargin() > 0 || blockFmt.indent() > 0 ||
                blockFmt.rightMargin() > 0 || blockFmt.topMargin() > 0 ||
                blockFmt.bottomMargin() > 0 ||
                blockFmt.lineHeightType() == QTextBlockFormat::FixedHeight ||
                !blockFmt.tabPositions().isEmpty();
            if (hasParaFormatting) out << "\\pard ";
        }

        out << AlignmentToRtf(blockFmt.alignment());

        auto emitIfPositive = [&](double val, const char* tag) {
            if (val > 0) {
                int halfPoints = static_cast<int>(val * 2.0);
                out << "\\" << tag << halfPoints;
            }
        };

        emitIfPositive(blockFmt.leftMargin(), "li");
        emitIfPositive(static_cast<double>(blockFmt.indent()), "fi");
        emitIfPositive(blockFmt.rightMargin(), "ri");
        emitIfPositive(blockFmt.topMargin(), "sb");
        emitIfPositive(blockFmt.bottomMargin(), "sa");

        int lhType = blockFmt.lineHeightType();
        if (lhType == QTextBlockFormat::FixedHeight) {
            double lhVal = blockFmt.lineHeight();
            int lhTwips = static_cast<int>(lhVal * 2.0);
            if (lhTwips > 0) {
                out << "\\sl" << lhTwips;
                out << "\\slmult1";
            }
        }

        {
            const QList<QTextOption::Tab> tabs = blockFmt.tabPositions();
            for (const QTextOption::Tab& tab : tabs) {
                switch (tab.type) {
                case QTextOption::LeftTab:
                    out << "\\tx" << static_cast<int>(tab.position * 2.0);
                    break;
                case QTextOption::RightTab:
                    out << "\\tqr\\tx" << static_cast<int>(tab.position * 2.0);
                    break;
                case QTextOption::CenterTab:
                    out << "\\tqc\\tx" << static_cast<int>(tab.position * 2.0);
                    break;
                default:
                    out << "\\tx" << static_cast<int>(tab.position * 2.0);
                    break;
                }
            }
        }

        out << "\n";

        // Check for images in this block
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
                        std::string pict = EmitImageAsPict(document, name, w, h);
                        if (!pict.empty()) {
                            out << pict << "\n";
                        }
                    }
                }
                itImg++;
            }
            if (inListGroup) out << '}';
            out << "\\plain\\par\n";
        } else {
            RtfRunFormat prev;
            RtfRunFormat lastEmitted{};
            lastEmitted.colorIndex = 0;
            lastEmitted.bgColorIndex = 0;
            lastEmitted.fontIndex = defaultFontIdx;
            bool firstRun = true;

            QTextBlock::iterator it = block.begin();
            while (it != block.end()) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid() || frag.length() == 0) { it++; continue; }

                QTextCharFormat charFmt = frag.charFormat();

                RtfRunFormat cur;
                qreal ptSize = charFmt.fontPointSize();
                if (ptSize <= 0) ptSize = defaultFont.pointSizeF();
                cur.fontSize = static_cast<int>(ptSize * 2);

                QString fam;
                {
                    QStringList fFams = charFmt.fontFamilies().toStringList();
                    fam = fFams.isEmpty() ? QString() : fFams.first();
                }
                if (fam.isEmpty()) fam = defaultFont.family();
                auto fIt = fontMap.find(fam.toStdString());
                cur.fontIndex = (fIt != fontMap.end()) ? fIt->second : defaultFontIdx;

                auto lookupColor = [&](const QColor& col, std::vector<QColor>& list) -> int {
                    if (col.isValid() && col.alpha() == 255) {
                        int idx = FindColorIndex(list, col);
                        if (idx >= 0) return idx + 1;
                    }
                    return 0;
                };
                cur.colorIndex = lookupColor(charFmt.foreground().color(), colorList);
                {
                    QBrush bgBrush = charFmt.background();
                    if (bgBrush.style() != Qt::NoBrush)
                        cur.bgColorIndex = lookupColor(bgBrush.color(), bgColorList);
                    else
                        cur.bgColorIndex = 0;
                }

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
                {
                    qreal spacing = charFmt.fontLetterSpacing();
                    if (spacing > 0) {
                        cur.expnd = static_cast<int>(spacing * 20.0 / ptSize + 0.5);
                    }
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

                    lastEmitted = cur;
                }

                out << RtfEscape(frag.text());
                prev = cur;
                firstRun = false;
                it++;
            }

            if (!firstRun) {
                if (inListGroup) out << '}';
                WritePlainTextOff(out, lastEmitted);
                out << "\\par\n";
            } else {
                if (inListGroup) out << '}';
                out << "\\par\n";
            }
        }
    };

    QTextFrame* rootFrame = document.rootFrame();
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
            std::vector<int> cellxPositions;
            int cumulative = 0;
            for (int c = 0; c < colCount; ++c) {
                double width = constraints[c].value(QTextLength::FixedLength);
                int widthTwips = static_cast<int>(width * 20.0);
                cumulative += widthTwips;
                cellxPositions.push_back(cumulative);
            }

            // Collect border colors from all cells
            {
                for (int r = 0; r < rowCount; ++r) {
                    for (int c = 0; c < colCount; ++c) {
                        QTextTableCell cell = table->cellAt(r, c);
                        if (!cell.isValid()) continue;
                        QTextTableCellFormat cf(cell.format().toTableCellFormat());
                        auto tryCollect = [&](const QBrush& brush) {
                            if (brush.style() != Qt::NoBrush && brush.color().isValid()) {
                                QColor col = brush.color();
                                if (col.alpha() == 255 &&
                                    !(col.red() == 0 && col.green() == 0 && col.blue() == 0) &&
                                    FindColorIndex(colorList, col) < 0)
                                    colorList.push_back(col);
                            }
                        };
                        tryCollect(cf.leftBorderBrush());
                        tryCollect(cf.topBorderBrush());
                        tryCollect(cf.rightBorderBrush());
                        tryCollect(cf.bottomBorderBrush());
                    }
                }
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

            auto readCellBorder = [&](const QTextTableCellFormat& cf, int side) -> CellBorderInfo {
                CellBorderInfo bi{};
                switch (side) {
                    case 0:
                        bi.width = cf.leftBorder();
                        bi.style = cf.leftBorderStyle();
                        if (cf.leftBorderBrush().style() != Qt::NoBrush)
                            bi.colorIdx = FindColorIndex(colorList, cf.leftBorderBrush().color()) + 1;
                        break;
                    case 1:
                        bi.width = cf.topBorder();
                        bi.style = cf.topBorderStyle();
                        if (cf.topBorderBrush().style() != Qt::NoBrush)
                            bi.colorIdx = FindColorIndex(colorList, cf.topBorderBrush().color()) + 1;
                        break;
                    case 2:
                        bi.width = cf.rightBorder();
                        bi.style = cf.rightBorderStyle();
                        if (cf.rightBorderBrush().style() != Qt::NoBrush)
                            bi.colorIdx = FindColorIndex(colorList, cf.rightBorderBrush().color()) + 1;
                        break;
                    default:
                        bi.width = cf.bottomBorder();
                        bi.style = cf.bottomBorderStyle();
                        if (cf.bottomBorderBrush().style() != Qt::NoBrush)
                            bi.colorIdx = FindColorIndex(colorList, cf.bottomBorderBrush().color()) + 1;
                        break;
                }
                return bi;
            };

            auto emitBorderSpec = [&](std::ostringstream& ostr, double width, QTextFrameFormat::BorderStyle style, int colorIdx) {
                if (width <= 0.0) return;
                int halfPts = static_cast<int>(width * 2.0 + 0.5);
                switch (style) {
                    case QTextFrameFormat::BorderStyle_Solid:  ostr << "\\brdrs"; break;
                    case QTextFrameFormat::BorderStyle_Dashed:  ostr << "\\brdrd"; break;
                    default: return;
                }
                ostr << "\\brdrw" << halfPts;
                if (colorIdx > 0) ostr << "\\brdrcf" << colorIdx;
            };

            auto emitCellBorderSide = [&](std::ostringstream& ostr, const char* sideTag, double width, QTextFrameFormat::BorderStyle style, int colorIdx) {
                if (width <= 0.0) return;
                ostr << sideTag;
                emitBorderSpec(ostr, width, style, colorIdx);
            };

            auto emitRowBorderSide = [&](std::ostringstream& ostr, const char* sideTag, double width, QTextFrameFormat::BorderStyle style, int colorIdx) {
                if (width <= 0.0) return;
                ostr << sideTag;
                emitBorderSpec(ostr, width, style, colorIdx);
            };

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
                std::vector<std::array<CellBorderInfo, 4>> rowCellBorders(colCount);

                for (int c = 0; c < colCount; ++c) {
                    QTextTableCell cell = table->cellAt(r, c);
                    if (cell.isValid()) {
                        QTextTableCellFormat cf(cell.format().toTableCellFormat());
                        for (int side = 0; side < 4; ++side) {
                            rowCellBorders[c][side] = readCellBorder(cf, side);
                        }
                    }
                }

                // Determine which sides have uniform borders across all cells
                bool uniformSide[4] = {true, true, true, true};
                CellBorderInfo uniformBorder[4]{};
                for (int side = 0; side < 4; ++side) {
                    uniformBorder[side] = rowCellBorders[0][side];
                    for (int c = 1; c < colCount; ++c) {
                        if (rowCellBorders[c][side] != uniformBorder[side]) {
                            uniformSide[side] = false;
                            break;
                        }
                    }
                    // Not uniform if no border (we don't emit \trbrdrl\brdrn)
                    if (uniformBorder[side].width <= 0.0) uniformSide[side] = false;
                }

                // Emit row-level borders for uniform sides
                if (uniformSide[0]) emitRowBorderSide(out, "\\trbrdrl", uniformBorder[0].width, uniformBorder[0].style, uniformBorder[0].colorIdx);
                if (uniformSide[1]) emitRowBorderSide(out, "\\trbrdrt", uniformBorder[1].width, uniformBorder[1].style, uniformBorder[1].colorIdx);
                if (uniformSide[2]) emitRowBorderSide(out, "\\trbrdrr", uniformBorder[2].width, uniformBorder[2].style, uniformBorder[2].colorIdx);
                if (uniformSide[3]) emitRowBorderSide(out, "\\trbrdrb", uniformBorder[3].width, uniformBorder[3].style, uniformBorder[3].colorIdx);

                for (int c = 0; c < colCount; ++c) {
                    QTextTableCell cell = table->cellAt(r, c);
                    if (!cell.isValid()) {
                        out << "\\intbl \\cell";
                        continue;
                    }

                    QTextTableCellFormat cf(cell.format().toTableCellFormat());

                    // Emit cell padding
                    auto emitIfPositive = [&](double val, const char* tag) {
                        if (val > 0) {
                            int halfPts = static_cast<int>(val * 2.0 + 0.5);
                            out << "\\" << tag << halfPts;
                        }
                    };
                    emitIfPositive(cf.leftPadding(), "clpadl");
                    emitIfPositive(cf.rightPadding(), "clpadr");
                    emitIfPositive(cf.topPadding(), "clpadt");
                    emitIfPositive(cf.bottomPadding(), "clpadb");

                    // Emit cell borders (skip sides that are emitted as row borders)
                    for (int side = 0; side < 4; ++side) {
                        if (uniformSide[side]) continue;
                        const char* sideTag = "";
                        switch (side) {
                            case 0: sideTag = "\\clbrdrl"; break;
                            case 1: sideTag = "\\clbrdrt"; break;
                            case 2: sideTag = "\\clbrdrr"; break;
                            default: sideTag = "\\clbrdrb"; break;
                        }
                        auto& bi = rowCellBorders[c][side];
                        emitCellBorderSide(out, sideTag, bi.width, bi.style, bi.colorIdx);
                    }

                    // Iterate over blocks within the cell
                    QTextBlock cellBlock = cell.firstCursorPosition().block();
                    int cellEndPos = cell.lastPosition();
                    bool first = true;
                    while (cellBlock.isValid() && cellBlock.position() < cellEndPos) {
                        if (!cellBlock.text().trimmed().isEmpty()) {
                            if (first) {
                                out << "\\intbl";
                                first = false;
                            }
                            exportBlock(cellBlock, true);
                        }
                        cellBlock = cellBlock.next();
                    }
                    if (first) out << "\\intbl";
                    out << "\\cell";
                }
                out << "\\row}";
            }
        } else {
            QTextBlock block = frameIt.currentBlock();
            if (block.isValid()) {
                exportBlock(block, false);
            }
        }
    }

    out << "}";
    return out.str();
}

std::string ExportHtml(const QTextDocument& document) {
    QString html = document.toHtml();
    return html.toStdString();
}

} // namespace Rte

#include "RtfExport.h"
#include "RtfTypes.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QFont>
#include <QTextBlockFormat>
#include <QString>
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

static void WriteConditionalFormatOff(std::ostringstream& out, const RtfRunFormat& fmt, bool trailingSpace) {
    const char* space = trailingSpace ? " " : "";
    if (fmt.bold) out << "\\b0" << space;
    if (fmt.italic) out << "\\i0" << space;
    if (fmt.superscript) out << "\\super0" << space;
    if (fmt.subscript) out << "\\sub0" << space;
    if (fmt.colorIndex > 0) out << "\\cf0" << space;
    if (fmt.bgColorIndex > 0) out << "\\cb0" << space;
    if (fmt.underlineStyle != UnderlineStyle::None) out << "\\ul0" << space;
    if (fmt.strikeOut) out << "\\strike0" << space;
    if (fmt.capitalization == Capitalization::AllCaps) out << "\\caps0" << space;
    if (fmt.capitalization == Capitalization::SmallCaps) out << "\\scaps0" << space;
    if (fmt.highlight >= 0) out << "\\highlight0" << space;
}

static void WriteFormatOff(std::ostringstream& out, const RtfRunFormat& fmt, bool trailingSpace) {
    const char* space = trailingSpace ? " " : "";
    if (fmt.underlineStyle != UnderlineStyle::None) out << "\\ul0" << space;
    if (fmt.strikeOut) out << "\\strike0" << space;
    if (fmt.capitalization == Capitalization::AllCaps) out << "\\caps0" << space;
    if (fmt.capitalization == Capitalization::SmallCaps) out << "\\scaps0" << space;
    if (fmt.highlight >= 0) out << "\\highlight0" << space;
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

int FindHighlightIndex(const QColor& color) {
    for (int i = 0; i < static_cast<int>(kHighlightPalette.size()); ++i) {
        QColor pal(kHighlightPalette[i][0], kHighlightPalette[i][1], kHighlightPalette[i][2]);
        if (color == pal) return i;
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
    if (alignment & Qt::AlignLeft) return "\\ql";
    return "\\ql";
}

} // namespace

std::string ExportRtf(const QTextDocument& document) {
    std::ostringstream out;

    QFont defaultFont = document.defaultFont();

    std::map<std::string, int> fontMap;
    std::vector<QColor> colorList;
    std::vector<QColor> bgColorList;
    int defaultFontIdx = 0;
    {
        int idx = 0;
        std::string defFamily = defaultFont.family().toStdString();
        fontMap[defFamily] = idx;
        defaultFontIdx = idx;
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
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
                collectColor(frag.charFormat().foreground().color(), colorList);
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

    out << "{\\fonttbl";
    std::map<std::string, std::string> typeHints = {
        {"arial", "\\fswiss"},
        {"times new roman", "\\froman"},
        {"courier new", "\\fmodern"},
    };
    for (const auto& [family, idx] : fontMap) {
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

    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QString text = block.text();
        bool hasText = !text.trimmed().isEmpty();

        if (!hasText) {
            bool hasFollowingContent = false;
            for (QTextBlock b = block.next(); b.isValid(); b = b.next()) {
                if (!b.text().trimmed().isEmpty()) {
                    hasFollowingContent = true;
                    break;
                }
            }
            if (hasFollowingContent) continue;
            continue;
        }

        QTextBlockFormat blockFmt = block.blockFormat();
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

        out << "\n";

        RtfRunFormat prev;
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
            }

            cur.bold = charFmt.fontWeight() >= 700;
            cur.italic = charFmt.fontItalic();
            cur.strikeOut = charFmt.fontStrikeOut();
            cur.superscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSuperScript;
            cur.subscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSubScript;
            cur.underlineStyle = EffectiveUnderlineStyle(charFmt);
            cur.capitalization = toCapitalization(charFmt.fontCapitalization());

            if (firstRun || cur != prev) {
                if (!firstRun) {
                    WriteConditionalFormatOff(out, prev, true);
                }

                if (cur.fontSize > 0 && (!firstRun || cur.fontSize != prev.fontSize))
                    out << "\\fs" << cur.fontSize << ' ';
                if (cur.fontIndex != prev.fontIndex)
                    out << "\\f" << cur.fontIndex << ' ';
                if (cur.colorIndex > 0) out << "\\cf" << cur.colorIndex << ' ';
                if (cur.bgColorIndex > 0) out << "\\cb" << cur.bgColorIndex << ' ';
                if (cur.bold) out << "\\b ";
                if (cur.italic) out << "\\i ";
                if (cur.strikeOut) out << "\\strike ";
                if (cur.underlineStyle != UnderlineStyle::None)
                    out << UnderlineStyleTag(cur.underlineStyle) << ' ';
                if (cur.superscript) out << "\\super ";
                if (cur.subscript) out << "\\sub ";
                if (cur.capitalization == Capitalization::AllCaps) out << "\\caps ";
                if (cur.capitalization == Capitalization::SmallCaps) out << "\\scaps ";
                if (cur.highlight >= 0) out << "\\highlight" << cur.highlight << ' ';
            }

            out << RtfEscape(frag.text());
            prev = cur;
            firstRun = false;
            it++;
        }

        if (!firstRun) {
            WriteFormatOff(out, prev, false);
            out << "\\b0\\i0\\super0\\sub0\\cf0\\par\n";
        } else {
            out << "\\b0\\i0\\super0\\sub0\\cf0\\par\n";
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

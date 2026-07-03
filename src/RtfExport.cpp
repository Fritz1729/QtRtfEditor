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

constexpr std::array<std::array<uint8_t, 3>, 17> kHighlightPalette = {{
    {0, 0, 0},       // 0  = black
    {128, 128, 128}, // 1  = dark gray
    {128, 0, 0},     // 2  = maroon
    {0, 128, 0},     // 3  = dark green
    {128, 128, 0},   // 4  = dark yellow
    {0, 0, 128},     // 5  = navy
    {128, 0, 128},   // 6  = purple
    {0, 128, 128},   // 7  = teal
    {192, 192, 192}, // 8  = silver
    {255, 255, 255}, // 9  = white
    {255, 0, 0},     // 10 = red
    {0, 255, 0},     // 11 = green
    {255, 255, 0},   // 12 = yellow
    {0, 0, 255},     // 13 = blue
    {255, 0, 255},   // 14 = magenta
    {0, 255, 255},   // 15 = cyan
    {128, 128, 128}, // 16 = gray 50%
}};

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
                QColor col = frag.charFormat().foreground().color();
                if (col.isValid() && col.alpha() == 255 &&
                    FindColorIndex(colorList, col) < 0) {
                    colorList.push_back(col);
                }
                QBrush bgBrush = frag.charFormat().background();
                if (bgBrush.style() != Qt::NoBrush) {
                    QColor bgCol = bgBrush.color();
                    if (bgCol.isValid() && bgCol.alpha() == 255 &&
                        FindColorIndex(bgColorList, bgCol) < 0) {
                        bgColorList.push_back(bgCol);
                    }
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

        double liVal = blockFmt.leftMargin();
        if (liVal > 0) {
            int liHalfPoints = static_cast<int>(liVal * 2.0);
            out << "\\li" << liHalfPoints;
        }
        int indent = blockFmt.indent();
        if (indent > 0) {
            int fiHalfPoints = static_cast<int>(indent * 2.0);
            out << "\\fi" << fiHalfPoints;
        }

        double riVal = blockFmt.rightMargin();
        if (riVal > 0) {
            int riHalfPoints = static_cast<int>(riVal * 2.0);
            out << "\\ri" << riHalfPoints;
        }

        double sbVal = blockFmt.topMargin();
        if (sbVal > 0) {
            int sbHalfPoints = static_cast<int>(sbVal * 2.0);
            out << "\\sb" << sbHalfPoints;
        }
        double saVal = blockFmt.bottomMargin();
        if (saVal > 0) {
            int saHalfPoints = static_cast<int>(saVal * 2.0);
            out << "\\sa" << saHalfPoints;
        }

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

            QColor col = charFmt.foreground().color();
            if (col.isValid() && col.alpha() == 255) {
                cur.colorIndex = FindColorIndex(colorList, col) + 1;
            } else {
                cur.colorIndex = 0;
            }

            QBrush bgBrush = charFmt.background();
            if (bgBrush.style() != Qt::NoBrush) {
                QColor bgCol = bgBrush.color();
                if (bgCol.isValid() && bgCol.alpha() == 255) {
                    int bgIdx = FindColorIndex(bgColorList, bgCol);
                    if (bgIdx >= 0) {
                        cur.bgColorIndex = bgIdx + 1;
                    }
                }
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
                    if (prev.superscript) out << "\\super0 ";
                    if (prev.subscript) out << "\\sub0 ";
                    if (prev.bold) out << "\\b0 ";
                    if (prev.italic) out << "\\i0 ";
                    if (prev.colorIndex > 0) out << "\\cf0 ";

                    if (prev.underlineStyle != UnderlineStyle::None) {
                        out << "\\ul0 ";
                    }
                    if (prev.strikeOut) out << "\\strike0 ";
                    if (prev.capitalization == Capitalization::AllCaps) out << "\\caps0 ";
                    if (prev.capitalization == Capitalization::SmallCaps) out << "\\scaps0 ";
                    if (prev.highlight >= 0) out << "\\highlight0 ";
                    if (prev.bgColorIndex > 0) out << "\\cb0 ";
                }

                if (cur.fontSize > 0 && (!firstRun || cur.fontSize != prev.fontSize))
                    out << "\\fs" << cur.fontSize << ' ';
                if (cur.fontIndex != prev.fontIndex)
                    out << "\\f" << cur.fontIndex << ' ';
                if (cur.colorIndex > 0)
                    out << "\\cf" << cur.colorIndex << ' ';
                if (cur.bgColorIndex > 0)
                    out << "\\cb" << cur.bgColorIndex << ' ';
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
            if (prev.underlineStyle != UnderlineStyle::None)
                out << "\\ul0";
            if (prev.strikeOut) out << "\\strike0";
            if (prev.capitalization == Capitalization::AllCaps) out << "\\caps0";
            if (prev.capitalization == Capitalization::SmallCaps) out << "\\scaps0";
            if (prev.highlight >= 0) out << "\\highlight0";
        }
        out << "\\b0\\i0\\super0\\sub0\\cf0\\par\n";
    }

    out << "}";
    return out.str();
}

std::string ExportHtml(const QTextDocument& document) {
    QString html = document.toHtml();
    return html.toStdString();
}

} // namespace Rte

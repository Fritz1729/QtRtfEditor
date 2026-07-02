#include "RtfExport.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QFont>
#include <QTextBlockFormat>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <map>
#include <sstream>

namespace Rte {

namespace {

std::string RtfEscape(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        switch (c) {
            case '\\':  result += "\\\\"; break;
            case '{':   result += "\\{"; break;
            case '}':   result += "\\}"; break;
            default:
                if (static_cast<unsigned char>(c) < 32 ||
                    static_cast<unsigned char>(c) > 126)
                {
                    result += QString("\\u%0?").arg(
                        static_cast<int>(static_cast<unsigned char>(c))).toStdString();
                } else {
                    result += c;
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

struct CharFormatState {
    int fontSize = 0;
    int fontIndex = 0;
    int colorIndex = 0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;

    bool operator==(const CharFormatState& other) const {
        return fontSize == other.fontSize &&
               fontIndex == other.fontIndex &&
               colorIndex == other.colorIndex &&
               bold == other.bold &&
               italic == other.italic &&
               underline == other.underline &&
               superscript == other.superscript &&
               subscript == other.subscript;
    }
};

int FindColorIndex(const std::vector<QColor>& colorList, const QColor& color) {
    for (int i = 0; i < static_cast<int>(colorList.size()); ++i) {
        if (colorList[i] == color) return i;
    }
    return -1;
}

} // namespace

std::string ExportRtf(const QTextDocument& document) {
    std::ostringstream out;

    QFont defaultFont = document.defaultFont();

    std::map<std::string, int> fontMap;
    std::vector<QColor> colorList;
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
            int liHalfPoints = static_cast<int>(liVal * 20.0);
            out << "\\li" << liHalfPoints;
        }
        int indent = blockFmt.indent();
        if (indent > 0) {
            int fiHalfPoints = static_cast<int>(indent * 20.0);
            out << "\\fi" << fiHalfPoints;
        }

        out << "\n";

        CharFormatState prev;
        bool firstRun = true;

        QTextBlock::iterator it = block.begin();
        while (it != block.end()) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid() || frag.length() == 0) { it++; continue; }

            QTextCharFormat charFmt = frag.charFormat();

            CharFormatState cur;
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
                cur.colorIndex = FindColorIndex(colorList, col) + 1; // RTF: 1-based (0=auto)
            } else {
                cur.colorIndex = 0; // RTF auto color
            }

            cur.bold = charFmt.fontWeight() >= 700;
            cur.italic = charFmt.fontItalic();
            cur.underline = charFmt.fontUnderline();
            cur.superscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSuperScript;
            cur.subscript = charFmt.verticalAlignment() == QTextCharFormat::AlignSubScript;

            if (firstRun || cur != prev) {
                if (!firstRun && !prev.superscript && !prev.subscript) {
                    if (!prev.bold) out << "\\b0 ";
                    if (!prev.italic) out << "\\i0 ";
                    if (!prev.underline) out << "\\ul0 ";
                    if (prev.colorIndex > 0) out << "\\cf0 ";
                } else if (!firstRun) {
                    if (prev.superscript) out << "\\super0 ";
                    if (prev.subscript) out << "\\sub0 ";
                }

                if (cur.fontSize > 0 && (!firstRun || cur.fontSize != prev.fontSize))
                    out << "\\fs" << cur.fontSize << ' ';
                if (cur.fontIndex != prev.fontIndex)
                    out << "\\f" << cur.fontIndex << ' ';
                if (cur.colorIndex > 0)
                    out << "\\cf" << cur.colorIndex << ' ';
                if (cur.bold) out << "\\b ";
                if (cur.italic) out << "\\i ";
                if (cur.underline) out << "\\ul ";
                if (cur.superscript) out << "\\super ";
                if (cur.subscript) out << "\\sub ";
            }

            out << RtfEscape(frag.text().toUtf8().toStdString());
            prev = cur;
            firstRun = false;
            it++;
        }

        out << "\\b0\\i0\\ul0\\super0\\sub0\\cf0\\par\n";
    }

    out << "}";
    return out.str();
}

std::string ExportHtml(const QTextDocument& document) {
    QString html = document.toHtml();
    return html.toStdString();
}

} // namespace Rte

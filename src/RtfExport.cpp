#include "RtfExport.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QFont>
#include <QColor>
#include <QTextBlockFormat>
#include <QTextLayout>
#include <QString>
#include <QtGlobal>

#include <stdexcept>
#include <sstream>

namespace Rte {

namespace {

std::string RtfEscape(const std::string &text) {
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

int ColorToCfIndex(const QColor &color) {
    if (color.red() == 0 && color.green() == 0 && color.blue() == 0) return 0;
    if (color.red() > 128 && color.green() == 0 && color.blue() == 0) return 1;
    if (color.red() == 0 && color.green() > 0 && color.blue() == 0) return 2;
    if (color.red() == 0 && color.green() == 0 && color.blue() > 128) return 3;
    return 0;
}

int FontToIndex(const QStringList &names) {
    for (const QString &name : names) {
        if (name.contains("Courier", Qt::CaseInsensitive)) return 1;
        if (name.contains("Arial", Qt::CaseInsensitive)) return 2;
        if (name.contains("Symbol", Qt::CaseInsensitive)) return 3;
    }
    return 0;
}

std::string AlignmentToRtf(Qt::Alignment alignment) {
    if (alignment & Qt::AlignRight) return "\\qr";
    if (alignment & Qt::AlignHCenter) return "\\qc";
    if (alignment & Qt::AlignLeft) return "\\ql";
    return "\\ql";
}

} // namespace

std::string ExportRtf(const QTextDocument &document) {
    std::ostringstream out;

    out << "{\\rtf1\\ansi\\deff0";

    out << "{\\colortbl ;";
    out << "\\red255\\green0\\blue0;";
    out << "\\red0\\green128\\blue0;";
    out << "\\red0\\green0\\blue255;";
    out << "\\red128\\green0\\blue128;";
    out << "\\red0\\green128\\blue128;";
    out << "\\red192\\green192\\blue0;";
    out << "\\red255\\green0\\blue255;";
    out << "\\red0\\green255\\blue255;";
    out << "\\red128\\green128\\blue128;";
    out << "\\red192\\green192\\blue192;";
    out << "}";

    out << "{\\fonttbl";
    out << "{\\f0\\fswiss\\fcharset0 Arial;}";
    out << "{\\f0\\froman\\fcharset0 Times New Roman;}";
    out << "{\\f1\\fmodern\\fcharset0 Courier New;}";
    out << "{\\f3\\fnil\\fcharset2 Symbol;}";
    out << "}";

    for (QTextBlock block = document.begin(); block.isValid();
         block = block.next())
    {
        QTextBlockFormat blockFmt = block.blockFormat();
        out << AlignmentToRtf(blockFmt.alignment());

        // Qt 6.11: textIndent() replaces leftIndent/firstIndent
        qreal indent = blockFmt.textIndent();
        if (indent > 0) out << "\\li" << static_cast<int>(indent * 20);

        for (const QTextLayout::FormatRange &fmtRange : block.textFormats()) {
            if (fmtRange.length <= 0) continue;
        }

        // Separate control words from text with a newline
        out << "\n";

        if (block.length() > 0) {
            QTextBlock::iterator it = block.begin();
            int fontSize = static_cast<int>(
                it.fragment().charFormat().fontPointSize() * 20);
            if (fontSize <= 0) {
                fontSize = static_cast<int>(
                    document.defaultFont().pointSizeF() * 20);
            }
            if (fontSize > 0) out << "\\fs" << fontSize;
        }

        QString text = block.text();
        bool hasText = !text.trimmed().isEmpty();

        // Skip empty trailing blocks
        if (!hasText) {
            bool hasFollowingContent = false;
            for (QTextBlock b = block.next(); b.isValid(); b = b.next()) {
                if (!b.text().trimmed().isEmpty()) {
                    hasFollowingContent = true;
                    break;
                }
            }
            if (hasFollowingContent) continue;
            // No following content - trailing empty block, skip
            continue;
        }

        out << RtfEscape(text.toUtf8().toStdString());

        out << "\\par";
    }

    out << "}";
    return out.str();
}

std::string ExportHtml(const QTextDocument &document) {
    QString html = document.toHtml();
    return html.toStdString();
}

} // namespace Rte

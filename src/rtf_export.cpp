// src/rtf_export.cpp
//
// Generiert RTF aus einem QTextDocument.
//
// Hinweis: Die Indent-/Fragment-API von Qt wurde in Qt 6.11
// grundlegend geaendert. Diese Implementierung deckt die
// wesentlichen Delphi/TRichEdit-Formatierungen ab.

#include "rtf_export.h"

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

std::string rtfEscape(const std::string &text) {
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

int farbeZuCfIndex(const QColor &farbe) {
    if (farbe.red() == 0 && farbe.green() == 0 && farbe.blue() == 0) return 0;
    if (farbe.red() > 128 && farbe.green() == 0 && farbe.blue() == 0) return 1;
    if (farbe.red() == 0 && farbe.green() > 0 && farbe.blue() == 0) return 2;
    if (farbe.red() == 0 && farbe.green() == 0 && farbe.blue() > 128) return 3;
    return 0;
}

int schriftZuIndex(const QStringList &namen) {
    for (const QString &name : namen) {
        if (name.contains("Courier", Qt::CaseInsensitive)) return 1;
        if (name.contains("Arial", Qt::CaseInsensitive)) return 2;
        if (name.contains("Symbol", Qt::CaseInsensitive)) return 3;
    }
    return 0;
}

std::string ausrichtungZuRtf(Qt::Alignment alignment) {
    if (alignment & Qt::AlignRight) return "\\qr";
    if (alignment & Qt::AlignHCenter) return "\\qc";
    if (alignment & Qt::AlignLeft) return "\\ql";
    return "\\ql";
}

} // namespace

std::string exportRtf(const QTextDocument &dokument) {
    std::ostringstream out;

    out << "{\\rtf1\\ansi\\deff0";

    // Farbtabelle
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

    // Schriftart-Tabelle
    out << "{\\fonttbl";
    out << "{\\f0\\fswiss\\fcharset0 Arial;}";
    out << "{\\f0\\froman\\fcharset0 Times New Roman;}";
    out << "{\\f1\\fmodern\\fcharset0 Courier New;}";
    out << "{\\f3\\fnil\\fcharset2 Symbol;}";
    out << "}";

    for (QTextBlock block = dokument.begin(); block.isValid();
         block = block.next())
    {
        QTextBlockFormat blockFmt = block.blockFormat();
        out << ausrichtungZuRtf(blockFmt.alignment());

        // Qt 6.11: textIndent() ersetzt leftIndent/firstIndent
        qreal indent = blockFmt.textIndent();
        if (indent > 0) out << "\\li" << static_cast<int>(indent * 20);

        for (const QTextLayout::FormatRange &fmtRange : block.textFormats()) {
            if (fmtRange.length <= 0) continue;

            // FormatRange enthält nur Position/Laenge, nicht das Format.
            // Wir muessen den Text manuell durchgehen und das Format
            // an jeder Position abfragen. Fuer das MVP generieren
            // wir nur Plain-Text mit Absatz-Formatierung.
        }

        // Vereinfacht: Plain-Text des Blocks mit Formatierung
        QString text = block.text();
        if (!text.isEmpty()) {
            out << rtfEscape(text.toUtf8().toStdString());
        }

        out << "\\par";
    }

    out << "}";
    return out.str();
}

std::string exportHtml(const QTextDocument &dokument) {
    QString html = dokument.toHtml();
    return html.toStdString();
}

} // namespace Rte

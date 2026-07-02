// src/rtf_export.cpp

#include "rtf_export.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QTextBlockFormat>
#include <QStringBuilder>
#include <stdexcept>
#include <sstream>

namespace Rte {

namespace {

/**
 * @brief Escaped RTF-Zeichen für bestimmte Sonderzeichen.
 */
std::string rtfEscape(const std::string &text) {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        switch (c) {
            case '\\':  result += "\\\\"; break;
            case '{':   result += "\\{"; break;
            case '}':   result += "\\}"; break;
            default:
                if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
                    // Nicht-ASCII: als Unicode-RTF-Code
                    result += QString("\\u%0?").arg(static_cast<int>(static_cast<unsigned char>(c))).toStdString();
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

/**
 * @brief Qt-Farbe in RTF-\\cf-index umwandeln.
 *
 * RTF-Farbtabelle: Index 1=rot, 2=grün, 3=blau, 4=schwarz, ...
 * Wir verwenden eine vereinfachte Abbildung.
 */
int farbeZuCfIndex(const QColor &farbe) {
    // Einfache Farbzuteilung für TRichEdit-kompatible Palette
    if (farbe.red() == 0 && farbe.green() == 0 && farbe.blue() == 0) return 0; // schwarz
    if (farbe.red() > 128 && farbe.green() == 0 && farbe.blue() == 0) return 1; // rot
    if (farbe.red() == 0 && farbe.green() > 0 && farbe.blue() == 0) return 2;  // grün
    if (farbe.red() == 0 && farbe.green() == 0 && farbe.blue() > 128) return 3; // blau
    // Standard: schwarz (Index 0)
    return 0;
}

/**
 * @brief Qt-Schriftart auf TRichEdit-kompatible RTF-Schrift mappen.
 */
int schriftZuIndex(const QString &name) {
    // TRichEdit-Schriftarten:
    // 0=Times New Roman, 1=Courier New, 2=Arial, 3=Symbol
    if (name.contains("Courier", Qt::CaseInsensitive)) return 1;
    if (name.contains("Arial", Qt::CaseInsensitive)) return 2;
    if (name.contains("Symbol", Qt::CaseInsensitive)) return 3;
    return 0; // Times New Roman
}

/**
 * @brief Schriftgröße von Punkten in Halftpunkte umwandeln (RTF-Einheit).
 */
int schriftGrad(int punkte) {
    return punkte * 2;
}

/**
 * @brief QTextBlockFormat::Alignment auf RTF-Tags mappen.
 */
std::string ausrichtungZuRtf(QTextBlockFormat::Alignment alignment) {
    switch (alignment) {
        case QTextBlockFormat::AlignLeftCenterByTab:
        case QTextBlockFormat::AlignLeft:
            return "\\ql";
        case QTextBlockFormat::AlignCenter:
            return "\\qc";
        case QTextBlockFormat::AlignRight:
            return "\\qr";
        case QTextBlockFormat::AlignJustify:
            return "\\qj";
        default:
            return "\\ql";
    }
}

/**
 * @brief Einrückung von Punkten in Twips (1/1440 inch, RTF-Einheit).
 *
 * Qt verwendet Pixels, RTF Twips. Umrechnung bei 96 DPI:
 * 1 Punkt = 1/72 inch = 20 Twips.
 */
int einrueckungZuTwips(int punkte) {
    return punkte * 20;
}

} // namespace

// === exportRtf() ===

std::string exportRtf(const QTextDocument &dokument) {
    std::ostringstream out;

    // RTF-Header
    out << "{\\rtf1\\ansi\\deff0";

    // Farbtabelle (minimale TRichEdit-Palette)
    out << "{\\colortbl ;";
    out << "\\red255\\green0\\blue0;";   // 1 rot
    out << "\\red0\\green128\\blue0;";   // 2 grün
    out << "\\red0\\green0\\blue255;";   // 3 blau
    out << "\\red128\\green0\\blue128;"; // 4 lila
    out << "\\red0\\green128\\blue128;"; // 5 teal
    out << "\\red192\\green192\\blue0;"; // 6 gelb
    out << "\\red255\\green0\\blue255;"; // 7 magenta
    out << "\\red0\\green255\\blue255;"; // 8 cyan
    out << "\\red128\\green128\\blue128;"; // 9 grau
    out << "\\red192\\green192\\blue192;"; // 10 hellgrau
    out << "}";

    // Schriftart-Tabelle
    out << "{\\fonttbl";
    out << "{\\f0\\fswiss\\fcharset0 Arial;}";
    out << "{\\f0\\froman\\fcharset0 Times New Roman;}";
    out << "{\\f1\\fmodern\\fcharset0 Courier New;}";
    out << "{\\f3\\fnil\\fcharset2 Symbol;}";
    out << "}";

    // Dokument-Body
    for (QTextBlock block = dokument.begin(); block.isValid();
         block = block.next())
    {
        // Block-Format (Ausrichtung + Einrückung)
        QTextBlockFormat blockFmt = block.blockFormat();
        out << ausrichtungZuRtf(blockFmt.alignment());

        int leftIndent = blockFmt.leftIndent();
        if (leftIndent > 0) {
            out << "\\li" << einrueckungZuTwips(leftIndent);
        }

        int firstIndent = blockFmt.firstIndent();
        if (firstIndent > 0) {
            out << "\\fi" << einrueckungZuTwips(firstIndent);
        }

        int rightIndent = blockFmt.rightIndent();
        if (rightIndent > 0) {
            out << "\\ri" << einrueckungZuTwips(rightIndent);
        }

        // Fragmente (Zeichensatz innerhalb des Blocks)
        for (QTextFragment fragment : block.textFragmentRange()) {
            if (!fragment.isValid()) continue;

            QTextCharFormat charFmt = fragment.charFormat();

            // Schriftart
            if (charFmt.fontFamily().isEmpty()) {
                out << "\\f0";
            } else {
                out << "\\f" << schriftZuIndex(charFmt.fontFamily());
            }

            // Schriftgröße
            int size = charFmt.fontPointSize();
            if (size > 0) {
                out << "\\fs" << schriftGrad(static_cast<int>(size));
            }

            // Fett
            if (charFmt.fontWeight() > QFont::Medium) {
                out << "\\b";
            }

            // Kursiv
            if (charFmt.fontItalic()) {
                out << "\\i";
            }

            // Unterstrichen
            if (charFmt.fontUnderline()) {
                out << "\\ul";
            }

            // Hoch-/Tiefstellung
            QTextCharFormat::VerticalAlignment valign =
                charFmt.verticalAlignment();
            if (valign == QTextCharFormat::AlignSuperScript) {
                out << "\\super";
            } else if (valign == QTextCharFormat::AlignSubScript) {
                out << "\\sub";
            }

            // Farbe
            QColor foreground = charFmt.foreground().color();
            if (!foreground.isValid() || foreground != Qt::black) {
                int cfIdx = farbeZuCfIndex(foreground);
                if (cfIdx > 0) {
                    out << "\\cf" << cfIdx;
                }
            }

            // Text
            out << rtfEscape(fragment.text().toUtf8().toStdString());

            // Format-Tags rückgängig machen (Scope-Reset)
            if (charFmt.fontWeight() > QFont::Medium) out << "\\b0";
            if (charFmt.fontItalic()) out << "\\i0";
            if (charFmt.fontUnderline()) out << "\\ul0";
            if (valign == QTextCharFormat::AlignSuperScript)
                out << "\\super0";
            if (valign == QTextCharFormat::AlignSubScript)
                out << "\\sub0";
        }

        // Absatzende
        out << "\\par";
    }

    out << "}";

    return out.str();
}

// === exportHtml() ===

std::string exportHtml(const QTextDocument &dokument) {
    // QTextDocument::toHtml() generiert Qt-spezifisches HTML
    // mit q:-namespaced Attributen. Für zukünftige HTML-
    // Migration ist das ausreichend — die Semantik bleibt
    // erhalten.
    QString html = dokument.toHtml("UTF-8");
    return html.toStdString();
}

} // namespace Rte

#include "RtfImport.h"

#include "RtfParser.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QFont>
#include <QColor>

namespace Rte {

namespace {

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

    for (const auto& para : doc.paragraphs) {
        QTextBlockFormat blockFmt;

        if (para.alignment == 1) blockFmt.setAlignment(Qt::AlignLeft);
        else if (para.alignment == 128) blockFmt.setAlignment(Qt::AlignHCenter);
        else if (para.alignment == 2) blockFmt.setAlignment(Qt::AlignRight);

        if (para.leftIndent > 0 || para.firstLineIndent > 0) {
            int liTwips = para.leftIndent * 20;
            int fiTwips = para.firstLineIndent * 20;
            blockFmt.setLeftMargin(static_cast<double>(liTwips) / 20.0);
            blockFmt.setIndent(fiTwips > 0 ? static_cast<int>(static_cast<double>(fiTwips) / 20.0) : 0);
        }

        cursor.insertBlock(blockFmt);

        for (const auto& run : para.runs) {
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
            if (run.format.underline) {
                charFmt.setFontUnderline(true);
            }

            if (run.format.fontIndex >= 0 &&
                run.format.fontIndex < static_cast<int>(doc.fonts.size())) {
                QString fam = QString::fromStdString(doc.fonts[run.format.fontIndex].family);
                if (!fam.isEmpty()) {
                    charFmt.setFontFamilies({fam});
                }
            }

            if (run.format.colorIndex >= 0 &&
                run.format.colorIndex < static_cast<int>(doc.colors.size())) {
                const auto& col = doc.colors[run.format.colorIndex];
                charFmt.setForeground(QColor(col.red, col.green, col.blue));
            }

            if (run.format.superscript) {
                charFmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            } else if (run.format.subscript) {
                charFmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
            }

            cursor.insertText(QString::fromUtf8(run.text.data(),
                                                 static_cast<int>(run.text.size())),
                              charFmt);
        }
    }
}

} // namespace

void ImportRtf(QTextDocument* document, const std::string& rtf) {
    RtfDocument doc = ParseRtf(rtf);
    BuildDocument(document, doc);
}

} // namespace Rte

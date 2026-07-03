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

        blockFmt.setAlignment(RtfAlignmentToQt(para.alignment));

        if (para.leftIndent > 0 || para.firstLineIndent > 0) {
            blockFmt.setLeftMargin(static_cast<double>(para.leftIndent) / 2.0);
            blockFmt.setIndent(para.firstLineIndent > 0 ? static_cast<int>(para.firstLineIndent / 2.0) : 0);
        }

        if (para.rightIndent > 0) {
            blockFmt.setRightMargin(static_cast<double>(para.rightIndent) / 2.0);
        }

        if (para.spaceBefore > 0) {
            blockFmt.setTopMargin(static_cast<double>(para.spaceBefore) / 2.0);
        }
        if (para.spaceAfter > 0) {
            blockFmt.setBottomMargin(static_cast<double>(para.spaceAfter) / 2.0);
        }

        if (para.lineHeight > 0) {
            blockFmt.setLineHeight(static_cast<double>(para.lineHeight) / 2.0,
                                   QTextBlockFormat::FixedHeight);
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
            if (run.format.strikeOut) {
                charFmt.setFontStrikeOut(true);
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

            if (run.format.bgColorIndex >= 0 &&
                run.format.bgColorIndex < static_cast<int>(doc.colors.size())) {
                const auto& col = doc.colors[run.format.bgColorIndex];
                charFmt.setBackground(QBrush(QColor(col.red, col.green, col.blue)));
            }

            if (run.format.underline) {
                if (run.format.underlineStyle == UnderlineStyle::Solid ||
                    run.format.underlineStyle == UnderlineStyle::None) {
                    charFmt.setFontUnderline(true);
                } else {
                    charFmt.setUnderlineStyle(qtUnderlineStyleFor(run.format.underlineStyle));
                }
            }

            if (run.format.capitalization == Capitalization::AllCaps) {
                charFmt.setFontCapitalization(QFont::AllUppercase);
            } else if (run.format.capitalization == Capitalization::SmallCaps) {
                charFmt.setFontCapitalization(QFont::SmallCaps);
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

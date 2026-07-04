#include "RtfImport.h"

#include "RtfParser.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextOption>
#include <QFont>
#include <QColor>
#include <QImageReader>
#include <QBuffer>

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

        if (!para.tabStops.empty()) {
            QList<QTextOption::Tab> tabs;
            for (const auto& ts : para.tabStops) {
                QTextOption::TabType type;
                switch (ts.alignment) {
                case 1: type = QTextOption::LeftTab; break;
                case 128: type = QTextOption::CenterTab; break;
                case 2: type = QTextOption::RightTab; break;
                case 3: type = QTextOption::LeftTab; break;
                default: type = QTextOption::LeftTab; break;
                }
                tabs << QTextOption::Tab(static_cast<double>(ts.position) / 2.0, type);
            }
            blockFmt.setTabPositions(tabs);
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

            if (run.format.kerning) {
                charFmt.setFontKerning(true);
            }
            if (run.format.expnd != 0) {
                double ptSize = run.format.fontSize > 0 ? run.format.fontSize / 2.0 : defaultFont.pointSizeF();
                charFmt.setFontLetterSpacing(run.format.expnd / 20.0 * ptSize);
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

    // Insert images
    static int imgCounter = 0;
    imgCounter = 0;
    for (const auto& img : doc.images) {
        cursor.insertBlock();

        // Determine image size in pixels
        qreal widthPx = 0, heightPx = 0;
        {
            QByteArray imageData = img.data;
            QBuffer buffer(&imageData);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            QSize size = reader.size();
            if (size.isValid() && size.width() > 0 && size.height() > 0) {
                widthPx = size.width() * 96.0 / 72.0;
                heightPx = size.height() * 96.0 / 72.0;
            }
        }

        if (widthPx <= 0 || heightPx <= 0) {
            widthPx = 100;
            heightPx = 100;
        }

        // Apply scaling
        widthPx *= img.picscalex / 100.0;
        heightPx *= img.picscaley / 100.0;

        // Register image as resource
        imgCounter++;
        const char* ext = "";
        switch (img.format) {
            case Rte::RtfImageFormat::Jpeg: ext = "jpg"; break;
            case Rte::RtfImageFormat::Png:  ext = "png"; break;
            case Rte::RtfImageFormat::Bmp:  ext = "bmp"; break;
            default:                        ext = "png"; break;
        }
        QString name = QString("rtfimage://%1.%2").arg(imgCounter).arg(ext);
        document->addResource(QTextDocument::ImageResource, QUrl(name), QVariant::fromValue(img.data));

        // Store original RTF hex string and format for byte-identical roundtrip
        if (!img.rtfPictHex.empty()) {
            QString propName = QString("rtfPictHex://img%1").arg(imgCounter);
            document->setProperty(qPrintable(propName),
                                  QVariant::fromValue(QString::fromLatin1(img.rtfPictHex.c_str())));
            QString fmtPropName = QString("rtfImageFormat://img%1").arg(imgCounter);
            const char* fmt = "";
            switch (img.format) {
                case Rte::RtfImageFormat::Jpeg: fmt = "jpg"; break;
                case Rte::RtfImageFormat::Png:  fmt = "png"; break;
                case Rte::RtfImageFormat::Bmp:  fmt = "bmp"; break;
                default:                        fmt = "png"; break;
            }
            document->setProperty(qPrintable(fmtPropName),
                                  QVariant::fromValue(QString(fmt)));
        }

        // Insert image
        // NOTE: BMP rendering does not work yet in Qt demo.
        // The export/roundtrip path is unaffected because it uses document
        // properties instead of QTextImageFormat::name().
        QTextImageFormat imgFmt;
        imgFmt.setName(name);
        imgFmt.setWidth(widthPx);
        imgFmt.setHeight(heightPx);
        cursor.insertImage(imgFmt);
    }
}

} // namespace

void ImportRtf(QTextDocument* document, const std::string& rtf) {
    RtfDocument doc = ParseRtf(rtf);
    BuildDocument(document, doc);
}

} // namespace Rte

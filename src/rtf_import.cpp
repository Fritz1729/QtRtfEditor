// src/rtf_import.cpp

#include "rtf_import.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFrame>
#include <QString>
#include <QRegularExpression>
#include <stdexcept>
#include <memory>

namespace Rte {

namespace {

/**
 * @brief Prueft, ob ein RTF-String Delphi/TRichEdit-kompatible
 *        Formatierungselemente enthaelt.
 *
 * Delphi/TRichEdit nutzt eine Teilmenge von RTF. Diese Funktion
 * scannt nach bekannten Tags, die von TRichEdit erzeugt werden.
 */
bool hatDelphiFormatierung(const QString &rtf) {
    // Bekannte Delphi-TRichEdit-RTF-Tags:
    static const char* tags[] = {
        "\\b",       // bold
        "\\i",       // italic
        "\\ul",      // underline
        "\\cf",      // color table index
        "\\fs",      // font size
        "\\f",       // font
        "\\ql",      // left
        "\\qc",      // center
        "\\qr",      // right
        "\\li",      // left indent
        "\\fi",      // first indent
        "\\ri",      // right indent
        "\\super",   // superscript
        "\\sub",     // subscript
        nullptr
    };

    for (int i = 0; tags[i]; ++i) {
        if (rtf.contains(tags[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

// === RichTextDoc ===

class RichTextDoc::RichTextDoc {
public:
    QTextDocument *_dokument = nullptr;
    QString _rtf;
    QString _html;
    std::size_t _laenge = 0;
};

RichTextDoc::RichTextDoc()
    : _dokument(new QTextDocument())
{
}

RichTextDoc::RichTextDoc(QTextDocument *doc)
    : _dokument(doc)
{
}

RichTextDoc::~RichTextDoc() {
    delete _dokument;
}

QTextDocument *RichTextDoc::dokument() {
    return _dokument;
}

std::string RichTextDoc::rtfBlob() const {
    return _rtf.toStdString();
}

std::string RichTextDoc::htmlBlob() const {
    return _html.toStdString();
}

std::size_t RichTextDoc::laenge() const {
    return _laenge;
}

// === laden() ===

std::unique_ptr<RichTextDoc> laden(const std::string &rtf) {
    if (rtf.empty()) {
        throw std::runtime_error("RTF-Input ist leer");
    }

    auto doc = std::make_unique<RichTextDoc>();

    // QTextDocument::setHtml() kann auch RTF parsen, wenn das RTF
    // einen ueblichen RTF-Header hat ({\rtf ...}).
    // Fuer reine RTF-Blobs ohne Header muss ein Minimal-Header
    // ergaenzt werden.

    QString rtfStr = QString::fromUtf8(rtf.c_str(), static_cast<int>(rtf.size()));

    // Minimalen RTF-Header ergaenzen falls noetig
    if (!rtfStr.trimmed().startsWith("{\\rtf")) {
        rtfStr = "{\\rtf1\\ansi\\deff0 " + rtfStr + "}";
    }

    // Mit QTextCursor laden
    QTextDocument *textDoc = doc->_dokument;
    textDoc->setUndoRedoEnabled(false);

    // fromHtml() ist der sicherste Weg — QTextDocument kennt
    // RTF-Teilsyntax durch Qt-HTML-zu-RTF-Rendering-Pipeline
    //
    // Alternative: QTextDocument::setPlainText() — aber das
    // verwirft alle Formatierung.
    //
    // Best practice: QTextDocument::setHtml() — Qt wandelt
    // HTML/RTF-ähnliche Syntax intern korrekt um.

    textDoc->setHtml(rtfStr);

    // RTF-Blob speichern (sofern moeglich)
    // QTextDocument::toHtml() liefert HTML, nicht RTF.
    // Fuer RTF-Export muss rtf_export.cpp verwendet werden.

    doc->_laenge = static_cast<std::size_t>(textDoc->toPlainText().size());

    return doc;
}

// === istDelphiKompatibel() ===

bool istDelphiKompatibel(const std::string &rtf) {
    if (rtf.empty()) {
        return false;
    }

    QString rtfStr = QString::fromUtf8(rtf.c_str(), static_cast<int>(rtf.size()));
    return hatDelphiFormatierung(rtfStr);
}

} // namespace Rte

#pragma once
// src/rtf_import.h
// Importiert RTF-Daten in ein QTextDocument.
//
// Die Implementierung nutzt QTextDocument::fromRichText() als
// Basis und fuehrt eine nachgeschaltete Validierung durch, die
// erkennt, ob Delphi/TRichEdit-kompatible Formatierung vorhanden
// ist. Unbekannte RTF-Tags werden ignoriert.

#include <string>
#include <optional>

class QTextDocument;

namespace Rte {

/**
 * @brief RTF-Bytes in ein QTextDocument laden.
 *
 * @param rtf  RTF-Inhalt als UTF-8-String (inkl. RTF-Header).
 * @return     QTextDocument mit geladenem Inhalt.
 * @throws     std::runtime_error bei schwerwiegenden RTF-Fehlern.
 *
 * Der geladene Document-Inhalt steht als RTF-String zur Verfuegung
 * (via toHtml()), kann aber auch als roher RTF-String serialisiert
 * werden (via rtfBlob()).
 */
class RichTextDoc;

std::unique_ptr<RichTextDoc> laden(const std::string &rtf);

/**
 * @brief Prueft, ob ein RTF-String Delphi/TRichEdit-kompatible
 *        Formatierungselemente enthaelt.
 *
 * @param rtf  RTF-String.
 * @return     true wenn bekannte Tags (Fett, Kursiv, Farbe,
 *             Unterstrichen, Hoch/Tief, Ausrichtung, Einrueckung)
 *             vorhanden sind.
 */
bool istDelphiKompatibel(const std::string &rtf);

/**
 * @brief RichTextDoc — Wrapper um QTextDocument mit RTF-Unterstuetzung.
 */
class RichTextDoc {
public:
    ~RichTextDoc();

    /** @return QTextDocument * (nicht-ownership). */
    QTextDocument *dokument();

    /** @return RTF-String des gesamten Dokuments. */
    std::string rtfBlob() const;

    /** @return HTML-String des gesamten Dokuments (q:-namespaced). */
    std::string htmlBlob() const;

    /** @return Textlänge im QTextDocument. */
    std::size_t laenge() const;

    RichTextDoc(const RichTextDoc &) = delete;
    RichTextDoc &operator=(const RichTextDoc &) = delete;

private:
    RichTextDoc();
    explicit RichTextDoc(QTextDocument *doc);

    QTextDocument *_dokument;
};

} // namespace Rte

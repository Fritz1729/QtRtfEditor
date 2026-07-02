#pragma once
// src/rtf_export.h
// Serialisiert ein QTextDocument als RTF-String.
//
// Der Export ist bewusst auf Delphi/TRichEdit-kompatible RTF-
// Tags beschraenkt. Fuer zukuenftige HTML-Unterstuetzung ist
// die Schnittstelle erweiterbar.

#include <string>

class QTextDocument;

namespace Rte {

/**
 * @brief QTextDocument als RTF-String serialisieren.
 *
 * @param dokument  Das zu serialisierende QTextDocument.
 * @return          RTF-String (UTF-8).
 *
 * Der Export nutzt eine manuelle RTF-Generierung, die nur
 * die von Delphi/TRichEdit verstandenen Tags erzeugt:
 *   - \b, \i, \ul, \cf (Farbe)
 *   - \fs (Schriftgroesse in Halftpunkten), \f (Schriftart)
 *   - \ql, \qc, \qr (Ausrichtung)
 *   - \li, \fi, \ri (Einrueckung)
 *   - \super, \sub (Hoch-/Tiefstellung)
 */
std::string exportRtf(const QTextDocument &dokument);

/**
 * @brief QTextDocument als HTML-String serialisieren.
 *
 * @param dokument  Das zu serialisierende QTextDocument.
 * @return          HTML-String (UTF-8, q:-namespaced).
 */
std::string exportHtml(const QTextDocument &dokument);

} // namespace Rte

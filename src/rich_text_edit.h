#pragma once
// src/rich_text_edit.h
//
// Wiederverwendbare RTF-fähige QTextEdit-Subclass.
//
// Unterstuetzt:
//   - Einlesen/Serialisieren von RTF (Delphi/TRichEdit-kompatibel)
//   - Geschuetzte Textbereiche mit konfigurierbarer Schutzrichtlinie
//   - Subclassing (virtuelle Methoden) für anwendungsspezifische
//     Erweiterungen (z. B. MV-spezifische Verweis-Tags)
//
// Lizenz: Dual-Licensing (LGPL-3.0-or-later + kommerzielle Lizenz).
// Siehe https://www.qt.io/licensing fuer Details.

#include <QTextEdit>
#include <QTextCursor>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "protected_range.h"

namespace Rte {

/**
 * @brief Schutzrichtlinie bei Schreiboperationen auf geschuetzten
 *        Textbereichen.
 */
enum class ProtectionPolicy {
    None,     // Kein Schutz — geschuetzte Bereiche werden ignoriert
    Warn,     // Warn-Dialog vor dem Löschen geschuetzter Bereiche
    Block,    // Loeschoperationen auf geschuetzten Bereichen werden
              // blockiert (ohne Dialog)
};

/**
 * @brief Signal-Callback für den Schutz-Verstoß-Fall.
 *
 * @param info     Informationen über den geschützten Bereich.
 * @param cursor   Cursor-Position der betroffenen Operation.
 * @return         true wenn die Operation fortgesetzt werden soll,
 *                 false wenn sie abgebrochen werden soll.
 */
using SchutzVerstoßHandler =
    std::function<bool(const SchutzInfo &info, const QTextCursor &cursor)>;

/**
 * @brief RTF-fähige QTextEdit-Subclass mit Schutz-System.
 *
 * RichTextEdit ermoeglicht das Laden und Speichern von RTF-Daten
 * (Delphi/TRichEdit-kompatibel) und bietet ein System zum Schutz
 * von Textbereichen vor Loeschoperationen.
 *
 * Das Widget ist fuer Subclassing konzipiert: Anwendun-gen
 * (wie Medienverwaltung) koennen virtuelle Methoden ueberschreiben,
 * um anwendungsspezifische Verweis-Tags und Schutz-Dialoge
 * zu integrieren.
 *
 * Beispiel (Medienverwaltung):
 * @code
 * class MvEditor : public Rte::RichTextEdit
 * {
 * protected:
 *     void pruefeSchutz(const QTextCursor &cursor, bool &erlaubt) override
 *     {
 *         // MV-spezifische Logik: Schutztypen übersetzen,
 *         // eigene Dialoge anzeigen
 *         Rte::RichTextEdit::pruefeSchutz(cursor, erlaubt);
 *     }
 * };
 * @endcode
 */
class QRTFEDITOR_EXPORT RichTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    /**
     * @brief RichTextEdit erstellen.
     *
     * @param parent  Eltern-Widget.
     */
    explicit RichTextEdit(QWidget *parent = nullptr);

    ~RichTextEdit() override;

    // === Ein-/Ausgabe (Format-agnostic) ===

    /**
     * @brief Inhalt aus einem RTF- oder HTML-Blob laden.
     *
     * @param blob    RTF- oder HTML-String (UTF-8).
     * @param mode    Formatmodus (Rtf oder Html).
     *
     * @throw std::runtime_error bei schwerwiegenden Lesefehlern.
     *
     * Der geladene Inhalt ersetzt den gesamten Dokumenteninhalt.
     * Geschützte Bereiche werden nicht automatisch wiederhergestellt —
     * das muss ggf. vom aufrufenden Code übernommen werden.
     */
    void laden(const std::string &blob, FormatMode mode);

    /**
     * @brief Inhalt als RTF- oder HTML-String serialisieren.
     *
     * @param mode    Formatmodus (Rtf oder Html).
     * @return        Serialisierter String (UTF-8).
     */
    std::string speichern(FormatMode mode) const;

    // === Geschützte Bereiche ===

    /**
     * @brief Geschützten Bereich setzen.
     *
     * @param start   Start-Position im Dokument (0-indexiert).
     * @param ende    Ende-Position (exkl.).
     * @param typ     Typ-Bezeichnung (z. B. "Lexikon", "Person").
     * @param ziel    Ziel-Referenz (z. B. "Schlagwort:Beispiel").
     *
     * Ein bestehender Bereich am gleichen Start wird überschrieben.
     * Neue Bereiche werden sortiert eingefügt.
     */
    void setzeSchutz(std::size_t start, std::size_t ende,
                     std::string typ, std::string ziel);

    /**
     * @brief Alle geschützten Bereiche löschen.
     */
    void loescheSchutz();

    /**
     * @brief Prüfen, ob ein Cursor eine Operation auf geschütztem
     *        Text durchführen darf.
     *
     * @param cursor   Der aktuelle Cursor.
     * @param erlaubt  Wird auf true gesetzt, wenn die Operation
     *                 erlaubt ist, sonst auf false.
     */
    void pruefeSchutz(const QTextCursor &cursor, bool &erlaubt) const;

    /**
     * @brief Prüfen, ob eine Position im geschützten Text liegt.
     *
     * @param position  Cursor-Position im Dokument.
     * @return          true wenn die Position geschützt ist.
     */
    bool istGeschuetzt(std::size_t position) const;

    /**
     * @brief Alle geschützten Bereiche abrufen.
     *
     * @return  Liste aller Schutz-Informationen.
     */
    [[nodiscard]] std::vector<SchutzInfo> alleSchutz() const;

    // === Konfiguration ===

    /**
     * @brief Schutzrichtlinie setzen.
     */
    void setProtectionPolicy(ProtectionPolicy policy);

    /**
     * @brief Schutzrichtlinie abfragen.
     */
    [[nodiscard]] ProtectionPolicy protectionPolicy() const;

    /**
     * @brief Handler für Schutzverstoß-Signale setzen.
     *
     * Der Handler wird aufgerufen, wenn eine Löschoperation auf
     * einem geschützten Bereich attempted wird (bei Warn-Modus).
     *
     * @param handler  Callback-Funktion.
     */
    void setSchutzVerstoßHandler(SchutzVerstoßHandler handler);

    /**
     * @brief Handler für Schutzverstoß-Signale abfragen.
     */
    [[nodiscard]] const SchutzVerstoßHandler &schutzVerstoßHandler() const;

    // === Subclassing (virtuelle Methoden) ===

    /**
     * @brief Tastendruck verarbeiten.
     *
     * Ueberschreiben für MV-spezifische Tastenkuerzel
     * (z. B. Verweis-Einfuegen per Ctrl+Shift+V).
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * @brief Text aus MimeType einfuegen.
     *
     * Ueberschreiben für MV-spezifische Paste-Logik
     * (z. B. Erkennung von MV-Verweis-Tags).
     */
    bool insertFromMimeData(const QString &source) override;

    /**
     * @brief Text einfügen.
     *
     * Ueberschreiben für MV-spezifische Einfüge-Prüfung
     * (z. B. Schutz-Prüfung an Einfügeposition).
     */
    void insertFromMimeData(const QMimeData *source) override;

protected:
    /**
     * @brief Geschützten Bereich setzen (von Subclasses aufrufbar).
     *
     * @param range  Schutz-Information.
     */
    void setzeSchutz(const SchutzInfo &range);

    /**
     * @brief Schutz-Prüfung für Löschoperationen.
     *
     * Diese Methode wird vor keyPressEvent (Backspace/Delete)
     * und insertFromMimeData aufgerufen. Subclasses können die
     * Logik erweitern oder durch eigene ersetzen.
     *
     * @param cursor  Cursor-Position der Operation.
     * @param erlaubt  Wird auf true/false gesetzt.
     */
    void pruefeSchutz(const QTextCursor &cursor, bool &erlaubt) override;

    /**
     * @brief Tastenlose Navigation verarbeiten (z. B. Pfeiltasten).
     *
     * Optional: von Subclasses für spezialisierte Navigation
     * überschrieben.
     */
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    /**
     * @brief RTF-Blob in QTextDocument laden.
     */
    void ladeRtf(const std::string &blob);

    /**
     * @brief HTML-Blob in QTextDocument laden.
     */
    void ladeHtml(const std::string &blob);

    /**
     * @brief QTextDocument als RTF-String serialisieren.
     */
    std::string serialisiereRtf() const;

    /**
     * @brief QTextDocument als HTML-String serialisieren.
     */
    std::string serialisiereHtml() const;

    /**
     * @brief Prüft, ob eine Position in einem geschützten Bereich liegt.
     */
    [[nodiscard]] bool positionInSchutz(std::size_t position) const;

    /**
     * @brief Aktualisiert die interne Liste der geschützten Bereiche.
     *
     * Wird nach jedem Dokumenten-Inhaltswechsel aufgerufen.
     */
    void aktualisiereSchutz();

    // Member
    ProtectionPolicy _protectionPolicy = ProtectionPolicy::None;
    SchutzVerstoßHandler _schutzVerstoßHandler;
    std::vector<ProtectedRange> _schutz;
};

} // namespace Rte

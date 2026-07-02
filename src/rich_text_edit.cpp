// src/rich_text_edit.cpp

#include "rich_text_edit.h"

#include "rtf_import.h"
#include "rtf_export.h"

#include <QTextDocument>
#include <algorithm>
#include <QTextCursor>
#include <QTextFragment>
#include <QKeyEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QInputDialog>
#include <stdexcept>

namespace Rte {

RichTextEdit::RichTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(true);
    setUndoRedoEnabled(true);
}

RichTextEdit::~RichTextEdit() = default;

// === Ein-/Ausgabe ===

void RichTextEdit::laden(const std::string &blob, FormatMode mode) {
    switch (mode) {
        case FormatMode::Rtf:
            ladeRtf(blob);
            break;
        case FormatMode::Html:
            ladeHtml(blob);
            break;
    }
    aktualisiereSchutz();
}

std::string RichTextEdit::speichern(FormatMode mode) const {
    switch (mode) {
        case FormatMode::Rtf:
            return serialisiereRtf();
        case FormatMode::Html:
            return serialisiereHtml();
    }
    return {};
}

// === Geschützte Bereiche ===

void RichTextEdit::setzeSchutz(std::size_t start, std::size_t ende,
                               std::string typ, std::string ziel)
{
    setzeSchutz({ start, ende, std::move(typ), std::move(ziel) });
}

void RichTextEdit::setzeSchutz(const SchutzInfo &range) {
    // Vorhandenen Bereich am gleichen Start überschreiben
    auto it = std::find_if(_schutz.begin(), _schutz.end(),
        [&range](const ProtectedRange &pr) {
            return pr.start() == range.start;
        });

    if (it != _schutz.end()) {
        _schutz.erase(it);
    }

    _schutz.emplace_back(range.start, range.ende,
                         range.typ, range.ziel);

    // Sortieren nach Start-Position
    std::sort(_schutz.begin(), _schutz.end(),
        [](const ProtectedRange &a, const ProtectedRange &b) {
            return a.start() < b.start();
        });
}

void RichTextEdit::loescheSchutz() {
    _schutz.clear();
}

void RichTextEdit::pruefeSchutz(const QTextCursor &cursor,
                                bool &erlaubt) const
{
    erlaubt = true;

    if (_protectionPolicy == ProtectionPolicy::None) {
        return;
    }

    if (!cursor.hasSelection()) {
        return;
    }

    std::size_t start = static_cast<std::size_t>(cursor.selectionStart());
    std::size_t ende = static_cast<std::size_t>(cursor.selectionEnd());

    // Prüfen, ob die Auswahl geschützte Bereiche überschneidet
    for (const auto &range : _schutz) {
        if (range.ueberschneidet(start, ende)) {
            if (_protectionPolicy == ProtectionPolicy::Block) {
                erlaubt = false;
                return;
            }

            // Warn-Modus: Handler aufrufen
            SchutzInfo info{ range.start(), range.ende(),
                             range.typ(), range.ziel() };
            if (_schutzVerstoßHandler) {
                erlaubt = _schutzVerstoßHandler(info, cursor);
            } else {
                // Standard-Dialog
                QMessageBox msgBox(QMessageBox::Warning,
                    "Geschützter Text",
                    QString("Der markierte Bereich enthält einen "
                            "geschützten Verweis (%1).")
                        .arg(QString::fromStdString(range.typ())),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    this);
                msgBox.setCheckBox(
                    new QMessageBox::StandardButton(
                        QMessageBox::Yes,
                        "Nicht wieder fragen"));
                int result = msgBox.exec();
                erlaubt = (result == QMessageBox::Yes);
            }
            if (!erlaubt) {
                return;
            }
        }
    }
}

bool RichTextEdit::istGeschuetzt(std::size_t position) const {
    return positionInSchutz(position);
}

std::vector<SchutzInfo> RichTextEdit::alleSchutz() const {
    std::vector<SchutzInfo> ergebnis;
    ergebnis.reserve(_schutz.size());
    for (const auto &range : _schutz) {
        ergebnis.push_back({ range.start(), range.ende(),
                             range.typ(), range.ziel() });
    }
    return ergebnis;
}

// === Konfiguration ===

void RichTextEdit::setProtectionPolicy(ProtectionPolicy policy) {
    _protectionPolicy = policy;
}

ProtectionPolicy RichTextEdit::protectionPolicy() const {
    return _protectionPolicy;
}

void RichTextEdit::setSchutzVerstoßHandler(SchutzVerstoßHandler handler) {
    _schutzVerstoßHandler = std::move(handler);
}

const SchutzVerstoßHandler &RichTextEdit::schutzVerstoßHandler() const {
    return _schutzVerstoßHandler;
}

// === Subclassing ===

void RichTextEdit::keyPressEvent(QKeyEvent *event) {
    bool erlaubt = true;
    QTextCursor cursor = textCursor();

    // Backspace / Delete: Schutz prüfen
    if (event->key() == Qt::Key_Backspace ||
        event->key() == Qt::Key_Delete)
    {
        if (cursor.hasSelection() || cursor.position() != cursor.anchor()) {
            pruefeSchutz(cursor, erlaubt);
            if (!erlaubt) {
                event->ignore();
                return;
            }
        }
    }

    QTextEdit::keyPressEvent(event);
}

bool RichTextEdit::insertFromMimeData(const QString &source) {
    bool erlaubt = true;
    QTextCursor cursor = textCursor();

    pruefeSchutz(cursor, erlaubt);
    if (!erlaubt) {
        return false;
    }

    return QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::insertFromMimeData(const QMimeData *source) {
    bool erlaubt = true;
    QTextCursor cursor = textCursor();

    pruefeSchutz(cursor, erlaubt);
    if (!erlaubt) {
        return;
    }

    QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::keyReleaseEvent(QKeyEvent *event) {
    QTextEdit::keyReleaseEvent(event);
}

// === Privates ===

void RichTextEdit::ladeRtf(const std::string &blob) {
    auto doc = laden(blob);
    setTextDocument(*doc->dokument());
}

void RichTextEdit::ladeHtml(const std::string &blob) {
    setHtml(QString::fromUtf8(blob.data(), static_cast<int>(blob.size())));
}

std::string RichTextEdit::serialisiereRtf() const {
    return exportRtf(*document());
}

std::string RichTextEdit::serialisiereHtml() const {
    return exportHtml(*document());
}

bool RichTextEdit::positionInSchutz(std::size_t position) const {
    for (const auto &range : _schutz) {
        if (range.umfasst(position)) {
            return true;
        }
    }
    return false;
}

void RichTextEdit::aktualisiereSchutz() {
    // Geschützte Bereiche bleiben nach dem Laden erhalten.
    // Wenn die Dokumenten-Laenge sich ändert, müssen die
    // Positionen ggf. angepasst werden.
    std::size_t docLen = static_cast<std::size_t>(document()->toPlainText().size());

    // Bereiche mit ende > docLen abschneiden
    _schutz.erase(
        std::remove_if(_schutz.begin(), _schutz.end(),
            [docLen](const ProtectedRange &pr) {
                return pr.start() >= docLen;
            }),
        _schutz.end());
}

} // namespace Rte

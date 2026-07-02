// src/rich_text_edit.cpp

#include "rich_text_edit.h"

#include "rtf_import.h"
#include "rtf_export.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QKeyEvent>
#include <QMimeData>
#include <QMessageBox>
#include <algorithm>
#include <stdexcept>

namespace Rte {

RichTextEdit::RichTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(true);
    setUndoRedoEnabled(true);
}

RichTextEdit::~RichTextEdit() = default;

// === I/O ===

void RichTextEdit::load(const std::string &blob, FormatMode mode) {
    switch (mode) {
        case FormatMode::Rtf:
            loadRtf(blob);
            break;
        case FormatMode::Html:
            loadHtml(blob);
            break;
    }
    updateProtection();
}

std::string RichTextEdit::save(FormatMode mode) const {
    switch (mode) {
        case FormatMode::Rtf:
            return serializeRtf();
        case FormatMode::Html:
            return serializeHtml();
    }
    return {};
}

// === Protected ranges ===

void RichTextEdit::setProtection(std::size_t start, std::size_t end,
                                 std::string type, std::string target)
{
    setProtection({ start, end, std::move(type), std::move(target) });
}

void RichTextEdit::setProtection(const ProtectedRangeInfo &info) {
    // Overwrite any existing range at the same start
    auto it = std::find_if(_protection.begin(), _protection.end(),
        [&info](const ProtectedRangeInfo &p) {
            return p.start == info.start;
        });

    if (it != _protection.end()) {
        _protection.erase(it);
    }

    _protection.push_back(info);

    // Sort by start position
    std::sort(_protection.begin(), _protection.end(),
        [](const ProtectedRangeInfo &a, const ProtectedRangeInfo &b) {
            return a.start < b.start;
        });
}

void RichTextEdit::clearProtection() {
    _protection.clear();
}

void RichTextEdit::checkProtection(const QTextCursor &cursor,
                                   bool &allowed) const
{
    allowed = true;

    if (_protectionPolicy == ProtectionPolicy::None) {
        return;
    }

    if (!cursor.hasSelection()) {
        return;
    }

    std::size_t start = static_cast<std::size_t>(cursor.selectionStart());
    std::size_t end = static_cast<std::size_t>(cursor.selectionEnd());

    // Check if the selection overlaps any protected range
    for (const auto &p : _protection) {
        if (start < p.end && end > p.start) {
            if (_protectionPolicy == ProtectionPolicy::Block) {
                allowed = false;
                return;
            }

            // Warn mode: call handler
            if (_protectionViolationHandler) {
                allowed = _protectionViolationHandler(p, cursor);
            } else {
                // Default dialog
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setWindowTitle("Protected Text");
                msgBox.setText(QString("The selected area contains a "
                                       "protected reference (%1).")
                                   .arg(QString::fromStdString(p.type)));
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No
                                          | QMessageBox::Cancel);
                int result = msgBox.exec();
                allowed = (result == QMessageBox::Yes);
            }
            if (!allowed) {
                return;
            }
        }
    }
}

bool RichTextEdit::isProtected(std::size_t position) const {
    return positionInProtection(position);
}

std::vector<ProtectedRangeInfo> RichTextEdit::allProtection() const {
    return _protection;
}

// === Configuration ===

void RichTextEdit::setProtectionPolicy(ProtectionPolicy policy) {
    _protectionPolicy = policy;
}

ProtectionPolicy RichTextEdit::protectionPolicy() const {
    return _protectionPolicy;
}

void RichTextEdit::setProtectionViolationHandler(
    ProtectionViolationHandler handler)
{
    _protectionViolationHandler = std::move(handler);
}

const ProtectionViolationHandler &
RichTextEdit::protectionViolationHandler() const
{
    return _protectionViolationHandler;
}

// === Subclassing ===

void RichTextEdit::keyPressEvent(QKeyEvent *event) {
    bool allowed = true;
    QTextCursor cursor = textCursor();

    // Backspace / Delete: check protection
    if (event->key() == Qt::Key_Backspace ||
        event->key() == Qt::Key_Delete)
    {
        if (cursor.hasSelection() ||
            cursor.position() != cursor.anchor())
        {
            checkProtection(cursor, allowed);
            if (!allowed) {
                event->ignore();
                return;
            }
        }
    }

    QTextEdit::keyPressEvent(event);
}

void RichTextEdit::insertFromMimeData(const QMimeData *source) {
    bool allowed = true;
    QTextCursor cursor = textCursor();

    checkProtection(cursor, allowed);
    if (!allowed) {
        return;
    }

    QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::checkProtection(const QTextCursor &cursor, bool &allowed) {
    // Delegate to the const version
    const RichTextEdit &c = *this;
    c.checkProtection(cursor, allowed);
}

void RichTextEdit::keyReleaseEvent(QKeyEvent *event) {
    QTextEdit::keyReleaseEvent(event);
}

// === Private ===

void RichTextEdit::loadRtf(const std::string &blob) {
    // QTextDocument::setHtml() can also parse RTF-like syntax.
    // For true RTF import with full Delphi compatibility,
    // a dedicated RTF parser will be implemented later.
    QString rtfStr = QString::fromUtf8(blob.data(),
                                       static_cast<int>(blob.size()));

    // Add minimal RTF header if needed
    if (!rtfStr.trimmed().startsWith("{\\rtf")) {
        rtfStr = "{\\rtf1\\ansi\\deff0 " + rtfStr + "}";
    }

    document()->setHtml(rtfStr);
}

void RichTextEdit::loadHtml(const std::string &blob) {
    setHtml(QString::fromUtf8(blob.data(),
                              static_cast<int>(blob.size())));
}

std::string RichTextEdit::serializeRtf() const {
    return exportRtf(*document());
}

std::string RichTextEdit::serializeHtml() const {
    return exportHtml(*document());
}

bool RichTextEdit::positionInProtection(std::size_t position) const {
    for (const auto &p : _protection) {
        if (position >= p.start && position < p.end) {
            return true;
        }
    }
    return false;
}

void RichTextEdit::updateProtection() {
    std::size_t docLen = static_cast<std::size_t>(
        document()->toPlainText().size());

    // Truncate ranges with start >= docLen
    _protection.erase(
        std::remove_if(_protection.begin(), _protection.end(),
            [docLen](const ProtectedRangeInfo &p) {
                return p.start >= docLen;
            }),
        _protection.end());
}

} // namespace Rte

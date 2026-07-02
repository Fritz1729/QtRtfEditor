#include "RichTextEdit.h"

#include "RtfExport.h"
#include "RtfImport.h"

#include <QMessageBox>
#include <algorithm>

namespace Rte {

namespace {

} // namespace

RichTextEdit::RichTextEdit(QWidget* parent)
    : QTextEdit(parent)
{
    setAcceptRichText(true);
    setUndoRedoEnabled(true);
}

RichTextEdit::~RichTextEdit() = default;

void RichTextEdit::Load(const std::string& blob, FormatMode mode) {
    switch (mode) {
        case FormatMode::Rtf:
            LoadRtf(blob);
            break;
        case FormatMode::Html:
            LoadHtml(blob);
            break;
    }
    UpdateProtection();
}

std::string RichTextEdit::Save(FormatMode mode) const {
    switch (mode) {
        case FormatMode::Rtf:
            return SerializeRtf();
        case FormatMode::Html:
            return SerializeHtml();
    }
    return {};
}

void RichTextEdit::SetProtection(std::size_t start, std::size_t end,
                                 std::string type, std::string target)
{
    SetProtection({ start, end, std::move(type), std::move(target) });
}

void RichTextEdit::SetProtection(const ProtectedRangeInfo& info) {
    // Overwrite any existing range at the same start
    auto it = std::find_if(_protection.begin(), _protection.end(),
        [&info](const ProtectedRangeInfo& p) {
            return p.start == info.start;
        });

    if (it != _protection.end()) {
        _protection.erase(it);
    }

    _protection.push_back(info);

    // Sort by start position
    std::sort(_protection.begin(), _protection.end(),
        [](const ProtectedRangeInfo& a, const ProtectedRangeInfo& b) {
            return a.start < b.start;
        });
}

void RichTextEdit::ClearProtection() {
    _protection.clear();
}

void RichTextEdit::CheckProtection(const QTextCursor& cursor,
                                    bool& allowed) const
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

    for (const auto& p : _protection) {
        if (start < p.end && end > p.start) {
            if (_protectionPolicy == ProtectionPolicy::Block) {
                allowed = false;
                return;
            }

            if (_protectionViolationHandler) {
                allowed = _protectionViolationHandler(p, cursor);
            } else {
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

bool RichTextEdit::IsProtected(std::size_t position) const {
    return PositionInProtection(position);
}

std::vector<ProtectedRangeInfo> RichTextEdit::AllProtection() const {
    return _protection;
}

void RichTextEdit::SetProtectionPolicy(ProtectionPolicy policy) {
    _protectionPolicy = policy;
}

ProtectionPolicy RichTextEdit::GetProtectionPolicy() const {
    return _protectionPolicy;
}

void RichTextEdit::SetProtectionViolationHandler(
    ProtectionViolationHandler handler)
{
    _protectionViolationHandler = std::move(handler);
}

const ProtectionViolationHandler&
RichTextEdit::GetProtectionViolationHandler() const
{
    return _protectionViolationHandler;
}

void RichTextEdit::keyPressEvent(QKeyEvent* event) {
    bool allowed = true;
    QTextCursor cursor = textCursor();

    if (event->key() == Qt::Key_Backspace ||
        event->key() == Qt::Key_Delete)
    {
        if (cursor.hasSelection()) {
            CheckProtection(cursor, allowed);
            if (!allowed) {
                event->ignore();
                return;
            }
        }
    }

    QTextEdit::keyPressEvent(event);
}

void RichTextEdit::insertFromMimeData(const QMimeData* source) {
    bool allowed = true;
    QTextCursor cursor = textCursor();

    CheckProtection(cursor, allowed);
    if (!allowed) {
        return;
    }

    QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::CheckProtection(const QTextCursor& cursor, bool& allowed) {
    const_cast<const RichTextEdit*>(this)->CheckProtection(cursor, allowed);
}

void RichTextEdit::LoadRtf(const std::string& blob) {
    ImportRtf(document(), blob);
}

void RichTextEdit::LoadHtml(const std::string& blob) {
    setHtml(QString::fromUtf8(blob.data(),
                              static_cast<int>(blob.size())));
}

std::string RichTextEdit::SerializeRtf() const {
    return ExportRtf(*document());
}

std::string RichTextEdit::SerializeHtml() const {
    return ExportHtml(*document());
}

bool RichTextEdit::PositionInProtection(std::size_t position) const {
    for (const auto& p : _protection) {
        if (position >= p.start && position < p.end) {
            return true;
        }
    }
    return false;
}

void RichTextEdit::UpdateProtection() {
    std::size_t docLen = static_cast<std::size_t>(
        document()->toPlainText().size());

    _protection.erase(
        std::remove_if(_protection.begin(), _protection.end(),
            [docLen](const ProtectedRangeInfo& p) {
                return p.start >= docLen;
            }),
        _protection.end());
}

} // namespace Rte

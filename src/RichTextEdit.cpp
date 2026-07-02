#include "RichTextEdit.h"

#include "RtfExport.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QKeyEvent>
#include <QMimeData>
#include <QMessageBox>
#include <algorithm>
#include <stdexcept>

namespace Rte {

namespace {

std::string ExtractRtfText(const std::string &rtf) {
    std::string result;
    bool inSkipGroup = false;
    int braceDepth = 0;
    bool justAfterControl = false;
    size_t i = 0;

    while (i < rtf.size()) {
        char c = rtf[i];

        if (c == '{') {
            // Detect groups to skip: fonttbl, colortbl, infotbl, etc.
            if (braceDepth == 1 && !inSkipGroup) {
                size_t j = i + 1;
                while (j < rtf.size() && (rtf[j] == ' ' || rtf[j] == '\t' ||
                       rtf[j] == '\r' || rtf[j] == '\n')) j++;
                if (j + 2 < rtf.size() && rtf[j] == '\\' &&
                    (rtf.compare(j + 1, 7, "fonttbl") == 0 ||
                     rtf.compare(j + 1, 8, "colortbl") == 0 ||
                     rtf.compare(j + 1, 7, "infotbl") == 0))
                {
                    inSkipGroup = true;
                    braceDepth = 1;
                    i++;
                    continue;
                }
            }
            braceDepth++;
            i++;
            continue;
        }

        if (c == '}') {
            if (inSkipGroup) {
                braceDepth--;
                if (braceDepth <= 0) {
                    inSkipGroup = false;
                    braceDepth = 1;
                }
            }
            i++;
            continue;
        }

        if (inSkipGroup) {
            i++;
            continue;
        }

        if (c == ';' ) {
            // Semicolon is an RTF separator, sets control mode
            justAfterControl = true;
            i++;
            continue;
        }

        if (c == '\\' && i + 1 < rtf.size()) {
            justAfterControl = false;
            char next = rtf[i + 1];
            if (next == '\\' || next == '{' || next == '}') {
                // Control symbol: \\ \{ \}
                result += next;
                i += 2;
                continue;
            }
            if (next >= '0' && next <= '9') {
                // Control symbol: \1, \2, etc.
                i += 2;
                if (i < rtf.size() && rtf[i] >= '0' && rtf[i] <= '9') i++;
                continue;
            }
            if (next == 'u' || next == 'U') {
                // Unicode escape: \uNNN?
                i += 2;
                while (i < rtf.size() && rtf[i] >= '0' && rtf[i] <= '9') i++;
                if (i < rtf.size() && rtf[i] == '?') i++;
                continue;
            }
            // Control word: skip letters and optional numeric arg
            i++; // skip '\'
            while (i < rtf.size() &&
                   ((rtf[i] >= 'a' && rtf[i] <= 'z') ||
                    (rtf[i] >= 'A' && rtf[i] <= 'Z'))) {
                i++;
            }
            if (i < rtf.size() && rtf[i] >= '0' && rtf[i] <= '9') {
                while (i < rtf.size() && rtf[i] >= '0' && rtf[i] <= '9') i++;
            }
            justAfterControl = true;
            continue;
        }

        // Text character
        if (justAfterControl) {
            if (c == '\n' || c == '\r') {
                // Preserve newlines for paragraph separation
                result += c;
                justAfterControl = false;
                i++;
                continue;
            }
            if (c == ' ' || c == '\t') {
                justAfterControl = false;
                i++;
                continue;
            }
            // Non-whitespace after control word — skip it (part of control
            // word or garbage text)
            i++;
            continue;
        }

        result += c;
        i++;
    }

    return result;
}

} // namespace

RichTextEdit::RichTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(true);
    setUndoRedoEnabled(true);
}

RichTextEdit::~RichTextEdit() = default;

void RichTextEdit::Load(const std::string &blob, FormatMode mode) {
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

void RichTextEdit::SetProtection(const ProtectedRangeInfo &info) {
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

void RichTextEdit::ClearProtection() {
    _protection.clear();
}

void RichTextEdit::CheckProtection(const QTextCursor &cursor,
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

    for (const auto &p : _protection) {
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

const ProtectionViolationHandler &
RichTextEdit::GetProtectionViolationHandler() const
{
    return _protectionViolationHandler;
}

void RichTextEdit::keyPressEvent(QKeyEvent *event) {
    bool allowed = true;
    QTextCursor cursor = textCursor();

    if (event->key() == Qt::Key_Backspace ||
        event->key() == Qt::Key_Delete)
    {
        if (cursor.hasSelection() ||
            cursor.position() != cursor.anchor())
        {
            CheckProtection(cursor, allowed);
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

    CheckProtection(cursor, allowed);
    if (!allowed) {
        return;
    }

    QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::CheckProtection(const QTextCursor &cursor, bool &allowed) {
    // Delegate to the const version
    const RichTextEdit &c = *this;
    c.CheckProtection(cursor, allowed);
}

void RichTextEdit::keyReleaseEvent(QKeyEvent *event) {
    QTextEdit::keyReleaseEvent(event);
}

void RichTextEdit::LoadRtf(const std::string &blob) {
    // Extract plain text content from RTF, stripping control words
    // and groups. For true RTF import with full Delphi compatibility,
    // a dedicated RTF parser will be implemented later.
    std::string text = ExtractRtfText(blob);
    QString textStr = QString::fromUtf8(text.data(),
                                         static_cast<int>(text.size()));
    document()->setPlainText(textStr);
}

void RichTextEdit::LoadHtml(const std::string &blob) {
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
    for (const auto &p : _protection) {
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
            [docLen](const ProtectedRangeInfo &p) {
                return p.start >= docLen;
            }),
        _protection.end());
}

} // namespace Rte

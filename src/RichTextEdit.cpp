#include "RichTextEdit.h"

#include "RtfExport.h"
#include "RtfImport.h"

#include <QTextBlock>
#include <QTextCursor>

namespace Rte {

RichTextEdit::RichTextEdit(QWidget* parent, int codePage)
    : QTextEdit(parent)
    , _codePage(codePage)
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

void RichTextEdit::SetProtection(std::size_t start, std::size_t end) {
    if (start >= end) return;

    // Set user property on the range (best effort — used for RTF export)
    QTextCursor cursor(document());
    cursor.setPosition(static_cast<int>(start));
    cursor.setPosition(static_cast<int>(end), QTextCursor::KeepAnchor);
    QTextCharFormat fmt;
    fmt.setProperty(UserPropProtect, true);
    cursor.mergeCharFormat(fmt);

    _protectedRanges.emplace_back(start, end);
}

void RichTextEdit::ClearProtection() {
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);

    QTextCharFormat fmt;
    fmt.setProperty(UserPropProtect, false);
    cursor.mergeCharFormat(fmt);

    _protectedRanges.clear();
}

bool RichTextEdit::IsProtected(std::size_t position) const {
    for (const auto& [start, end] : _protectedRanges) {
        if (position >= start && position < end) {
            return true;
        }
    }
    return false;
}

void RichTextEdit::SetCodePage(int codePage) {
    _codePage = codePage;
}

int RichTextEdit::codePage() const {
    return _codePage;
}

void RichTextEdit::SyncProtectedRanges() {
    _protectedRanges.clear();

    int docLen = document()->characterCount();
    if (docLen == 0) return;

    std::size_t runStart = 0;
    bool inProtected = false;

    auto checkPos = [docLen, this](std::size_t pos) -> bool {
        if (pos >= static_cast<std::size_t>(docLen)) return false;
        QTextCursor cursor(document());
        cursor.setPosition(static_cast<int>(pos));
        return cursor.charFormat().property(UserPropProtect).toBool();
    };

    for (int i = 0; i <= docLen; ++i) {
        bool protected_ = (i < docLen) && checkPos(static_cast<std::size_t>(i));
        if (protected_ != inProtected) {
            if (inProtected) {
                _protectedRanges.emplace_back(runStart, static_cast<std::size_t>(i));
            } else {
                runStart = static_cast<std::size_t>(i);
            }
            inProtected = protected_;
        }
    }
}

bool RichTextEdit::RangeHasProtected(int start, int end) const {
    for (int i = start; i < end; ++i) {
        if (IsProtected(static_cast<std::size_t>(i))) {
            return true;
        }
    }
    return false;
}

void RichTextEdit::keyPressEvent(QKeyEvent* event) {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();

    if (event->key() == Qt::Key_Backspace) {
        if (cursor.hasSelection() && RangeHasProtected(cursor.selectionStart(), cursor.selectionEnd())) {
            event->ignore();
            return;
        } else if (pos > 0 && IsProtected(static_cast<std::size_t>(pos - 1))) {
            event->ignore();
            return;
        }
    } else if (event->key() == Qt::Key_Delete) {
        if (cursor.hasSelection() && RangeHasProtected(cursor.selectionStart(), cursor.selectionEnd())) {
            event->ignore();
            return;
        } else {
            int docLen = document()->characterCount();
            if (IsProtected(static_cast<std::size_t>(pos)) ||
                (pos < docLen - 1 && IsProtected(static_cast<std::size_t>(pos + 1))))
            {
                event->ignore();
                return;
            }
        }
    }

    QTextEdit::keyPressEvent(event);
    SyncProtectedRanges();
    ClampCursor();
}

void RichTextEdit::mousePressEvent(QMouseEvent* event) {
    QTextEdit::mousePressEvent(event);

    int pos = textCursor().position();
    if (IsProtected(static_cast<std::size_t>(pos))) {
        int start = pos;
        while (start > 0 && IsProtected(static_cast<std::size_t>(start - 1))) {
            start--;
        }
        int end = pos;
        int docLen = document()->characterCount();
        while (end < docLen && IsProtected(static_cast<std::size_t>(end))) {
            end++;
        }

        QTextCursor run(document());
        run.setPosition(start);
        run.setPosition(end, QTextCursor::KeepAnchor);

        emit protectedRegionClicked(start, end, run.selectedText());
    }

    ClampCursor();
}

void RichTextEdit::insertFromMimeData(const QMimeData* source) {
    ClampCursor();
    QTextEdit::insertFromMimeData(source);
    SyncProtectedRanges();
    ClampCursor();
}

void RichTextEdit::ClampCursor() {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();

    if (!IsProtected(static_cast<std::size_t>(pos))) {
        return;
    }

    // Skip forward past the protected run
    int docLen = document()->characterCount();
    while (pos < docLen && IsProtected(static_cast<std::size_t>(pos))) {
        pos++;
    }
    cursor.setPosition(pos);
    setTextCursor(cursor);
}

void RichTextEdit::LoadRtf(const std::string& blob) {
    ImportRtf(document(), blob, _codePage);
    SyncProtectedRanges();

    // Apply default tab stop distance (stored during import as twips)
    int tabStopTwips = document()->property(UserPropMetaDefaultTabStopTwips).toInt();
    if (tabStopTwips > 0) {
        // Convert twips to points (1 twip = 1/1440 inch, 1 point = 1/72 inch)
        // Then to pixels (assume 96 DPI: 1 point = 4/3 pixels)
        qreal twipsToPixels = 96.0 / 1440.0;
        setTabStopDistance(tabStopTwips * twipsToPixels);
    }
}

void RichTextEdit::LoadHtml(const std::string& blob) {
    setHtml(QString::fromUtf8(blob.data(),
                                static_cast<int>(blob.size())));
    _protectedRanges.clear();
}

std::string RichTextEdit::SerializeRtf() const {
    return ExportRtf(*document());
}

std::string RichTextEdit::SerializeHtml() const {
    return ExportHtml(*document());
}

} // namespace Rte

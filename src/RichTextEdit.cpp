#include "RichTextEdit.h"

#include "RtfExport.h"
#include "RtfImport.h"

#include <QTextBlock>
#include <QTextCursor>
#include <algorithm>

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

    QTextCursor cursor(document());
    cursor.setPosition(static_cast<int>(start));
    cursor.setPosition(static_cast<int>(end), QTextCursor::KeepAnchor);
    QTextCharFormat fmt;
    fmt.setProperty(UserPropProtect, true);
    cursor.mergeCharFormat(fmt);
}

void RichTextEdit::ClearProtection() {
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);

    QTextCharFormat fmt;
    fmt.setProperty(UserPropProtect, false);
    cursor.mergeCharFormat(fmt);
}

bool RichTextEdit::IsProtected(std::size_t position) const {
    if (position >= static_cast<std::size_t>(document()->characterCount())) {
        return false;
    }
    QTextBlock block = document()->findBlock(static_cast<int>(position));
    if (!block.isValid()) {
        return false;
    }
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment& frag = it.fragment();
        int fragStart = frag.position();
        int fragEnd = fragStart + frag.length();
        if (static_cast<std::size_t>(fragStart) <= position && position < static_cast<std::size_t>(fragEnd)) {
            return frag.charFormat().property(UserPropProtect).toBool();
        }
    }
    return false;
}

void RichTextEdit::SetCodePage(int codePage) {
    _codePage = codePage;
}

int RichTextEdit::CodePage() const {
    return _codePage;
}


bool RichTextEdit::RangeHasProtected(int start, int end) const {
    if (start >= end) {
        return false;
    }
    QTextBlock block = document()->findBlock(start);
    int pos = start;
    while (block.isValid() && pos < end) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment& frag = it.fragment();
            int fragStart = frag.position();
            int fragEnd = fragStart + frag.length();
            if (fragStart >= end) {
                break;
            }
            if (fragEnd > start && frag.charFormat().property(UserPropProtect).toBool()) {
                return true;
            }
            pos = std::max(pos, fragEnd);
        }
        block = block.next();
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
    bool forward = (event->key() != Qt::Key_Left);
    ClampCursor(forward);
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
        run.setPosition(end < document()->characterCount() ? end : document()->characterCount(), QTextCursor::KeepAnchor);

        emit protectedRegionClicked(start, end, run.selectedText());
    }

    ClampCursor();
}

void RichTextEdit::insertFromMimeData(const QMimeData* source) {
    ClampCursor();
    QTextEdit::insertFromMimeData(source);
    ClampCursor();
}

void RichTextEdit::ClampCursor(bool forward) {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    int docLen = document()->characterCount();

    if (!IsProtected(static_cast<std::size_t>(pos))) {
        return;
    }

    if (forward) {
        while (pos < docLen && IsProtected(static_cast<std::size_t>(pos))) {
            pos++;
        }
    } else {
        while (pos > 0 && IsProtected(static_cast<std::size_t>(pos - 1))) {
            pos--;
        }
    }
    cursor.setPosition(pos < document()->characterCount() ? pos : document()->characterCount());
    setTextCursor(cursor);
}

void RichTextEdit::LoadRtf(const std::string& blob) {
    ImportRtf(document(), blob, _codePage);

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
}

std::string RichTextEdit::SerializeRtf() const {
    return ExportRtf(*document());
}

std::string RichTextEdit::SerializeHtml() const {
    return ExportHtml(*document());
}

} // namespace Rte

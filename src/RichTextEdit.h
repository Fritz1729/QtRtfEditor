#pragma once

// Reusable RTF-capable QTextEdit subclass.
//
// Supports:
//   - Loading/saving RTF (Delphi/TRichEdit-compatible)
//   - Protected text ranges via \protect format — cursor skips
//     over protected text, preventing any edit within it
//   - Subclassing (virtual methods) for application-specific
//     extensions
//
// License: Dual (LGPL-3.0-or-later + commercial license).
// See https://www.qt.io/licensing for details.

#include <QTextEdit>
#include <QTextCursor>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <string>
#include <vector>

// Symbol export macro (DLL/shared builds)
#ifndef RTE_EXPORT
#  ifdef RTE_BUILD_LIBRARY
#    if defined(Q_OS_WIN)
#      define RTE_EXPORT __declspec(dllexport)
#    else
#      define RTE_EXPORT __attribute__((visibility("default")))
#    endif
#  else
#    define RTE_EXPORT
#  endif
#endif

#include "RtfTypes.h"

namespace Rte {

enum class FormatMode { Rtf, Html };

/**
 * @brief RTF-capable QTextEdit subclass with \protect support.
 *
 * RichTextEdit allows loading and saving RTF data
 * (Delphi/TRichEdit-compatible) and supports the \protect
 * character formatting flag. The cursor cannot enter
 * protected text — it is clamped to the nearest unprotected
 * position after every cursor move.
 *
 * Single-click on protected text emits the protectedRegionClicked
 * signal with the content of the clicked region, allowing the
 * embedding application to respond (e.g., navigate to a reference).
 */
class RTE_EXPORT RichTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit RichTextEdit(QWidget* parent = nullptr, int codePage = 1252);

    ~RichTextEdit() override;

    /**
     * @brief Load content from an RTF or HTML blob.
     * @param blob  RTF or HTML string (UTF-8).
     * @param mode  Format mode (Rtf or Html).
     *
     * @throws std::runtime_error on serious read errors.
     *
     * Loaded content replaces all document content.
     * \protect format is preserved via QTextCharFormat user property.
     */
    void Load(const std::string& blob, FormatMode mode);

    std::string Save(FormatMode mode) const;

    /**
     * @brief Apply protected format to a text range.
     * @param start  Start position in document (0-indexed).
     * @param end    End position (exclusive).
     *
     * Text in the range receives the \protect format property.
     * The cursor will skip over this range.
     */
    void SetProtection(std::size_t start, std::size_t end);

    /**
     * @brief Remove protected format from all text in the document.
     */
    void ClearProtection();

    /**
     * @brief Check whether a position lies within protected text.
     * @param position  Cursor position in the document.
     * @return          true if the character at the position is protected.
     */
    [[nodiscard]] bool IsProtected(std::size_t position) const;

    /**
     * @brief Set the default code page for ANSI hex escape decoding.
     * @param codePage  Windows code page number (default 1252).
     */
    void SetCodePage(int codePage);

    /**
     * @brief Get the default code page for ANSI hex escape decoding.
     * @return Code page number (default 1252).
     */
    [[nodiscard]] int codePage() const;

    void keyPressEvent(QKeyEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;

    void insertFromMimeData(const QMimeData* source) override;

signals:
    /**
     * @brief Emitted when the user clicks inside protected text.
     * @param start  Start position of the protected run.
     * @param end    End position of the protected run (exclusive).
     * @param text   Text content of the protected run.
     *
     * The cursor is clamped out of the protected region after
     * this signal is emitted.
     */
    void protectedRegionClicked(std::size_t start, std::size_t end,
                                 const QString& text);

private:
    void LoadRtf(const std::string& blob);

    void LoadHtml(const std::string& blob);

    std::string SerializeRtf() const;

    std::string SerializeHtml() const;

    /**
     * @brief If cursor is inside protected text, move it to the
     *        nearest unprotected position.
     * @param forward  If true, skip forward past the protected run.
     *                 If false, skip backward past the protected run.
     */
    void ClampCursor(bool forward = true);

    /**
     * @brief Rebuild _protectedRanges from user property in the document.
     */
    void SyncProtectedRanges();

    bool RangeHasProtected(int start, int end) const;

    std::vector<std::pair<std::size_t, std::size_t>> _protectedRanges;
    int _codePage = 1252;
};

} // namespace Rte

#pragma once
// src/rich_text_edit.h
//
// Reusable RTF-capable QTextEdit subclass.
//
// Supports:
//   - Loading/saving RTF (Delphi/TRichEdit-compatible)
//   - Protected text ranges with configurable protection policy
//   - Subclassing (virtual methods) for application-specific
//     extensions (e.g., MV-specific reference tags)
//
// License: Dual (LGPL-3.0-or-later + commercial license).
// See https://www.qt.io/licensing for details.

#include <QTextEdit>
#include <QTextCursor>
#include <QKeyEvent>
#include <QMimeData>
#include <memory>
#include <string>
#include <vector>
#include <functional>

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

#include "protected_range.h"

namespace Rte {

/**
 * @brief Format mode: RTF or HTML.
 */
enum class FormatMode { Rtf, Html };

/**
 * @brief Protection policy for write operations on protected
 *        text ranges.
 */
enum class ProtectionPolicy {
    None,     // No protection — protected ranges are ignored
    Warn,     // Warning dialog before deleting protected text
    Block,    // Delete operations on protected ranges are blocked
              // (without dialog)
};

/**
 * @brief Signal callback for protected-range violations.
 *
 * @param info     Information about the protected range.
 * @param cursor   Cursor position of the affected operation.
 * @return         true if the operation should proceed,
 *                 false if it should be cancelled.
 */
using ProtectionViolationHandler =
    std::function<bool(const ProtectedRangeInfo &info,
                       const QTextCursor &cursor)>;

/**
 * @brief RTF-capable QTextEdit subclass with a protection system.
 *
 * RichTextEdit allows loading and saving RTF data
 * (Delphi/TRichEdit-compatible) and provides a system for protecting
 * text ranges from deletion operations.
 *
 * The widget is designed for subclassing: applications
 * (such as Medienverwaltung) can override virtual methods to
 * integrate application-specific reference tags and protection dialogs.
 *
 * Example (Medienverwaltung):
 * @code
 * class MvEditor : public Rte::RichTextEdit
 * {
 * protected:
 *     void checkProtection(const QTextCursor &cursor, bool &allowed) override
 *     {
 *         // MV-specific logic: translate protection types,
 *         // show custom dialogs
 *         Rte::RichTextEdit::checkProtection(cursor, allowed);
 *     }
 * };
 * @endcode
 */
class RTE_EXPORT RichTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    /**
     * @brief Create a RichTextEdit.
     *
     * @param parent  Parent widget.
     */
    explicit RichTextEdit(QWidget *parent = nullptr);

    ~RichTextEdit() override;

    // === I/O (format-agnostic) ===

    /**
     * @brief Load content from an RTF or HTML blob.
     *
     * @param blob    RTF or HTML string (UTF-8).
     * @param mode    Format mode (Rtf or Html).
     *
     * @throws std::runtime_error on serious read errors.
     *
     * The loaded content replaces all document content.
     * Protected ranges are not automatically restored —
     * the calling code must handle this if needed.
     */
    void load(const std::string &blob, FormatMode mode);

    /**
     * @brief Serialize content as RTF or HTML string.
     *
     * @param mode    Format mode (Rtf or Html).
     * @return        Serialized string (UTF-8).
     */
    std::string save(FormatMode mode) const;

    // === Protected ranges ===

    /**
     * @brief Set a protected range.
     *
     * @param start   Start position in document (0-indexed).
     * @param end     End position (exclusive).
     * @param type    Type label (e.g., "lexicon", "person").
     * @param target  Target reference (e.g., "entry:Example").
     *
     * An existing range at the same start is overwritten.
     * New ranges are inserted in sorted order.
     */
    void setProtection(std::size_t start, std::size_t end,
                       std::string type, std::string target);

    /**
     * @brief Clear all protected ranges.
     */
    void clearProtection();

    /**
     * @brief Check whether a cursor may perform an operation on
     *        protected text.
     *
     * @param cursor  The current cursor.
     * @param allowed Set to true if the operation is allowed,
     *                otherwise false.
     */
    void checkProtection(const QTextCursor &cursor, bool &allowed) const;

    /**
     * @brief Check whether a position lies within a protected range.
     *
     * @param position  Cursor position in the document.
     * @return          true if the position is protected.
     */
    [[nodiscard]] bool isProtected(std::size_t position) const;

    /**
     * @brief Retrieve all protected ranges.
     *
     * @return  List of all protection information.
     */
    [[nodiscard]] std::vector<ProtectedRangeInfo> allProtection() const;

    // === Configuration ===

    /**
     * @brief Set the protection policy.
     */
    void setProtectionPolicy(ProtectionPolicy policy);

    /**
     * @brief Get the protection policy.
     */
    [[nodiscard]] ProtectionPolicy protectionPolicy() const;

    /**
     * @brief Set a handler for protection violations.
     *
     * The handler is called when a deletion operation attempts
     * to modify a protected range (in Warn mode).
     *
     * @param handler  Callback function.
     */
    void setProtectionViolationHandler(ProtectionViolationHandler handler);

    /**
     * @brief Get the current protection violation handler.
     */
    [[nodiscard]] const ProtectionViolationHandler &protectionViolationHandler() const;

    // === Subclassing (virtual methods) ===

    /**
     * @brief Process keyboard input.
     *
     * Override for MV-specific keyboard shortcuts
     * (e.g., inserting references via Ctrl+Shift+V).
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * @brief Insert text from MIME data.
     *
     * Override for MV-specific paste logic
     * (e.g., detecting MV reference tags).
     */
    void insertFromMimeData(const QMimeData *source) override;

protected:
    /**
     * @brief Set a protected range (callable from subclasses).
     *
     * @param info  Protection information.
     */
    void setProtection(const ProtectedRangeInfo &info);

    /**
     * @brief Protection check for deletion operations.
     *
     * This method is called before keyPressEvent (Backspace/Delete)
     * and insertFromMimeData. Subclasses can extend or replace
     * the logic with custom behavior.
     *
     * @param cursor  Cursor position of the operation.
     * @param allowed Set to true/false.
     */
    virtual void checkProtection(const QTextCursor &cursor, bool &allowed);

    /**
     * @brief Process key release events (e.g., arrow keys).
     *
     * Optional: override for specialized navigation.
     */
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    /**
     * @brief Load an RTF blob into the QTextDocument.
     */
    void loadRtf(const std::string &blob);

    /**
     * @brief Load an HTML blob into the QTextDocument.
     */
    void loadHtml(const std::string &blob);

    /**
     * @brief Serialize the QTextDocument as an RTF string.
     */
    std::string serializeRtf() const;

    /**
     * @brief Serialize the QTextDocument as an HTML string.
     */
    std::string serializeHtml() const;

    /**
     * @brief Check whether a position falls within a protected range.
     */
    [[nodiscard]] bool positionInProtection(std::size_t position) const;

    /**
     * @brief Update the internal list of protected ranges.
     *
     * Called after every document content change.
     */
    void updateProtection();

    // Members
    ProtectionPolicy _protectionPolicy = ProtectionPolicy::None;
    ProtectionViolationHandler _protectionViolationHandler;
    std::vector<ProtectedRangeInfo> _protection;
};

} // namespace Rte

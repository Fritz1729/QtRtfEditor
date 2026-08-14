#pragma once

#include "RtfControl.h"
#include "RtfTypes.h"

#include <string>
#include <vector>

namespace Rte {

/**
 * @brief Manages table parsing state during RTF document parsing.
 *
 * Tracks table/row/cell nesting, accumulates cell runs and formatting,
 * and handles border/padding control words. The main parser delegates
 * table-related control words and run finalization to this class.
 */
class TableParser {
public:
    explicit TableParser(RtfDocument& doc);

    // State queries.
    [[nodiscard]] bool InTable() const;
    [[nodiscard]] bool InRow() const;
    [[nodiscard]] bool InCell() const;

    // Handle a table control word.
    void HandleControl(RtfControl::TableCtrlWord ctrl, int arg);

    // Finalize a run into the current cell. Call only when InCell() is true.
    void FinalizeRunInCell(std::string&& text, const RtfRunFormat& format);

    // Close the current cell: apply pending border, add to row, reset cell state.
    void CloseCell();

    // Flush pending row when encountering \par or group exit.
    void FlushOnParagraph();

    // Flush pending row at end of input.
    void FlushOnEof();

    // Reset all state for a new parse session.
    void Reset();

    // Accessors for direct cell run access (field result parsing).
    std::vector<RtfRun>& MutableCellRuns();

private:
    struct PendingBorder {
        TableSide side = Side_Undefined;
        int style = 0;
        int width = 0;
        int color = 0;
        bool isRow = false;
    };

    void ApplyPendingBorder();
    void ResetPendingBorder();
    void BeginBorderSide(TableSide side, bool isRow);
    void AddCurrentCellToRow();
    void EmitTableRow();

    static bool TableRowHasNonWhitespaceContent(const RtfTableRowEntry& row);

    RtfDocument& _doc;

    bool _inTable = false;
    bool _inRow = false;
    bool _inCell = false;
    size_t _currentCellIndex = 0;

    RtfTableRowEntry _currentRow;
    std::vector<RtfRun> _currentCellRuns;
    TableCellFormat _currentCellFormat;

    PendingBorder _pendingBorder;
};

} // namespace Rte

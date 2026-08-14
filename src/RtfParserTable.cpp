#include "RtfParserTable.h"
#include "RtfParserContext.h"

namespace Rte {

namespace {

static_assert(static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignCenter) -
              static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignTop) == 1);
static_assert(static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignBottom) -
              static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignCenter) == 1);

constexpr TableSide CtrlWordToSide(RtfControl::TableCtrlWord ctrl) {
    switch (ctrl) {
        case RtfControl::TableCtrlWord::ClBorderLeft:
        case RtfControl::TableCtrlWord::ClPadLeft:
        case RtfControl::TableCtrlWord::TrPadLeft:
        case RtfControl::TableCtrlWord::TrBorderLeft:
            return Side_Left;
        case RtfControl::TableCtrlWord::ClBorderTop:
        case RtfControl::TableCtrlWord::ClPadTop:
        case RtfControl::TableCtrlWord::TrPadTop:
        case RtfControl::TableCtrlWord::TrBorderTop:
            return Side_Top;
        case RtfControl::TableCtrlWord::ClBorderRight:
        case RtfControl::TableCtrlWord::ClPadRight:
        case RtfControl::TableCtrlWord::TrPadRight:
        case RtfControl::TableCtrlWord::TrBorderRight:
            return Side_Right;
        case RtfControl::TableCtrlWord::ClBorderBottom:
        case RtfControl::TableCtrlWord::ClPadBottom:
        case RtfControl::TableCtrlWord::TrPadBottom:
        case RtfControl::TableCtrlWord::TrBorderBottom:
            return Side_Bottom;
        case RtfControl::TableCtrlWord::Trowd:
        case RtfControl::TableCtrlWord::Cellx:
        case RtfControl::TableCtrlWord::Cell:
        case RtfControl::TableCtrlWord::Row:
        case RtfControl::TableCtrlWord::Intbl:
        case RtfControl::TableCtrlWord::ClShading:
        case RtfControl::TableCtrlWord::ClVertAlignTop:
        case RtfControl::TableCtrlWord::ClVertAlignCenter:
        case RtfControl::TableCtrlWord::ClVertAlignBottom:
        case RtfControl::TableCtrlWord::BrdrSolid:
        case RtfControl::TableCtrlWord::BrdrWidth:
        case RtfControl::TableCtrlWord::BrdrColor:
        case RtfControl::TableCtrlWord::ClMerge:
        case RtfControl::TableCtrlWord::BrdrNone:
        case RtfControl::TableCtrlWord::BrdrDashed:
        case RtfControl::TableCtrlWord::TrAlignLeft:
        case RtfControl::TableCtrlWord::TrAlignCenter:
        case RtfControl::TableCtrlWord::TrAlignRight:
        case RtfControl::TableCtrlWord::TrLeft:
        case RtfControl::TableCtrlWord::TrWidth:
        default:
            return Side_Undefined;
    }
}

} // namespace

TableParser::TableParser(RtfDocument& doc)
    : _doc(doc)
{
}

bool TableParser::InTable() const { return _inTable; }
bool TableParser::InRow() const { return _inRow; }
bool TableParser::InCell() const { return _inCell; }

void TableParser::Reset() {
    _inTable = false;
    _inRow = false;
    _inCell = false;
    _currentCellIndex = 0;
    _currentRow = {};
    _currentCellRuns.clear();
    _currentCellFormat = {};
    ResetPendingBorder();
}

void TableParser::HandleControl(RtfControl::TableCtrlWord ctrl, int arg) {
    switch (ctrl) {
    case RtfControl::TableCtrlWord::Trowd:
        if (!_inTable) {
            _inTable = true;
        }
        _inRow = true;
        _inCell = false;
        _currentCellIndex = 0;
        _currentCellRuns.clear();
        _currentCellFormat = {};
        ResetPendingBorder();
        _currentRow = {};
        _currentRow.tableAlignment = Qt::AlignLeft;
        break;

    case RtfControl::TableCtrlWord::Cellx:
        if (arg >= 0) {
            _currentRow.cellxPositions.push_back(arg);
        }
        break;

    case RtfControl::TableCtrlWord::Intbl:
        _inCell = true;
        _currentCellRuns.clear();
        break;

    case RtfControl::TableCtrlWord::Cell:
        if (_inCell) {
            CloseCell();
        }
        _inCell = false;
        _currentCellIndex++;
        _currentCellFormat = {};
        ResetPendingBorder();
        break;

    case RtfControl::TableCtrlWord::Row:
        if (_inCell) {
            CloseCell();
            _inCell = false;
        }
        ApplyPendingBorder();
        EmitTableRow();
        _inRow = false;
        _currentCellIndex = 0;
        ResetPendingBorder();
        break;

    case RtfControl::TableCtrlWord::ClShading:
        if (arg >= 0) {
            _currentCellFormat.shadingColor = arg;
        }
        break;

    case RtfControl::TableCtrlWord::ClVertAlignTop:
    case RtfControl::TableCtrlWord::ClVertAlignCenter:
    case RtfControl::TableCtrlWord::ClVertAlignBottom:
        _currentCellFormat.vertAlign = static_cast<int>(ctrl) -
            static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignTop);
        break;

    case RtfControl::TableCtrlWord::ClBorderLeft:
    case RtfControl::TableCtrlWord::ClBorderTop:
    case RtfControl::TableCtrlWord::ClBorderRight:
    case RtfControl::TableCtrlWord::ClBorderBottom:
        BeginBorderSide(CtrlWordToSide(ctrl), false);
        break;

    case RtfControl::TableCtrlWord::BrdrSolid:
        _pendingBorder.style = 1;
        break;

    case RtfControl::TableCtrlWord::BrdrWidth:
        if (arg >= 0) _pendingBorder.width = arg;
        break;

    case RtfControl::TableCtrlWord::BrdrColor:
        if (arg >= 0) _pendingBorder.color = arg;
        break;

    case RtfControl::TableCtrlWord::ClMerge:
        break;

    case RtfControl::TableCtrlWord::BrdrNone:
        _pendingBorder.style = 0;
        break;

    case RtfControl::TableCtrlWord::BrdrDashed:
        _pendingBorder.style = 2;
        break;

    case RtfControl::TableCtrlWord::ClPadLeft:
    case RtfControl::TableCtrlWord::ClPadTop:
    case RtfControl::TableCtrlWord::ClPadRight:
    case RtfControl::TableCtrlWord::ClPadBottom:
        if (arg >= 0) {
            _currentCellFormat.padding[CtrlWordToSide(ctrl)] = arg;
        }
        break;

    case RtfControl::TableCtrlWord::TrPadLeft:
    case RtfControl::TableCtrlWord::TrPadTop:
    case RtfControl::TableCtrlWord::TrPadRight:
    case RtfControl::TableCtrlWord::TrPadBottom:
        if (arg >= 0) {
            _currentRow.rowPadding[CtrlWordToSide(ctrl)] = arg;
        }
        break;

    case RtfControl::TableCtrlWord::TrAlignLeft:
        _currentRow.tableAlignment = Qt::AlignLeft;
        break;
    case RtfControl::TableCtrlWord::TrAlignCenter:
        _currentRow.tableAlignment = Qt::AlignHCenter;
        break;
    case RtfControl::TableCtrlWord::TrAlignRight:
        _currentRow.tableAlignment = Qt::AlignRight;
        break;

    case RtfControl::TableCtrlWord::TrLeft:
        if (arg >= 0) _currentRow.tableLeftPosition = arg;
        break;

    case RtfControl::TableCtrlWord::TrWidth:
        if (arg >= 0) _currentRow.tableWidth = arg;
        break;

    case RtfControl::TableCtrlWord::TrBorderLeft:
    case RtfControl::TableCtrlWord::TrBorderTop:
    case RtfControl::TableCtrlWord::TrBorderRight:
    case RtfControl::TableCtrlWord::TrBorderBottom:
        BeginBorderSide(CtrlWordToSide(ctrl), true);
        break;
    }
}

void TableParser::FinalizeRunInCell(std::string&& text, const RtfRunFormat& format) {
    _currentCellRuns.emplace_back(std::move(text), format);
}

void TableParser::CloseCell() {
    ApplyPendingBorder();
    AddCurrentCellToRow();
}

void TableParser::FlushOnParagraph() {
    if (!_inTable || !_inRow) {
        _inTable = false;
        _inRow = false;
        _inCell = false;
        _currentCellIndex = 0;
        _currentRow = {};
        return;
    }
    EmitTableRow();
    _inTable = false;
    _inRow = false;
    _inCell = false;
    _currentCellIndex = 0;
}

void TableParser::FlushOnEof() {
    if (_inCell) {
        CloseCell();
    }
    if (_inRow) {
        EmitTableRow();
    }
    _inTable = false;
    _inRow = false;
    _inCell = false;
    _currentCellIndex = 0;
}

void TableParser::ApplyPendingBorder() {
    if (_pendingBorder.side == Side_Undefined) return;
    auto& borders = _pendingBorder.isRow ? _currentRow.rowBorders : _currentCellFormat.borders;
    int style = _pendingBorder.style;
    if (style == 0 && _pendingBorder.width > 0) style = 1;
    const TableCellBorderMember& members = kBorderMembers[_pendingBorder.side];
    borders.*(members.width) = _pendingBorder.width;
    borders.*(members.color) = _pendingBorder.color;
    borders.*(members.style) = static_cast<BorderStyle>(style);
    ResetPendingBorder();
}

void TableParser::ResetPendingBorder() {
    _pendingBorder = PendingBorder{};
}

void TableParser::BeginBorderSide(TableSide side, bool isRow) {
    ApplyPendingBorder();
    _pendingBorder.side = side;
    _pendingBorder.style = 0;
    _pendingBorder.width = 0;
    _pendingBorder.color = 0;
    _pendingBorder.isRow = isRow;
}

void TableParser::AddCurrentCellToRow() {
    while (_currentRow.cells.size() <= _currentCellIndex) {
        _currentRow.cells.push_back({{}, {}});
    }
    _currentRow.cells[_currentCellIndex] =
        {std::move(_currentCellRuns), _currentCellFormat};
    _currentCellRuns.clear();
    _currentCellFormat = {};
}

void TableParser::EmitTableRow() {
    if (TableRowHasNonWhitespaceContent(_currentRow)) {
        _doc.elements.emplace_back(std::move(_currentRow));
    }
    _currentRow = {};
}

std::vector<RtfRun>& TableParser::MutableCellRuns() {
    return _currentCellRuns;
}

} // namespace Rte

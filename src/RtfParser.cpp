#include "RtfParser.h"
#include "RtfCharset.h"
#include "RtfControl.h"
#include "RtfTypes.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <cctype>
#include <cstring>
#include <map>
#include <stdexcept>

using namespace std;

namespace Rte {

namespace {

static_assert(static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignCenter) -
              static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignTop) == 1);
static_assert(static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignBottom) -
              static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignCenter) == 1);
static_assert(static_cast<int>(RtfControl::TableCtrlWord::TrAlignCenter) -
              static_cast<int>(RtfControl::TableCtrlWord::TrAlignLeft) == 1);
static_assert(static_cast<int>(RtfControl::TableCtrlWord::TrAlignRight) -
              static_cast<int>(RtfControl::TableCtrlWord::TrAlignCenter) == 1);

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

class RtfParserImpl {
public:

    RtfDocument Parse(const string& rtf, int codePage) {
        _doc = RtfDocument{};
        _codePage = codePage;
        _doc.codePage = codePage;
        _rtf = rtf;
        _pos = 0;
        _len = rtf.size();
        _literalText.clear();
        _skipLeadingWsTrim = false;
        _paragraphFlushed = false;
        _iter = 0;

        _format = RtfRunFormat{};
        _para = ParagraphFormatting{};
        _formatStack.clear();
        _inColortbl = false;
        _inFonttbl = false;
        _inPict = false;
        _inListtable = false;
        _listIdToStyle.clear();
        _paraStateStack.clear();
        _inTable = false;
        _inRow = false;
        _inTableCell = false;
        _currentCellIndex = 0;
        _pendingBorderSide = Side_Undefined;
        _listId = 0;
        _listLevel = 0;
        _listStyle = ListStyle::None;
        _inFieldRslt = false;
        _fieldAnchorHref.clear();

        Parse();
        FinalizeRun();
        // Flush current paragraph if it has content
        FlushCurrentParagraph();
        // Remove trailing empty paragraphs and table rows from elements
        RemoveTrailingEmptyElements();

        return _doc;
    }

private:
    static constexpr size_t kMaxIter = 10'000'000;

    void CheckIter() {
        if (++_iter > kMaxIter) throw runtime_error("parser iteration limit");
    }

    void SkipGroup() {
        _pos++;
        int depth = 1;
        while (_pos < _len && depth > 0) {
            CheckIter();
            char c = _rtf[_pos++];
            if (c == '{') depth++;
            else if (c == '}') depth--;
        }
    }

    const RtfControl* FindControl(const char* word) const {
        return Rte::FindControl(word);
    }

    void Dispatch(const RtfControl& ctrl, int arg) {
        switch (ctrl.action) {
        case RtfControl::Action::ToggleCharProp: {
            FinalizeRun();
            // arg < 0 = no argument provided → treat as on
            bool on = (arg >= 0) ? (arg != 0) : true;
            // Preserve leading whitespace after toggle-OFF (e.g. \b0),
            // but trim it after toggle-ON (e.g. \b) since it's just
            // whitespace inside a formatted group.
            _skipLeadingWsTrim = !on;
            const RtfControl::CharProp prop = ctrl.value.charProp;
            switch (prop) {
            case RtfControl::CharProp::Bold:
                _format.bold = on;
                break;
            case RtfControl::CharProp::Italic:
                _format.italic = on;
                break;
            case RtfControl::CharProp::Subscript:
                _format.subscript = on;
                break;
            case RtfControl::CharProp::Superscript:
                _format.superscript = on;
                if (on) _format.subscript = false;
                break;
            case RtfControl::CharProp::Strike:
                _format.strikeOut = on;
                break;
            case RtfControl::CharProp::Kerning:
                _format.kerning = on;
                break;
            case RtfControl::CharProp::Protect:
                _format.protected_ = on;
                break;
            case RtfControl::CharProp::Underline:
            default:
                break;
            }
            break;
        }
        case RtfControl::Action::SetCharProp: {
            FinalizeRun();
            const RtfControl::CharSetProp prop = ctrl.value.charSetProp;
            if (arg < 0) break;
            switch (prop) {
            case RtfControl::CharSetProp::FontIndex:
                _format.fontIndex = arg;
                break;
            case RtfControl::CharSetProp::FontSize:
                _format.fontSize = arg;
                break;
            case RtfControl::CharSetProp::ColorIndex:
                _format.colorIndex = arg;
                break;
            case RtfControl::CharSetProp::BgColorIndex:
                _format.bgColorIndex = arg;
                break;
            case RtfControl::CharSetProp::UpOffset:
                _format.upOffset = arg;
                break;
            case RtfControl::CharSetProp::DnOffset:
                _format.dnOffset = arg;
                break;
            case RtfControl::CharSetProp::Expnd:
                _format.expnd = arg;
                break;
            case RtfControl::CharSetProp::ListId:
                _listId = arg;
                {
                    auto it = _listIdToStyle.find(arg);
                    _listStyle = (it != _listIdToStyle.end()) ? it->second : ListStyle::Number;
                }
                break;
            case RtfControl::CharSetProp::UlColorIndex:
                _format.ulColorIndex = arg;
                break;
            case RtfControl::CharSetProp::HighlightIndex:
                _format.highlightIndex = arg;
                break;
            case RtfControl::CharSetProp::LangId:
                _format.langId = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetParaProp: {
            const RtfControl::ParaProp prop = ctrl.value.paraProp;
            switch (prop) {
            case RtfControl::ParaProp::LeftIndent:
                _para.leftIndent = arg;
                break;
            case RtfControl::ParaProp::FirstLineIndent:
                _para.firstLineIndent = arg;
                break;
            case RtfControl::ParaProp::RightIndent:
                _para.rightIndent = arg;
                break;
            case RtfControl::ParaProp::SpaceBefore:
                _para.spaceBefore = arg;
                break;
            case RtfControl::ParaProp::SpaceAfter:
                _para.spaceAfter = arg;
                break;
            case RtfControl::ParaProp::LineHeight:
                _para.lineHeight = arg;
                break;
            case RtfControl::ParaProp::SlMult:
                if (arg >= 0) _para.slMult = arg;
                break;
            case RtfControl::ParaProp::TabStop:
                if (arg >= 0) {
                    _para.tabStops.push_back({arg, _pendingTabAlignment});
                    _pendingTabAlignment = 1;
                }
                break;
            case RtfControl::ParaProp::ListLevel:
                if (arg >= 0) _listLevel = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetAlignment: {
            const RtfControl::Align align = ctrl.value.align;
            if (align == RtfControl::Align::Center) _para.alignment = 128;
            else if (align == RtfControl::Align::Right) _para.alignment = 2;
            else if (align == RtfControl::Align::Justified) _para.alignment = 4;
            else _para.alignment = 1;
            break;
        }
        case RtfControl::Action::SetTabAlign: {
            const RtfControl::TabAlign tabAlign = ctrl.value.tabAlign;
            switch (tabAlign) {
            case RtfControl::TabAlign::Left:
                _pendingTabAlignment = 1;
                break;
            case RtfControl::TabAlign::Center:
                _pendingTabAlignment = 128;
                break;
            case RtfControl::TabAlign::Right:
                _pendingTabAlignment = 2;
                break;
            case RtfControl::TabAlign::Decimal:
                _pendingTabAlignment = 3;
                break;
            }
            break;
        }
        case RtfControl::Action::SetUlStyle: {
            FinalizeRun();
            const RtfControl::RtfUlStyle style = ctrl.value.ulStyle;
            switch (style) {
            case RtfControl::RtfUlStyle::UlSolid:
                if (arg == 0) {
                    _format.underlineStyle = UnderlineStyle::None;
                    _format.underline = false;
                } else {
                    _format.underlineStyle = UnderlineStyle::Solid;
                    _format.underline = true;
                }
                break;
            case RtfControl::RtfUlStyle::UlDotted:
                _format.underlineStyle = UnderlineStyle::Dotted;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDashed:
                _format.underlineStyle = UnderlineStyle::Dashed;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDashDot:
                _format.underlineStyle = UnderlineStyle::DashDot;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDashDotDot:
                _format.underlineStyle = UnderlineStyle::DashDotDot;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDouble:
                _format.underlineStyle = UnderlineStyle::Double;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlThick:
                _format.underlineStyle = UnderlineStyle::Thick;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlNone:
                _format.underlineStyle = UnderlineStyle::None;
                _format.underline = false;
                break;
            }
            break;
        }
        case RtfControl::Action::SetCapitalization: {
            FinalizeRun();
            const RtfControl::RtfCaps caps = ctrl.value.caps;
            switch (caps) {
            case RtfControl::RtfCaps::CapsAll:
                _format.capitalization = Capitalization::AllCaps;
                break;
            case RtfControl::RtfCaps::CapsSmall:
                _format.capitalization = Capitalization::SmallCaps;
                break;
            case RtfControl::RtfCaps::CapsNone:
                _format.capitalization = Capitalization::None;
                break;
            }
            break;
        }
        case RtfControl::Action::EmitParagraph:
            if (_inTableCell) {
                FinalizeRun();
            } else {
                HandleParagraph();
            }
            return;

        case RtfControl::Action::HeaderControl:
            break;
        case RtfControl::Action::HeaderMetadata:
            if (strcmp(ctrl.keyword, "pard") == 0) {
                // \pard resets paragraph formatting to defaults without creating
                // a new paragraph. Formatting applies to the current paragraph.
                _para = ParagraphFormatting{};
                _pendingTabAlignment = 1;
                _listId = 0;
                _listLevel = 0;
                _listStyle = ListStyle::None;
                return;
            }
            if (strcmp(ctrl.keyword, "plain") == 0) {
                FinalizeRun();
                _format = RtfRunFormat{};
                _skipLeadingWsTrim = false;
                return;
            }
            if (strcmp(ctrl.keyword, "uc") == 0) {
                _currentUc = (arg >= 0) ? arg : 1;
                _doc.ucByteCount = _currentUc;
                return;
            }
            if (strcmp(ctrl.keyword, "deflang") == 0) {
                if (arg >= 0) _doc.defaultLangId = arg;
                return;
            }
            if (strcmp(ctrl.keyword, "viewkind") == 0) {
                if (arg >= 0) _doc.viewKind = arg;
                return;
            }
            if (strcmp(ctrl.keyword, "ansicpg") == 0) {
                if (arg >= 0) _doc.codePage = arg;
                return;
            }
            break;
        case RtfControl::Action::TableControl:
            break;
        case RtfControl::Action::TableControlWord:
            HandleTableControl(ctrl, arg);
            return;
        case RtfControl::Action::GroupPersistent:
            if (strcmp(ctrl.keyword, "deff") == 0) {
                if (arg >= 0) _currentDeff = arg;
            } else if (strcmp(ctrl.keyword, "deftab") == 0) {
                if (arg >= 0) _currentDeftab = arg;
            }
            break;
        case RtfControl::Action::FieldControl:
             break;
         case RtfControl::Action::SpecialChar:
             AppendUtf8(ctrl.value.specialChar);
             break;
        case RtfControl::Action::Unknown:
            break;
        }
    }

    void HandleParagraph() {
        if (_inTable) {
            FlushPendingTableRow();
        }
        FinalizeRun();
        _skipLeadingWsTrim = false;
        FlushCurrentParagraph();
        // Create paragraph for content that follows.
        // \par resets tab stops and list state (paragraph-local) but preserves
        // alignment, indents, and spacing (which persist across paragraphs).
        _currentParagraph = {};
        _para.tabStops.clear();
        _pendingTabAlignment = 1;
        _listId = 0;
        _listLevel = 0;
        _listStyle = ListStyle::None;
    }

    void HandleTableControl(const RtfControl& ctrl, int arg) {
        switch (ctrl.value.tableCtrlWord) {
        case RtfControl::TableCtrlWord::Trowd:
            if (!_inTable) {
                _inTable = true;
                FinalizeRun();
                if (ParagraphHasNonWhitespaceContent(_currentParagraph)) {
                    FlushCurrentParagraph();
                }
            }
            _inRow = true;
            _currentCellIndex = 0;
            _currentCellRuns.clear();
            _currentCellFormat = {};
            ResetPendingBorder();
            _currentRow = {};
            _currentRow.tableAlignment = 0;
            break;

        case RtfControl::TableCtrlWord::Cellx:
            if (arg >= 0) {
                _currentRow.cellxPositions.push_back(arg);
            }
            break;

        case RtfControl::TableCtrlWord::Intbl:
            _inTableCell = true;
            FinalizeRun();
            _currentCellRuns.clear();
            break;

        case RtfControl::TableCtrlWord::Cell:
            if (_inTableCell) {
                FinalizeRun();
                ApplyPendingBorder();
                AddCurrentCellToRow();
                _inTableCell = false;
                _currentCellIndex++;
            }
            _currentCellFormat = {};
            ResetPendingBorder();
            break;

        case RtfControl::TableCtrlWord::Row:
            if (_inTableCell) {
                FinalizeRun();
                ApplyPendingBorder();
                AddCurrentCellToRow();
                _inTableCell = false;
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
            _currentCellFormat.vertAlign = static_cast<int>(ctrl.value.tableCtrlWord) -
                static_cast<int>(RtfControl::TableCtrlWord::ClVertAlignTop);
            break;

        case RtfControl::TableCtrlWord::ClBorderLeft:
        case RtfControl::TableCtrlWord::ClBorderTop:
        case RtfControl::TableCtrlWord::ClBorderRight:
        case RtfControl::TableCtrlWord::ClBorderBottom:
            BeginBorderSide(CtrlWordToSide(ctrl.value.tableCtrlWord), false);
            break;

        case RtfControl::TableCtrlWord::BrdrSolid:
            _pendingBorderStyle = 1;
            break;

        case RtfControl::TableCtrlWord::BrdrWidth:
            if (arg >= 0) _pendingBorderWidth = arg;
            break;

        case RtfControl::TableCtrlWord::BrdrColor:
            if (arg >= 0) _pendingBorderColor = arg;
            break;

        case RtfControl::TableCtrlWord::ClMerge:
            break;

        case RtfControl::TableCtrlWord::BrdrNone:
            _pendingBorderStyle = 0;
            break;

        case RtfControl::TableCtrlWord::BrdrDashed:
            _pendingBorderStyle = 2;
            break;

        case RtfControl::TableCtrlWord::ClPadLeft:
        case RtfControl::TableCtrlWord::ClPadTop:
        case RtfControl::TableCtrlWord::ClPadRight:
        case RtfControl::TableCtrlWord::ClPadBottom:
            if (arg >= 0) {
                _currentCellFormat.padding[CtrlWordToSide(ctrl.value.tableCtrlWord)] = arg;
            }
            break;

        case RtfControl::TableCtrlWord::TrPadLeft:
        case RtfControl::TableCtrlWord::TrPadTop:
        case RtfControl::TableCtrlWord::TrPadRight:
        case RtfControl::TableCtrlWord::TrPadBottom:
            if (arg >= 0) {
                _currentRow.rowPadding[CtrlWordToSide(ctrl.value.tableCtrlWord)] = arg;
            }
            break;

        case RtfControl::TableCtrlWord::TrAlignLeft:
        case RtfControl::TableCtrlWord::TrAlignCenter:
        case RtfControl::TableCtrlWord::TrAlignRight:
            _currentRow.tableAlignment = static_cast<int>(ctrl.value.tableCtrlWord) -
                static_cast<int>(RtfControl::TableCtrlWord::TrAlignLeft);
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
            BeginBorderSide(CtrlWordToSide(ctrl.value.tableCtrlWord), true);
            break;
        }
    }

    void ApplyPendingBorder() {
        if (_pendingBorderSide == Side_Undefined) return;
        auto& borders = _pendingBorderIsRow ? _currentRow.rowBorders : _currentCellFormat.borders;
        int style = _pendingBorderStyle;
        if (style == 0 && _pendingBorderWidth > 0) style = 1;
        const TableCellBorderMember& members = kBorderMembers[_pendingBorderSide];
        borders.*(members.width) = _pendingBorderWidth;
        borders.*(members.color) = _pendingBorderColor;
        borders.*(members.style) = static_cast<BorderStyle>(style);
        ResetPendingBorder();
    }

    void ResetPendingBorder() {
        _pendingBorderSide = Side_Undefined;
        _pendingBorderStyle = 0;
        _pendingBorderWidth = 0;
        _pendingBorderColor = 0;
        _pendingBorderIsRow = false;
    }

    void BeginBorderSide(TableSide side, bool isRow) {
        ApplyPendingBorder();
        _pendingBorderSide = side;
        _pendingBorderStyle = 0;
        _pendingBorderWidth = 0;
        _pendingBorderColor = 0;
        _pendingBorderIsRow = isRow;
    }

    void AddCurrentCellToRow() {
        while (_currentRow.cells.size() <= _currentCellIndex) {
            _currentRow.cells.push_back({{}, {}});
        }
        _currentRow.cells[_currentCellIndex] =
            {std::move(_currentCellRuns), _currentCellFormat};
        _currentCellRuns.clear();
        _currentCellFormat = {};
    }

    void EmitTableRow() {
        if (ParagraphHasNonWhitespaceContent(_currentRow)) {
            _doc.elements.emplace_back(std::move(_currentRow));
        }
        _currentRow = {};
    }

    void FlushPendingTableRow() {
        if (!_inTable || !_inRow) {
            _inTable = false;
            _inRow = false;
            _inTableCell = false;
            _currentCellIndex = 0;
            _currentRow = {};
            return;
        }
        EmitTableRow();
        _inTable = false;
        _inRow = false;
        _inTableCell = false;
        _currentCellIndex = 0;
    }

    void FlushCurrentParagraph() {
        // Skip the initial empty paragraph before any content has been flushed.
        // Empty paragraphs after the first flush are preserved (blank lines).
        if (!_paragraphFlushed && !ParagraphHasNonWhitespaceContent(_currentParagraph)) {
            _currentParagraph = {};
            return;
        }
        _paragraphFlushed = true;
        _currentParagraph.setFormatting(_para);
        _currentParagraph.listId = _listId;
        _currentParagraph.listLevel = _listLevel;
        _currentParagraph.listStyle = _listStyle;
        _currentParagraph.listIndent = _para.leftIndent;
        _currentParagraph.defaultFontIndex = _currentDeff;
        _currentParagraph.defaultTabStopTwips = _currentDeftab;
        _doc.elements.push_back(std::move(_currentParagraph));
        _currentParagraph = {};
    }

    static bool RunHasNonWhitespaceContent(const RtfRun& run) {
        if (!run.text.empty()) {
            for (char c : run.text) {
                if (!isspace(static_cast<unsigned char>(c))) return true;
            }
        }
        return false;
    }

    static bool RunsHaveNonWhitespaceContent(const vector<RtfRun>& runs) {
        for (const RtfRun& r : runs) {
            if (RunHasNonWhitespaceContent(r)) return true;
        }
        return false;
    }

    static bool ParagraphHasNonWhitespaceContent(const RtfParagraph& p) {
        return RunsHaveNonWhitespaceContent(p.runs);
    }

    static bool ParagraphHasNonWhitespaceContent(const RtfTableRowEntry& r) {
        for (const auto& [runs, _] : r.cells) {
            if (RunsHaveNonWhitespaceContent(runs)) return true;
        }
        return false;
    }

    void RemoveTrailingEmptyElements() {
        while (!_doc.elements.empty()) {
            bool hasText = visit([](const auto& elem) -> bool {
                using T = decay_t<decltype(elem)>;
                if constexpr (is_same_v<T, RtfParagraph>) return ParagraphHasNonWhitespaceContent(elem);
                else if constexpr (is_same_v<T, RtfTableRowEntry>) return ParagraphHasNonWhitespaceContent(elem);
                else return true;
            }, _doc.elements.back());
            if (!hasText) _doc.elements.pop_back();
            else break;
        }
    }

    RtfDocument _doc;
    RtfParagraph _currentParagraph;
    string _rtf;
    size_t _pos = 0;
    size_t _len = 0;
    string _literalText;
    bool _skipLeadingWsTrim = false;
    bool _paragraphFlushed = false;
    size_t _iter = 0;
    int _codePage = 1252;
    RtfRunFormat _format;
    ParagraphFormatting _para;
    vector<RtfRunFormat> _formatStack;
    bool _inColortbl = false;
    bool _inFonttbl = false;
    bool _inPict = false;
    bool _inListtable = false;
    bool _inPntext = false;
    map<int, ListStyle> _listIdToStyle;
    vector<ParagraphFormatting> _paraStateStack;
    vector<int> _pendingTabAlignmentStack;
    int _pendingTabAlignment = 1;

    // Group-persistent control words: push on group enter, pop on group exit
    vector<int> _deffStack;
    int _currentDeff = 0;
    vector<int> _deftabStack;
    int _currentDeftab = 180;  // RTF spec default = 180 twips (1/8 inch)
    vector<int> _ucStack;
    int _currentUc = 1;  // RTF spec default = 1 fallback byte after \uXXXX
    int _listId = 0;
    int _listLevel = 0;
    ListStyle _listStyle = ListStyle::None;

    // Pict state
    QByteArray _pictData;
    string _pictFormat;  // "jpg", "png", "bmp"
    int _pictPicw = 0;
    int _pictPich = 0;
    int _pictPicwgoal = 0;
    int _pictPichgoal = 0;
    int _pictPicscalex = 100;
    int _pictPicscaley = 100;
    int _pictPiccropl = 0;
    int _pictPiccropr = 0;
    int _pictPiccropt = 0;
    int _pictPiccropb = 0;

    // Group nesting depth for document-level save
    int _groupDepth = 0;

    // Field parsing state
    bool _inFieldRslt = false;
    string _fieldAnchorHref;

    // Table state
    bool _inTable = false;
    bool _inRow = false;
    bool _inTableCell = false;
    size_t _currentCellIndex = 0;
    RtfTableRowEntry _currentRow;
    vector<RtfRun> _currentCellRuns;
    TableCellFormat _currentCellFormat;
    TableSide _pendingBorderSide = Side_Undefined;
    int _pendingBorderStyle = 0;
    int _pendingBorderWidth = 0;
    int _pendingBorderColor = 0;
    bool _pendingBorderIsRow = false;

    void Parse() {
        while (_pos < _len) {
            CheckIter();
            char c = _rtf[_pos];
            if (c == '{') {
                ParseGroup();
            } else if (c == '}') {
                // Group closing — handled by parseGroup
                break;
            } else if (c == '\\') {
                ParseControl();
            } else if (IsPrintable(c)) {
                AccumulateLiteral(c);
            } else {
                _pos++;
            }
        }
        // Flush pending table row at end of input (outermost parse only)
        if (_inTable && _pos >= _len) {
            FlushPendingTableRow();
        }
    }

    void ParseGroup() {
        _pos++; // skip '{'
        _groupDepth++;
        PushState();

        // Check for known table groups
        SkipWhitespace();
        if (_pos < _len && Matches("\\colortbl")) {
            // Consume \\colortbl control word and optional argument
            _pos += 9;
            while (_pos < _len && IsDigit(_rtf[_pos])) _pos++;
            SkipWhitespace();

            _inColortbl = true;
            ParseColortbl();
            _inColortbl = false;
            RestoreState();
            // parseColortbl consumes the closing '}'
            return;
        }
        if (_pos < _len && Matches("\\fonttbl")) {
            // Consume \\fonttbl control word and optional argument
            _pos += 7;
            while (_pos < _len && IsDigit(_rtf[_pos])) _pos++;
            SkipWhitespace();

            _inFonttbl = true;
            ParseFonttbl();
            _inFonttbl = false;
            RestoreState();
            // parseFonttbl consumes the closing '}'
            return;
        }

        if (_pos < _len && Matches("\\listtable")) {
            _pos += 9;
            SkipWhitespace();
            _inListtable = true;
            ParseListtable();
            _inListtable = false;
            RestoreState();
            return;
        }

        if (_pos < _len && Matches("\\pict")) {
            // Consume \\pict control word
            _pos += 4;
            SkipWhitespace();

            _inPict = true;
            ParsePict();
            _inPict = false;
            RestoreState();
            // parsePict consumes the closing '}'
            return;
        }

        if (_pos < _len && Matches("\\field")) {
            _pos += 5;
            SkipWhitespace();
            ParseField();
            // ParseField consumes the closing '}'
            return;
        }

        if (_pos < _len && Matches("\\pntext")) {
            // Capture raw RTF fragment for roundtrip preservation, then parse content normally
            _pos += 7; // consume \pntext
            while (_pos < _len && IsDigit(_rtf[_pos])) _pos++;
            SkipWhitespace();
            size_t fragStart = _pos;
            _inPntext = true;
            _format.inPntext = true;
            Parse();
            // Finalize pending pntext content as a run with inPntext=true
            // before RestoreState() clears the flag
            FinalizeRun();
            _format.inPntext = false;
            _inPntext = false;
            string frag = _rtf.substr(fragStart, _pos - fragStart);
            _currentParagraph.pntextRtf = frag;
            // Consume closing '}'
            if (_pos < _len && _rtf[_pos] == '}') _pos++;
            return;
        }

        // Check for star-prefixed (destination) group: {\*\word ...}
        // RTF spec: unknown destinations starting with \* are silently ignored
        if (_pos + 1 < _len && _rtf[_pos] == '\\' && _rtf[_pos + 1] == '*') {
            // Skip whitespace after \* and read the destination word
            size_t starPos = _pos;
            _pos += 2; // skip \*
            SkipWhitespace();
            string destWord;
            if (_pos < _len && _rtf[_pos] == '\\') {
                _pos++; // skip the \ before the control word
                SkipWhitespace();
                if (_pos < _len && IsWordChar(_rtf[_pos])) {
                    auto [w, a] = ReadControlWord();
                    destWord = w;
                }
            }
            _pos = starPos; // restore position

            // If unknown destination, skip the entire group silently
            if (!destWord.empty() && !FindControl(destWord.c_str())) {
                RestoreState();
                SkipGroup();
                return;
            }
        }

        // Unknown group — parse contents normally
        Parse();

        // Expect '}'
        if (_pos < _len && _rtf[_pos] == '}') {
            _pos++;
        }

        RestoreState();
    }

    void PushState() {
        _formatStack.push_back(_format);
        _paraStateStack.push_back(_para);
        _pendingTabAlignmentStack.push_back(_pendingTabAlignment);
        _deffStack.push_back(_currentDeff);
        _deftabStack.push_back(_currentDeftab);
        _ucStack.push_back(_currentUc);
    }

    void RestoreState() {
        FinalizeRun();
        if (!_formatStack.empty()) {
            _format = _formatStack.back();
            _formatStack.pop_back();
        }
        if (!_paraStateStack.empty()) {
            _para = _paraStateStack.back();
            _paraStateStack.pop_back();
        }
        if (!_pendingTabAlignmentStack.empty()) {
            _pendingTabAlignment = _pendingTabAlignmentStack.back();
            _pendingTabAlignmentStack.pop_back();
        }
        // Save document-level group-persistent values when exiting outermost group
        if (!_deffStack.empty()) {
            if (_groupDepth == 1)
                _doc.defaultFontIndex = _currentDeff;
            _currentDeff = _deffStack.back();
            _deffStack.pop_back();
        }
        if (!_deftabStack.empty()) {
            if (_groupDepth == 1)
                _doc.defaultTabStopTwips = _currentDeftab;
            _currentDeftab = _deftabStack.back();
            _deftabStack.pop_back();
        }
        if (!_ucStack.empty()) {
            _currentUc = _ucStack.back();
            _ucStack.pop_back();
        }
        if (_groupDepth > 0) _groupDepth--;
    }

    void ParseControl() {
        _pos++; // skip '\'

        if (_pos >= _len) return;

        char c = _rtf[_pos];

        if (IsDigit(c)) {
            // Control symbol: single digit
            _pos++;
        } else if (c == '{') {
            // Escaped literal {
            _pos++;
            _literalText += '{';
        } else if (c == '}') {
            // Escaped literal }
            _pos++;
            _literalText += '}';
        } else if (c == '\\') {
            // Escaped literal backslash.
            // RTF spec: \ followed by { or } produces the literal character
            // (e.g. backslash-brace), handled as single units so { doesn't
            // start a group.
            _literalText += '\\';
            if (_pos + 1 < _len && _rtf[_pos + 1] == '{') {
                _literalText += '{';
                _pos += 2; // skip both \ and {
            } else if (_pos + 1 < _len && _rtf[_pos + 1] == '}') {
                _literalText += '}';
                _pos += 2; // skip both \ and }
            } else {
                _pos++; // skip second backslash only
            }
        } else if (c == 't') {
            // Tab character — only if not followed by more word chars (\trowd etc.)
            if (_pos + 1 >= _len || !IsWordChar(_rtf[_pos + 1])) {
                _pos++;
                _literalText += static_cast<char>(9);
                // Consume space delimiter for \t
                if (_pos < _len && _rtf[_pos] == ' ') _pos++;
            } else {
                ParseControlWord();
                // parseControlWord already consumes the delimiter
            }
        } else if (c == '~') {
            // Non-breaking space
            _pos++;
            AppendUtf8(0x00A0);
        } else if (c == '\'') {
            // Hex escape: \\'hh — charset-aware decoding
            _pos++;
            int val = 0;
            for (int h = 0; h < 2 && _pos < _len; ++h) {
                char hc = _rtf[_pos++];
                int digit;
                if (hc >= '0' && hc <= '9') digit = hc - '0';
                else if (hc >= 'a' && hc <= 'f') digit = hc - 'a' + 10;
                else digit = hc - 'A' + 10;
                val = val * 16 + digit;
            }
            int fcharset = 0;
            string fontFamily;
            int fi = _format.fontIndex;
            if (fi >= 0 && fi < ssize(_doc.fonts)) {
                fcharset = _doc.fonts[static_cast<size_t>(fi)].fcharset;
                fontFamily = _doc.fonts[static_cast<size_t>(fi)].family;
            }
            AppendUtf8(static_cast<uint32_t>(MapHexByteToCodepoint(val, fcharset, _doc.codePage, fontFamily)));
        } else if ((c == 'u' || c == 'U') && _pos + 1 < _len && (IsDigit(_rtf[_pos + 1]) || _rtf[_pos + 1] == '-')) {
            // Unicode escape: \uNNN? (only if 'u' is immediately followed by digit)
            ParseUnicodeEscape();
        } else {
            ParseControlWord();
        }
    }

    void ParseControlWord() {
        auto [word, arg] = ReadControlWord();
        bool hasArg = arg >= 0;

        // Handle negative arguments: \word-NNN (no space between - and digits)
        // RTF spec: minus sign must be followed immediately by one or more digits
        if (!hasArg && _pos + 1 < _len && _rtf[_pos] == '-' && IsDigit(_rtf[_pos + 1])) {
            _pos++; // skip '-'
            arg = 0;
            while (_pos < _len && IsDigit(_rtf[_pos])) {
                arg = arg * 10 + (_rtf[_pos] - '0');
                _pos++;
            }
            arg = -arg;
            hasArg = true;
        }

        ConsumeControlDelimiter(arg, hasArg);
        if (word.empty()) {
            // Consume lone \* — RTF star modifier, no-op when not followed by a control word
            if (_pos < _len && _rtf[_pos] == '*') _pos++;
            return;
        }
        ProcessControlWord(word, arg);
    }

    void ParseUnicodeEscape() {
        _pos++; // skip 'u'
        bool negative = false;
        if (_pos < _len && _rtf[_pos] == '-') {
            negative = true;
            _pos++;
        }
        int val = 0;
        while (_pos < _len && IsDigit(_rtf[_pos])) {
            val = val * 10 + (_rtf[_pos] - '0');
            _pos++;
        }
        if (negative) val = -val;

        // Convert UTF-16 to UTF-8
        int cp = val;
        if (cp < 0) cp += 65536;  // normalize negative UTF-16 values
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate — expect low surrogate
            if (_pos + 1 < _len &&
                _rtf[_pos] == '\\' && _rtf[_pos + 1] == 'u') {
                _pos += 2;
                int low = 0;
                while (_pos < _len && IsDigit(_rtf[_pos])) {
                    low = low * 10 + (_rtf[_pos] - '0');
                    _pos++;
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
        }
         AppendUtf8(static_cast<uint32_t>(cp));

        // Skip \ucN fallback bytes (alternate ANSI encoding after \uXXXX)
        // RTF spec: \ucN sets how many bytes follow \uNNNN for backward compat
        for (int i = 0; i < _currentUc && _pos < _len; ++i) {
            _pos++;
        }
    }

    void ParseColortbl() {
        // {\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
        // Each ';' separates a color entry. First entry is "auto" (may be empty).
        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();
            SkipWhitespace();

            int r = 0, g = 0, b = 0;

            while (_pos < _len && _rtf[_pos] != ';' && _rtf[_pos] != '}') {
                if (Matches("\\red")) {
                    _pos += 4;
                    r = ParseInt();
                } else if (Matches("\\green")) {
                    _pos += 6;
                    g = ParseInt();
                } else if (Matches("\\blue")) {
                    _pos += 5;
                    b = ParseInt();
                } else if (IsPrintable(_rtf[_pos])) {
                    _pos++;
                } else {
                    break;
                }
            }

            // Always add color entry (first entry may be empty "auto" color)
            _doc.colors.push_back({r, g, b});

            if (_pos < _len) _pos++; // skip ';' or '}'
        }
        // Consume the closing '}' of the colortbl group
        if (_pos < _len && _rtf[_pos] == '}') _pos++;
    }

    void ParseFonttbl() {
        // {\fonttbl{\f0\froman\fcharset0 Times New Roman;}
        //        {\f1\fswiss\fcharset0 Arial;}}
        // Each font entry is a group containing \fN and the family name
        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();
            if (_rtf[_pos] == '{') {
                // Font entry group
                _pos++;

                int fcharset = 0;
                string family;

                while (_pos < _len && _rtf[_pos] != '}') {
                    if (_rtf[_pos] == '\\') {
                        if (Matches("\\fcharset")) {
                            _pos += 9;
                            fcharset = ParseInt();
                        } else if (Matches("\\f")) {
                            _pos += 2;
                            ParseInt();
                        } else {
                            ParseControl();
                        }
                    } else if (IsPrintable(_rtf[_pos])) {
                        family += _rtf[_pos++];
                    } else {
                        _pos++;
                    }
                }

                if (_pos < _len) _pos++; // skip '}'

                // Remove leading/trailing whitespace from family
                while (!family.empty() && family.back() == ' ') family.pop_back();
                size_t firstNonSpace = 0;
                while (firstNonSpace < family.size() && family[firstNonSpace] == ' ') firstNonSpace++;
                if (firstNonSpace > 0) family.erase(family.begin(), family.begin() + static_cast<ptrdiff_t>(firstNonSpace));

                if (!family.empty()) {
                    _doc.fonts.push_back({family, fcharset});
                }
            } else {
                _pos++;
            }
        }
        // Consume the closing '}' of the fonttbl group
        if (_pos < _len && _rtf[_pos] == '}') _pos++;
    }

    void ParsePict() {
        // {\pict\pngblip\picw500\pich500\picscalex500\picwgoal250 <hex-data>}
        // Collect hex-encoded image data between control words
        _pictData.clear();
        _pictFormat.clear();
        _pictPicw = 0;
        _pictPich = 0;
        _pictPicwgoal = 0;
        _pictPichgoal = 0;
        _pictPicscalex = 100;
        _pictPicscaley = 100;
        _pictPiccropl = 0;
        _pictPiccropr = 0;
        _pictPiccropt = 0;
        _pictPiccropb = 0;

        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();
            if (_rtf[_pos] == '\\') {
                ParsePictControl();
                // Skip whitespace after control words
                while (_pos < _len && _rtf[_pos] != '}' && IsWhitespace(_rtf[_pos])) {
                    _pos++;
                }
            } else {
                // Hex data — but skip any non-hex characters (whitespace, etc.)
                char c = _rtf[_pos];
                // Only collect uppercase/lowercase hex digits
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                    _pictData += c;
                }
                _pos++;
            }
        }

        if (_pos < _len && _rtf[_pos] == '}') {
            _pos++;
        }

        // Finalize image
        if (!_pictFormat.empty() && !_pictData.isEmpty()) {
            QByteArray rawBytes = QByteArray::fromHex(_pictData.toUpper());
            RtfImage img;
            img.data = rawBytes;
            if (_pictFormat == "jpg") img.format = RtfImageFormat::Jpeg;
            else if (_pictFormat == "png") img.format = RtfImageFormat::Png;
            else if (_pictFormat == "bmp") img.format = RtfImageFormat::Bmp;
            img.picw = _pictPicw;
            img.pich = _pictPich;
            img.picwgoal = _pictPicwgoal;
            img.pichgoal = _pictPichgoal;
            img.picscalex = _pictPicscalex;
            img.picscaley = _pictPicscaley;
            img.piccropl = _pictPiccropl;
            img.piccropr = _pictPiccropr;
            img.piccropt = _pictPiccropt;
            img.piccropb = _pictPiccropb;
            img.rtfPictHex = _pictData.toStdString();
            FlushCurrentParagraph();
            _doc.elements.push_back(std::move(img));
        }
    }

    void ParseField() {
        // {\field{\*\fldinst HYPERLINK "URL"}{\*\fldrslt display text}}
        _fieldAnchorHref.clear();
        _inFieldRslt = false;
        PushState();

        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();

            if (_rtf[_pos] == '{') {
                // Parse sub-group
                _pos++;

                // Check for star-prefixed sub-group: {\*\word ...}
                SkipWhitespace();
                if (_pos + 1 < _len && _rtf[_pos] == '\\' && _rtf[_pos + 1] == '*') {
                    size_t starPos = _pos;
                    _pos += 2;
                    SkipWhitespace();
                    string destWord;
                    if (_pos < _len && _rtf[_pos] == '\\') {
                        _pos++;
                        SkipWhitespace();
                        if (_pos < _len && IsWordChar(_rtf[_pos])) {
                            auto [w, a] = ReadControlWord();
                            destWord = w;
                        }
                    }
                    _pos = starPos;

                    if (destWord == "fldinst") {
                        ParseFldInst();
                    } else if (destWord == "fldrslt") {
                        ParseFldRslt();
                    } else {
                        SkipGroup();
                    }
                } else {
                    SkipGroup();
                }
            } else if (_rtf[_pos] == '\\') {
                // Control word at field level — skip
                ParseControl();
                while (_pos < _len && _rtf[_pos] != '}' && IsWhitespace(_rtf[_pos])) {
                    _pos++;
                }
            } else {
                _pos++;
            }
        }

        // Consume closing '}'
        if (_pos < _len && _rtf[_pos] == '}') _pos++;

        // Save accumulated text and anchor state before RestoreState clears _format
        string savedText = _literalText;
        bool savedIsAnchor = _format.isAnchor;
        string savedHref = _format.anchorHref;
        _literalText.clear();

        RestoreState();

        // Finalize the accumulated text with anchor format after state restoration
        if (!savedText.empty()) {
            RtfRunFormat runFmt = _format;
            if (savedIsAnchor) {
                runFmt.isAnchor = true;
                runFmt.anchorHref = savedHref;
            }
            if (_inTableCell) {
                _currentCellRuns.emplace_back(savedText, runFmt);
            } else {
                _currentParagraph.runs.emplace_back(savedText, runFmt);
            }
        }

        _inFieldRslt = false;
        _fieldAnchorHref.clear();
    }

    void ParseFldInst() {
        // {\*\fldinst HYPERLINK "URL"}
        // Collect instruction text to extract HYPERLINK target
        string inst;
        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();
            if (_rtf[_pos] == '\\') {
                _pos++;
                if (_pos < _len) {
                    char c = _rtf[_pos];
                    if (c == '\\') { _pos++; inst += '\\'; }
                    else if (c == '{') { _pos++; inst += '{'; }
                    else if (c == '}') { _pos++; inst += '}'; }
                    else if (c == '_') {
                        // RTF escape: \_ produces literal space
                        _pos++;
                        inst += ' ';
                    }
                    else {
                        // Control word in instruction — read it
                        auto [w, _] = ReadControlWord();
                        inst += w;
                    }
                }
            } else {
                inst += _rtf[_pos++];
            }
        }
        // Consume closing '}'
        if (_pos < _len && _rtf[_pos] == '}') _pos++;

        // Parse HYPERLINK "URL" from instruction
        // Also handle HYPERLINK \\bkmk3 Name (internal bookmark reference)
        string lowerInst;
        lowerInst.reserve(inst.size());
        transform(inst.begin(), inst.end(), back_inserter(lowerInst),
            [](unsigned char c) { return tolower(c); });

        size_t hyperlinkPos = lowerInst.find("hyperlink");
        if (hyperlinkPos != string::npos) {
            size_t afterHyperlink = hyperlinkPos + 9;
            // Skip whitespace
            while (afterHyperlink < inst.size() && inst[afterHyperlink] == ' ') afterHyperlink++;
            if (afterHyperlink < inst.size()) {
                if (inst[afterHyperlink] == '"') {
                    // Quoted URL: HYPERLINK "https://..."
                    size_t start = afterHyperlink + 1;
                    size_t end = inst.find('"', start);
                    if (end != string::npos) {
                        string rawUrl = inst.substr(start, end - start);
                        // Unescape RTF escapes in URL
                        _fieldAnchorHref = UnescapeRtfString(rawUrl);
                    }
                } else if (inst[afterHyperlink] == '\\') {
                    // Internal bookmark: HYPERLINK \bkmk3 Name
                    // For now, treat \bkmk as bookmark reference
                    size_t skip = afterHyperlink + 1;
                    while (skip < inst.size() && IsWordChar(inst[skip])) skip++;
                    while (skip < inst.size() && (inst[skip] == ' ' || inst[skip] == '_')) skip++;
                    _fieldAnchorHref = string("#") + inst.substr(skip);
                } else {
                    // Unquoted URL — rare but possible
                    size_t start = afterHyperlink;
                    size_t end = start;
                    while (end < inst.size() && inst[end] != ' ' && inst[end] != '\t') end++;
                    _fieldAnchorHref = inst.substr(start, end - start);
                }
            }
        }
    }

    void ParseFldRslt() {
        // {\*\fldrslt display text}
        // Parse content as normal text, but apply anchor format to all runs
        _inFieldRslt = true;
        if (!_fieldAnchorHref.empty()) {
            _format.isAnchor = true;
            _format.anchorHref = _fieldAnchorHref;
        }

        // Parse content normally
        Parse();

        // Consume closing '}'
        if (_pos < _len && _rtf[_pos] == '}') _pos++;

        // Format restored by ParseField before RestoreState call
        _inFieldRslt = false;
    }

    static string UnescapeRtfString(const string& s) {
        string result;
        result.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char next = s[i + 1];
                if (next == '\\' || next == '{' || next == '}') {
                    result += next;
                    i += 2;
                } else if (next == '_') {
                    result += ' ';
                    i += 2;
                } else {
                    result += s[i++];
                }
            } else {
                result += s[i++];
            }
        }
        return result;
    }

    void ParseListtable() {
        while (_pos < _len && _rtf[_pos] != '}') {
            CheckIter();
            if (_rtf[_pos] == '{') {
                _pos++;
                while (_pos < _len && _rtf[_pos] != '}') {
                    if (_rtf[_pos] == '\\') {
                        ParseListtableControl();
                    } else {
                        _pos++;
                    }
                }
                if (_pos < _len && _rtf[_pos] == '}') _pos++;
            } else if (_rtf[_pos] == '\\') {
                ParseListtableControl();
            } else {
                _pos++;
            }
        }
        if (_pos < _len && _rtf[_pos] == '}') _pos++;
    }

    void ParseListtableControl() {
        _pos++;
        if (_pos >= _len) return;

        auto [word, arg] = ReadControlWord();
        bool hasArg = arg >= 0;

        if (word == "list" || word == "listoverride") {
            if (_currentListId > 0 && _currentListStyle != ListStyle::None) {
                _listIdToStyle[_currentListId] = _currentListStyle;
            }
            _currentListId = 0;
            _currentListStyle = ListStyle::None;
        } else if (word == "listid" && hasArg) {
            _currentListId = arg;
            _currentListStyle = ListStyle::None;
        } else if (word == "liststylenum" || word == "liststyletype") {
            if (hasArg && _currentListId > 0) {
                _currentListStyle = RtfStyleTypeToListStyle(arg);
                _listIdToStyle[_currentListId] = _currentListStyle;
            }
        } else if (word == "liststylebulletsimple") {
            if (_currentListId > 0) {
                _currentListStyle = ListStyle::Disc;
                _listIdToStyle[_currentListId] = _currentListStyle;
            }
        } else if (word == "liststylenumberinparen") {
            if (_currentListId > 0) {
                _currentListStyle = ListStyle::Number;
                _listIdToStyle[_currentListId] = _currentListStyle;
            }
        } else if (word == "listname") {
            while (_pos < _len && _rtf[_pos] != '"') _pos++;
            while (_pos < _len && _rtf[_pos] != '"') _pos++;
        } else if (word == "listlevel" && hasArg) {
            if (_currentListId > 0 && _currentListStyle != ListStyle::None) {
                _listIdToStyle[_currentListId] = _currentListStyle;
            }
        }
    }

    static ListStyle RtfStyleTypeToListStyle(int type) {
        switch (type) {
            case 0: return ListStyle::Bullet;
            case 1: return ListStyle::Disc;
            case 2: return ListStyle::Box;
            case 3: return ListStyle::Number;
            case 4: return ListStyle::Roman;
            case 5: return ListStyle::Roman;
            case 6: return ListStyle::Letter;
            case 7: return ListStyle::Letter;
            case 8: return ListStyle::Check;
            case 9: return ListStyle::Check;
            default: return ListStyle::Number;
        }
    }

    int _currentListId = 0;
    ListStyle _currentListStyle = ListStyle::None;

    void ParsePictControl() {
        _pos++; // skip '\'

        if (_pos >= _len) return;

        char c = _rtf[_pos];
        if (IsWordChar(c)) {
            auto [word, arg] = ReadControlWord();
            bool hasArg = arg >= 0;
            if (word.empty()) return;

            // Known pict control words
            if (word == "jpegblip") { _pictFormat = "jpg"; }
            else if (word == "pngblip") { _pictFormat = "png"; }
            else if (word == "dibitmap") { _pictFormat = "bmp"; }
            else if (word == "picw" && hasArg) { _pictPicw = arg; }
            else if (word == "pich" && hasArg) { _pictPich = arg; }
            else if (word == "picwgoal" && hasArg) { _pictPicwgoal = arg; }
            else if (word == "pichgoal" && hasArg) { _pictPichgoal = arg; }
            else if (word == "picscalex" && hasArg) { _pictPicscalex = arg; }
            else if (word == "picscaley" && hasArg) { _pictPicscaley = arg; }
            else if (word == "piccropl" && hasArg) { _pictPiccropl = arg; }
            else if (word == "piccropr" && hasArg) { _pictPiccropr = arg; }
            else if (word == "piccropt" && hasArg) { _pictPiccropt = arg; }
            else if (word == "piccropb" && hasArg) { _pictPiccropb = arg; }
            // Unknown pict control words are silently ignored
            return;
        }

        // Skip other control symbols (digits, etc.)
        if (IsDigit(c)) {
            _pos++;
        }
    }

    void ProcessControlWord(const string& word, int arg) {
        // Table group markers (should have been caught in parseGroup)
        if (word == "colortbl" || word == "fonttbl") return;
        // Star prefix: only meaningful as part of destination {\*\word}
        if (word == "*") return;

        auto* ctrl = FindControl(word.c_str());
        if (ctrl) {
            Dispatch(*ctrl, arg);
        } else {
            // Unknown tag — record for preservation
            string tag = "\\" + word;
            if (arg >= 0) {
                tag += to_string(arg);
            }
            _doc.unknownTags.push_back(tag);
        }
    }

    void AccumulateLiteral(char c) {
        _literalText += c;
        _pos++;
    }

    void FinalizeRun() {
        if (_literalText.empty()) return;

        if (_inTableCell) {
            _currentCellRuns.emplace_back(std::move(_literalText), _format);
        } else {
            _currentParagraph.runs.emplace_back(std::move(_literalText), _format);
        }
     }

    void AppendUtf8(uint32_t cp) {
        if (cp < 0x80) {
            _literalText += static_cast<char>(cp);
        } else if (cp < 0x800) {
            _literalText += static_cast<char>(0xC0 | (cp >> 6));
            _literalText += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            _literalText += static_cast<char>(0xE0 | (cp >> 12));
            _literalText += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            _literalText += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            _literalText += static_cast<char>(0xF0 | (cp >> 18));
            _literalText += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            _literalText += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            _literalText += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool Matches(const char* s) {
        size_t len = strlen(s);
        if (_pos + len > _len) return false;
        for (size_t i = 0; i < len; ++i) {
            if (static_cast<unsigned char>(_rtf[_pos + i]) !=
                static_cast<unsigned char>(s[i]))
            {
                return false;
            }
        }
        // Verify it's a proper control word boundary
        if (_pos + len < _len && IsWordChar(_rtf[_pos + len])) return false;
        return true;
    }

    int ParseInt() {
        int val = 0;
        while (_pos < _len && IsDigit(_rtf[_pos])) {
            val = val * 10 + (_rtf[_pos] - '0');
            _pos++;
        }
        return val;
    }

    pair<string, int> ReadControlWord() {
        string word;
        int arg = 0;
        bool hasArg = false;
        while (_pos < _len && (IsWordChar(_rtf[_pos]) || IsDigit(_rtf[_pos]))) {
            if (_rtf[_pos] >= '0' && _rtf[_pos] <= '9') {
                arg = arg * 10 + (_rtf[_pos] - '0');
                hasArg = true;
            } else {
                word += static_cast<char>(static_cast<unsigned char>(_rtf[_pos]) | 0x20);
            }
            _pos++;
        }
        return {word, hasArg ? arg : -1};
    }

    void ConsumeControlDelimiter(int arg, bool hasArg) {
        (void)arg;
        if (hasArg && _pos < _len && !IsWordChar(_rtf[_pos]) && _rtf[_pos] != '\\' && _rtf[_pos] != '}' && _rtf[_pos] != '{') {
            _pos++;
        } else if (!hasArg && _pos < _len && _rtf[_pos] == ' ') {
            _pos++;
        }
    }

    void SkipWhitespace() {
        while (_pos < _len && IsWhitespace(_rtf[_pos])) {
            _pos++;
        }
    }

    bool IsWordChar(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool IsDigit(char c) const {
        return c >= '0' && c <= '9';
    }

    bool IsWhitespace(char c) const {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    bool IsPrintable(char c) const {
        return c != ';' && static_cast<unsigned char>(c) >= 32 &&
               static_cast<unsigned char>(c) <= 126;
    }
};

} // namespace

RtfDocument ParseRtf(const string& rtf, int codePage) {
    RtfParserImpl impl;
    return impl.Parse(rtf, codePage);
}

} // namespace Rte

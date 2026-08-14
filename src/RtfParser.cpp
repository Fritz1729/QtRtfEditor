#include "RtfParser.h"
#include "RtfParserContext.h"

#include "RtfCharset.h"
#include "RtfControl.h"
#include "RtfInputReader.h"

#include "RtfTypes.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <cctype>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string_view>

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
private:
    struct ParserScope {
        RtfParserImpl& parser;
        ParserScope(RtfParserImpl& p) : parser(p) {
            p._groupDepth++;
            p.PushState();
        }
        ~ParserScope() { parser.RestoreState(); }
    };

public:

    RtfDocument Parse(const string& rtf, int codePage) {
        _doc = RtfDocument{};
        _codePage = codePage;
        _doc.codePage = codePage;
        _input = InputReader(rtf);
        _literalText.clear();
        _skipLeadingWsTrim = false;
        _paragraphFlushed = false;
        _iter = 0;
        _listIdToStyle.clear();
        _inTable = false;
        _inRow = false;
        _inTableCell = false;
        _currentCellIndex = 0;
        _pendingBorderSide = Side_Undefined;
        _listId = 0;
        _listLevel = 0;
        _listStyle = RtfListStyle::None;
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
    void SkipGroup() {
        // The group's own "{" has already been consumed by the caller.
        Rte::SkipGroup(_input, _iter);
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
                _formatScope.get().bold = on;
                break;
            case RtfControl::CharProp::Italic:
                _formatScope.get().italic = on;
                break;
            case RtfControl::CharProp::Subscript:
                _formatScope.get().subscript = on;
                break;
            case RtfControl::CharProp::Superscript:
                _formatScope.get().superscript = on;
                if (on) _formatScope.get().subscript = false;
                break;
            case RtfControl::CharProp::Strike:
                _formatScope.get().strikeOut = on;
                break;
            case RtfControl::CharProp::Kerning:
                _formatScope.get().kerning = on;
                break;
            case RtfControl::CharProp::Protect:
                _formatScope.get().protected_ = on;
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
                _formatScope.get().fontIndex = arg;
                break;
            case RtfControl::CharSetProp::FontSize:
                _formatScope.get().fontSize = arg;
                break;
            case RtfControl::CharSetProp::ColorIndex:
                _formatScope.get().colorIndex = arg;
                break;
            case RtfControl::CharSetProp::BgColorIndex:
                _formatScope.get().bgColorIndex = arg;
                break;
            case RtfControl::CharSetProp::UpOffset:
                _formatScope.get().upOffset = arg;
                break;
            case RtfControl::CharSetProp::DnOffset:
                _formatScope.get().dnOffset = arg;
                break;
            case RtfControl::CharSetProp::Expnd:
                _formatScope.get().expnd = arg;
                break;
            case RtfControl::CharSetProp::ListId:
                _listId = arg;
                {
                    auto it = _listIdToStyle.find(arg);
                    _listStyle = (it != _listIdToStyle.end()) ? it->second : RtfListStyle::Number;
                }
                break;
            case RtfControl::CharSetProp::UlColorIndex:
                _formatScope.get().ulColorIndex = arg;
                break;
            case RtfControl::CharSetProp::HighlightIndex:
                _formatScope.get().highlightIndex = arg;
                break;
            case RtfControl::CharSetProp::LangId:
                _formatScope.get().langId = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetParaProp: {
            const RtfControl::ParaProp prop = ctrl.value.paraProp;
            switch (prop) {
            case RtfControl::ParaProp::LeftIndent:
                _paraScope.get().leftIndent = arg;
                break;
            case RtfControl::ParaProp::FirstLineIndent:
                _paraScope.get().firstLineIndent = arg;
                break;
            case RtfControl::ParaProp::RightIndent:
                _paraScope.get().rightIndent = arg;
                break;
            case RtfControl::ParaProp::SpaceBefore:
                _paraScope.get().spaceBefore = arg;
                break;
            case RtfControl::ParaProp::SpaceAfter:
                _paraScope.get().spaceAfter = arg;
                break;
            case RtfControl::ParaProp::LineHeight:
                _paraScope.get().lineHeight = arg;
                break;
            case RtfControl::ParaProp::SlMult:
                if (arg >= 0) _paraScope.get().slMult = arg;
                break;
            case RtfControl::ParaProp::TabStop:
                if (arg >= 0) {
                    _paraScope.get().tabStops.push_back({arg, _tabAlignScope.get()});
                    _tabAlignScope.get() = Qt::AlignLeft;
                }
                break;
            case RtfControl::ParaProp::ListLevel:
                if (arg >= 0) _listLevel = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetAlignment: {
            _paraScope.get().alignment = static_cast<Qt::Alignment>(ctrl.value.raw);
            break;
        }
        case RtfControl::Action::SetTabAlign: {
            _tabAlignScope.get() = static_cast<Qt::Alignment>(ctrl.value.raw);
            break;
        }
        case RtfControl::Action::SetUlStyle: {
            FinalizeRun();
            const auto style = static_cast<RtfUnderlineStyle>(ctrl.value.raw);
            if (style == RtfUnderlineStyle::NoUnderline) {
                _formatScope.get().underlineStyle = RtfUnderlineStyle::NoUnderline;
                _formatScope.get().underline = false;
            } else if (style == RtfUnderlineStyle::Single && arg == 0) {
                _formatScope.get().underlineStyle = RtfUnderlineStyle::NoUnderline;
                _formatScope.get().underline = false;
            } else {
                _formatScope.get().underlineStyle = style;
                _formatScope.get().underline = (style != RtfUnderlineStyle::NoUnderline);
            }
            break;
        }
        case RtfControl::Action::SetCapitalization: {
            FinalizeRun();
            _formatScope.get().capitalization = static_cast<QFont::Capitalization>(ctrl.value.raw);
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
                _paraScope.get() = ParagraphFormat{};
                _tabAlignScope.get() = Qt::AlignLeft;
                _listId = 0;
                _listLevel = 0;
                _listStyle = RtfListStyle::None;
                return;
            }
            if (strcmp(ctrl.keyword, "plain") == 0) {
                FinalizeRun();
                _formatScope.get() = RtfRunFormat{};
                _skipLeadingWsTrim = false;
                return;
            }
            if (strcmp(ctrl.keyword, "uc") == 0) {
                _ucScope.get() = (arg >= 0) ? arg : 1;
                _doc.ucByteCount = _ucScope.get();
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
                if (arg >= 0) _deffScope.get() = arg;
            } else if (strcmp(ctrl.keyword, "deftab") == 0) {
                if (arg >= 0) _deftabScope.get() = arg;
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
        _paraScope.get().tabStops.clear();
        _tabAlignScope.get() = Qt::AlignLeft;
        _listId = 0;
        _listLevel = 0;
        _listStyle = RtfListStyle::None;
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
            _currentRow.tableAlignment = Qt::AlignLeft;
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
        if (TableRowHasNonWhitespaceContent(_currentRow)) {
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
        RtfParserScopeContext ctx(_input, _doc, _currentParagraph, _iter,
                             {_formatScope, _paraScope, _tabAlignScope, _deffScope, _deftabScope, _ucScope},
                             {_listId, _listLevel, _listStyle, _paragraphFlushed, _listIdToStyle});
        Rte::FlushCurrentParagraph(ctx);
    }

    static bool TableRowHasNonWhitespaceContent(const RtfTableRowEntry& r) {
        for (const auto& [runs, _] : r.cells) {
            for (const auto& run : runs) {
                for (char c : run.text) {
                    if (!isspace(static_cast<unsigned char>(c))) return true;
                }
            }
        }
        return false;
    }

    void RemoveTrailingEmptyElements() {
        while (!_doc.elements.empty()) {
            bool hasText = visit([](const auto& elem) -> bool {
                using T = decay_t<decltype(elem)>;
                if constexpr (is_same_v<T, RtfParagraph>) return ParagraphHasNonWhitespaceContent(elem);
                else if constexpr (is_same_v<T, RtfTableRowEntry>) return TableRowHasNonWhitespaceContent(elem);
                else return true;
            }, _doc.elements.back());
            if (!hasText) _doc.elements.pop_back();
            else break;
        }
    }

    RtfDocument _doc;
    RtfParagraph _currentParagraph;
    InputReader _input;
    string _literalText;
    bool _skipLeadingWsTrim = false;
    bool _paragraphFlushed = false;
    size_t _iter = 0;
    int _codePage = 1252;
    ScopeStack<RtfRunFormat> _formatScope;
    ScopeStack<ParagraphFormat> _paraScope;
    bool _inPntext = false;
    map<int, RtfListStyle> _listIdToStyle;
    ScopeStack<Qt::Alignment> _tabAlignScope{Qt::AlignLeft};

    // Group-persistent control words: push on group enter, pop on group exit
    ScopeStack<int> _deffScope{0};
    ScopeStack<int> _deftabScope{180};  // RTF spec default = 180 twips (1/8 inch)
    ScopeStack<int> _ucScope{1};  // RTF spec default = 1 fallback byte after \uXXXX
    int _listId = 0;
    int _listLevel = 0;
    RtfListStyle _listStyle = RtfListStyle::None;

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
        while (!_input.IsEof()) {
            CheckIter(_iter);
            char c = _input.Peek();
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
                _input.Advance();
            }
        }
        // Flush pending table row at end of input (outermost parse only)
        if (_inTable && _input.IsEof()) {
            FlushPendingTableRow();
        }
    }

    void ParseGroup() {
        _input.SkipAs('{');
        {
            ParserScope scope{*this};

            _input.SkipWhitespace();
            if (_input.ConsumeMatch("\\colortbl")) {
                _input.SkipDigits();
                _input.SkipWhitespace();
                RtfParserContext ctx{_input, _doc, _iter};
                ParseColortbl(ctx);
                return;
            }
            if (_input.ConsumeMatch("\\fonttbl")) {
                _input.SkipDigits();
                _input.SkipWhitespace();
                RtfParserContext ctx{_input, _doc, _iter};
                ParseFonttbl(ctx);
                return;
            }

            if (_input.ConsumeMatch("\\listtable")) {
                _input.SkipWhitespace();
                RtfListtableContext ctx{_input, _iter, _listIdToStyle};
                ParseListtable(ctx);
                return;
            }

            if (_input.ConsumeMatch("\\pict")) {
                _input.SkipWhitespace();
                RtfParserScopeContext ctx(_input, _doc, _currentParagraph, _iter,
                                          {_formatScope, _paraScope, _tabAlignScope, _deffScope, _deftabScope, _ucScope},
                                          {_listId, _listLevel, _listStyle, _paragraphFlushed, _listIdToStyle});
                ParsePict(ctx);
                return;
            }

            if (_input.ConsumeMatch("\\field")) {
                _input.SkipWhitespace();
                ParseField();
                return;
            }

            if (_input.ConsumeMatch("\\pntext")) {
                // Capture raw RTF fragment for roundtrip preservation, then parse content normally
                _input.SkipDigits();
                _input.SkipWhitespace();
                size_t fragStart = _input.Pos();
                _inPntext = true;
                _formatScope.get().inPntext = true;
                Parse();
                FinalizeRun();
                _formatScope.get().inPntext = false;
                _inPntext = false;
                string frag = _input.Substring(fragStart, _input.Pos());
                _currentParagraph.pntextRtf = frag;
                _input.SkipAs('}');
                return;
            }

            // Check for star-prefixed (destination) group: {\*\word ...}
            // RTF spec: unknown destinations starting with \* are silently ignored
            if (_input.PeekIs("\\*")) {
                size_t starPos = _input.Pos();
                _input.AdvanceBy(2); // skip \*
                _input.SkipWhitespace();
                string destWord;
                if (!_input.IsEof() && _input.PeekIs('\\')) {
                    _input.Advance();
                    _input.SkipWhitespace();
                    if (!_input.IsEof() && Rte::IsWordChar(_input.Peek())) {
                        auto [w, a] = _input.ReadControlWord();
                        destWord = w;
                    }
                }
                _input.Seek(starPos);

                // If unknown destination, skip the entire group silently
                if (!destWord.empty() && !FindControl(destWord.c_str())) {
                    SkipGroup();
                    return;
                }
            }

            // Unknown group — parse contents normally
            Parse();
        }

        _input.SkipAs('}');
    }

    void PushState() {
        _formatScope.enterScope();
        _paraScope.enterScope();
        _tabAlignScope.enterScope();
        _deffScope.enterScope();
        _deftabScope.enterScope();
        _ucScope.enterScope();
    }

    void RestoreState() {
        FinalizeRun();
        _formatScope.leaveScope();
        _paraScope.leaveScope();
        _tabAlignScope.leaveScope();
        // Save document-level group-persistent values when exiting outermost group
        if (_groupDepth == 1) {
            _doc.defaultFontIndex = _deffScope.get();
            _doc.defaultTabStopTwips = _deftabScope.get();
        }
        _deffScope.leaveScope();
        _deftabScope.leaveScope();
        _ucScope.leaveScope();
        if (_groupDepth > 0) _groupDepth--;
    }

    void ParseControl() {
        _input.SkipAs('\\');

        if (_input.IsEof()) return;

        char c = _input.Peek();

        if (Rte::IsDigit(c)) {
            _input.SkipAs(c);
        } else if (c == '{') {
            _input.SkipAs('{');
            _literalText += '{';
        } else if (c == '}') {
            _input.SkipAs('}');
            _literalText += '}';
        } else if (c == '\\') {
            // Escaped literal backslash.
            // RTF spec: \ followed by { or } produces the literal character
            // (e.g. backslash-brace), handled as single units so { doesn't
            // start a group.
            _literalText += '\\';
            if (_input.PeekIs('{', 1)) {
                _literalText += '{';
                _input.AdvanceBy(2);
            } else if (_input.PeekIs('}', 1)) {
                _literalText += '}';
                _input.AdvanceBy(2);
            } else {
                _input.SkipAs('\\');
            }
        } else if (c == 't') {
            // Tab character — only if not followed by more word chars (\trowd etc.)
            if (_input.IsEof() || !_input.PeekIf(Rte::IsWordChar, 1)) {
                _input.SkipAs('t');
                _literalText += static_cast<char>(9);
                // Consume space delimiter for \t
                if (!_input.IsEof() && _input.PeekIs(' ')) _input.SkipAs(' ');
            } else {
                ParseControlWord();
                // parseControlWord already consumes the delimiter
            }
        } else if (c == '~') {
            // Non-breaking space
            _input.SkipAs('~');
            AppendUtf8(0x00A0);
        } else if (c == '\'') {
            // Hex escape: \\'hh — charset-aware decoding
            _input.SkipAs('\'');
            int val = 0;
            for (int h = 0; h < 2 && !_input.IsEof(); ++h) {
                char hc = _input.Advance();
                int digit;
                if (hc >= '0' && hc <= '9') digit = hc - '0';
                else if (hc >= 'a' && hc <= 'f') digit = hc - 'a' + 10;
                else digit = hc - 'A' + 10;
                val = val * 16 + digit;
            }
            int fcharset = 0;
            string fontFamily;
            int fi = _formatScope.get().fontIndex;
            if (fi >= 0 && fi < ssize(_doc.fonts)) {
                fcharset = _doc.fonts[static_cast<size_t>(fi)].fcharset;
                fontFamily = _doc.fonts[static_cast<size_t>(fi)].family;
            }
            AppendUtf8(static_cast<uint32_t>(MapHexByteToCodepoint(val, fcharset, _doc.codePage, fontFamily)));
        } else if ((c == 'u' || c == 'U') && !_input.IsEof() && (_input.PeekIf(Rte::IsDigit, 1) || _input.PeekIs('-', 1))) {
            // Unicode escape: \uNNN? (only if 'u' is immediately followed by digit)
            ParseUnicodeEscape();
        } else {
            ParseControlWord();
        }
    }

    void ParseControlWord() {
        auto [word, arg] = ParseControlWordWithArg(_input);
        if (word.empty()) {
            // Consume lone \* — RTF star modifier, no-op when not followed by a control word
            _input.AdvanceIf('*');
            return;
        }
        ProcessControlWord(word, arg);
    }

    void ParseUnicodeEscape() {
        // Guard at entry already verified current char is 'u' or 'U'
        _input.Advance();
        bool negative = false;
        if (!_input.IsEof() && _input.PeekIs('-')) {
            negative = true;
            _input.SkipAs('-');
        }
        int val = 0;
        while (!_input.IsEof() && Rte::IsDigit(_input.Peek())) {
            val = val * 10 + (_input.Advance() - '0');
        }
        if (negative) val = -val;

        // Convert UTF-16 to UTF-8
        int cp = val;
        if (cp < 0) cp += 65536;  // normalize negative UTF-16 values
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate — expect low surrogate
            if (_input.PeekIs("\\u")) {
                _input.AdvanceBy(2);
                int low = 0;
                while (!_input.IsEof() && Rte::IsDigit(_input.Peek())) {
                    low = low * 10 + (_input.Advance() - '0');
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
        }
        AppendUtf8(static_cast<uint32_t>(cp));

        // Skip \ucN fallback bytes (alternate ANSI encoding after \uXXXX)
        // RTF spec: \ucN sets how many bytes follow \uNNNN for backward compat
        for (int i = 0; i < _ucScope.get() && !_input.IsEof(); ++i) {
            _input.Advance();
        }
    }

    void ParseField() {
        // {\field{\*\fldinst HYPERLINK "URL"}{\*\fldrslt display text}}
        _fieldAnchorHref.clear();
        _inFieldRslt = false;

        while (!_input.IsEof() && !_input.PeekIs('}')) {
            CheckIter(_iter);

            if (_input.PeekIs('{')) {
                _input.SkipAs('{');

                // Check for star-prefixed sub-group: {\*\word ...}
                _input.SkipWhitespace();
                if (_input.PeekIs("\\*")) {
                    size_t starPos = _input.Pos();
                    _input.AdvanceBy(2);
                    _input.SkipWhitespace();
                    string destWord;
                    if (!_input.IsEof() && _input.PeekIs('\\')) {
                        _input.SkipAs('\\');
                        _input.SkipWhitespace();
                        if (!_input.IsEof() && Rte::IsWordChar(_input.Peek())) {
                            auto [w, a] = _input.ReadControlWord();
                            destWord = w;
                        }
                    }
                    _input.Seek(starPos);

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
            } else if (_input.PeekIs('\\')) {
                // Control word at field level — skip
                ParseControl();
                while (!_input.IsEof() && !_input.PeekIs('}') && Rte::IsWhitespace(_input.Peek())) {
                    _input.Advance();
                }
            } else {
                _input.Advance();
            }
        }

        _input.SkipAs('}');
        string savedText = _literalText;
        bool savedIsAnchor = _formatScope.get().isAnchor;
        string savedHref = _formatScope.get().anchorHref;
        _literalText.clear();

        // Finalize accumulated text with anchor format
        if (!savedText.empty()) {
            RtfRunFormat runFmt = _formatScope.get();
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
        while (!_input.IsEof() && !_input.PeekIs('}')) {
            CheckIter(_iter);
            if (_input.PeekIs('\\')) {
                _input.SkipAs('\\');
                if (!_input.IsEof()) {
                    char c = _input.Peek();
                    if (c == '\\') { _input.SkipAs('\\'); inst += '\\'; }
                    else if (c == '{') { _input.SkipAs('{'); inst += '{'; }
                    else if (c == '}') { _input.SkipAs('}'); inst += '}'; }
                    else if (c == '_') {
                        // RTF escape: \_ produces literal space
                        _input.SkipAs('_');
                        inst += ' ';
                    }
                    else {
                        // Control word in instruction — read it
                        auto [w, _] = _input.ReadControlWord();
                        inst += w;
                    }
                }
            } else {
                inst += _input.Advance();
            }
        }
        _input.SkipAs('}');
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
            _formatScope.get().isAnchor = true;
            _formatScope.get().anchorHref = _fieldAnchorHref;
        }

        // Parse content normally
        Parse();

        _input.SkipAs('}');
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
        _input.Advance();
    }

    void FinalizeRun() {
        if (_literalText.empty()) return;

        if (_inTableCell) {
            _currentCellRuns.emplace_back(std::move(_literalText), _formatScope.get());
        } else {
            _currentParagraph.runs.emplace_back(std::move(_literalText), _formatScope.get());
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
};

} // namespace

void FlushCurrentParagraph(RtfParserScopeContext& ctx) {
    // Skip the initial empty paragraph before any content has been flushed.
    // Empty paragraphs after the first flush are preserved (blank lines).
    if (!ctx.listState.paragraphFlushed && !ParagraphHasNonWhitespaceContent(ctx.currentParagraph)) {
        ctx.currentParagraph = {};
        return;
    }
    ctx.listState.paragraphFlushed = true;
    ctx.currentParagraph.format = ctx.scopes.paraScope.get();
    ctx.currentParagraph.listId = ctx.listState.listId;
    ctx.currentParagraph.listLevel = ctx.listState.listLevel;
    ctx.currentParagraph.listStyle = ctx.listState.listStyle;
    ctx.currentParagraph.listIndent = ctx.scopes.paraScope.get().leftIndent;
    ctx.currentParagraph.defaultFontIndex = ctx.scopes.deffScope.get();
    ctx.currentParagraph.defaultTabStopTwips = ctx.scopes.deftabScope.get();
    ctx.doc.elements.push_back(std::move(ctx.currentParagraph));
    ctx.currentParagraph = {};
}

RtfDocument ParseRtf(const string& rtf, int codePage) {
    RtfParserImpl impl;
    return impl.Parse(rtf, codePage);
}

} // namespace Rte

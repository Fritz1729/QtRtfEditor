#include "RtfParser.h"
#include "RtfControl.h"
#include "RtfTypes.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>
#include <map>

namespace Rte {

namespace {

class RtfParserImpl {
public:

    RtfDocument parse(const std::string& rtf) {
        _doc = RtfDocument{};
        _rtf = rtf;
        _pos = 0;
        _len = rtf.size();
        _literalText.clear();
        _skipLeadingWsTrim = false;
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
        _pendingBorderSide = -1;
        _listId = 0;
        _listLevel = 0;
        _listStyle = ListStyle::None;

        parse();
        finalizeRun();
        // Flush current paragraph if it has content
        flushCurrentParagraph();
        // Remove trailing empty paragraphs and table rows from elements
        removeTrailingEmptyElements();

        return _doc;
    }

private:
    static constexpr size_t kMaxIter = 10'000'000;

    const RtfControl* findControl(const char* word) const {
        for (auto& ctrl : rtfControlTable) {
            if (strcmp(word, ctrl.keyword) == 0) return &ctrl;
        }
        return nullptr;
    }

    void dispatch(const RtfControl& ctrl, int arg) {
        switch (ctrl.action) {
        case RtfControl::Action::ToggleCharProp: {
            finalizeRun();
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
            }
            break;
        }
        case RtfControl::Action::SetCharProp: {
            finalizeRun();
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
                finalizeRun();
                _format.ulColorIndex = arg;
                break;
            case RtfControl::CharSetProp::LangId:
                finalizeRun();
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
            finalizeRun();
            const RtfControl::RtfUlStyle style = ctrl.value.ulStyle;
            switch (style) {
            case RtfControl::RtfUlStyle::UlSolid:
                // arg < 0 = no argument → toggle on
                // arg >= 0 = explicit argument (e.g. \ul0) → arg is ignored
                _format.underlineStyle = UnderlineStyle::Solid;
                _format.underline = true;
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
            finalizeRun();
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
                finalizeRun();
            } else {
                handleParagraph();
            }
            return;

        case RtfControl::Action::HeaderControl:
        case RtfControl::Action::TableControl:
            break;
        case RtfControl::Action::TableControlWord:
            handleTableControl(ctrl, arg);
            return;
        case RtfControl::Action::Unknown:
            break;
        }
    }

    void handleParagraph() {
        if (_inTable) {
            flushPendingTableRow();
        }
        finalizeRun();
        _skipLeadingWsTrim = false;
        flushCurrentParagraph();
        // Remove trailing empty paragraphs and table rows from elements
        removeTrailingEmptyElements();
        // Create paragraph for content that follows
        _currentParagraph = {};
        // Reset paragraph formatting
        _para = ParagraphFormatting{};
        _pendingTabAlignment = 1;
        _listId = 0;
        _listLevel = 0;
        _listStyle = ListStyle::None;
    }

    void handleTableControl(const RtfControl& ctrl, int arg) {
        switch (ctrl.value.tableCtrlWord) {
        case RtfControl::TableCtrlWord::Trowd:
            if (!_inTable) {
                _inTable = true;
                flushCurrentParagraph();
            }
            _inRow = true;
            _currentCellIndex = 0;
            _currentCellRuns.clear();
            _currentCellFormat = {};
            resetPendingBorder();
            _currentRow = {};
            break;

        case RtfControl::TableCtrlWord::Cellx:
            if (arg >= 0) {
                _currentRow.cellxPositions.push_back(arg);
            }
            break;

        case RtfControl::TableCtrlWord::Intbl:
            _inTableCell = true;
            finalizeRun();
            _currentCellRuns.clear();
            break;

        case RtfControl::TableCtrlWord::Cell:
            if (_inTableCell) {
                finalizeRun();
                applyPendingBorder();
                addCurrentCellToRow();
                _inTableCell = false;
                _currentCellIndex++;
            }
            _currentCellFormat = {};
            resetPendingBorder();
            break;

        case RtfControl::TableCtrlWord::Row:
            if (_inTableCell) {
                finalizeRun();
                applyPendingBorder();
                addCurrentCellToRow();
                _inTableCell = false;
            }
            emitTableRow();
            _inRow = false;
            _currentCellIndex = 0;
            resetPendingBorder();
            break;

        case RtfControl::TableCtrlWord::ClShading:
            if (arg >= 0) {
                _currentCellFormat.shadingColor = arg;
            }
            break;

        case RtfControl::TableCtrlWord::ClVertAlignTop:
            _currentCellFormat.vertAlign = 0;
            break;

        case RtfControl::TableCtrlWord::ClVertAlignCenter:
            _currentCellFormat.vertAlign = 1;
            break;

        case RtfControl::TableCtrlWord::ClVertAlignBottom:
            _currentCellFormat.vertAlign = 2;
            break;

        case RtfControl::TableCtrlWord::ClBorderLeft:
            beginBorderSide(0);
            break;

        case RtfControl::TableCtrlWord::ClBorderTop:
            beginBorderSide(1);
            break;

        case RtfControl::TableCtrlWord::ClBorderRight:
            beginBorderSide(2);
            break;

        case RtfControl::TableCtrlWord::ClBorderBottom:
            beginBorderSide(3);
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
        }
    }

    void applyPendingBorder() {
        if (_pendingBorderSide < 0) return;
        auto& borders = _currentCellFormat.borders;
        switch (_pendingBorderSide) {
            case 0:
                borders.leftWidth = _pendingBorderWidth;
                borders.leftColor = _pendingBorderColor;
                break;
            case 1:
                borders.topWidth = _pendingBorderWidth;
                borders.topColor = _pendingBorderColor;
                break;
            case 2:
                borders.rightWidth = _pendingBorderWidth;
                borders.rightColor = _pendingBorderColor;
                break;
            case 3:
                borders.bottomWidth = _pendingBorderWidth;
                borders.bottomColor = _pendingBorderColor;
                break;
        }
        resetPendingBorder();
    }

    void resetPendingBorder() {
        _pendingBorderSide = -1;
        _pendingBorderStyle = 0;
        _pendingBorderWidth = 0;
        _pendingBorderColor = 0;
    }

    void beginBorderSide(int side) {
        applyPendingBorder();
        _pendingBorderSide = side;
        _pendingBorderStyle = 0;
        _pendingBorderWidth = 0;
        _pendingBorderColor = 0;
    }

    void addCurrentCellToRow() {
        while (static_cast<int>(_currentRow.cells.size()) <= _currentCellIndex) {
            _currentRow.cells.push_back({{}, {}});
        }
        _currentRow.cells[_currentCellIndex] =
            {std::move(_currentCellRuns), _currentCellFormat};
        _currentCellRuns.clear();
        _currentCellFormat = {};
    }

    void emitTableRow() {
        if (hasTextContent(_currentRow)) {
            _doc.elements.push_back(std::move(_currentRow));
        }
        _currentRow = {};
    }

    void flushPendingTableRow() {
        if (!_inTable || !_inRow) {
            _inTable = false;
            _inRow = false;
            _inTableCell = false;
            _currentCellIndex = 0;
            _currentRow = {};
            return;
        }
        if (hasTextContent(_currentRow)) {
            _doc.elements.push_back(std::move(_currentRow));
        }
        _inTable = false;
        _inRow = false;
        _inTableCell = false;
        _currentCellIndex = 0;
        _currentRow = {};
    }

    void flushCurrentParagraph() {
        if (!_currentParagraph.runs.empty()) {
            _currentParagraph.setFormatting(_para);
            _currentParagraph.listId = _listId;
            _currentParagraph.listLevel = _listLevel;
            _currentParagraph.listStyle = _listStyle;
            _currentParagraph.listIndent = _para.leftIndent;
            _doc.elements.push_back(std::move(_currentParagraph));
        }
        _currentParagraph = {};
    }

    bool hasTextContent(const RtfParagraph& p) {
        return std::any_of(p.runs.begin(), p.runs.end(),
            [](const RtfRun& r) { return !r.text.empty(); });
    }

    bool hasTextContent(const RtfTableRowEntry& r) {
        for (const auto& [runs, _] : r.cells) {
            for (const auto& run : runs) {
                if (!run.text.empty()) return true;
            }
        }
        return false;
    }

    void removeTrailingEmptyElements() {
        while (!_doc.elements.empty()) {
            bool hasText = std::visit([this](const auto& elem) -> bool {
                using T = std::decay_t<decltype(elem)>;
                if constexpr (std::is_same_v<T, RtfParagraph>) return hasTextContent(elem);
                else if constexpr (std::is_same_v<T, RtfTableRowEntry>) return hasTextContent(elem);
                else return true;
            }, _doc.elements.back());
            if (!hasText) {
                _doc.elements.pop_back();
            } else {
                break;
            }
        }
    }

    RtfDocument _doc;
    RtfParagraph _currentParagraph;
    std::string _rtf;
    size_t _pos = 0;
    size_t _len = 0;
    std::string _literalText;
    bool _skipLeadingWsTrim = false;
    size_t _iter = 0;
    RtfRunFormat _format;
    ParagraphFormatting _para;
    std::vector<RtfRunFormat> _formatStack;
    bool _inColortbl = false;
    bool _inFonttbl = false;
    bool _inPict = false;
    bool _inListtable = false;
    std::map<int, ListStyle> _listIdToStyle;
    std::vector<ParagraphFormatting> _paraStateStack;
    std::vector<int> _pendingTabAlignmentStack;
    int _pendingTabAlignment = 1;
    int _listId = 0;
    int _listLevel = 0;
    ListStyle _listStyle = ListStyle::None;

    // Pict state
    QByteArray _pictData;
    std::string _pictFormat;  // "jpg", "png", "bmp"
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

    // Table state
    bool _inTable = false;
    bool _inRow = false;
    bool _inTableCell = false;
    int _currentCellIndex = 0;
    RtfTableRowEntry _currentRow;
    std::vector<RtfRun> _currentCellRuns;
    TableCellFormat _currentCellFormat;
    int _pendingBorderSide = -1;
    int _pendingBorderStyle = 0;
    int _pendingBorderWidth = 0;
    int _pendingBorderColor = 0;

    void parse() {
        while (_pos < _len) {
            if (++_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            char c = _rtf[_pos];
            if (c == '{') {
                parseGroup();
            } else if (c == '}') {
                // Group closing — handled by parseGroup
                break;
            } else if (c == '\\') {
                parseControl();
            } else if (isPrintable(c)) {
                accumulateLiteral(c);
            } else {
                _pos++;
            }
        }
        // Flush pending table row at end of input (outermost parse only)
        if (_inTable && _pos >= _len) {
            flushPendingTableRow();
        }
    }

    void parseGroup() {
        _pos++; // skip '{'

        // Push state
        _formatStack.push_back(_format);
        _paraStateStack.push_back(_para);
        _pendingTabAlignmentStack.push_back(_pendingTabAlignment);

        // Check for known table groups
        skipWhitespace();
        if (_pos < _len && matches("\\colortbl")) {
            // Consume \\colortbl control word and optional argument
            _pos += 9;
            while (_pos < _len && isDigit(_rtf[_pos])) _pos++;
            skipWhitespace();

            _inColortbl = true;
            parseColortbl();
            _inColortbl = false;
            restoreState();
            // parseColortbl consumes the closing '}'
            return;
        }
        if (_pos < _len && matches("\\fonttbl")) {
            // Consume \\fonttbl control word and optional argument
            _pos += 7;
            while (_pos < _len && isDigit(_rtf[_pos])) _pos++;
            skipWhitespace();

            _inFonttbl = true;
            parseFonttbl();
            _inFonttbl = false;
            restoreState();
            // parseFonttbl consumes the closing '}'
            return;
        }

        if (_pos < _len && matches("\\listtable")) {
            _pos += 9;
            skipWhitespace();
            _inListtable = true;
            parseListtable();
            _inListtable = false;
            restoreState();
            return;
        }

        if (_pos < _len && matches("\\pict")) {
            // Consume \\pict control word
            _pos += 4;
            skipWhitespace();

            _inPict = true;
            parsePict();
            _inPict = false;
            restoreState();
            // parsePict consumes the closing '}'
            return;
        }

        // Unknown group — parse contents normally
        parse();

        // Expect '}'
        if (_pos < _len && _rtf[_pos] == '}') {
            _pos++;
        }

        restoreState();
    }

    void restoreState() {
        finalizeRun();
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
    }

    void parseControl() {
        _pos++; // skip '\'

        if (_pos >= _len) return;

        char c = _rtf[_pos];

        if (isDigit(c)) {
            // Control symbol: single digit
            _pos++;
        } else if (c == '{') {
            // Braced control symbol: \{
            _pos++;
            std::string digits;
            while (_pos < _len && _rtf[_pos] != '}' && isDigit(_rtf[_pos])) {
                digits += _rtf[_pos++];
            }
            if (_pos < _len) _pos++; // skip '}'
        } else if (c == 't') {
            // Tab character — only if not followed by more word chars (\trowd etc.)
            if (_pos + 1 >= _len || !isWordChar(_rtf[_pos + 1])) {
                _pos++;
                _literalText += static_cast<char>(9);
            } else {
                parseControlWord();
            }
        } else if (c == '~') {
            // Non-breaking space
            _pos++;
            appendUtf8(0x00A0);
        } else if (c == '\'') {
            // Hex escape: \\'hh
            _pos++;
            int val = 0;
            for (int h = 0; h < 2 && _pos < _len; ++h) {
                char hc = _rtf[_pos++];
                val = val * 16 + (hc >= '0' && hc <= '9' ? hc - '0' : (hc >= 'a' && hc <= 'f' ? hc - 'a' + 10 : hc - 'A' + 10));
            }
            appendUtf8(val);
        } else if ((c == 'u' || c == 'U') && _pos + 1 < _len && isDigit(_rtf[_pos + 1])) {
            // Unicode escape: \uNNN? (only if 'u' is immediately followed by digit)
            parseUnicodeEscape();
        } else {
            parseControlWord();
        }
    }

    void parseControlWord() {
        std::string word;
        int arg = 0;
        bool hasArg = false;

        while (_pos < _len && (isWordChar(_rtf[_pos]) || isDigit(_rtf[_pos]))) {
            if (_rtf[_pos] >= '0' && _rtf[_pos] <= '9') {
                arg = arg * 10 + (_rtf[_pos] - '0');
                hasArg = true;
            } else {
                word += static_cast<char>(
                    static_cast<unsigned char>(_rtf[_pos]) | 0x20); // lowercase
            }
            _pos++;
        }

        if (hasArg && _pos < _len && !isWordChar(_rtf[_pos]) && _rtf[_pos] != '\\' && _rtf[_pos] != '}' && _rtf[_pos] != '{') {
            _pos++; // skip trailing whitespace after numeric argument
        }

        if (word.empty()) return;

        processControlWord(word, hasArg ? arg : -1);
    }

    void parseUnicodeEscape() {
        _pos++; // skip 'u'
        int val = 0;
        while (_pos < _len && isDigit(_rtf[_pos])) {
            val = val * 10 + (_rtf[_pos] - '0');
            _pos++;
        }

        // Convert UTF-16 to UTF-8
        int cp = val;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate — expect low surrogate
            if (_pos + 1 < _len &&
                _rtf[_pos] == '\\' && _rtf[_pos + 1] == 'u') {
                _pos += 2;
                int low = 0;
                while (_pos < _len && isDigit(_rtf[_pos])) {
                    low = low * 10 + (_rtf[_pos] - '0');
                    _pos++;
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
        }
        appendUtf8(cp);

        // Skip '?' delimiter
        if (_pos < _len && _rtf[_pos] == '?') {
            _pos++;
        }
    }

    void parseColortbl() {
        // {\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
        // Each ';' separates a color entry. First entry is "auto" (may be empty).
        while (_pos < _len && _rtf[_pos] != '}') {
            if (++_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            skipWhitespace();

            int r = 0, g = 0, b = 0;
            bool haveR = false, haveG = false, haveB = false;

            while (_pos < _len && _rtf[_pos] != ';' && _rtf[_pos] != '}') {
                if (matches("\\red")) {
                    _pos += 4;
                    r = parseInt();
                    haveR = true;
                } else if (matches("\\green")) {
                    _pos += 6;
                    g = parseInt();
                    haveG = true;
                } else if (matches("\\blue")) {
                    _pos += 5;
                    b = parseInt();
                    haveB = true;
                } else if (isPrintable(_rtf[_pos])) {
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

    void parseFonttbl() {
        // {\fonttbl{\f0\froman\fcharset0 Times New Roman;}
        //        {\f1\fswiss\fcharset0 Arial;}}
        // Each font entry is a group containing \fN and the family name
        while (_pos < _len && _rtf[_pos] != '}') {
            if (++_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            if (_rtf[_pos] == '{') {
                // Font entry group
                _pos++;

                int index = 0;
                std::string family;

                while (_pos < _len && _rtf[_pos] != '}') {
                    if (_rtf[_pos] == '\\') {
                        parseControl();
                    } else if (isPrintable(_rtf[_pos])) {
                        family += _rtf[_pos++];
                    } else {
                        _pos++;
                    }
                }

                if (_pos < _len) _pos++; // skip '}'

                // Remove leading/trailing whitespace from family
                while (!family.empty() && family.back() == ' ') family.pop_back();
                while (!family.empty() && family.front() == ' ') family.erase(family.begin());

                if (!family.empty()) {
                    _doc.fonts.push_back({family});
                }
            } else {
                _pos++;
            }
        }
        // Consume the closing '}' of the fonttbl group
        if (_pos < _len && _rtf[_pos] == '}') _pos++;
    }

    void parsePict() {
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
            if (++_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            if (_rtf[_pos] == '\\') {
                parsePictControl();
                // Skip whitespace after control words
                while (_pos < _len && _rtf[_pos] != '}' && isWhitespace(_rtf[_pos])) {
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
            flushCurrentParagraph();
            _doc.elements.push_back(std::move(img));
        }
    }

    void parseListtable() {
        while (_pos < _len && _rtf[_pos] != '}') {
            if (++_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            if (_rtf[_pos] == '{') {
                _pos++;
                while (_pos < _len && _rtf[_pos] != '}') {
                    if (_rtf[_pos] == '\\') {
                        parseListtableControl();
                    } else {
                        _pos++;
                    }
                }
                if (_pos < _len && _rtf[_pos] == '}') _pos++;
            } else if (_rtf[_pos] == '\\') {
                parseListtableControl();
            } else {
                _pos++;
            }
        }
        if (_pos < _len && _rtf[_pos] == '}') _pos++;
    }

    void parseListtableControl() {
        _pos++;
        if (_pos >= _len) return;

        std::string word;
        int arg = 0;
        bool hasArg = false;

        while (_pos < _len && (isWordChar(_rtf[_pos]) || isDigit(_rtf[_pos]))) {
            if (_rtf[_pos] >= '0' && _rtf[_pos] <= '9') {
                arg = arg * 10 + (_rtf[_pos] - '0');
                hasArg = true;
            } else {
                word += static_cast<char>(static_cast<unsigned char>(_rtf[_pos]) | 0x20);
            }
            _pos++;
        }

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
                _currentListStyle = rtfStyleTypeToListStyle(arg);
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

    static ListStyle rtfStyleTypeToListStyle(int type) {
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

    void parsePictControl() {
        _pos++; // skip '\'

        if (_pos >= _len) return;

        char c = _rtf[_pos];
        if (isWordChar(c)) {
            std::string word;
            int arg = 0;
            bool hasArg = false;

            while (_pos < _len && (isWordChar(_rtf[_pos]) || isDigit(_rtf[_pos]))) {
                if (_rtf[_pos] >= '0' && _rtf[_pos] <= '9') {
                    arg = arg * 10 + (_rtf[_pos] - '0');
                    hasArg = true;
                } else {
                    word += static_cast<char>(
                        static_cast<unsigned char>(_rtf[_pos]) | 0x20);
                }
                _pos++;
            }

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
        if (isDigit(c)) {
            _pos++;
        }
    }

    void processControlWord(const std::string& word, int arg) {
        // Table group markers (should have been caught in parseGroup)
        if (word == "colortbl" || word == "fonttbl") return;

        // Handle \deffN separately — set default font index
        if (word == "deff") {
            _doc.defaultFontIndex = arg;
            return;
        }

        // Special typographic characters (RE 2.0)
        if (word == "bullet") { appendUtf8(0x2022); return; }
        if (word == "emdash") { appendUtf8(0x2014); return; }
        if (word == "endash") { appendUtf8(0x2013); return; }
        if (word == "lquote") { appendUtf8(0x2018); return; }
        if (word == "rquote") { appendUtf8(0x2019); return; }
        if (word == "ldblquote") { appendUtf8(0x201C); return; }
        if (word == "rdblquote") { appendUtf8(0x201D); return; }
        if (word == "tab") { _literalText += static_cast<char>(9); return; }

        auto* ctrl = findControl(word.c_str());
        if (ctrl) {
            dispatch(*ctrl, arg);
        } else {
            // Unknown tag — record for preservation
            std::string tag = "\\" + word;
            if (arg >= 0) {
                tag += std::to_string(arg);
            }
            _doc.unknownTags.push_back(tag);
        }
    }

    void accumulateLiteral(char c) {
        _literalText += c;
        _pos++;
    }

    void finalizeRun() {
        if (_literalText.empty()) return;

        std::string trimmed = _literalText;

        // Only trim leading whitespace at paragraph start, or when we're
        // not right after a formatting toggle. After a toggle (e.g. \b0),
        // leading whitespace is intentional content.
        if (_skipLeadingWsTrim) {
            _skipLeadingWsTrim = false;
        } else {
            size_t start = trimmed.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                trimmed = trimmed.substr(start);
            } else {
                _literalText.clear();
                return;
            }
        }

        if (_inTableCell) {
            _currentCellRuns.emplace_back(trimmed, _format);
        } else {
            _currentParagraph.runs.emplace_back(trimmed, _format);
        }
        _literalText.clear();
    }

    void appendUtf8(int cp) {
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

    bool matches(const char* s) {
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
        if (_pos + len < _len && isWordChar(_rtf[_pos + len])) return false;
        return true;
    }

    int parseInt() {
        int val = 0;
        while (_pos < _len && isDigit(_rtf[_pos])) {
            val = val * 10 + (_rtf[_pos] - '0');
            _pos++;
        }
        return val;
    }

    void skipWhitespace() {
        while (_pos < _len && isWhitespace(_rtf[_pos])) {
            _pos++;
        }
    }

    bool isWordChar(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool isDigit(char c) const {
        return c >= '0' && c <= '9';
    }

    bool isWhitespace(char c) const {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    bool isPrintable(char c) const {
        return c != ';' && static_cast<unsigned char>(c) >= 32 &&
               static_cast<unsigned char>(c) <= 126;
    }
};

} // namespace

RtfDocument RtfParser::parse(const std::string& rtf) {
    return RtfParserImpl().parse(rtf);
}

RtfDocument ParseRtf(const std::string& rtf) {
    return RtfParserImpl().parse(rtf);
}

} // namespace Rte

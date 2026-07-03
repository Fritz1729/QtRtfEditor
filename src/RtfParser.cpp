#include "RtfParser.h"
#include "RtfControl.h"
#include "RtfTypes.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

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
        _iter = 0;

        _format = RtfRunFormat{};
        _para = ParagraphFormatting{};
        _formatStack.clear();
        _inColortbl = false;
        _inFonttbl = false;
        _paraStateStack.clear();

        parse();
        finalizeRun();
        // Remove trailing empty paragraph (from trailing \par)
        if (!_doc.paragraphs.empty()) {
            const auto& last = _doc.paragraphs.back();
            bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                [](const RtfRun& r) { return !r.text.empty(); });
            if (!hasText) {
                _doc.paragraphs.pop_back();
            }
        }

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
            const RtfControl::CharProp prop = ctrl.value.charProp;
            switch (prop) {
            case RtfControl::CharProp::Bold:
                _format.bold = on;
                break;
            case RtfControl::CharProp::Italic:
                _format.italic = on;
                break;
            case RtfControl::CharProp::Underline:
                _format.underline = on;
                if (on) _format.underlineStyle = UnderlineStyle::Solid;
                else _format.underlineStyle = UnderlineStyle::None;
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
            }
            break;
        }
        case RtfControl::Action::SetCharProp: {
            finalizeRun();
            const RtfControl::CharSetProp prop = ctrl.value.charSetProp;
            switch (prop) {
            case RtfControl::CharSetProp::FontIndex:
                if (arg >= 0) _format.fontIndex = arg;
                break;
            case RtfControl::CharSetProp::FontSize:
                if (arg >= 0) _format.fontSize = arg;
                break;
            case RtfControl::CharSetProp::ColorIndex:
                if (arg >= 0) _format.colorIndex = arg;
                break;
            case RtfControl::CharSetProp::BgColorIndex:
                if (arg >= 0) _format.bgColorIndex = arg;
                break;
            case RtfControl::CharSetProp::Highlight:
                if (arg >= 0) _format.highlight = arg;
                break;
            case RtfControl::CharSetProp::UpOffset:
                if (arg >= 0) _format.upOffset = arg;
                break;
            case RtfControl::CharSetProp::DnOffset:
                if (arg >= 0) _format.dnOffset = arg;
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
            }
            break;
        }
        case RtfControl::Action::SetAlignment: {
            const RtfControl::Align align = ctrl.value.align;
            switch (align) {
            case RtfControl::Align::Left:
                _para.alignment = 1;
                break;
            case RtfControl::Align::Center:
                _para.alignment = 128;
                break;
            case RtfControl::Align::Right:
                _para.alignment = 2;
                break;
            }
            break;
        }
        case RtfControl::Action::SetUlStyle: {
            finalizeRun();
            const RtfControl::RtfUlStyle style = ctrl.value.ulStyle;
            switch (style) {
            case RtfControl::RtfUlStyle::UlDotted:
                _format.underlineStyle = UnderlineStyle::Dotted;
                _format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDashed:
                _format.underlineStyle = UnderlineStyle::Dashed;
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
            handleParagraph();
            return;

        case RtfControl::Action::HeaderControl:
        case RtfControl::Action::TableControl:
            break;
        case RtfControl::Action::Unknown:
            break;
        }
    }

    void handleParagraph() {
        finalizeRun();
        // Apply paragraph formatting to the last non-empty paragraph
        for (int k = static_cast<int>(_doc.paragraphs.size()) - 1; k >= 0; --k) {
            if (!_doc.paragraphs[k].runs.empty() &&
                std::any_of(_doc.paragraphs[k].runs.begin(), _doc.paragraphs[k].runs.end(),
                    [](const RtfRun& r) { return !r.text.empty(); }))
            {
                _doc.paragraphs[k].setFormatting(_para);
                break;
            }
        }
        // Remove trailing empty paragraphs
        while (!_doc.paragraphs.empty()) {
            const auto& last = _doc.paragraphs.back();
            bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                [](const RtfRun& r) { return !r.text.empty(); });
            if (!hasText) {
                _doc.paragraphs.pop_back();
            } else {
                break;
            }
        }
        // Create paragraph for content that follows
        _doc.paragraphs.push_back({});
        // Reset paragraph formatting
        _para = ParagraphFormatting{};
    }

    RtfDocument _doc;
    std::string _rtf;
    size_t _pos = 0;
    size_t _len = 0;
    std::string _literalText;
    size_t _iter = 0;
    RtfRunFormat _format;
    ParagraphFormatting _para;
    std::vector<RtfRunFormat> _formatStack;
    bool _inColortbl = false;
    bool _inFonttbl = false;
    std::vector<ParagraphFormatting> _paraStateStack;

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
    }

    void parseGroup() {
        _pos++; // skip '{'

        // Push state
        _formatStack.push_back(_format);
        _paraStateStack.push_back(_para);

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
            // Tab character
            _pos++;
            _literalText += static_cast<char>(9);
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

        // Trim leading/trailing whitespace from the literal
        // but preserve internal whitespace
        std::string trimmed = _literalText;
        size_t start = trimmed.find_first_not_of(" \t\n\r");
        size_t end = trimmed.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            // All whitespace — skip
            _literalText.clear();
            return;
        }
        trimmed = trimmed.substr(start, end - start + 1);

        if (trimmed.empty()) {
            _literalText.clear();
            return;
        }

        if (_doc.paragraphs.empty()) {
            _doc.paragraphs.push_back({});
        }

        _doc.paragraphs.back().runs.emplace_back(trimmed, _format);
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

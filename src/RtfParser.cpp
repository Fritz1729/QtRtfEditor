#include "RtfParser.h"
#include "RtfControl.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

namespace Rte {

namespace {

struct FormatState {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int fontIndex = 0;
    int fontSize = 0;
    int colorIndex = -1;
    int bgColorIndex = -1;
    int highlight = -1;
    bool superscript = false;
    bool subscript = false;
    bool strikeOut = false;
    UnderlineStyle underlineStyle = UnderlineStyle::None;
    Capitalization capitalization = Capitalization::None;
    int upOffset = 0;
    int dnOffset = 0;
};

struct ParagraphState {
    int alignment = 1;            // Qt::AlignLeft
    int leftIndent = 0;
    int firstLineIndent = 0;
    int rightIndent = 0;
    int spaceBefore = 0;
    int spaceAfter = 0;
    int lineHeight = 0;
    int slMult = 1;
};

class RtfParserImpl {
public:

    RtfDocument parse(const std::string& rtf) {
        m_doc = RtfDocument{};
        m_rtf = rtf;
        m_pos = 0;
        m_len = rtf.size();
        m_literalText.clear();
        m_iter = 0;

        m_format = FormatState{};
        m_para = ParagraphState{};
        m_formatStack.clear();
        m_inColortbl = false;
        m_inFonttbl = false;
        m_paraStateStack.clear();

        parse();
        finalizeRun();
        // Remove trailing empty paragraph (from trailing \par)
        if (!m_doc.paragraphs.empty()) {
            const auto& last = m_doc.paragraphs.back();
            bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                [](const RtfRun& r) { return !r.text.empty(); });
            if (!hasText) {
                m_doc.paragraphs.pop_back();
            }
        }

        return m_doc;
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
            auto prop = static_cast<RtfControl::CharProp>(ctrl.value);
            switch (prop) {
            case RtfControl::CharProp::Bold:
                m_format.bold = on;
                break;
            case RtfControl::CharProp::Italic:
                m_format.italic = on;
                break;
            case RtfControl::CharProp::Underline:
                m_format.underline = on;
                if (on) m_format.underlineStyle = UnderlineStyle::Solid;
                else m_format.underlineStyle = UnderlineStyle::None;
                break;
            case RtfControl::CharProp::Subscript:
                m_format.subscript = on;
                break;
            case RtfControl::CharProp::Superscript:
                m_format.superscript = on;
                if (on) m_format.subscript = false;
                break;
            case RtfControl::CharProp::Strike:
                m_format.strikeOut = on;
                break;
            }
            break;
        }
        case RtfControl::Action::SetCharProp: {
            finalizeRun();
            auto prop = static_cast<RtfControl::CharSetProp>(ctrl.value);
            switch (prop) {
            case RtfControl::CharSetProp::FontIndex:
                if (arg >= 0) m_format.fontIndex = arg;
                break;
            case RtfControl::CharSetProp::FontSize:
                if (arg >= 0) m_format.fontSize = arg;
                break;
            case RtfControl::CharSetProp::ColorIndex:
                if (arg >= 0) m_format.colorIndex = arg;
                break;
            case RtfControl::CharSetProp::BgColorIndex:
                if (arg >= 0) m_format.bgColorIndex = arg;
                break;
            case RtfControl::CharSetProp::Highlight:
                if (arg >= 0) m_format.highlight = arg;
                break;
            case RtfControl::CharSetProp::UpOffset:
                if (arg >= 0) m_format.upOffset = arg;
                break;
            case RtfControl::CharSetProp::DnOffset:
                if (arg >= 0) m_format.dnOffset = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetParaProp: {
            auto prop = static_cast<RtfControl::ParaProp>(ctrl.value);
            switch (prop) {
            case RtfControl::ParaProp::LeftIndent:
                m_para.leftIndent = arg;
                break;
            case RtfControl::ParaProp::FirstLineIndent:
                m_para.firstLineIndent = arg;
                break;
            case RtfControl::ParaProp::RightIndent:
                m_para.rightIndent = arg;
                break;
            case RtfControl::ParaProp::SpaceBefore:
                m_para.spaceBefore = arg;
                break;
            case RtfControl::ParaProp::SpaceAfter:
                m_para.spaceAfter = arg;
                break;
            case RtfControl::ParaProp::LineHeight:
                m_para.lineHeight = arg;
                break;
            case RtfControl::ParaProp::SlMult:
                if (arg >= 0) m_para.slMult = arg;
                break;
            }
            break;
        }
        case RtfControl::Action::SetAlignment: {
            auto align = static_cast<RtfControl::Align>(ctrl.value);
            switch (align) {
            case RtfControl::Align::Left:
                m_para.alignment = 1;
                break;
            case RtfControl::Align::Center:
                m_para.alignment = 128;
                break;
            case RtfControl::Align::Right:
                m_para.alignment = 2;
                break;
            }
            break;
        }
        case RtfControl::Action::SetUlStyle: {
            finalizeRun();
            auto style = static_cast<RtfControl::RtfUlStyle>(ctrl.value);
            switch (style) {
            case RtfControl::RtfUlStyle::UlDotted:
                m_format.underlineStyle = UnderlineStyle::Dotted;
                m_format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDashed:
                m_format.underlineStyle = UnderlineStyle::Dashed;
                m_format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlDouble:
                m_format.underlineStyle = UnderlineStyle::Double;
                m_format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlThick:
                m_format.underlineStyle = UnderlineStyle::Thick;
                m_format.underline = true;
                break;
            case RtfControl::RtfUlStyle::UlNone:
                m_format.underlineStyle = UnderlineStyle::None;
                m_format.underline = false;
                break;
            }
            break;
        }
        case RtfControl::Action::SetCapitalization: {
            finalizeRun();
            auto caps = static_cast<RtfControl::RtfCaps>(ctrl.value);
            switch (caps) {
            case RtfControl::RtfCaps::CapsAll:
                m_format.capitalization = Capitalization::AllCaps;
                break;
            case RtfControl::RtfCaps::CapsSmall:
                m_format.capitalization = Capitalization::SmallCaps;
                break;
            case RtfControl::RtfCaps::CapsNone:
                m_format.capitalization = Capitalization::None;
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
        for (int k = static_cast<int>(m_doc.paragraphs.size()) - 1; k >= 0; --k) {
            if (!m_doc.paragraphs[k].runs.empty() &&
                std::any_of(m_doc.paragraphs[k].runs.begin(), m_doc.paragraphs[k].runs.end(),
                    [](const RtfRun& r) { return !r.text.empty(); }))
            {
                m_doc.paragraphs[k].alignment = m_para.alignment;
                m_doc.paragraphs[k].leftIndent = m_para.leftIndent;
                m_doc.paragraphs[k].firstLineIndent = m_para.firstLineIndent;
                m_doc.paragraphs[k].rightIndent = m_para.rightIndent;
                m_doc.paragraphs[k].spaceBefore = m_para.spaceBefore;
                m_doc.paragraphs[k].spaceAfter = m_para.spaceAfter;
                m_doc.paragraphs[k].lineHeight = m_para.lineHeight;
                m_doc.paragraphs[k].slMult = m_para.slMult;
                break;
            }
        }
        // Remove trailing empty paragraphs
        while (!m_doc.paragraphs.empty()) {
            const auto& last = m_doc.paragraphs.back();
            bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                [](const RtfRun& r) { return !r.text.empty(); });
            if (!hasText) {
                m_doc.paragraphs.pop_back();
            } else {
                break;
            }
        }
        // Create paragraph for content that follows
        m_doc.paragraphs.push_back({});
        // Reset paragraph formatting
        m_para.alignment = 1;
        m_para.leftIndent = 0;
        m_para.firstLineIndent = 0;
        m_para.rightIndent = 0;
        m_para.spaceBefore = 0;
        m_para.spaceAfter = 0;
        m_para.lineHeight = 0;
        m_para.slMult = 1;
    }

    RtfDocument m_doc;
    std::string m_rtf;
    size_t m_pos = 0;
    size_t m_len = 0;
    std::string m_literalText;
    size_t m_iter = 0;
    FormatState m_format;
    ParagraphState m_para;
    std::vector<FormatState> m_formatStack;
    bool m_inColortbl = false;
    bool m_inFonttbl = false;
    std::vector<ParagraphState> m_paraStateStack;

    void parse() {
        while (m_pos < m_len) {
            if (++m_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            char c = m_rtf[m_pos];
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
                m_pos++;
            }
        }
    }

    void parseGroup() {
        m_pos++; // skip '{'

        // Push state
        m_formatStack.push_back(m_format);
        m_paraStateStack.push_back(m_para);

        // Check for known table groups
        skipWhitespace();
        if (m_pos < m_len && matches("\\colortbl")) {
            // Consume \\colortbl control word and optional argument
            m_pos += 9;
            while (m_pos < m_len && isDigit(m_rtf[m_pos])) m_pos++;
            skipWhitespace();

            m_inColortbl = true;
            parseColortbl();
            m_inColortbl = false;
            restoreState();
            // parseColortbl consumes the closing '}'
            return;
        }
        if (m_pos < m_len && matches("\\fonttbl")) {
            // Consume \\fonttbl control word and optional argument
            m_pos += 7;
            while (m_pos < m_len && isDigit(m_rtf[m_pos])) m_pos++;
            skipWhitespace();

            m_inFonttbl = true;
            parseFonttbl();
            m_inFonttbl = false;
            restoreState();
            // parseFonttbl consumes the closing '}'
            return;
        }

        // Unknown group — parse contents normally
        parse();

        // Expect '}'
        if (m_pos < m_len && m_rtf[m_pos] == '}') {
            m_pos++;
        }

        restoreState();
    }

    void restoreState() {
        finalizeRun();
        if (!m_formatStack.empty()) {
            m_format = m_formatStack.back();
            m_formatStack.pop_back();
        }
        if (!m_paraStateStack.empty()) {
            m_para = m_paraStateStack.back();
            m_paraStateStack.pop_back();
        }
    }

    void parseControl() {
        m_pos++; // skip '\'

        if (m_pos >= m_len) return;

        char c = m_rtf[m_pos];

        if (isDigit(c)) {
            // Control symbol: single digit
            m_pos++;
        } else if (c == '{') {
            // Braced control symbol: \par\par
            m_pos++;
            std::string digits;
            while (m_pos < m_len && m_rtf[m_pos] != '}' && isDigit(m_rtf[m_pos])) {
                digits += m_rtf[m_pos++];
            }
            if (m_pos < m_len) m_pos++; // skip '}'
        } else if ((c == 'u' || c == 'U') && m_pos + 1 < m_len && isDigit(m_rtf[m_pos + 1])) {
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

        while (m_pos < m_len && (isWordChar(m_rtf[m_pos]) || isDigit(m_rtf[m_pos]))) {
            if (m_rtf[m_pos] >= '0' && m_rtf[m_pos] <= '9') {
                arg = arg * 10 + (m_rtf[m_pos] - '0');
                hasArg = true;
            } else {
                word += static_cast<char>(
                    static_cast<unsigned char>(m_rtf[m_pos]) | 0x20); // lowercase
            }
            m_pos++;
        }

        if (hasArg && m_pos < m_len && !isWordChar(m_rtf[m_pos]) && m_rtf[m_pos] != '\\' && m_rtf[m_pos] != '}' && m_rtf[m_pos] != '{') {
            m_pos++; // skip trailing whitespace after numeric argument
        }

        if (word.empty()) return;

        processControlWord(word, hasArg ? arg : -1);
    }

    void parseUnicodeEscape() {
        m_pos++; // skip 'u'
        int val = 0;
        while (m_pos < m_len && isDigit(m_rtf[m_pos])) {
            val = val * 10 + (m_rtf[m_pos] - '0');
            m_pos++;
        }

        // Convert UTF-16 to UTF-8
        int cp = val;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate — expect low surrogate
            if (m_pos + 1 < m_len &&
                m_rtf[m_pos] == '\\' && m_rtf[m_pos + 1] == 'u') {
                m_pos += 2;
                int low = 0;
                while (m_pos < m_len && isDigit(m_rtf[m_pos])) {
                    low = low * 10 + (m_rtf[m_pos] - '0');
                    m_pos++;
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
        }
        appendUtf8(cp);

        // Skip '?' delimiter
        if (m_pos < m_len && m_rtf[m_pos] == '?') {
            m_pos++;
        }
    }

    void parseColortbl() {
        // {\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
        // Each ';' separates a color entry. First entry is "auto" (may be empty).
        while (m_pos < m_len && m_rtf[m_pos] != '}') {
            if (++m_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            skipWhitespace();

            int r = 0, g = 0, b = 0;
            bool haveR = false, haveG = false, haveB = false;

            while (m_pos < m_len && m_rtf[m_pos] != ';' && m_rtf[m_pos] != '}') {
                if (matches("\\red")) {
                    m_pos += 4;
                    r = parseInt();
                    haveR = true;
                } else if (matches("\\green")) {
                    m_pos += 6;
                    g = parseInt();
                    haveG = true;
                } else if (matches("\\blue")) {
                    m_pos += 5;
                    b = parseInt();
                    haveB = true;
                } else if (isPrintable(m_rtf[m_pos])) {
                    m_pos++;
                } else {
                    break;
                }
            }

            // Always add color entry (first entry may be empty "auto" color)
            m_doc.colors.push_back({r, g, b});

            if (m_pos < m_len) m_pos++; // skip ';' or '}'
        }
        // Consume the closing '}' of the colortbl group
        if (m_pos < m_len && m_rtf[m_pos] == '}') m_pos++;
    }

    void parseFonttbl() {
        // {\fonttbl{\f0\froman\fcharset0 Times New Roman;}
        //        {\f1\fswiss\fcharset0 Arial;}}
        // Each font entry is a group containing \fN and the family name
        while (m_pos < m_len && m_rtf[m_pos] != '}') {
            if (++m_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            if (m_rtf[m_pos] == '{') {
                // Font entry group
                m_pos++;

                int index = 0;
                std::string family;

                while (m_pos < m_len && m_rtf[m_pos] != '}') {
                    if (m_rtf[m_pos] == '\\') {
                        parseControl();
                    } else if (isPrintable(m_rtf[m_pos])) {
                        family += m_rtf[m_pos++];
                    } else {
                        m_pos++;
                    }
                }

                if (m_pos < m_len) m_pos++; // skip '}'

                // Remove leading/trailing whitespace from family
                while (!family.empty() && family.back() == ' ') family.pop_back();
                while (!family.empty() && family.front() == ' ') family.erase(family.begin());

                if (!family.empty()) {
                    m_doc.fonts.push_back({family});
                }
            } else {
                m_pos++;
            }
        }
        // Consume the closing '}' of the fonttbl group
        if (m_pos < m_len && m_rtf[m_pos] == '}') m_pos++;
    }

    void processControlWord(const std::string& word, int arg) {
        // Table group markers (should have been caught in parseGroup)
        if (word == "colortbl" || word == "fonttbl") return;

        // Handle \deffN separately — set default font index
        if (word == "deff") {
            m_doc.defaultFontIndex = arg;
            return;
        }

        auto* ctrl = findControl(word.c_str());
        if (ctrl) {
            dispatch(*ctrl, arg);
        } else {
            // Unknown tag — record for preservation
            std::string tag = "\\" + word;
            if (arg >= 0) {
                tag += std::to_string(arg);
            }
            m_doc.unknownTags.push_back(tag);
        }
    }

    void accumulateLiteral(char c) {
        m_literalText += c;
        m_pos++;
    }

    void finalizeRun() {
        if (m_literalText.empty()) return;

        // Trim leading/trailing whitespace from the literal
        // but preserve internal whitespace
        std::string trimmed = m_literalText;
        size_t start = trimmed.find_first_not_of(" \t\n\r");
        size_t end = trimmed.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            // All whitespace — skip
            m_literalText.clear();
            return;
        }
        trimmed = trimmed.substr(start, end - start + 1);

        if (trimmed.empty()) {
            m_literalText.clear();
            return;
        }

        if (m_doc.paragraphs.empty()) {
            m_doc.paragraphs.push_back({});
        }

        RtfRunFormat fmt;
        fmt.bold = m_format.bold;
        fmt.italic = m_format.italic;
        fmt.underline = m_format.underline;
        fmt.fontIndex = m_format.fontIndex;
        fmt.fontSize = m_format.fontSize;
        fmt.colorIndex = m_format.colorIndex;
        fmt.bgColorIndex = m_format.bgColorIndex;
        fmt.highlight = m_format.highlight;
        fmt.superscript = m_format.superscript;
        fmt.subscript = m_format.subscript;
        fmt.strikeOut = m_format.strikeOut;
        fmt.underlineStyle = m_format.underlineStyle;
        fmt.capitalization = m_format.capitalization;
        fmt.upOffset = m_format.upOffset;
        fmt.dnOffset = m_format.dnOffset;

        m_doc.paragraphs.back().runs.emplace_back(trimmed, std::move(fmt));
        m_literalText.clear();
    }

    void appendUtf8(int cp) {
        if (cp < 0x80) {
            m_literalText += static_cast<char>(cp);
        } else if (cp < 0x800) {
            m_literalText += static_cast<char>(0xC0 | (cp >> 6));
            m_literalText += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            m_literalText += static_cast<char>(0xE0 | (cp >> 12));
            m_literalText += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            m_literalText += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            m_literalText += static_cast<char>(0xF0 | (cp >> 18));
            m_literalText += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            m_literalText += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            m_literalText += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool matches(const char* s) {
        size_t len = strlen(s);
        if (m_pos + len > m_len) return false;
        for (size_t i = 0; i < len; ++i) {
            if (static_cast<unsigned char>(m_rtf[m_pos + i]) !=
                static_cast<unsigned char>(s[i]))
            {
                return false;
            }
        }
        // Verify it's a proper control word boundary
        if (m_pos + len < m_len && isWordChar(m_rtf[m_pos + len])) return false;
        return true;
    }

    int parseInt() {
        int val = 0;
        while (m_pos < m_len && isDigit(m_rtf[m_pos])) {
            val = val * 10 + (m_rtf[m_pos] - '0');
            m_pos++;
        }
        return val;
    }

    void skipWhitespace() {
        while (m_pos < m_len && isWhitespace(m_rtf[m_pos])) {
            m_pos++;
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

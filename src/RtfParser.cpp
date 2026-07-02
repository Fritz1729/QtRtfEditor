#include "RtfParser.h"

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
    bool superscript = false;
    bool subscript = false;
};

struct ParagraphState {
    int alignment = 1;            // Qt::AlignLeft
    int leftIndent = 0;
    int firstLineIndent = 0;
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

        // Paragraph formatting
        if (word == "ql" || word == "qj") { m_para.alignment = 1; return; }
        if (word == "qc") { m_para.alignment = 128; return; }
        if (word == "qr") { m_para.alignment = 2; return; }
        if (word == "li") { m_para.leftIndent = arg; return; }
        if (word == "fi") { m_para.firstLineIndent = arg; return; }

        // Paragraph separator
        if (word == "par") {
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
            return;
        }

        // Character formatting
        if (word == "b" && arg == 0) { finalizeRun(); m_format.bold = false; return; }
        if (word == "b0") { finalizeRun(); m_format.bold = false; return; }
        if (word == "b") { finalizeRun(); m_format.bold = true; return; }
        if (word == "i" && arg == 0) { finalizeRun(); m_format.italic = false; return; }
        if (word == "i0") { finalizeRun(); m_format.italic = false; return; }
        if (word == "i") { finalizeRun(); m_format.italic = true; return; }
        if (word == "ul" && arg == 0) { finalizeRun(); m_format.underline = false; return; }
        if (word == "ul0") { finalizeRun(); m_format.underline = false; return; }
        if (word == "ul") { finalizeRun(); m_format.underline = true; return; }
        if (word == "sub" && arg == 0) { finalizeRun(); m_format.subscript = false; return; }
        if (word == "sub0") { finalizeRun(); m_format.subscript = false; return; }
        if (word == "sub") { finalizeRun(); m_format.subscript = true; m_format.superscript = false; return; }
        if (word == "super" && arg == 0) { finalizeRun(); m_format.superscript = false; m_format.subscript = false; return; }
        if (word == "super0") { finalizeRun(); m_format.superscript = false; m_format.subscript = false; return; }
        if (word == "super") { finalizeRun(); m_format.superscript = true; m_format.subscript = false; return; }
        if (word == "f" && arg >= 0) { finalizeRun(); m_format.fontIndex = arg; return; }
        if (word == "fs" && arg >= 0) { finalizeRun(); m_format.fontSize = arg; return; }
        if (word == "cf" && arg >= 0) { finalizeRun(); m_format.colorIndex = arg; return; }

        // Table entry markers (inside table groups, handled by table parsers)
        if (word == "red" || word == "green" || word == "blue") return;
        if (word == "froman" || word == "fswiss" || word == "fmodern" || word == "fnil") return;
        if (word == "fcharset") return;

        // RTF header control words — capture deffN, ignore rest
        if (word == "deff") { m_doc.defaultFontIndex = arg; return; }
        if (word == "rtf" || word == "ansi" || word == "deff0" ||
            word == "mac" || word == "pca" || word == "ansicpg" ||
            word == "ucci") return;

        // Unknown tag
        std::string tag = "\\" + word;
        if (arg >= 0) {
            tag += std::to_string(arg);
        }
        m_doc.unknownTags.push_back(tag);
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
        fmt.superscript = m_format.superscript;
        fmt.subscript = m_format.subscript;

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

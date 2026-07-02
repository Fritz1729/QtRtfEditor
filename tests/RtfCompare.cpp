#include "RtfCompare.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

namespace Rte {

bool RtfColorEntry::operator==(const RtfColorEntry &other) const {
    return red == other.red && green == other.green && blue == other.blue;
}

bool RtfFontEntry::operator==(const RtfFontEntry &other) const {
    return family == other.family;
}

bool RtfRunFormat::operator==(const RtfRunFormat &other) const {
    return bold == other.bold &&
           italic == other.italic &&
           underline == other.underline &&
           fontIndex == other.fontIndex &&
           fontSize == other.fontSize &&
           colorIndex == other.colorIndex &&
           superscript == other.superscript &&
           subscript == other.subscript;
}

bool RtfRun::operator==(const RtfRun &other) const {
    return text == other.text && format == other.format;
}

bool RtfParagraph::operator==(const RtfParagraph &other) const {
    if (alignment != other.alignment ||
        leftIndent != other.leftIndent ||
        firstLineIndent != other.firstLineIndent ||
        rightIndent != other.rightIndent)
    {
        return false;
    }
    if (runs.size() != other.runs.size()) {
        return false;
    }
    for (size_t i = 0; i < runs.size(); ++i) {
        if (runs[i] != other.runs[i]) {
            return false;
        }
    }
    return true;
}

// ── Internal parser state ──────────────────────────────────────

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
    int rightIndent = 0;
};

class RtfParser {
public:
    RtfDocument parse(const std::string &rtf) {
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
            const auto &last = m_doc.paragraphs.back();
            bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                [](const RtfRun &r) { return !r.text.empty(); });
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

    // ── Tokenization ──────────────────────────────────────────

    void parse() {
        while (m_pos < m_len) {
            if (++m_iter > kMaxIter) throw std::runtime_error("parser iteration limit");
            char c = m_rtf[m_pos];
            if (c == '{') {
                parseGroup();
            } else if (c == '}') {
                m_pos++;
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
        if (!m_formatStack.empty()) {
            m_format = m_formatStack.back();
            m_formatStack.pop_back();
        }
        if (!m_paraStateStack.empty()) {
            m_para = m_paraStateStack.back();
            m_paraStateStack.pop_back();
        }
        finalizeRun();
    }

    void parseControl() {
        m_pos++; // skip '\'
        skipWhitespace();

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
        } else if (c == 'u' || c == 'U') {
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

        if (hasArg && m_pos < m_len && !isWordChar(m_rtf[m_pos]) && m_rtf[m_pos] != '\\' && m_rtf[m_pos] != '}') {
            m_pos++; // skip trailing whitespace after numeric argument
        }

        if (word.empty()) return;

        processControlWord(word, hasArg ? arg : 0);
    }

    void parseUnicode() {
        // \uNNN? — UTF-16 code point followed by '?' delimiter
        if (m_pos < m_len && m_rtf[m_pos] == '?') {
            m_pos++; // skip '?'
        }
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
    }

    // ── Control word dispatch ─────────────────────────────────

    void processControlWord(const std::string &word, int arg) {
        // Table group markers (should have been caught in parseGroup)
        if (word == "colortbl" || word == "fonttbl") return;

        // Paragraph formatting
        if (word == "ql" || word == "qj") { m_para.alignment = 1; return; }
        if (word == "qc") { m_para.alignment = 128; return; }
        if (word == "qr") { m_para.alignment = 2; return; }
        if (word == "li") { m_para.leftIndent = arg; return; }
        if (word == "fi") { m_para.firstLineIndent = arg; return; }
        if (word == "ri") { m_para.rightIndent = arg; return; }

        // Paragraph separator
        if (word == "par") {
            finalizeRun();
            // Apply paragraph formatting to the last non-empty paragraph
            for (int k = static_cast<int>(m_doc.paragraphs.size()) - 1; k >= 0; --k) {
                if (!m_doc.paragraphs[k].runs.empty() &&
                    std::any_of(m_doc.paragraphs[k].runs.begin(), m_doc.paragraphs[k].runs.end(),
                        [](const RtfRun &r) { return !r.text.empty(); }))
                {
                    m_doc.paragraphs[k].alignment = m_para.alignment;
                    m_doc.paragraphs[k].leftIndent = m_para.leftIndent;
                    m_doc.paragraphs[k].firstLineIndent = m_para.firstLineIndent;
                    m_doc.paragraphs[k].rightIndent = m_para.rightIndent;
                    break;
                }
            }
            // Remove trailing empty paragraphs
            while (!m_doc.paragraphs.empty()) {
                const auto &last = m_doc.paragraphs.back();
                bool hasText = std::any_of(last.runs.begin(), last.runs.end(),
                    [](const RtfRun &r) { return !r.text.empty(); });
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
            return;
        }

        // Character formatting
        if (word == "b") { m_format.bold = true; return; }
        if (word == "b0") { m_format.bold = false; return; }
        if (word == "i") { m_format.italic = true; return; }
        if (word == "i0") { m_format.italic = false; return; }
        if (word == "ul") { m_format.underline = true; return; }
        if (word == "ul0") { m_format.underline = false; return; }
        if (word == "f") { m_format.fontIndex = arg; return; }
        if (word == "fs") { m_format.fontSize = arg; return; }
        if (word == "cf") { m_format.colorIndex = arg; return; }
        if (word == "sub") { m_format.subscript = true; m_format.superscript = false; return; }
        if (word == "sub0") { m_format.subscript = false; return; }
        if (word == "super") { m_format.superscript = true; m_format.subscript = false; return; }
        if (word == "super0") { m_format.superscript = false; return; }

        // Table entry markers (inside table groups, handled by table parsers)
        if (word == "red" || word == "green" || word == "blue") return;
        if (word == "froman" || word == "fswiss" || word == "fmodern" || word == "fnil") return;
        if (word == "fcharset") return;

        // RTF header control words — ignore
        if (word == "rtf" || word == "ansi" || word == "deff0" ||
            word == "deff" || word == "mac" || word == "pca" || word == "ansicpg" ||
            word == "ucci") return;

        // Unknown tag
        std::string tag = "\\" + word;
        if (arg >= 0) {
            tag += std::to_string(arg);
        }
        m_doc.unknownTags.push_back(tag);
    }

    // ── Helpers ───────────────────────────────────────────────

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

    bool matches(const char *s) {
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

// ── Public API ─────────────────────────────────────────────────

RtfDocument ParseRtf(const std::string &rtf) {
    return RtfParser().parse(rtf);
}

static bool IsColorInBounds(int index, size_t tableSize) {
    return index >= 0 && index < static_cast<int>(tableSize);
}

static bool ResolveAndCompareColors(int paraIdx, int runIdx,
    const RtfRun &runA, const RtfRun &runB,
    const RtfDocument &a, const RtfDocument &b,
    std::string &reason) {
    // Resolve to actual RGB for semantic comparison
    RtfColorEntry resolvedA = {0, 0, 0};
    RtfColorEntry resolvedB = {0, 0, 0};
    bool hasA = IsColorInBounds(runA.format.colorIndex, a.colors.size());
    bool hasB = IsColorInBounds(runB.format.colorIndex, b.colors.size());
    if (hasA) resolvedA = a.colors[runA.format.colorIndex];
    if (hasB) resolvedB = b.colors[runB.format.colorIndex];

    std::string boundMsg;
    if (!hasA || !hasB) {
        boundMsg = " [out-of-bounds: A=" + std::to_string(runA.format.colorIndex) +
            "/" + std::to_string(a.colors.size()) +
            ", B=" + std::to_string(runB.format.colorIndex) +
            "/" + std::to_string(b.colors.size()) + "]";
    }

    if (hasA && hasB && resolvedA.red == resolvedB.red &&
        resolvedA.green == resolvedB.green && resolvedA.blue == resolvedB.blue) {
        // Same resolved color despite different indices — not a diff
        return false;
    }

    std::string rgbA = "(" + std::to_string(resolvedA.red) + "," +
        std::to_string(resolvedA.green) + "," +
        std::to_string(resolvedA.blue) + ")";
    std::string rgbB = "(" + std::to_string(resolvedB.red) + "," +
        std::to_string(resolvedB.green) + "," +
        std::to_string(resolvedB.blue) + ")";
    reason = "Paragraph " + std::to_string(paraIdx) +
        " run " + std::to_string(runIdx) +
        " colorIndex: " + std::to_string(runA.format.colorIndex) +
        " (" + rgbA + ") vs " + std::to_string(runB.format.colorIndex) +
        " (" + rgbB + ")" + boundMsg;
    return true;
}

RtfCompareResult CompareRtf(const RtfDocument &a, const RtfDocument &b,
                            std::string &reason) {
    // Check for unknown tags
    if (!a.unknownTags.empty()) {
        reason = "Unknown tag in input: " + a.unknownTags.back();
        return RtfCompareResult::UnknownTag;
    }
    if (!b.unknownTags.empty()) {
        reason = "Unknown tag in output: " + b.unknownTags.back();
        return RtfCompareResult::UnknownTag;
    }

    // Compare paragraph count
    if (a.paragraphs.size() != b.paragraphs.size()) {
        reason = "Paragraph count: " + std::to_string(a.paragraphs.size()) +
                 " vs " + std::to_string(b.paragraphs.size());
        return RtfCompareResult::StructuralDiff;
    }

    for (size_t i = 0; i < a.paragraphs.size(); ++i) {
        const auto &paraA = a.paragraphs[i];
        const auto &paraB = b.paragraphs[i];

        // Resolve alignment names
        std::string alignA, alignB;
        if (paraA.alignment == 1) alignA = "left";
        else if (paraA.alignment == 128) alignA = "center";
        else if (paraA.alignment == 2) alignA = "right";
        else alignA = "unknown(" + std::to_string(paraA.alignment) + ")";

        if (paraB.alignment == 1) alignB = "left";
        else if (paraB.alignment == 128) alignB = "center";
        else if (paraB.alignment == 2) alignB = "right";
        else alignB = "unknown(" + std::to_string(paraB.alignment) + ")";

        // Compare paragraph-level formatting
        if (paraA.alignment != paraB.alignment) {
            reason = "Paragraph " + std::to_string(i) +
                     " alignment: " + alignA + " vs " + alignB;
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.leftIndent != paraB.leftIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " leftIndent: " + std::to_string(paraA.leftIndent) +
                     " vs " + std::to_string(paraB.leftIndent);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.firstLineIndent != paraB.firstLineIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " firstLineIndent: " + std::to_string(paraA.firstLineIndent) +
                     " vs " + std::to_string(paraB.firstLineIndent);
            return RtfCompareResult::StructuralDiff;
        }
        if (paraA.rightIndent != paraB.rightIndent) {
            reason = "Paragraph " + std::to_string(i) +
                     " rightIndent: " + std::to_string(paraA.rightIndent) +
                     " vs " + std::to_string(paraB.rightIndent);
            return RtfCompareResult::StructuralDiff;
        }

        // Compare runs
        if (paraA.runs.size() != paraB.runs.size()) {
            reason = "Paragraph " + std::to_string(i) +
                     " run count: " + std::to_string(paraA.runs.size()) +
                     " vs " + std::to_string(paraB.runs.size());
            return RtfCompareResult::StructuralDiff;
        }

        for (size_t j = 0; j < paraA.runs.size(); ++j) {
            const auto &runA = paraA.runs[j];
            const auto &runB = paraB.runs[j];

            // Compare text
            if (runA.text != runB.text) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " text: '" + runA.text + "' vs '" + runB.text + "'";
                return RtfCompareResult::StructuralDiff;
            }

            // Compare formatting
            if (runA.format.bold != runB.format.bold) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " bold: " + std::string(runA.format.bold ? "true" : "false") +
                         " vs " + std::string(runB.format.bold ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.italic != runB.format.italic) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " italic: " + std::string(runA.format.italic ? "true" : "false") +
                         " vs " + std::string(runB.format.italic ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.underline != runB.format.underline) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " underline: " + std::string(runA.format.underline ? "true" : "false") +
                         " vs " + std::string(runB.format.underline ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.fontIndex != runB.format.fontIndex) {
                // Resolve font family name for semantic comparison
                std::string familyA, familyB;
                bool hasFontA = runA.format.fontIndex >= 0 &&
                    runA.format.fontIndex < static_cast<int>(a.fonts.size());
                bool hasFontB = runB.format.fontIndex >= 0 &&
                    runB.format.fontIndex < static_cast<int>(b.fonts.size());
                if (hasFontA) familyA = a.fonts[runA.format.fontIndex].family;
                if (hasFontB) familyB = b.fonts[runB.format.fontIndex].family;
                if (hasFontA && hasFontB && familyA == familyB) {
                    // Same resolved font family — skip
                } else {
                    reason = "Paragraph " + std::to_string(i) +
                             " run " + std::to_string(j) +
                             " fontIndex: " + std::to_string(runA.format.fontIndex) +
                             " (" + familyA + ") vs " + std::to_string(runB.format.fontIndex) +
                             " (" + familyB + ")";
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.fontSize != runB.format.fontSize) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " fontSize: " + std::to_string(runA.format.fontSize) +
                         " vs " + std::to_string(runB.format.fontSize);
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.colorIndex != runB.format.colorIndex) {
                if (ResolveAndCompareColors(i, j, runA, runB, a, b, reason)) {
                    return RtfCompareResult::StructuralDiff;
                }
            }
            if (runA.format.colorIndex >= 0 &&
                (!IsColorInBounds(runA.format.colorIndex, a.colors.size()) ||
                 !IsColorInBounds(runA.format.colorIndex, b.colors.size()))) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " colorIndex " + std::to_string(runA.format.colorIndex) +
                         " out of bounds in color table (A: " +
                         std::to_string(a.colors.size()) +
                         ", B: " + std::to_string(b.colors.size()) + ")";
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.superscript != runB.format.superscript) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " superscript: " + std::string(runA.format.superscript ? "true" : "false") +
                         " vs " + std::string(runB.format.superscript ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
            if (runA.format.subscript != runB.format.subscript) {
                reason = "Paragraph " + std::to_string(i) +
                         " run " + std::to_string(j) +
                         " subscript: " + std::string(runA.format.subscript ? "true" : "false") +
                         " vs " + std::string(runB.format.subscript ? "true" : "false");
                return RtfCompareResult::StructuralDiff;
            }
        }
    }

    return RtfCompareResult::Identical;
}

RtfCompareResult CompareRtf(const std::string &rtfA, const std::string &rtfB,
                            std::string &reason) {
    RtfDocument docA = ParseRtf(rtfA);
    RtfDocument docB = ParseRtf(rtfB);
    return CompareRtf(docA, docB, reason);
}

} // namespace Rte

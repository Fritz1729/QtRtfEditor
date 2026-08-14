#pragma once

#include <cstdint>
#include <string>

#include "RteExport.h"
#include "RtfTypes.h"
#include "RtfParserTable.h"
#include "RtfParserContext.h"

namespace Rte {

struct RtfControl;

/**
 * @brief Recursive-descent RTF parser.
 *
 * Single-pass tokenizer with group-aware scope management. Construct, call
 * Parse(), read the result. The parser is stateful and reusable: each Parse()
 * resets all internal state.
 */
class RTE_EXPORT RtfParser {
public:
    /**
     * @brief Parse an RTF string into a structural representation.
     * @param rtf      RTF string (UTF-8).
     * @param codePage Default code page for ANSI hex escapes (default 1252).
     */
    RtfDocument Parse(const std::string& rtf, int codePage = 1252);

private:
    struct ParserScope {
        RtfParser& parser;
        ParserScope(RtfParser& p) : parser(p) {
            p._groupDepth++;
            p.PushState();
        }
        ~ParserScope() { parser.RestoreState(); }
    };

    void SkipGroup();
    const RtfControl* FindControl(const char* word) const;
    void Dispatch(const RtfControl& ctrl, int arg);
    void HandleParagraph();
    void FlushCurrentParagraph();
    void RemoveTrailingEmptyElements();
    void Parse();
    void ParseGroup();
    void PushState();
    void RestoreState();
    void ParseControl();
    void ParseControlWord();
    void ParseUnicodeEscape();
    void ParseField();
    void ParseFldInst();
    void ParseFldRslt();
    void ProcessControlWord(const std::string& word, int arg);
    void AccumulateLiteral(char c);
    void FinalizeRun();
    void AppendUtf8(uint32_t cp);
    static std::string UnescapeRtfString(const std::string& s);

    RtfDocument _doc;
    TableParser _tableParser{_doc};
    RtfParagraph _currentParagraph;
    InputReader _input;
    std::string _literalText;
    bool _skipLeadingWsTrim = false;
    int _codePage = 1252;
    FormatScopes _scopes;
    ListState _list;

    // Group nesting depth for document-level save
    int _groupDepth = 0;

    // Field parsing state
    bool _inFieldRslt = false;
    std::string _fieldAnchorHref;
};

} // namespace Rte

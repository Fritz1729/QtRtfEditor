#include "RtfParserContext.h"

#include <cctype>

namespace Rte {

void SkipGroup(InputReader& input) {
    // The group's own "{" has already been consumed by the caller.
    int depth = 1;
    while (!input.IsEof() && depth > 0) {
        input.CheckIteration();
        char c = input.Advance();
        if (c == '{') depth++;
        else if (c == '}') depth--;
    }
}

bool HasNonWhitespaceText(const std::vector<RtfRun>& runs) {
    for (const auto& run : runs)
        for (char c : run.text)
            if (!std::isspace(static_cast<unsigned char>(c))) return true;
    return false;
}

bool ParagraphHasNonWhitespaceContent(const RtfParagraph& p) {
    return HasNonWhitespaceText(p.runs);
}

bool TableRowHasNonWhitespaceContent(const RtfTableRowEntry& row) {
    for (const auto& [runs, _] : row.cells)
        if (HasNonWhitespaceText(runs)) return true;
    return false;
}

void RecordUnknownTag(RtfDocument& doc, const std::string& word, int arg) {
    std::string tag = "\\" + word;
    if (arg >= 0) tag += std::to_string(arg);
    doc.unknownTags.push_back(tag);
}

std::pair<std::string, int> ParseControlWordWithArg(InputReader& input) {
    auto [word, arg] = input.ReadControlWord();
    bool hasArg = arg >= 0;
    if (!hasArg && input.PeekIs('-') && input.PeekIf(IsDigit, 1)) {
        input.SkipAs('-');
        arg = 0;
        while (!input.IsEof() && IsDigit(input.Peek())) {
            arg = arg * 10 + (input.Advance() - '0');
        }
        arg = -arg;
        hasArg = true;
    }
    input.ConsumeControlDelimiter(arg, hasArg);
    return {word, arg};
}

void ResetListAndTabState(FormatScopes& scopes, ListState& list) {
    scopes.tabAlign.get() = Qt::AlignLeft;
    list.listId = 0;
    list.listLevel = 0;
    list.listStyle = RtfListStyle::None;
}

void FlushCurrentParagraph(RtfDocument& doc, RtfParagraph& currentParagraph,
                           FormatScopes& scopes, ListState& list) {
    // Skip the initial empty paragraph before any content has been flushed.
    // Empty paragraphs after the first flush are preserved (blank lines).
    if (!list.paragraphFlushed && !ParagraphHasNonWhitespaceContent(currentParagraph)) {
        currentParagraph = {};
        return;
    }
    list.paragraphFlushed = true;
    currentParagraph.format = scopes.para.get();
    currentParagraph.listId = list.listId;
    currentParagraph.listLevel = list.listLevel;
    currentParagraph.listStyle = list.listStyle;
    currentParagraph.listIndent = scopes.para.get().leftIndent;
    currentParagraph.defaultFontIndex = scopes.deff.get();
    currentParagraph.defaultTabStopTwips = scopes.deftab.get();
    doc.elements.push_back(std::move(currentParagraph));
    currentParagraph = {};
}

} // namespace Rte

#include "RtfParserContext.h"

#include <cctype>

namespace Rte {

namespace {
    constexpr size_t kParserMaxIter = 10'000'000;
}

void CheckIter(size_t& iter) {
    if (++iter > kParserMaxIter) throw std::runtime_error("parser iteration limit");
}

void SkipGroup(InputReader& input, size_t& iter) {
    // The group's own "{" has already been consumed by the caller.
    int depth = 1;
    while (!input.IsEof() && depth > 0) {
        CheckIter(iter);
        char c = input.Advance();
        if (c == '{') depth++;
        else if (c == '}') depth--;
    }
}

bool ParagraphHasNonWhitespaceContent(const RtfParagraph& p) {
    for (const auto& run : p.runs) {
        for (char c : run.text) {
            if (!std::isspace(static_cast<unsigned char>(c))) return true;
        }
    }
    return false;
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

} // namespace Rte

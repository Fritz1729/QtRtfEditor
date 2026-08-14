#include "RtfParserContext.h"
#include "RtfControl.h"

namespace Rte {

void ParseFonttbl(InputReader& input, RtfDocument& doc) {
    // {\fonttbl{\f0\froman\fcharset0 Times New Roman;}
    //        {\f1\fswiss\fcharset0 Arial;}}
    // Each font entry is a group containing \fN and the family name
    while (!input.IsEof() && !input.PeekIs('}')) {
        input.CheckIteration();
        if (input.PeekIs('{')) {
            // Font entry group
            input.SkipAs('{');

            int fcharset = 0;
            std::string family;

            while (!input.IsEof() && !input.PeekIs('}')) {
                if (input.PeekIs('\\')) {
                    if (input.ConsumeMatch("\\fcharset")) {
                        fcharset = input.ParseInt();
                    } else if (input.ConsumeMatch("\\f")) {
                        input.ParseInt();
                    } else {
                        // Parse control word; record truly unknown ones
                        input.SkipAs('\\');
                        auto [word, arg] = ParseControlWordWithArg(input);
                        if (!word.empty() && !Rte::FindControl(word.c_str())) {
                            RecordUnknownTag(doc, word, arg);
                        }
                    }
                } else if (input.PeekIs('{')) {
                    // Nested group — skip all (e.g. {\*\fname ...} or any sub-group)
                    input.SkipAs('{');
                    SkipGroup(input);
                } else if (!input.PeekIs(';') && IsPrintable(input.Peek())) {
                    family += input.Advance();
                } else {
                    input.Advance();
                }
            }

            if (!input.IsEof()) input.SkipAs('}');

            // Remove leading/trailing whitespace from family
            while (!family.empty() && family.back() == ' ') family.pop_back();
            size_t firstNonSpace = 0;
            while (firstNonSpace < family.size() && family[firstNonSpace] == ' ') firstNonSpace++;
            if (firstNonSpace > 0) family.erase(family.begin(), family.begin() + static_cast<ptrdiff_t>(firstNonSpace));

            if (!family.empty()) {
                doc.fonts.push_back({family, fcharset});
            }
        } else {
            input.Advance();
        }
    }
    input.SkipAs('}');
}

} // namespace Rte

#include "RtfParserContext.h"

namespace Rte {

void ParseColortbl(InputReader& input, RtfDocument& doc) {
    // {\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
    // Each ';' separates a color entry. First entry is "auto" (may be empty).
    while (!input.IsEof() && !input.PeekIs('}')) {
        input.CheckIteration();
        input.SkipWhitespace();

        int r = 0, g = 0, b = 0;

        while (!input.IsEof() && !input.PeekIs(';') && !input.PeekIs('}')) {
            if (input.ConsumeMatch("\\red")) {
                r = input.ParseInt();
            } else if (input.ConsumeMatch("\\green")) {
                g = input.ParseInt();
            } else if (input.ConsumeMatch("\\blue")) {
                b = input.ParseInt();
            } else if (IsPrintable(input.Peek())) {
                input.Advance();
            } else {
                break;
            }
        }

        // Always add color entry (first entry may be empty "auto" color)
        doc.colors.push_back({r, g, b});

        if (!input.IsEof()) input.Advance(); // skip ';' or '}'
    }
    input.SkipAs('}');
}

} // namespace Rte

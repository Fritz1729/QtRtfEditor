#include "RtfParserContext.h"

namespace Rte {

void ParseColortbl(RtfParserContext& ctx) {
    // {\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
    // Each ';' separates a color entry. First entry is "auto" (may be empty).
    while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
        CheckIter(ctx.iter);
        ctx.input.SkipWhitespace();

        int r = 0, g = 0, b = 0;

        while (!ctx.input.IsEof() && !ctx.input.PeekIs(';') && !ctx.input.PeekIs('}')) {
            if (ctx.input.ConsumeMatch("\\red")) {
                r = ctx.input.ParseInt();
            } else if (ctx.input.ConsumeMatch("\\green")) {
                g = ctx.input.ParseInt();
            } else if (ctx.input.ConsumeMatch("\\blue")) {
                b = ctx.input.ParseInt();
            } else if (IsPrintable(ctx.input.Peek())) {
                ctx.input.Advance();
            } else {
                break;
            }
        }

        // Always add color entry (first entry may be empty "auto" color)
        ctx.doc.colors.push_back({r, g, b});

        if (!ctx.input.IsEof()) ctx.input.Advance(); // skip ';' or '}'
    }
    ctx.input.SkipAs('}');
}

} // namespace Rte

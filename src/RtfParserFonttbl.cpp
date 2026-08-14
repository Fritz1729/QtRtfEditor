#include "RtfParserContext.h"
#include "RtfControl.h"

namespace Rte {

void ParseFonttbl(RtfParserContext& ctx) {
    // {\fonttbl{\f0\froman\fcharset0 Times New Roman;}
    //        {\f1\fswiss\fcharset0 Arial;}}
    // Each font entry is a group containing \fN and the family name
    while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
        CheckIter(ctx.iter);
        if (ctx.input.PeekIs('{')) {
            // Font entry group
            ctx.input.SkipAs('{');

            int fcharset = 0;
            std::string family;

            while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
                if (ctx.input.PeekIs('\\')) {
                    if (ctx.input.ConsumeMatch("\\fcharset")) {
                        fcharset = ctx.input.ParseInt();
                    } else if (ctx.input.ConsumeMatch("\\f")) {
                        ctx.input.ParseInt();
                    } else {
                        // Parse control word; record truly unknown ones
                        ctx.input.SkipAs('\\');
                        auto [word, arg] = ParseControlWordWithArg(ctx.input);
                        if (!word.empty() && !Rte::FindControl(word.c_str())) {
                            std::string tag = "\\" + word;
                            if (arg >= 0) tag += std::to_string(arg);
                            ctx.doc.unknownTags.push_back(tag);
                        }
                    }
                } else if (ctx.input.PeekIs('{')) {
                    // Nested group — skip all (e.g. {\*\fname ...} or any sub-group)
                    ctx.input.SkipAs('{');
                    SkipGroup(ctx.input, ctx.iter);
                } else if (!ctx.input.PeekIs(';') && IsPrintable(ctx.input.Peek())) {
                    family += ctx.input.Advance();
                } else {
                    ctx.input.Advance();
                }
            }

            if (!ctx.input.IsEof()) ctx.input.SkipAs('}');

            // Remove leading/trailing whitespace from family
            while (!family.empty() && family.back() == ' ') family.pop_back();
            size_t firstNonSpace = 0;
            while (firstNonSpace < family.size() && family[firstNonSpace] == ' ') firstNonSpace++;
            if (firstNonSpace > 0) family.erase(family.begin(), family.begin() + static_cast<ptrdiff_t>(firstNonSpace));

            if (!family.empty()) {
                ctx.doc.fonts.push_back({family, fcharset});
            }
        } else {
            ctx.input.Advance();
        }
    }
    ctx.input.SkipAs('}');
}

} // namespace Rte

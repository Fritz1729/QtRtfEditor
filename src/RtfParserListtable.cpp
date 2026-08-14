#include "RtfParserContext.h"

namespace Rte {

namespace {

RtfListStyle RtfLevelNfcToListStyle(int nfc) {
    switch (static_cast<RtfLevelNfc>(nfc)) {
        case RtfLevelNfc::Arabic:           return RtfListStyle::Number;
        case RtfLevelNfc::UpperRoman:       return RtfListStyle::Roman;
        case RtfLevelNfc::LowerRoman:       return RtfListStyle::Roman;
        case RtfLevelNfc::UpperAlpha:       return RtfListStyle::Letter;
        case RtfLevelNfc::LowerAlpha:       return RtfListStyle::Letter;
        case RtfLevelNfc::Ordinal:          return RtfListStyle::Number;
        case RtfLevelNfc::CardinalText:     return RtfListStyle::Number;
        case RtfLevelNfc::OrdinalText:      return RtfListStyle::Number;
        case RtfLevelNfc::ArabicLeadingZero: return RtfListStyle::Number;
        case RtfLevelNfc::Bullet:           return RtfListStyle::Disc;
        case RtfLevelNfc::NoNumber:         return RtfListStyle::None;
    }
    return RtfListStyle::Number;
}

void ParseListtableControl(InputReader& input, size_t& iter,
                           int& listId, RtfListStyle& listStyle,
                           std::map<int, RtfListStyle>& listIdToStyle) {
    input.SkipAs('\\');
    if (input.IsEof()) return;

    auto [word, arg] = input.ReadControlWord();
    bool hasArg = arg >= 0;

    if (word == "list" || word == "listoverride") {
        if (listId > 0 && listStyle != RtfListStyle::None) {
            listIdToStyle[listId] = listStyle;
        }
        listId = 0;
        listStyle = RtfListStyle::None;
    } else if (word == "listid" && hasArg) {
        listId = arg;
        listStyle = RtfListStyle::None;
    } else if (word == "listlevel") {
        // \listlevel opens a group {\listlevel ... \levelnfcN ...}
        input.SkipWhitespace();
        if (!input.IsEof() && input.PeekIs('{')) {
            input.SkipAs('{');
            while (!input.IsEof() && !input.PeekIs('}')) {
                CheckIter(iter);
                if (input.PeekIs('\\')) {
                    input.SkipAs('\\');
                    auto [innerWord, innerArg] = input.ReadControlWord();
                    if (innerWord == "levelnfc" && innerArg >= 0) {
                        if (listId > 0) {
                            listStyle = RtfLevelNfcToListStyle(innerArg);
                            listIdToStyle[listId] = listStyle;
                        }
                    }
                } else {
                    input.Advance();
                }
            }
            input.SkipAs('}');
        }
    } else if (word == "listname") {
        while (!input.IsEof() && !input.PeekIs('"')) input.Advance();
        while (!input.IsEof() && !input.PeekIs('"')) input.Advance();
    }
}

} // namespace

void ParseListtable(RtfListtableContext& ctx) {
    int currentListId = 0;
    RtfListStyle currentListStyle = RtfListStyle::None;

    while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
        CheckIter(ctx.iter);
        if (ctx.input.PeekIs('{')) {
            ctx.input.SkipAs('{');
            while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
                CheckIter(ctx.iter);
                if (ctx.input.PeekIs('\\')) {
                    ParseListtableControl(ctx.input, ctx.iter,
                                          currentListId, currentListStyle,
                                          ctx.listIdToStyle);
                } else {
                    ctx.input.Advance();
                }
            }
            ctx.input.SkipAs('}');
        } else if (ctx.input.PeekIs('\\')) {
            ParseListtableControl(ctx.input, ctx.iter,
                                  currentListId, currentListStyle,
                                  ctx.listIdToStyle);
        } else {
            ctx.input.Advance();
        }
    }
    ctx.input.SkipAs('}');
}

} // namespace Rte

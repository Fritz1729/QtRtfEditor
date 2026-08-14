#include "RtfParserContext.h"

namespace Rte {

namespace {

void ParsePictControl(RtfParserScopeContext& ctx) {
    ctx.input.SkipAs('\\');

    if (ctx.input.IsEof()) return;

    char c = ctx.input.Peek();
    if (IsWordChar(c)) {
        auto [word, arg] = ctx.input.ReadControlWord();
        bool hasArg = arg >= 0;
        if (word.empty()) return;

        // Known pict control words
        if (word == "jpegblip") { ctx.pict.format = "jpg"; }
        else if (word == "pngblip") { ctx.pict.format = "png"; }
        else if (word == "dibitmap") { ctx.pict.format = "bmp"; }
        else if (word == "picw" && hasArg) { ctx.pict.picw = arg; }
        else if (word == "pich" && hasArg) { ctx.pict.pich = arg; }
        else if (word == "picwgoal" && hasArg) { ctx.pict.picwgoal = arg; }
        else if (word == "pichgoal" && hasArg) { ctx.pict.pichgoal = arg; }
        else if (word == "picscalex" && hasArg) { ctx.pict.picscalex = arg; }
        else if (word == "picscaley" && hasArg) { ctx.pict.picscaley = arg; }
        else if (word == "piccropl" && hasArg) { ctx.pict.piccropl = arg; }
        else if (word == "piccropr" && hasArg) { ctx.pict.piccropr = arg; }
        else if (word == "piccropt" && hasArg) { ctx.pict.piccropt = arg; }
        else if (word == "piccropb" && hasArg) { ctx.pict.piccropb = arg; }
        // Unknown pict control words are silently ignored
        return;
    }

    // Skip other control symbols (digits, etc.)
    if (IsDigit(c)) {
        ctx.input.Advance();
    }
}

RtfImageFormat ImageFormatFromString(const std::string& s) {
    if (s == "jpg") return RtfImageFormat::Jpeg;
    if (s == "png") return RtfImageFormat::Png;
    if (s == "bmp") return RtfImageFormat::Bmp;
    return RtfImageFormat::Unknown;
}

std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(Rte::HexCharValue(hex[i]) << 4 | Rte::HexCharValue(hex[i + 1]));
    }
    return result;
}

} // namespace

void ParsePict(RtfParserScopeContext& ctx) {
    // {\pict\pngblip\picw500\pich500\picscalex500\picwgoal250 <hex-data>}
    // Collect hex-encoded image data between control words
    ctx.pict = RtfPictState{};

    while (!ctx.input.IsEof() && !ctx.input.PeekIs('}')) {
        CheckIter(ctx.iter);
        if (ctx.input.PeekIs('\\')) {
            ParsePictControl(ctx);
            // Skip whitespace after control words
            while (!ctx.input.IsEof() && !ctx.input.PeekIs('}') && IsWhitespace(ctx.input.Peek())) {
                ctx.input.Advance();
            }
        } else {
            // Hex data — but skip any non-hex characters (whitespace, etc.)
            char c = ctx.input.Peek();
            // Only collect uppercase/lowercase hex digits
            if (IsHexDigit(c)) {
                ctx.pict.data += c;
            }
            ctx.input.Advance();
        }
    }

    ctx.input.SkipAs('}');
    if (!ctx.pict.format.empty() && !ctx.pict.data.empty()) {
        RtfImage img;
        img.data = HexToBytes(ctx.pict.data);
        img.format = ImageFormatFromString(ctx.pict.format);
        img.picw = ctx.pict.picw;
        img.pich = ctx.pict.pich;
        img.picwgoal = ctx.pict.picwgoal;
        img.pichgoal = ctx.pict.pichgoal;
        img.picscalex = ctx.pict.picscalex;
        img.picscaley = ctx.pict.picscaley;
        img.piccropl = ctx.pict.piccropl;
        img.piccropr = ctx.pict.piccropr;
        img.piccropt = ctx.pict.piccropt;
        img.piccropb = ctx.pict.piccropb;
        img.rtfPictHex = ctx.pict.data;
        FlushCurrentParagraph(ctx);
        ctx.doc.elements.push_back(std::move(img));
    }
}

} // namespace Rte

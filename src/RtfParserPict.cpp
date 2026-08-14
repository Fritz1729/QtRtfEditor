#include "RtfParserContext.h"

namespace Rte {

namespace {

void ParsePictControl(InputReader& input, RtfPictState& pict) {
    input.SkipAs('\\');

    if (input.IsEof()) return;

    char c = input.Peek();
    if (IsWordChar(c)) {
        auto [word, arg] = input.ReadControlWord();
        bool hasArg = arg >= 0;
        if (word.empty()) return;

        // Known pict control words
        if (word == "jpegblip") { pict.format = "jpg"; }
        else if (word == "pngblip") { pict.format = "png"; }
        else if (word == "dibitmap") { pict.format = "bmp"; }
        else if (word == "picw" && hasArg) { pict.picw = arg; }
        else if (word == "pich" && hasArg) { pict.pich = arg; }
        else if (word == "picwgoal" && hasArg) { pict.picwgoal = arg; }
        else if (word == "pichgoal" && hasArg) { pict.pichgoal = arg; }
        else if (word == "picscalex" && hasArg) { pict.picscalex = arg; }
        else if (word == "picscaley" && hasArg) { pict.picscaley = arg; }
        else if (word == "piccropl" && hasArg) { pict.piccropl = arg; }
        else if (word == "piccropr" && hasArg) { pict.piccropr = arg; }
        else if (word == "piccropt" && hasArg) { pict.piccropt = arg; }
        else if (word == "piccropb" && hasArg) { pict.piccropb = arg; }
        // Unknown pict control words are silently ignored
        return;
    }

    // Skip other control symbols (digits, etc.)
    if (IsDigit(c)) {
        input.Advance();
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

void ParsePict(InputReader& input, RtfDocument& doc, RtfParagraph& currentParagraph,
               FormatScopes& scopes, ListState& list, RtfPictState& pict) {
    // {\pict\pngblip\picw500\pich500\picscalex500\picwgoal250 <hex-data>}
    // Collect hex-encoded image data between control words
    pict = RtfPictState{};

    while (!input.IsEof() && !input.PeekIs('}')) {
        input.CheckIteration();
        if (input.PeekIs('\\')) {
            ParsePictControl(input, pict);
            // Skip whitespace after control words
            while (!input.IsEof() && !input.PeekIs('}') && IsWhitespace(input.Peek())) {
                input.Advance();
            }
        } else {
            // Hex data — but skip any non-hex characters (whitespace, etc.)
            char c = input.Peek();
            // Only collect uppercase/lowercase hex digits
            if (IsHexDigit(c)) {
                pict.data += c;
            }
            input.Advance();
        }
    }

    input.SkipAs('}');
    if (!pict.format.empty() && !pict.data.empty()) {
        RtfImage img;
        img.data = HexToBytes(pict.data);
        img.format = ImageFormatFromString(pict.format);
        img.picw = pict.picw;
        img.pich = pict.pich;
        img.picwgoal = pict.picwgoal;
        img.pichgoal = pict.pichgoal;
        img.picscalex = pict.picscalex;
        img.picscaley = pict.picscaley;
        img.piccropl = pict.piccropl;
        img.piccropr = pict.piccropr;
        img.piccropt = pict.piccropt;
        img.piccropb = pict.piccropb;
        img.rtfPictHex = pict.data;
        FlushCurrentParagraph(doc, currentParagraph, scopes, list);
        doc.elements.push_back(std::move(img));
    }
}

} // namespace Rte

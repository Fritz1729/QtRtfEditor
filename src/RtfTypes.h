#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QFont>
#include <QTextCharFormat>

namespace Rte {

enum class UnderlineStyle : uint8_t {
    None,
    Solid,
    Dotted,
    Dashed,
    Double,
    Thick,
};

enum class Capitalization : uint8_t {
    None,
    AllCaps,
    SmallCaps,
};

struct RtfRunFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeOut = false;
    int fontIndex = 0;
    int fontSize = 0;
    int colorIndex = -1;
    int bgColorIndex = -1;
    int highlight = -1;
    bool superscript = false;
    bool subscript = false;
    UnderlineStyle underlineStyle = UnderlineStyle::None;
    Capitalization capitalization = Capitalization::None;
    int upOffset = 0;
    int dnOffset = 0;

    bool operator==(const RtfRunFormat& other) const = default;
};

struct RtfRun {
    std::string text;
    RtfRunFormat format;

    RtfRun() = default;
    RtfRun(std::string t, RtfRunFormat f) : text(std::move(t)), format(std::move(f)) {}

    bool operator==(const RtfRun &) const = default;
};

struct ParagraphFormatting {
    int alignment = 1;
    int leftIndent = 0;
    int firstLineIndent = 0;
    int rightIndent = 0;
    int spaceBefore = 0;
    int spaceAfter = 0;
    int lineHeight = 0;
    int slMult = 1;
};

struct RtfParagraph : ParagraphFormatting {
    std::vector<RtfRun> runs;

    RtfParagraph() = default;
    explicit RtfParagraph(ParagraphFormatting fmt) : ParagraphFormatting(fmt) {}

    void setFormatting(ParagraphFormatting fmt) {
        *static_cast<ParagraphFormatting*>(this) = fmt;
    }

    bool operator==(const RtfParagraph &) const = default;
};

inline std::string AlignmentToString(int align) {
    switch (align) {
        case 1: return "left";
        case 128: return "center";
        case 2: return "right";
        default: return "unknown(" + std::to_string(align) + ")";
    }
}

inline Qt::Alignment RtfAlignmentToQt(int align) {
    switch (align) {
        case 1: return Qt::AlignLeft;
        case 128: return Qt::AlignHCenter;
        case 2: return Qt::AlignRight;
        default: return Qt::AlignLeft;
    }
}

constexpr std::array<std::array<uint8_t, 3>, 17> kHighlightPalette = {{
    {0, 0, 0},       // 0  = black
    {128, 128, 128}, // 1  = dark gray
    {128, 0, 0},     // 2  = maroon
    {0, 128, 0},     // 3  = dark green
    {128, 128, 0},   // 4  = dark yellow
    {0, 0, 128},     // 5  = navy
    {128, 0, 128},   // 6  = purple
    {0, 128, 128},   // 7  = teal
    {192, 192, 192}, // 8  = silver
    {255, 255, 255}, // 9  = white
    {255, 0, 0},     // 10 = red
    {0, 255, 0},     // 11 = green
    {255, 255, 0},   // 12 = yellow
    {0, 0, 255},     // 13 = blue
    {255, 0, 255},   // 14 = magenta
    {0, 255, 255},   // 15 = cyan
    {128, 128, 128}, // 16 = gray 50%
}};

inline UnderlineStyle toUnderlineStyle(QTextCharFormat::UnderlineStyle qtStyle) {
    switch (qtStyle) {
        case QTextCharFormat::NoUnderline:    return UnderlineStyle::None;
        case QTextCharFormat::SingleUnderline: return UnderlineStyle::Solid;
        case QTextCharFormat::DotLine:        return UnderlineStyle::Dotted;
        case QTextCharFormat::DashUnderline:  return UnderlineStyle::Dashed;
        case QTextCharFormat::WaveUnderline:  return UnderlineStyle::Thick;
        default:                              return UnderlineStyle::None;
    }
}

inline QTextCharFormat::UnderlineStyle qtUnderlineStyleFor(UnderlineStyle style) {
    switch (style) {
        case UnderlineStyle::Dotted:        return QTextCharFormat::DotLine;
        case UnderlineStyle::Dashed:        return QTextCharFormat::DashUnderline;
        case UnderlineStyle::Double:        return QTextCharFormat::SingleUnderline;
        case UnderlineStyle::Thick:         return QTextCharFormat::WaveUnderline;
        default:                            return QTextCharFormat::SingleUnderline;
    }
}

inline Capitalization toCapitalization(QFont::Capitalization qtCaps) {
    switch (qtCaps) {
        case QFont::MixedCase:      return Capitalization::None;
        case QFont::AllUppercase:   return Capitalization::AllCaps;
        case QFont::SmallCaps:      return Capitalization::SmallCaps;
        default:                    return Capitalization::None;
    }
}

} // namespace Rte

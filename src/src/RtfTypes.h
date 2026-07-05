#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QFont>
#include <QTextCharFormat>
#include <QTextListFormat>

namespace Rte {

enum class UnderlineStyle : uint8_t {
    None,
    Solid,
    Dotted,
    Dashed,
    DashDot,
    DashDotDot,
    Double,
    Thick,
};

enum class Capitalization : uint8_t {
    None,
    AllCaps,
    SmallCaps,
};

enum class ListStyle : uint8_t {
    None,
    Disc,
    Bullet,
    Box,
    Check,
    Number,
    Letter,
    Roman,
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
    bool superscript = false;
    bool subscript = false;
    bool kerning = false;
    int expnd = 0;
    UnderlineStyle underlineStyle = UnderlineStyle::None;
    Capitalization capitalization = Capitalization::None;
    int upOffset = 0;
    int dnOffset = 0;
    int ulColorIndex = 0;
    int langId = 0;

    bool operator==(const RtfRunFormat& other) const = default;
};

struct RtfRun {
    std::string text;
    RtfRunFormat format;

    RtfRun() = default;
    RtfRun(std::string t, RtfRunFormat f) : text(std::move(t)), format(std::move(f)) {}

    bool operator==(const RtfRun &) const = default;
};

struct TabStop {
    int position;
    int alignment; // 1=left, 128=center, 2=right

    bool operator==(const TabStop& other) const = default;
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
    std::vector<TabStop> tabStops;
};

struct RtfParagraph : ParagraphFormatting {
    std::vector<RtfRun> runs;
    int listId = 0;
    int listLevel = 0;
    ListStyle listStyle = ListStyle::None;
    int listIndent = 0;

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
        case 3: return "decimal";
        case 4: return "justified";
        default: return "unknown(" + std::to_string(align) + ")";
    }
}

inline Qt::Alignment RtfAlignmentToQt(int align) {
    switch (align) {
        case 1: return Qt::AlignLeft;
        case 128: return Qt::AlignHCenter;
        case 2: return Qt::AlignRight;
        case 4: return Qt::AlignJustify;
        default: return Qt::AlignLeft;
    }
}

// \highlightN is intentionally not supported:
// - RTF 1.5 spec defines names (Black, Blue, Cyan, …) but no RGB values
// - RTF 1.9.1 spec says "index of the color table" but defines no palette
// - The old hardcoded 17-entry table did not match the 1.5 spec names
// - No reliable source for RGB values exists in the spec files

inline UnderlineStyle toUnderlineStyle(QTextCharFormat::UnderlineStyle qtStyle) {
    switch (qtStyle) {
        case QTextCharFormat::NoUnderline:     return UnderlineStyle::None;
        case QTextCharFormat::SingleUnderline: return UnderlineStyle::Solid;
        case QTextCharFormat::DotLine:         return UnderlineStyle::Dotted;
        case QTextCharFormat::DashUnderline:   return UnderlineStyle::Dashed;
        case QTextCharFormat::DashDotLine:     return UnderlineStyle::DashDot;
        case QTextCharFormat::DashDotDotLine:  return UnderlineStyle::DashDotDot;
        case QTextCharFormat::WaveUnderline:   return UnderlineStyle::Thick;
        default:                               return UnderlineStyle::None;
    }
}

inline QTextCharFormat::UnderlineStyle qtUnderlineStyleFor(UnderlineStyle style) {
    switch (style) {
        case UnderlineStyle::Dotted:        return QTextCharFormat::DotLine;
        case UnderlineStyle::Dashed:        return QTextCharFormat::DashUnderline;
        case UnderlineStyle::DashDot:       return QTextCharFormat::DashDotLine;
        case UnderlineStyle::DashDotDot:    return QTextCharFormat::DashDotDotLine;
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

inline QTextListFormat::Style RtfListStyleToQt(ListStyle style) {
    switch (style) {
        case ListStyle::Disc:   return QTextListFormat::ListDisc;
        case ListStyle::Bullet: return QTextListFormat::ListCircle;
        case ListStyle::Box:    return QTextListFormat::ListSquare;
        case ListStyle::Check:  return QTextListFormat::ListDisc;
        case ListStyle::Number: return QTextListFormat::ListDecimal;
        case ListStyle::Letter: return QTextListFormat::ListLowerAlpha;
        case ListStyle::Roman:  return QTextListFormat::ListLowerRoman;
        default:                return QTextListFormat::ListDisc;
    }
}

inline ListStyle QtListStyleToRtf(QTextListFormat::Style style) {
    switch (style) {
        case QTextListFormat::ListDisc:            return ListStyle::Disc;
        case QTextListFormat::ListCircle:          return ListStyle::Bullet;
        case QTextListFormat::ListSquare:          return ListStyle::Box;
        case QTextListFormat::ListDecimal:         return ListStyle::Number;
        case QTextListFormat::ListLowerAlpha:      return ListStyle::Letter;
        case QTextListFormat::ListLowerRoman:      return ListStyle::Roman;
        default:                                   return ListStyle::None;
    }
}

} // namespace Rte

#pragma once

#include <cstdint>
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

struct RtfParagraph {
    int alignment = 1;
    int leftIndent = 0;
    int firstLineIndent = 0;
    int rightIndent = 0;
    int spaceBefore = 0;
    int spaceAfter = 0;
    int lineHeight = 0;
    int slMult = 1;
    std::vector<RtfRun> runs;

    RtfParagraph() = default;
    explicit RtfParagraph(ParagraphFormatting fmt) : alignment(fmt.alignment),
        leftIndent(fmt.leftIndent), firstLineIndent(fmt.firstLineIndent),
        rightIndent(fmt.rightIndent), spaceBefore(fmt.spaceBefore),
        spaceAfter(fmt.spaceAfter), lineHeight(fmt.lineHeight),
        slMult(fmt.slMult) {}

    void setFormatting(ParagraphFormatting fmt) {
        alignment = fmt.alignment;
        leftIndent = fmt.leftIndent;
        firstLineIndent = fmt.firstLineIndent;
        rightIndent = fmt.rightIndent;
        spaceBefore = fmt.spaceBefore;
        spaceAfter = fmt.spaceAfter;
        lineHeight = fmt.lineHeight;
        slMult = fmt.slMult;
    }

    bool operator==(const RtfParagraph &) const = default;
};

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

inline Capitalization toCapitalization(QFont::Capitalization qtCaps) {
    switch (qtCaps) {
        case QFont::MixedCase:      return Capitalization::None;
        case QFont::AllUppercase:   return Capitalization::AllCaps;
        case QFont::SmallCaps:      return Capitalization::SmallCaps;
        default:                    return Capitalization::None;
    }
}

} // namespace Rte

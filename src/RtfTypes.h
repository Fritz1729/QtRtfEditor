#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <variant>

#include <QFont>
#include <QtGlobal>

namespace Rte {

enum class RtfUnderlineStyle : int {
    NoUnderline = 0, Single = 1, Dash = 2, DotLine = 3, DashDotLine = 4,
    DashDotDotLine = 5, Wave = 6, SpellCheck = 7,
    Double = 8, Thick = 9,
    COUNT
};

enum class RtfListStyle : int {
    None = 0,
    Disc = 1, Circle = 2, Square = 3, Box = 4, Check = 5,
    Number = 6, Letter = 7, Roman = 8,
};

// RTF \levelnfcN values (RTF 1.5 spec, section 5.8)
enum class RtfLevelNfc : int {
    Arabic = 0,
    UpperRoman = 1,
    LowerRoman = 2,
    UpperAlpha = 3,
    LowerAlpha = 4,
    Ordinal = 5,
    CardinalText = 6,
    OrdinalText = 7,
    ArabicLeadingZero = 22,
    Bullet = 23,
    NoNumber = 255,
};

enum TableSide : size_t {
    Side_Left = 0,
    Side_Top = 1,
    Side_Right = 2,
    Side_Bottom = 3,
    Side_Undefined,
};

constexpr std::array<TableSide, 4> kTableSides = {{
    Side_Left, Side_Top, Side_Right, Side_Bottom
}};

// RTF border/padding tags for table sides (left, top, right, bottom)
constexpr std::array<const char*, 4> kRowBorderSideTags = {{
    "\\trbrdrl", "\\trbrdrt", "\\trbrdrr", "\\trbrdrb"
}};
constexpr std::array<const char*, 4> kCellBorderSideTags = {{
    "\\clbrdrl", "\\clbrdrt", "\\clbrdrr", "\\clbrdrb"
}};
constexpr std::array<const char*, 4> kCellPaddingTags = {{
    "clpadl", "clpadt", "clpadr", "clpadb"
}};

template<typename Fn>
void IterateTableSides(Fn&& fn) {
    for (TableSide side : kTableSides) fn(side);
}

enum class BorderStyle : uint8_t {
    None = 0,
    Solid = 1,
    Dashed = 2,
};

struct TableCellBorders {
    int leftWidth = 0;
    int topWidth = 0;
    int rightWidth = 0;
    int bottomWidth = 0;
    int leftColor = 0;
    int topColor = 0;
    int rightColor = 0;
    int bottomColor = 0;
    BorderStyle leftStyle = BorderStyle::None;
    BorderStyle topStyle = BorderStyle::None;
    BorderStyle rightStyle = BorderStyle::None;
    BorderStyle bottomStyle = BorderStyle::None;

    bool operator==(const TableCellBorders& other) const = default;
};

struct TableCellBorderMember {
    int TableCellBorders::*width;
    BorderStyle TableCellBorders::*style;
    int TableCellBorders::*color;
};

constexpr std::array<TableCellBorderMember, 4> kBorderMembers = {{
    { &TableCellBorders::leftWidth,  &TableCellBorders::leftStyle,  &TableCellBorders::leftColor },
    { &TableCellBorders::topWidth,   &TableCellBorders::topStyle,   &TableCellBorders::topColor },
    { &TableCellBorders::rightWidth, &TableCellBorders::rightStyle, &TableCellBorders::rightColor },
    { &TableCellBorders::bottomWidth,&TableCellBorders::bottomStyle,&TableCellBorders::bottomColor },
}};

struct TableCellFormat {
    int width = 0;
    int vertAlign = 0;
    int shadingColor = -1;
    TableCellBorders borders;
    std::array<int, 4> padding = {{0, 0, 0, 0}};

    bool operator==(const TableCellFormat& other) const = default;
};

enum class RtfImageFormat : uint8_t {
    Jpeg,
    Png,
    Bmp,
    Unknown,
};

struct RtfImage {
    std::vector<uint8_t> data;
    RtfImageFormat format = RtfImageFormat::Unknown;
    int picw = 0;
    int pich = 0;
    int picwgoal = 0;
    int pichgoal = 0;
    int picscalex = 100;
    int picscaley = 100;
    int piccropl = 0;
    int piccropr = 0;
    int piccropt = 0;
    int piccropb = 0;
    std::string rtfPictHex;

    bool operator==(const RtfImage &) const = default;
};

struct RtfColorEntry {
    int red = 0, green = 0, blue = 0;
    bool operator==(const RtfColorEntry &) const = default;
};

struct RtfFontEntry {
    std::string family;
    int fcharset = 0;
    bool operator==(const RtfFontEntry &) const = default;
};

inline const RtfColorEntry* ResolveColorEntry(int idx, const std::vector<RtfColorEntry>& colors) {
    if (idx >= 0 && idx < static_cast<int>(colors.size()))
        return &colors[static_cast<std::size_t>(idx)];
    return nullptr;
}

inline TableCellBorders NormalizeCellBorders(const TableCellBorders& cellBorders,
                                              const TableCellBorders& rowBorders) {
    TableCellBorders normalized = cellBorders;
    auto FillFromRow = [](int& cellVal, int& cellColor, BorderStyle& cellStyle,
                                          int rowVal, int rowColor, BorderStyle rowStyle) {
        if (cellVal <= 0 && cellStyle == BorderStyle::None) {
            cellVal = rowVal;
            cellColor = rowColor;
            cellStyle = rowStyle;
        }
    };
    FillFromRow(normalized.leftWidth, normalized.leftColor, normalized.leftStyle,
                rowBorders.leftWidth, rowBorders.leftColor, rowBorders.leftStyle);
    FillFromRow(normalized.topWidth, normalized.topColor, normalized.topStyle,
                rowBorders.topWidth, rowBorders.topColor, rowBorders.topStyle);
    FillFromRow(normalized.rightWidth, normalized.rightColor, normalized.rightStyle,
                rowBorders.rightWidth, rowBorders.rightColor, rowBorders.rightStyle);
    FillFromRow(normalized.bottomWidth, normalized.bottomColor, normalized.bottomStyle,
                rowBorders.bottomWidth, rowBorders.bottomColor, rowBorders.bottomStyle);
    return normalized;
}

inline int EffectiveCellPadding(int cellPad, int rowPad) {
    return (cellPad > 0 || rowPad > 0) ? std::max(cellPad, rowPad) : 0;
}

struct BorderValues { int width; BorderStyle style; int color; };

// Unit conversion constants
constexpr double kTwipsToHalfPt = 20.0;
constexpr double kHalfPtToPoint = 2.0;
constexpr double kExpndToEm = 20.0;
constexpr double kDpiToPoints = 96.0 / 72.0;
constexpr int kDefaultTabStopTwips = 180;

struct TabStop {
    int position;
    Qt::Alignment alignment = Qt::AlignLeft;

    bool operator==(const TabStop& other) const = default;
};

struct ParagraphFormat {
    Qt::Alignment alignment = Qt::AlignLeft;
    int leftIndent = 0;
    int firstLineIndent = 0;
    int rightIndent = 0;
    int spaceBefore = 0;
    int spaceAfter = 0;
    int lineHeight = 0;
    int slMult = 1;
    std::vector<TabStop> tabStops;

    bool operator==(const ParagraphFormat& other) const = default;
};

struct RtfRunFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeOut = false;
    bool protected_ = false;
    int fontIndex = 0;
    int fontSize = 24;
    int colorIndex = -1;
    int bgColorIndex = -1;
    bool superscript = false;
    bool subscript = false;
    bool kerning = false;
    int expnd = 0;
    RtfUnderlineStyle underlineStyle = RtfUnderlineStyle::NoUnderline;
    QFont::Capitalization capitalization = QFont::MixedCase;
    int upOffset = 0;
    int dnOffset = 0;
    int ulColorIndex = 0;
    int highlightIndex = 0;
    int langId = 0;
    bool isAnchor = false;
    std::string anchorHref;
    bool inPntext = false;

    bool operator==(const RtfRunFormat& other) const = default;
};

struct RtfRun {
    std::string text;
    RtfRunFormat format;

    RtfRun() = default;
    RtfRun(std::string t, RtfRunFormat f) : text(std::move(t)), format(std::move(f)) {}

    bool operator==(const RtfRun &) const = default;
};

struct RtfParagraph {
    ParagraphFormat format;
    std::vector<RtfRun> runs;
    int listId = 0;
    int listLevel = 0;
    RtfListStyle listStyle = RtfListStyle::None;
    int listIndent = 0;
    int defaultFontIndex = 0;
    int defaultTabStopTwips = 180;
    std::string pntextRtf;

    bool operator==(const RtfParagraph& other) const = default;
};

struct RtfTableRowEntry {
    std::vector<int> cellxPositions;
    std::vector<std::pair<std::vector<RtfRun>, TableCellFormat>> cells;
    TableCellBorders rowBorders;
    std::array<int, 4> rowPadding = {{0, 0, 0, 0}};
    Qt::Alignment tableAlignment = Qt::AlignLeft;
    int tableLeftPosition = 0;
    int tableWidth = 0;

    bool operator==(const RtfTableRowEntry& other) const = default;
};

struct RtfDocument {
    int defaultFontIndex = 0;
    int defaultFontSize = 0;
    int defaultLangId = 0;
    int viewKind = 0;
    int ucByteCount = 1;
    int codePage = 1252;
    int defaultTabStopTwips = 180;
    std::vector<RtfColorEntry> colors;
    std::vector<RtfFontEntry> fonts;
    std::vector<std::variant<RtfParagraph, RtfTableRowEntry, RtfImage>> elements;
    std::vector<std::string> unknownTags;
};

} // namespace Rte

#include "RtfTypes.h"

#include <cmath>
#include <stdexcept>

namespace Rte {

RtfListStyle RtfLevelNfcToListStyle(int nfc) {
    switch (static_cast<RtfLevelNfc>(nfc)) {
        case RtfLevelNfc::Arabic:            return RtfListStyle::Number;
        case RtfLevelNfc::UpperRoman:        return RtfListStyle::Roman;
        case RtfLevelNfc::LowerRoman:        return RtfListStyle::Roman;
        case RtfLevelNfc::UpperAlpha:        return RtfListStyle::Letter;
        case RtfLevelNfc::LowerAlpha:        return RtfListStyle::Letter;
        case RtfLevelNfc::Ordinal:           return RtfListStyle::Number;
        case RtfLevelNfc::CardinalText:      return RtfListStyle::Number;
        case RtfLevelNfc::OrdinalText:       return RtfListStyle::Number;
        case RtfLevelNfc::ArabicLeadingZero: return RtfListStyle::Number;
        case RtfLevelNfc::Bullet:            return RtfListStyle::Disc;
        case RtfLevelNfc::NoNumber:          return RtfListStyle::None;
    }
    return RtfListStyle::Number;
}

RtfLevelNfc ListStyleToLevelNfc(RtfListStyle style) {
    switch (style) {
        case RtfListStyle::Number: return RtfLevelNfc::Arabic;
        case RtfListStyle::Roman:  return RtfLevelNfc::LowerRoman;
        case RtfListStyle::Letter: return RtfLevelNfc::LowerAlpha;
        case RtfListStyle::Disc:     [[fallthrough]];
        case RtfListStyle::Circle:   [[fallthrough]];
        case RtfListStyle::Square:   [[fallthrough]];
        case RtfListStyle::Box:      [[fallthrough]];
        case RtfListStyle::Check:    [[fallthrough]];
        case RtfListStyle::None:     [[fallthrough]];
        default:                     return RtfLevelNfc::Bullet;
    }
}

const char* ImageFormatExtension(RtfImageFormat fmt) {
    switch (fmt) {
        case RtfImageFormat::Jpeg: return "jpg";
        case RtfImageFormat::Png:  return "png";
        case RtfImageFormat::Bmp:  return "bmp";
        case RtfImageFormat::Unknown:
        default:
            throw std::runtime_error("unknown image format");
    }
}

RtfImageFormat ImageFormatFromString(const std::string& s) {
    if (s == "jpg") return RtfImageFormat::Jpeg;
    if (s == "png") return RtfImageFormat::Png;
    if (s == "bmp") return RtfImageFormat::Bmp;
    return RtfImageFormat::Unknown;
}

const RtfColorEntry* ResolveColorEntry(int idx, const std::vector<RtfColorEntry>& colors) {
    if (idx >= 0 && idx < static_cast<int>(colors.size()))
        return &colors[static_cast<std::size_t>(idx)];
    return nullptr;
}

TableCellBorders NormalizeCellBorders(const TableCellBorders& cellBorders,
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

int EffectiveCellPadding(int cellPad, int rowPad) {
    return (cellPad > 0 || rowPad > 0) ? std::max(cellPad, rowPad) : 0;
}

BorderValues GetBorderValues(const TableCellBorders& b, TableSide side) {
    const auto& m = kBorderMembers[side];
    return { b.*(m.width), b.*(m.style), b.*(m.color) };
}

double TwipsToHalfPt(double twips) { return twips / 20.0; }
double MarginTwipsToPoints(double twips) { return twips / 2.0; }
int PointsToTwips(double pts) { return lround(pts * 20.0); }
int PointsToHalfPtTwips(double pts) { return lround(pts * 2.0); }

} // namespace Rte

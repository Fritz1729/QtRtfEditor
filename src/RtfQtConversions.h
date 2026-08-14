#pragma once

#include <QByteArray>
#include <QFont>
#include <QTextCharFormat>
#include <QTextListFormat>

#include "RtfTypes.h"

namespace Rte {

// User property IDs for QTextCharFormat and QTextDocument storage
constexpr int UserPropProtect = 1000;
constexpr int UserPropUpOffset = 1004;
constexpr int UserPropDnOffset = 1005;
constexpr int UserPropLangId = 1006;
constexpr int UserPropParaDefaultFontIndex = 1007;
constexpr int UserPropParaDefaultTabStopTwips = 1008;
constexpr int UserPropHighlightIndex = 1009;
constexpr const char* UserPropMetaDefaultLangId = "rtf_meta_defaultLangId";
constexpr const char* UserPropMetaViewKind = "rtf_meta_viewKind";
constexpr const char* UserPropMetaUcByteCount = "rtf_meta_ucByteCount";
constexpr const char* UserPropMetaDefaultTabStopTwips = "rtf_meta_defaultTabStopTwips";
constexpr int UserPropBlockPntextRtf = 1010;
constexpr int UserPropBlockPntextFontFamily = 1014;
constexpr int UserPropUlStyle = 1011;
constexpr int UserPropUlColorIndex = 1012;
constexpr int UserPropSlMult = 1013;

inline UnderlineStyle toUnderlineStyle(QTextCharFormat::UnderlineStyle qtStyle) {
    switch (qtStyle) {
        case QTextCharFormat::NoUnderline:     return UnderlineStyle::None;
        case QTextCharFormat::SingleUnderline: return UnderlineStyle::Solid;
        case QTextCharFormat::DotLine:         return UnderlineStyle::Dotted;
        case QTextCharFormat::DashUnderline:   return UnderlineStyle::Dashed;
        case QTextCharFormat::DashDotLine:     return UnderlineStyle::DashDot;
        case QTextCharFormat::DashDotDotLine:  return UnderlineStyle::DashDotDot;
        case QTextCharFormat::WaveUnderline:   return UnderlineStyle::Thick;
        case QTextCharFormat::SpellCheckUnderline:
        default:                               return UnderlineStyle::None;
    }
}

inline QTextCharFormat::UnderlineStyle qtUnderlineStyleFor(UnderlineStyle style) {
    switch (style) {
        case UnderlineStyle::None:          return QTextCharFormat::NoUnderline;
        case UnderlineStyle::Solid:         return QTextCharFormat::SingleUnderline;
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
        case QFont::MixedCase:
        case QFont::AllLowercase:
        case QFont::Capitalize:
        default:                    return Capitalization::None;
        case QFont::AllUppercase:   return Capitalization::AllCaps;
        case QFont::SmallCaps:      return Capitalization::SmallCaps;
    }
}

inline QTextListFormat::Style RtfListStyleToQt(ListStyle style) {
    switch (style) {
        case ListStyle::None:
        default:                return QTextListFormat::ListDisc;
        case ListStyle::Disc:   return QTextListFormat::ListDisc;
        case ListStyle::Bullet: return QTextListFormat::ListCircle;
        case ListStyle::Box:    return QTextListFormat::ListSquare;
        case ListStyle::Check:  return QTextListFormat::ListDisc;
        case ListStyle::Number: return QTextListFormat::ListDecimal;
        case ListStyle::Letter: return QTextListFormat::ListLowerAlpha;
        case ListStyle::Roman:  return QTextListFormat::ListLowerRoman;
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
        case QTextListFormat::ListUpperAlpha:
        case QTextListFormat::ListUpperRoman:
        case QTextListFormat::ListStyleUndefined:
        default:                                   return ListStyle::None;
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

// QByteArray <-> std::vector<uint8_t> conversion helpers (for RtfImage::data)
inline std::vector<uint8_t> ByteArrayToVector(const QByteArray& ba) {
    return {ba.begin(), ba.end()};
}

inline QByteArray VectorToByteArray(const std::vector<uint8_t>& v) {
    return QByteArray(reinterpret_cast<const char*>(v.data()), static_cast<int>(v.size()));
}

} // namespace Rte

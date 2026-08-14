#pragma once

#include <stdexcept>

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

// QByteArray <-> std::vector<uint8_t> conversion helpers (for RtfImage::data)
inline std::vector<uint8_t> ByteArrayToVector(const QByteArray& ba) {
    return {ba.begin(), ba.end()};
}

inline QByteArray VectorToByteArray(const std::vector<uint8_t>& v) {
    return QByteArray(reinterpret_cast<const char*>(v.data()), static_cast<int>(v.size()));
}

// RtfUnderlineStyle <-> QTextCharFormat::UnderlineStyle
inline QTextCharFormat::UnderlineStyle QtUlStyleFor(RtfUnderlineStyle v) {
    if (v > RtfUnderlineStyle::SpellCheck) return QTextCharFormat::SingleUnderline;
    return static_cast<QTextCharFormat::UnderlineStyle>(v);
}

inline RtfUnderlineStyle RtfUlStyleFor(QTextCharFormat::UnderlineStyle v) {
    if (v >= static_cast<QTextCharFormat::UnderlineStyle>(RtfUnderlineStyle::COUNT)) throw std::runtime_error("unknown QTextCharFormat::UnderlineStyle");
    return static_cast<RtfUnderlineStyle>(v);
}

// RtfListStyle <-> QTextListFormat::Style
inline QTextListFormat::Style QtListStyleFor(RtfListStyle v) {
    switch (v) {
        case RtfListStyle::Disc:   return QTextListFormat::ListDisc;
        case RtfListStyle::Circle: return QTextListFormat::ListCircle;
        case RtfListStyle::Square: return QTextListFormat::ListSquare;
        case RtfListStyle::Box:    return QTextListFormat::ListDisc;
        case RtfListStyle::Check:  return QTextListFormat::ListDisc;
        case RtfListStyle::Number: return QTextListFormat::ListDecimal;
        case RtfListStyle::Letter: return QTextListFormat::ListLowerAlpha;
        case RtfListStyle::Roman:  return QTextListFormat::ListLowerRoman;
        case RtfListStyle::None:
        default:                   return QTextListFormat::ListStyleUndefined;
    }
}

inline RtfListStyle RtfListStyleFor(QTextListFormat::Style v) {
    switch (v) {
        case QTextListFormat::ListDisc:           return RtfListStyle::Disc;
        case QTextListFormat::ListCircle:         return RtfListStyle::Circle;
        case QTextListFormat::ListSquare:         return RtfListStyle::Square;
        case QTextListFormat::ListDecimal:        return RtfListStyle::Number;
        case QTextListFormat::ListLowerRoman:     return RtfListStyle::Roman;
        case QTextListFormat::ListUpperRoman:     return RtfListStyle::Roman;
        case QTextListFormat::ListLowerAlpha:     return RtfListStyle::Letter;
        case QTextListFormat::ListUpperAlpha:     return RtfListStyle::Letter;
        case QTextListFormat::ListStyleUndefined:
        default:                                  return RtfListStyle::None;
    }
}

} // namespace Rte

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Rte {

/**
 * @brief RTF control word descriptor and dispatch action.
 *
 * Each entry maps a lowercase keyword to an action type and, where
 * applicable, a property or parameter index.
 */
struct RtfControl {
    const char* keyword;

    enum class Action : uint8_t {
        ToggleCharProp,
        SetCharProp,
        SetParaProp,
        SetAlignment,
        SetUlStyle,
        SetCapitalization,
        EmitParagraph,
        HeaderControl,
        TableControl,
        SetTabAlign,
        Unknown,
    };

    enum class CharProp : uint8_t {
        Bold, Italic, Underline, Subscript, Superscript, Strike,
    };
    enum class Align : uint8_t { Left, Center, Right };
    enum class ParaProp : uint8_t {
        LeftIndent, FirstLineIndent, RightIndent, SpaceBefore, SpaceAfter,
        LineHeight, SlMult, TabStop,
    };
    enum class CharSetProp : uint8_t {
        FontIndex, FontSize, ColorIndex, BgColorIndex, UpOffset,
        DnOffset,
    };
    enum class RtfUlStyle : uint8_t {
        UlNone, UlSolid, UlDotted, UlDashed, UlDouble, UlThick,
    };
    enum class RtfCaps : uint8_t { CapsNone, CapsAll, CapsSmall };
    enum class TabAlign : uint8_t { Left, Center, Right, Decimal };

    Action action;

    union Value {
        int raw;
        CharProp charProp;
        CharSetProp charSetProp;
        ParaProp paraProp;
        Align align;
        RtfUlStyle ulStyle;
        RtfCaps caps;
        TabAlign tabAlign;
    };

    Value value;
};

constexpr std::size_t kRtfControlTableSize = 60;

extern const std::array<RtfControl, kRtfControlTableSize> rtfControlTable;

} // namespace Rte

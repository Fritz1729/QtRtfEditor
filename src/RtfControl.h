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
        Unknown,
    };

    enum class CharProp : uint8_t {
        Bold, Italic, Underline, Subscript, Superscript, Strike,
    };
    enum class Align : uint8_t { Left, Center, Right };
    enum class ParaProp : uint8_t {
        LeftIndent, FirstLineIndent, RightIndent, SpaceBefore, SpaceAfter,
        LineHeight, SlMult,
    };
    enum class CharSetProp : uint8_t {
        FontIndex, FontSize, ColorIndex, BgColorIndex, Highlight, UpOffset,
        DnOffset,
    };
    enum class RtfUlStyle : uint8_t {
        UlNone, UlSolid, UlDotted, UlDashed, UlDouble, UlThick,
    };
    enum class RtfCaps : uint8_t { CapsNone, CapsAll, CapsSmall };

    Action action;
    int value;
};

constexpr std::size_t kRtfControlTableSize = 57;

extern const std::array<RtfControl, kRtfControlTableSize> rtfControlTable;

} // namespace Rte

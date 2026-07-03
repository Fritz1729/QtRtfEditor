#pragma once

#include <array>
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
        EmitParagraph,
        HeaderControl,
        TableControl,
        Unknown,
    };

    enum class CharProp : uint8_t { Bold, Italic, Underline, Subscript, Superscript };
    enum class Align : uint8_t { Left, Center, Right };
    enum class ParaProp : uint8_t { LeftIndent, FirstLineIndent };
    enum class CharSetProp : uint8_t { FontIndex, FontSize, ColorIndex };

    Action action;
    int value;
};

extern const std::array<RtfControl, 38> rtfControlTable;

} // namespace Rte

#pragma once

#include <array>
#include <cstdint>

#include "RteExport.h"

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
        HeaderMetadata,
        TableControl,
        SetTabAlign,
        TableControlWord,
        GroupPersistent,
        FieldControl,
        SpecialChar,
        Unknown,
    };

    enum class CharProp : uint8_t {
        Bold, Italic, Underline, Subscript, Superscript, Strike, Kerning,
        Protect,
    };
    enum class ParaProp : uint8_t {
        LeftIndent, FirstLineIndent, RightIndent, SpaceBefore, SpaceAfter,
        LineHeight, SlMult, TabStop, ListLevel,
    };
    enum class CharSetProp : uint8_t {
        FontIndex, FontSize, ColorIndex, BgColorIndex, UpOffset,
        DnOffset, Expnd, ListId, UlColorIndex, HighlightIndex, LangId,
    };
    enum class TableCtrlWord : uint8_t {
        Trowd, Cellx, Cell, Row, Intbl,
        ClShading, ClVertAlignTop, ClVertAlignCenter, ClVertAlignBottom,
        ClBorderLeft, ClBorderTop, ClBorderRight, ClBorderBottom,
        BrdrSolid, BrdrWidth, BrdrColor, ClMerge,
        BrdrNone, BrdrDashed,
        ClPadLeft, ClPadTop, ClPadRight, ClPadBottom,
        TrPadLeft, TrPadTop, TrPadRight, TrPadBottom,
        TrAlignLeft, TrAlignCenter, TrAlignRight,
        TrLeft, TrWidth,
        TrBorderLeft, TrBorderTop, TrBorderRight, TrBorderBottom,
    };

    Action action;

    union Value {
        int raw;
        CharProp charProp;
        CharSetProp charSetProp;
        ParaProp paraProp;
        TableCtrlWord tableCtrlWord;
        uint32_t specialChar;
    };

    Value value;
};

/**
 * @brief Look up a control by keyword. O(1) via static unordered_map.
 * @return Pointer to the control entry, or nullptr if not found.
 */
RTE_EXPORT const RtfControl* FindControl(const char* keyword);

} // namespace Rte

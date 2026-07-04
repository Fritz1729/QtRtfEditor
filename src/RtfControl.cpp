#include "RtfControl.h"

namespace Rte {

using Action = RtfControl::Action;
using CharProp = RtfControl::CharProp;
using CharSetProp = RtfControl::CharSetProp;
using ParaProp = RtfControl::ParaProp;
using Align = RtfControl::Align;
using TabAlign = RtfControl::TabAlign;
using UlStyle = RtfControl::RtfUlStyle;
using Caps = RtfControl::RtfCaps;

#define DATA(keyword, action, prop) \
    { keyword, action, { .raw = static_cast<int>(prop) } }
#define DATA_TAB(keyword, action, align) \
    { keyword, action, { .tabAlign = align } }

constexpr RtfControl rtfControlTableEntries[] = {
    // Character toggles
    DATA("b",        Action::ToggleCharProp,   CharProp::Bold),
    DATA("b0",       Action::ToggleCharProp,   CharProp::Bold),
    DATA("i",        Action::ToggleCharProp,   CharProp::Italic),
    DATA("i0",       Action::ToggleCharProp,   CharProp::Italic),
    DATA("ul",       Action::ToggleCharProp,   CharProp::Underline),
    DATA("ul0",      Action::ToggleCharProp,   CharProp::Underline),
    DATA("sub",      Action::ToggleCharProp,   CharProp::Subscript),
    DATA("sub0",     Action::ToggleCharProp,   CharProp::Subscript),
    DATA("super",    Action::ToggleCharProp,   CharProp::Superscript),
    DATA("super0",   Action::ToggleCharProp,   CharProp::Superscript),
    DATA("strike",   Action::ToggleCharProp,   CharProp::Strike),
    DATA("strike0",  Action::ToggleCharProp,   CharProp::Strike),
    DATA("kerning",    Action::ToggleCharProp,   CharProp::Kerning),
    DATA("kerning0",   Action::ToggleCharProp,   CharProp::Kerning),

    // Character properties with numeric argument
    DATA("f",        Action::SetCharProp,      CharSetProp::FontIndex),
    DATA("fs",       Action::SetCharProp,      CharSetProp::FontSize),
    DATA("cf",       Action::SetCharProp,      CharSetProp::ColorIndex),
    DATA("cb",       Action::SetCharProp,      CharSetProp::BgColorIndex),
    DATA("up",       Action::SetCharProp,      CharSetProp::UpOffset),
    DATA("dn",       Action::SetCharProp,      CharSetProp::DnOffset),
    DATA("expnd",      Action::SetCharProp,      CharSetProp::Expnd),
    DATA("expndtw",    Action::SetCharProp,      CharSetProp::Expnd),

    // Paragraph properties
    DATA("li",       Action::SetParaProp,      ParaProp::LeftIndent),
    DATA("fi",       Action::SetParaProp,      ParaProp::FirstLineIndent),
    DATA("ri",       Action::SetParaProp,      ParaProp::RightIndent),
    DATA("sb",       Action::SetParaProp,      ParaProp::SpaceBefore),
    DATA("sa",       Action::SetParaProp,      ParaProp::SpaceAfter),
    DATA("sl",       Action::SetParaProp,      ParaProp::LineHeight),
    DATA("slmult",   Action::SetParaProp,      ParaProp::SlMult),

    // Alignment
    DATA("ql",       Action::SetAlignment,     Align::Left),
    DATA("qj",       Action::SetAlignment,     Align::Left),
    DATA("qc",       Action::SetAlignment,     Align::Center),
    DATA("qr",       Action::SetAlignment,     Align::Right),

    // Tab stop alignment
    DATA_TAB("tqc",   Action::SetTabAlign,      TabAlign::Center),
    DATA_TAB("tqd",   Action::SetTabAlign,      TabAlign::Decimal),
    DATA_TAB("tqr",   Action::SetTabAlign,      TabAlign::Right),

    // Tab stop position
    DATA("tx",       Action::SetParaProp,      ParaProp::TabStop),

    // Underline styles
    DATA("uld",        Action::SetUlStyle, UlStyle::UlDotted),
    DATA("uldash",     Action::SetUlStyle, UlStyle::UlDashed),
    DATA("uldashd",    Action::SetUlStyle, UlStyle::UlDashDot),
    DATA("uldashdd",   Action::SetUlStyle, UlStyle::UlDashDotDot),
    DATA("uldb",       Action::SetUlStyle, UlStyle::UlDouble),
    DATA("ulth",       Action::SetUlStyle, UlStyle::UlThick),

    // Capitalization
    DATA("caps",     Action::SetCapitalization, Caps::CapsAll),
    DATA("caps0",    Action::SetCapitalization, Caps::CapsNone),
    DATA("scaps",    Action::SetCapitalization, Caps::CapsSmall),
    DATA("scaps0",   Action::SetCapitalization, Caps::CapsNone),

    // Paragraph break
    DATA("par",      Action::EmitParagraph,    0),

    // Header controls
    DATA("rtf",      Action::HeaderControl,    0),
    DATA("ansi",     Action::HeaderControl,    0),
    DATA("deff0",    Action::HeaderControl,    0),
    DATA("mac",      Action::HeaderControl,    0),
    DATA("pca",      Action::HeaderControl,    0),
    DATA("ansicpg",  Action::HeaderControl,    0),
    DATA("ucci",     Action::HeaderControl,    0),
    DATA("deff",     Action::HeaderControl,    0),

    // Table controls
    DATA("colortbl", Action::TableControl,     0),
    DATA("fonttbl",  Action::TableControl,     0),
    DATA("red",      Action::TableControl,     0),
    DATA("green",    Action::TableControl,     0),
    DATA("blue",     Action::TableControl,     0),
    DATA("froman",   Action::TableControl,     0),
    DATA("fswiss",   Action::TableControl,     0),
    DATA("fmodern",  Action::TableControl,     0),
    DATA("fnil",     Action::TableControl,     0),
    DATA("fcharset", Action::TableControl,     0),

    // Image controls (\pict)
    DATA("jpegblip", Action::TableControl,     0),
    DATA("pngblip",  Action::TableControl,     0),
    DATA("dibitmap", Action::TableControl,     0),
    DATA("picw",     Action::TableControl,     0),
    DATA("pich",     Action::TableControl,     0),
    DATA("picscalex",Action::TableControl,     0),
    DATA("picscaley",Action::TableControl,     0),
    DATA("picwgoal", Action::TableControl,     0),
    DATA("pichgoal", Action::TableControl,     0),
    DATA("piccropl", Action::TableControl,     0),
    DATA("piccropr", Action::TableControl,     0),
    DATA("piccropt", Action::TableControl,     0),
    DATA("piccropb", Action::TableControl,     0),
    DATA("pict",     Action::TableControl,     0),
};

static_assert(std::size(rtfControlTableEntries) == kRtfControlTableSize,
    "rtfControlTable entry count does not match kRtfControlTableSize in RtfControl.h");

const std::array<RtfControl, std::size(rtfControlTableEntries)> rtfControlTable =
    std::to_array(rtfControlTableEntries);

} // namespace Rte

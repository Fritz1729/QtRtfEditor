#include "RtfControl.h"

namespace Rte {

using Action = RtfControl::Action;
using CharProp = RtfControl::CharProp;
using CharSetProp = RtfControl::CharSetProp;
using ParaProp = RtfControl::ParaProp;
using Align = RtfControl::Align;

#define DATA(keyword, action, prop) \
    { keyword, action, static_cast<int>(prop) }

const std::array<RtfControl, 38> rtfControlTable = {{
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

    DATA("f",        Action::SetCharProp,      CharSetProp::FontIndex),
    DATA("fs",       Action::SetCharProp,      CharSetProp::FontSize),
    DATA("cf",       Action::SetCharProp,      CharSetProp::ColorIndex),

    DATA("li",       Action::SetParaProp,      ParaProp::LeftIndent),
    DATA("fi",       Action::SetParaProp,      ParaProp::FirstLineIndent),

    DATA("ql",       Action::SetAlignment,     Align::Left),
    DATA("qj",       Action::SetAlignment,     Align::Left),
    DATA("qc",       Action::SetAlignment,     Align::Center),
    DATA("qr",       Action::SetAlignment,     Align::Right),

    DATA("par",      Action::EmitParagraph,    0),

    DATA("rtf",      Action::HeaderControl,    0),
    DATA("ansi",     Action::HeaderControl,    0),
    DATA("deff0",    Action::HeaderControl,    0),
    DATA("mac",      Action::HeaderControl,    0),
    DATA("pca",      Action::HeaderControl,    0),
    DATA("ansicpg",  Action::HeaderControl,    0),
    DATA("ucci",     Action::HeaderControl,    0),
    DATA("deff",     Action::HeaderControl,    0),

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
}};

} // namespace Rte

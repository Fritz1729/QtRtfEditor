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
using TableCtrlWord = RtfControl::TableCtrlWord;

#define DATA(keyword, action, prop) \
    { keyword, action, { .raw = static_cast<int>(prop) } }
#define DATA_TAB(keyword, action, align) \
    { keyword, action, { .tabAlign = align } }
#define DATA_TABLE(keyword, ctrlWord) \
    { keyword, Action::TableControlWord, { .tableCtrlWord = ctrlWord } }

constexpr RtfControl rtfControlTableEntries[] = {
    // Character toggles
    DATA("b",        Action::ToggleCharProp,   CharProp::Bold),
    DATA("b0",       Action::ToggleCharProp,   CharProp::Bold),
    DATA("i",        Action::ToggleCharProp,   CharProp::Italic),
    DATA("i0",       Action::ToggleCharProp,   CharProp::Italic),
    DATA("ul",       Action::SetUlStyle,       UlStyle::UlSolid),
    DATA("ul0",      Action::SetUlStyle,       UlStyle::UlNone),
    DATA("ulnone",   Action::SetUlStyle,       UlStyle::UlNone),
    DATA("sub",      Action::ToggleCharProp,   CharProp::Subscript),
    DATA("sub0",     Action::ToggleCharProp,   CharProp::Subscript),
    DATA("super",    Action::ToggleCharProp,   CharProp::Superscript),
    DATA("super0",   Action::ToggleCharProp,   CharProp::Superscript),
    DATA("strike",   Action::ToggleCharProp,   CharProp::Strike),
    DATA("strike0",  Action::ToggleCharProp,   CharProp::Strike),
    DATA("kerning",    Action::ToggleCharProp,   CharProp::Kerning),
    DATA("kerning0",   Action::ToggleCharProp,   CharProp::Kerning),
    DATA("protect",    Action::ToggleCharProp,   CharProp::Protect),
    DATA("protect0",   Action::ToggleCharProp,   CharProp::Protect),

    // Character properties with numeric argument
    DATA("f",        Action::SetCharProp,      CharSetProp::FontIndex),
    DATA("fs",       Action::SetCharProp,      CharSetProp::FontSize),
    DATA("cf",       Action::SetCharProp,      CharSetProp::ColorIndex),
    DATA("cb",       Action::SetCharProp,      CharSetProp::BgColorIndex),
    DATA("up",       Action::SetCharProp,      CharSetProp::UpOffset),
    DATA("dn",       Action::SetCharProp,      CharSetProp::DnOffset),
    DATA("expnd",      Action::SetCharProp,      CharSetProp::Expnd),
    DATA("expndtw",    Action::SetCharProp,      CharSetProp::Expnd),
    DATA("ulc",        Action::SetCharProp,      CharSetProp::UlColorIndex),
    DATA("ulc0",       Action::SetCharProp,      CharSetProp::UlColorIndex),
    DATA("lang",       Action::SetCharProp,      CharSetProp::LangId),
    DATA("chlang",     Action::SetCharProp,      CharSetProp::LangId),
    DATA("langfe",     Action::SetCharProp,      CharSetProp::LangId),

    // Paragraph properties
    DATA("li",       Action::SetParaProp,      ParaProp::LeftIndent),
    DATA("fi",       Action::SetParaProp,      ParaProp::FirstLineIndent),
    DATA("ri",       Action::SetParaProp,      ParaProp::RightIndent),
    DATA("sb",       Action::SetParaProp,      ParaProp::SpaceBefore),
    DATA("sa",       Action::SetParaProp,      ParaProp::SpaceAfter),
    DATA("sl",       Action::SetParaProp,      ParaProp::LineHeight),
    DATA("slmult",   Action::SetParaProp,      ParaProp::SlMult),
    // \deftabN: group-persistent per RTF 1.5/1.9.1 spec (pushed/popped with groups).
    DATA("deftab",   Action::GroupPersistent,  0),

    // Alignment
    DATA("ql",       Action::SetAlignment,     Align::Left),
    DATA("qj",       Action::SetAlignment,     Align::Justified),
    DATA("qc",       Action::SetAlignment,     Align::Center),
    DATA("qr",       Action::SetAlignment,     Align::Right),

    // Tab stop alignment
    DATA_TAB("tqc",   Action::SetTabAlign,      TabAlign::Center),
    DATA_TAB("tqd",   Action::SetTabAlign,      TabAlign::Decimal),
    DATA_TAB("tqr",   Action::SetTabAlign,      TabAlign::Right),
    DATA_TAB("tabcenter", Action::SetTabAlign,  TabAlign::Center),
    DATA_TAB("tabright",  Action::SetTabAlign,  TabAlign::Right),
    DATA("tabgrid",   Action::TableControl,      0),
    DATA("tableft",   Action::TableControl,      0),

    // Tab stop position
    DATA("tx",       Action::SetParaProp,      ParaProp::TabStop),

    // List controls
    DATA("listid",   Action::SetCharProp,      CharSetProp::ListId),
    DATA("listlevel",Action::SetParaProp,      ParaProp::ListLevel),
    DATA("list",     Action::HeaderControl,    0),
    DATA("listtext", Action::HeaderControl,    0),
    DATA("listoverride", Action::HeaderControl, 0),
    DATA("listtable", Action::HeaderControl,   0),
    DATA("liststylebulletsimple", Action::HeaderControl, 0),
    DATA("liststylenum", Action::HeaderControl, 0),
    DATA("liststyletype", Action::HeaderControl, 0),
    DATA("liststyle", Action::HeaderControl,   0),
    DATA("listname", Action::HeaderControl,    0),

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
    DATA("mac",      Action::HeaderControl,    0),
    DATA("pca",      Action::HeaderControl,    0),
    DATA("ucci",     Action::HeaderControl,    0),
    // \deffN: group-persistent per RTF 1.5/1.9.1 spec (pushed/popped with groups).
    DATA("deff",     Action::GroupPersistent,  0),
    DATA("deff0",    Action::GroupPersistent,  0),
    DATA("qi",       Action::SetAlignment,     Align::Justified),

    // Header metadata (roundtrip preservation)
    DATA("pard",     Action::HeaderMetadata,   0),
    DATA("plain",    Action::HeaderMetadata,   0),
    DATA("uc",       Action::HeaderMetadata,   0),
    DATA("deflang",  Action::HeaderMetadata,   0),
    DATA("viewkind", Action::HeaderMetadata,   0),
    DATA("ansicpg",  Action::HeaderMetadata,   0),

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
    // \fprqN (font pitch and quality) — recognized but not preserved.
    // Qt has no API to set font pitch; QFont::setFixedPitch() is a
    // matching hint, not a pitch property. Preserving \fprq would
    // be impossible since QFont::fixedPitch() reflects the underlying
    // font file, not the value we set.
    DATA("fprq",     Action::TableControl,     0),

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

    // Table structure
    DATA_TABLE("trowd",     TableCtrlWord::Trowd),
    DATA_TABLE("cellx",     TableCtrlWord::Cellx),
    DATA_TABLE("cell",      TableCtrlWord::Cell),
    DATA_TABLE("row",       TableCtrlWord::Row),
    DATA_TABLE("intbl",     TableCtrlWord::Intbl),
    DATA_TABLE("clshdn",    TableCtrlWord::ClShading),
    DATA_TABLE("clvertalt", TableCtrlWord::ClVertAlignTop),
    DATA_TABLE("clvertalc", TableCtrlWord::ClVertAlignCenter),
    DATA_TABLE("clvertalb", TableCtrlWord::ClVertAlignBottom),
    DATA_TABLE("clbrdrl",   TableCtrlWord::ClBorderLeft),
    DATA_TABLE("clbrdrt",   TableCtrlWord::ClBorderTop),
    DATA_TABLE("clbrdrr",   TableCtrlWord::ClBorderRight),
    DATA_TABLE("clbrdrb",   TableCtrlWord::ClBorderBottom),
    DATA_TABLE("brdrs",     TableCtrlWord::BrdrSolid),
    DATA_TABLE("brdrw",     TableCtrlWord::BrdrWidth),
    DATA_TABLE("brdrcf",    TableCtrlWord::BrdrColor),
    DATA_TABLE("clmrg",     TableCtrlWord::ClMerge),
    DATA_TABLE("brdrn",     TableCtrlWord::BrdrNone),
    DATA_TABLE("brdrd",     TableCtrlWord::BrdrDashed),
    DATA_TABLE("clpadl",    TableCtrlWord::ClPadLeft),
    DATA_TABLE("clpadr",    TableCtrlWord::ClPadRight),
    DATA_TABLE("clpadt",    TableCtrlWord::ClPadTop),
    DATA_TABLE("clpadb",    TableCtrlWord::ClPadBottom),
    DATA_TABLE("trpaddl",   TableCtrlWord::TrPadLeft),
    DATA_TABLE("trpaddr",   TableCtrlWord::TrPadRight),
    DATA_TABLE("trpadt",    TableCtrlWord::TrPadTop),
    DATA_TABLE("trpadb",    TableCtrlWord::TrPadBottom),
    DATA_TABLE("trql",      TableCtrlWord::TrAlignLeft),
    DATA_TABLE("trqc",      TableCtrlWord::TrAlignCenter),
    DATA_TABLE("trqr",      TableCtrlWord::TrAlignRight),
    DATA_TABLE("trleft",    TableCtrlWord::TrLeft),
    DATA_TABLE("trw",       TableCtrlWord::TrWidth),
    DATA_TABLE("trbrdrl",   TableCtrlWord::TrBorderLeft),
    DATA_TABLE("trbrdrt",   TableCtrlWord::TrBorderTop),
    DATA_TABLE("trbrdrr",   TableCtrlWord::TrBorderRight),
    DATA_TABLE("trbrdrb",   TableCtrlWord::TrBorderBottom),
};

static_assert(std::size(rtfControlTableEntries) == kRtfControlTableSize,
    "rtfControlTable entry count does not match kRtfControlTableSize in RtfControl.h");

const std::array<RtfControl, std::size(rtfControlTableEntries)> rtfControlTable =
    std::to_array(rtfControlTableEntries);

} // namespace Rte

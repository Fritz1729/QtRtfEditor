#pragma once

#include <cstdint>
#include <string>

#include "RichTextEdit.h"

namespace Rte {

RTE_EXPORT int MapSymbolByte(int byte);

RTE_EXPORT int MapCp1252Byte(int byte);

RTE_EXPORT int MapHexByteToCodepoint(int byte, int fcharset, int codePage, const std::string& fontFamily);

} // namespace Rte

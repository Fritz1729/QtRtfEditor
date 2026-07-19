#pragma once

#include <cstdint>
#include <string>

namespace Rte {

int MapSymbolByte(int byte);

int MapCp1252Byte(int byte);

int MapHexByteToCodepoint(int byte, int fcharset, int codePage, const std::string& fontFamily);

} // namespace Rte

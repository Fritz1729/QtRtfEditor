#pragma once

#include <cstdint>

namespace Rte {

int MapSymbolByte(int byte);

int MapCp1252Byte(int byte);

int MapHexByteToCodepoint(int byte, int fcharset, int codePage);

} // namespace Rte

#pragma once

#include <string>

#include "RtfTypes.h"

#ifndef RTE_EXPORT
#  if defined(Q_OS_WIN)
#    ifdef RTE_BUILD_LIBRARY
#      define RTE_EXPORT __declspec(dllexport)
#    elif defined(RTE_STATIC)
#      define RTE_EXPORT
#    else
#      define RTE_EXPORT __declspec(dllimport)
#    endif
#  else
#    ifdef RTE_BUILD_LIBRARY
#      define RTE_EXPORT __attribute__((visibility("default")))
#    else
#      define RTE_EXPORT
#    endif
#  endif
#endif

namespace Rte {

/**
 * @brief Parse an RTF string into a structural representation.
 * @param rtf      RTF string (UTF-8).
 * @param codePage Default code page for ANSI hex escapes (default 1252).
 */
RTE_EXPORT RtfDocument ParseRtf(const std::string& rtf, int codePage = 1252);

} // namespace Rte

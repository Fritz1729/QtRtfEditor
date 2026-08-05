#pragma once

#ifndef RTE_EXPORT
#  if defined(_WIN32) || defined(__CYGWIN__)
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

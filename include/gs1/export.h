#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(GS1_BUILD_DLL)
#    define GS1_API __declspec(dllexport)
#  elif defined(GS1_USE_DLL)
#    define GS1_API __declspec(dllimport)
#  else
#    define GS1_API
#  endif
#else
#  define GS1_API __attribute__((visibility("default")))
#endif

#define GS1_NOEXCEPT noexcept

#ifndef main_h
#define main_h

#include <stdio.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#if defined(_MSC_VER)
#define trap() __debugbreak()
#elif defined(__clang__)
#define trap() __builtin_debugtrap()
#elif defined(__GNUC__)
#define trap() __builtin_trap()
#else
#define trap() ((void)0)
#endif

#if defined(_MSC_VER)
#define unreach() __assume(0)
#else
#define unreach() __builtin_unreachable()
#endif

#define assert(expr)                                                                               \
  do                                                                                               \
  {                                                                                                \
    if (!(expr))                                                                                   \
    {                                                                                              \
      fprintf(stderr, "Assertion failed: %s %s:%d\n", #expr, __FILE__, __LINE__);                  \
      trap();                                                                                      \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

#define UNREACHABLE(message)                                                                       \
  do                                                                                               \
  {                                                                                                \
    assert(!message);                                                                              \
    unreach();                                                                                     \
  } while (0)

#endif

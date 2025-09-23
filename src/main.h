#ifndef main_h
#define main_h

#include <stdio.h>

#if defined(_MSC_VER)
#define trap() __debugbreak()
#elif defined(__clang__)
#define trap() __builtin_debugtrap()
#elif defined(__GNUC__)
#define trap() __builtin_trap()
#else
#define trap() ((void)0)
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

#ifdef _WIN32
#include <malloc.h>

#define UNREACHABLE(message)                                                                       \
  do                                                                                               \
  {                                                                                                \
    assert(!message);                                                                              \
    __assume(0);                                                                                   \
  } while (0)
#else
#define UNREACHABLE(message)                                                                       \
  do                                                                                               \
  {                                                                                                \
    assert(!message);                                                                              \
    __builtin_unreachable();                                                                       \
  } while (0)
#endif

void error(int start_line, int start_column, int end_line, int end_column, const char* message);

#endif

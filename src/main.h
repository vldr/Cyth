#ifndef main_h
#define main_h

#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#if defined(_MSC_VER)
#define TRAP() __debugbreak()
#elif defined(__clang__)
#define TRAP() __builtin_debugtrap()
#elif defined(__GNUC__)
#define TRAP() __builtin_trap()
#else
#define TRAP() ((void)0)
#endif

#if defined(_MSC_VER)
#define UNREACH() __assume(0)
#else
#define UNREACH() __builtin_unreachable()
#endif

#define EXPECT(expr)                                                                               \
  do                                                                                               \
  {                                                                                                \
    if (!(expr))                                                                                   \
    {                                                                                              \
      fprintf(stderr, "Assertion failed: %s %s:%d\n", #expr, __FILE__, __LINE__);                  \
      TRAP();                                                                                      \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

#define UNREACHABLE(message)                                                                       \
  do                                                                                               \
  {                                                                                                \
    EXPECT(!message);                                                                              \
    UNREACH();                                                                                     \
  } while (0)

#endif

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

#define UNREACHABLE(message)                                                                       \
  do                                                                                               \
  {                                                                                                \
    assert(!message);                                                                              \
    TRAP();                                                                                        \
    UNREACH();                                                                                     \
  } while (0)

#endif

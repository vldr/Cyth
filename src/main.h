#ifndef main_h
#define main_h

#include <assert.h>

#ifdef _WIN32
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

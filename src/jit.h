#ifndef jit_h
#define jit_h

#include "statement.h"

#define init_static_string(name, value)                                                            \
  static struct                                                                                    \
  {                                                                                                \
    int size;                                                                                      \
    char data[sizeof(value)];                                                                      \
  } name = { .size = sizeof(value) - 1, .data = value }

typedef struct _JIT Jit;
typedef struct _STRING
{
  int size;
  char data[];
} String;

typedef struct _ARRAY
{
  int size;
  int capacity;
  void* data;
} Array;

Jit* jit_init(ArrayStmt statements);
void* jit_alloc(bool atomic, size_t size);
void jit_set_function(Jit* jit, const char* name, uintptr_t func);
void jit_generate(Jit* jit, bool logging);
void jit_run(Jit* jit);
void jit_destroy(Jit* jit);

#endif

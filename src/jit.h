#ifndef jit_h
#define jit_h

#include "statement.h"
#include <setjmp.h>

#define jit_init_string(name, value)                                                               \
  static struct                                                                                    \
  {                                                                                                \
    int size;                                                                                      \
    char data[sizeof(value)];                                                                      \
  } name = { .size = sizeof(value) - 1, .data = value }

#define jit_try_catch(_jit, _block)                                                                \
  do                                                                                               \
  {                                                                                                \
    jmp_buf _new;                                                                                  \
    jmp_buf* _old = jit_push_jmp(_jit, &_new);                                                     \
                                                                                                   \
    if (setjmp(_new) == 0)                                                                         \
      _block                                                                                       \
                                                                                                   \
        jit_pop_jmp(_jit, _old);                                                                   \
  } while (0)

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
void jit_generate(Jit* jit, bool logging);
void jit_run(Jit* jit);
void jit_destroy(Jit* jit);

void* jit_push_jmp(Jit* jit, void* new);
void jit_pop_jmp(Jit* jit, void* old);

void jit_set_function(Jit* jit, const char* name, uintptr_t func);
uintptr_t jit_get_function(Jit* jit, const char* name);

#endif

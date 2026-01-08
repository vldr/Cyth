#ifndef jit_h
#define jit_h

#include <setjmp.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WIN32
  int jit_setjmp(jmp_buf buf);
  void jit_longjmp(jmp_buf buf, int n);
#else
#define jit_setjmp(buf) sigsetjmp(buf, 1)
#define jit_longjmp siglongjmp
#endif

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
    jmp_buf* _old = (jmp_buf*)jit_push_jmp(_jit, (void*)&_new);                                    \
                                                                                                   \
    if (jit_setjmp(_new) == 0)                                                                     \
      _block                                                                                       \
                                                                                                   \
        jit_pop_jmp(_jit, (void*)_old);                                                            \
  } while (0)

  typedef struct _JIT Jit;

  Jit* jit_init(char* source,
                void (*error_callback)(int start_line, int start_column, int end_line,
                                       int end_column, const char* message),
                void (*panic_callback)(const char* function, int line, int column));
  void* jit_alloc(int atomic, uintptr_t size);
  void jit_generate(Jit* jit, int logging);
  void jit_run(Jit* jit);
  void jit_destroy(Jit* jit);

  void* jit_push_jmp(Jit* jit, void* new_jmp);
  void jit_pop_jmp(Jit* jit, void* old_jmp);

  void jit_set_function(Jit* jit, const char* name, uintptr_t func);
  uintptr_t jit_get_function(Jit* jit, const char* name);
  uintptr_t jit_get_variable(Jit* jit, const char* name);

#ifdef __cplusplus
}

struct String
{
  int size;
  char data[1];
};

template <typename T> struct Array
{
  int size;
  int capacity;
  T* data;
};

template <typename Func> void jit_set_function_typed(Jit* jit, const char* name, Func* fn)
{
  jit_set_function(jit, name, (uintptr_t)fn);
}

template <typename Func> Func* jit_get_function_typed(Jit* jit, const char* name)
{
  uintptr_t addr = jit_get_function(jit, name);
  if (!addr)
    return nullptr;

  return reinterpret_cast<Func*>(addr);
}

template <typename T> T* jit_get_variable_typed(Jit* jit, const char* name)
{
  uintptr_t addr = jit_get_variable(jit, name);
  if (!addr)
    return nullptr;

  return reinterpret_cast<T*>(addr);
}
#else

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

#endif
#endif

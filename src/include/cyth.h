#ifndef cyth_h
#define cyth_h

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
#ifndef EMSCRIPTEN
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

  // Create a new JIT instance.
  //
  // This will return NULL if a compilation error has occurred.
  // This function can only be called on a single thread.
  //
  // [source] is the source code to be compiled.
  //
  // [error_callback] will be called when a compilation error occurs.
  // This can be NULL, in which case it will not be called.
  //
  // [panic_callback] will be called when a runtime error occurs.
  // This can be NULL, in which case it will not be called.
  //
  // The callback will be called multiple times. The first call is a special case, where zero will
  // be passed into the line and column, and the error reason will be in the function string.
  // Subsequent calls will be for each function line/column combination in the stack trace.
  Jit* cyth_init(char* source,
                 void (*error_callback)(int start_line, int start_column, int end_line,
                                        int end_column, const char* message),
                 void (*panic_callback)(const char* function, int line, int column));

  // Adds an external C function.
  //
  // You MUST call this after "cyth_init" but before "cyth_generate".
  //
  // [name] must be in the format: <module>.<function name>.<signature>
  //
  // For example, if I have the following Cyth code:
  //
  //    import "env"
  //      void println(string data)
  //
  // The corresponding C code would look like:
  //
  //    void println(String* string) {
  //      printf("%s\n", string->data);
  //    }
  //
  //    cyth_set_function(jit, "env.println.void(string)", (uintptr_t)println);
  //
  // [func] must be the address to the corresponding external C function.
  void cyth_set_function(Jit* jit, const char* name, uintptr_t func);

  // Generates the assembly instructions for a given JIT instance.
  // After this function, it is safe to run the generated code.
  //
  // [logging] is 1, then generated IR instructions will be printed to stdout.
  void cyth_generate(Jit* jit, int logging);

  // Runs the top-level scope of the program (which is the <start> function).
  // This can only be called on a single thread.
  void cyth_run(Jit* jit);

  // Destroys a JIT instance.
  //
  // This MUST be called after calling "cyth_init" and "cyth_generate" respectively.
  // After this function, it is unsafe to run the generated code.
  void cyth_destroy(Jit* jit);

  // Allocates a block of memory and returns a pointer to that memory.
  //
  // This memory will be automatically cleaned up by the garbage collector.
  // Do not store the returned pointer outside the program as the garbage
  // collector won't be able to find it.
  //
  // [atomic] is 0, if the memory you're allocating contains pointers to
  // heap allocated like arrays and objects.
  //
  // It is 1, if the memory you're allocating does NOT contain any pointers.
  //
  // If you're confused, just pass 0 always.
  //
  // [size] is the size in bytes to allocate.
  void* cyth_alloc(int atomic, uintptr_t size);

  // Returns the address to a Cyth function.
  //
  // You MUST wrap all calls to Cyth functions with "cyth_try_catch" (see below).
  //
  // You MUST call "cyth_run" before calling functions obtained from this function, otherwise global
  // variables will be uninitialized.
  //
  // [name] must be in the format: <function name>.<signature>
  //
  // For example, if I have the following Cyth code:
  //
  //    int adder(int a, int b)
  //      return a + b
  //
  // The corresponding C code would look like:
  //
  //    int(*)(int, int) adder = (int(*)(int, int)) cyth_get_function(jit, "adder.int(int, int)");
  //
  //    cyth_try_catch(jit, {
  //      adder(10, 10);
  //    } else {
  //      printf("error!");
  //    })
  //
  uintptr_t cyth_get_function(Jit* jit, const char* name);

  // Returns the address to the memory that contains a global variable (top-level scope).
  //
  // You MUST call "cyth_run" before accessing global variables, otherwise
  // they will be uninitialized.
  //
  // [name] must be in the format: <function name>.<signature>
  //
  // For example, if I have the following Cyth code:
  //
  //    int globalVariable = 10
  //
  // The corresponding C code would look like:
  //
  //    cyth_get_variable(jit, "globalVariable.int");
  //
  uintptr_t cyth_get_variable(Jit* jit, const char* name);

  // Executes a block of code and catches any runtime panics.
  //
  // This macro installs a temporary exception handler using setjmp/longjmp and signals (VEH on
  // Windows). If a runtime error (panic) occurs while executing the block, control flow will
  // jump to the "else" clause instead of terminating the program. The "else" clause is optional.
  // This can be used recursively.
  //
  // You MUST use this whenever calling generated code, otherwise the program will crash or get into
  // a corrupted state.
  //
  // You MUST never call "return" or "break" inside this macro, otherwise the program will get into
  // a corrupted state.
  //
  //   jit_try_catch(jit, {
  //     foo(1, 2);
  //   } else {
  //     printf("Runtime error!\n");
  //   });
  //
#define cyth_try_catch(_jit, _block)                                                               \
  do                                                                                               \
  {                                                                                                \
    void* cyth_push_jmp(Jit * jit, void* new_jmp);                                                 \
    void cyth_pop_jmp(Jit * jit, void* old_jmp);                                                   \
                                                                                                   \
    jmp_buf _new;                                                                                  \
    jmp_buf* _old = (jmp_buf*)cyth_push_jmp(_jit, (void*)&_new);                                   \
                                                                                                   \
    if (cyth_setjmp(_new) == 0)                                                                    \
      _block                                                                                       \
                                                                                                   \
        cyth_pop_jmp(_jit, (void*)_old);                                                           \
  } while (0)

// Declares a static Cyth string variable with the [name] and [value].
#define cyth_static_string(name, value)                                                            \
  static struct                                                                                    \
  {                                                                                                \
    int size;                                                                                      \
    char data[sizeof(value)];                                                                      \
  } name = { .size = sizeof(value) - 1, .data = value }

#ifdef _WIN32
  int cyth_setjmp(jmp_buf buf);
  void cyth_longjmp(jmp_buf buf, int n);
#else
#define cyth_setjmp(buf) sigsetjmp(buf, 1)
#define cyth_longjmp siglongjmp
#endif
#endif

#ifdef WASM
  // Initializes a WASM code generation instance.
  // This will return 0, if a compilation error has occurred. Otherwise, it will return 1.
  //
  // [source] is the source code to be compiled.
  //
  // [error_callback] will be called when a compilation error occurs.
  // This can be NULL, in which case it will not be called.
  //
  // [result_callback] will be called after "cyth_wasm_generate" with the WASM binary data and
  // sourcemap text data.
  // This can be NULL, in which case it will not be called.
  int cyth_wasm_init(char* source,
                     void (*error_callback)(int start_line, int start_column, int end_line,
                                            int end_column, const char* message),
                     void (*result_callback)(size_t size, void* data, size_t source_map_size,
                                             void* source_map));

  // Generates WebAssembly instructions.
  //
  // [logging] is 1, then generated WAT instructions will be printed to stdout.
  void cyth_wasm_generate(int logging);

#endif
#ifdef __cplusplus
}
#endif
#endif

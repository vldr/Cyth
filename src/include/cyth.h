#ifndef cyth_h
#define cyth_h

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct _CY_VM CyVM;
  typedef struct _CY_STRING
  {
    int size;
    char data[];
  } CyString;

  typedef struct _CY_ARRAY
  {
    int size;
    int capacity;
    void* data;
  } CyArray;

  typedef int (*CySetJMP)(jmp_buf buf);
  typedef void (*CyLongJMP)(jmp_buf buf, int n);

  // Creates a new VM instance.
  CyVM* cyth_init(void);

  // Loads a string to compile.
  //
  // You MUST call this after "cyth_init" but before "cyth_compile".
  //
  // [filename] is the name to be associated with the provided source code
  // (will appear in the error callback).
  //
  // [string] is the source code to be compiled.
  //
  // This function will return 1 if the file was successfully loaded,
  // or return 0, if an error has occurred (which will also call the error callback).
  int cyth_load_string(CyVM* vm, const char* filename, const char* string);

  // Loads a file to compile.
  //
  // You MUST call this after "cyth_init" but before "cyth_compile".
  //
  // [filename] is the path to a file that contains source code to be compiled.
  //
  // This function will return 1 if the file was successfully loaded,
  // or return 0, if an error has occurred (which will also call the error callback).
  int cyth_load_file(CyVM* vm, const char* filename);

  // Loads an external C function to compile.
  //
  // You MUST call this after "cyth_init" but before "cyth_compile".
  //
  // [signature] is the declaration of the function.
  //
  // For example, if I want to import and use the "print" function in Cyth:
  //
  //    print("Hello from Cyth!")
  //
  // The corresponding C code would look like:
  //
  //    void print(CyString* string) {
  //      printf("%s\n", string->data);
  //    }
  //
  //    cyth_load_function(vm, "void print(string text)", (uintptr_t)print);
  //
  // [func] must be the address to the external C function.
  //
  // This function will return 1 if the function was successfully loaded,
  // or return 0, if an error has occurred (which will also call the error callback).
  int cyth_load_function(CyVM* vm, const char* signature, uintptr_t func);

  // Compiles the Cyth source code to machine instructions.
  //
  // After calling this function, you can safely run the generated code.
  //
  // This function will return 1 if the program was successfully compiled,
  // or return 0, if an error has occurred (which will also call the error callback).
  int cyth_compile(CyVM* vm);

  // Runs the top-level scope of the program (which is called the <start> function).
  //
  // Note: calling Cyth code is not thread safe.
  void cyth_run(CyVM* vm);

  // Destroys a VM instance.
  //
  // This MUST be called after calling "cyth_init" and "cyth_compile" respectively.
  //
  // After this function is called, it is UNSAFE to run generated code as it will be deleted.
  void cyth_destroy(CyVM* vm);

  // Allocates a block of memory and returns a pointer to that memory.
  //
  // This memory is managed by the garbage collector and will be automatically
  // cleaned up.
  //
  // Do not store the returned pointer outside the program as the garbage
  // collector won't be able to find it and might prematurely deallocate it.
  //
  // [atomic] is 0, if the memory you're allocating contains pointers to
  // heap allocated strings, arrays and objects.
  //
  // It is 1, if the memory you're allocating does NOT contain any pointers.
  //
  // If you're confused, just pass 0 always.
  //
  // [size] is the size in bytes to allocate.
  void* cyth_alloc(int atomic, uintptr_t size);

  // Sets the error callback function.
  //
  // Using this function is optional, Cyth will use a default error callback function.
  //
  // [error_callback] will be called when a compilation error occurs.
  void cyth_set_error_callback(CyVM* vm,
                               void (*error_callback)(const char* filename, int start_line,
                                                      int start_column, int end_line,
                                                      int end_column, const char* message));

  // Sets the panic callback function.
  //
  // Using this function is optional, Cyth will use a default panic callback function.
  //
  // [panic_callback] will be called when a runtime error occurs.
  //
  // This callback will be called multiple times. The first call is a special case, where zero will
  // be passed into the line and column parameters and the error reason will be passed into both the
  // filename and function parameter.
  //
  // Subsequent calls will be for each function line/column combination in the stack trace.
  void cyth_set_panic_callback(CyVM* vm,
                               void (*panic_callback)(const char* filename, const char* function,
                                                      int line, int column));

  // Enable/disable logging.
  //
  // [logging] is 1, logging is enabled. When 0, logging is disabled.
  void cyth_set_logging(CyVM* vm, int logging);

  // Returns the address to a Cyth function.
  //
  // You MUST wrap all calls to Cyth functions with "cyth_try_catch" (see below).
  //
  // You MUST call "cyth_run" before calling functions obtained from this function, otherwise global
  // variables will be uninitialized.
  //
  // [name] must be in the format: <function name>.<type name>
  //
  // For example, if I have the following Cyth code:
  //
  //    int adder(int a, int b)
  //      return a + b
  //
  // The corresponding C code would look like:
  //
  //    typedef int (*Func)(int, int);
  //    Func adder = (Func) cyth_get_function(vm, "adder.int(int, int)");
  //
  //    cyth_try_catch(vm, {
  //      adder(10, 10);
  //    } else {
  //      printf("error!");
  //    })
  //
  uintptr_t cyth_get_function(CyVM* vm, const char* name);

  // Returns the address to memory that contains a global variable (top-level scope).
  //
  // You MUST call "cyth_run" before accessing global variables, otherwise
  // they will be uninitialized.
  //
  // [name] must be in the format: <variable name>.<type name>
  //
  // For example, if I have the following Cyth code:
  //
  //    int globalVariable = 10
  //
  // The corresponding C code would look like:
  //
  //    int* myVariable = (int*) cyth_get_variable(vm, "globalVariable.int");
  //
  uintptr_t cyth_get_variable(CyVM* vm, const char* name);

  // Executes a block of code and catches any runtime panics.
  //
  // This macro installs a temporary exception handler using setjmp/longjmp and signals (VEH on
  // Windows).
  //
  // If a runtime error (panic) occurs while executing the block, control flow will jump to the
  // "else" clause instead of terminating the program. Providing the "else" clause is optional. This
  // can be used recursively.
  //
  // You MUST use this whenever calling generated code, otherwise the program will crash or get into
  // a corrupted state.
  //
  // You MUST never call "return" or "break" inside this macro, otherwise the program will get into
  // a corrupted state (since clean up code will be skipped).
  //
  //   cyth_try_catch(vm, {
  //     foo(1, 2);
  //   } else {
  //     printf("Runtime error!\n");
  //   });
  //
#define cyth_try_catch(_vm, _block)                                                                \
  do                                                                                               \
  {                                                                                                \
    jmp_buf _new;                                                                                  \
    jmp_buf* _old = (jmp_buf*)cyth_push_jmp((_vm), (void*)&_new);                                  \
                                                                                                   \
    if (cyth_setjmp()(_new) == 0)                                                                  \
      _block                                                                                       \
                                                                                                   \
        cyth_pop_jmp((_vm), (void*)_old);                                                          \
  } while (0)

  // Declares a static Cyth string variable with the [name] and [value].
#define cyth_static_string(name, value)                                                            \
  static struct                                                                                    \
  {                                                                                                \
    int size;                                                                                      \
    char data[sizeof(value)];                                                                      \
  } name = { .size = sizeof(value) - 1, .data = value }

  CySetJMP cyth_setjmp(void);
  CyLongJMP cyth_longjmp(void);
  void* cyth_push_jmp(CyVM* vm, void* new_jmp);
  void cyth_pop_jmp(CyVM* vm, void* old_jmp);

  void cyth_wasm_init(void);
  int cyth_wasm_load_string(const char* filename, const char* string);
  int cyth_wasm_load_function(const char* signature, const char* module);
  int cyth_wasm_compile(int compile, int logging);
  void cyth_wasm_set_error_callback(void (*error_callback)(const char* filename, int start_line,
                                                           int start_column, int end_line,
                                                           int end_column, const char* message));
  void cyth_wasm_set_result_callback(void (*result_callback)(size_t size, void* data,
                                                             size_t source_map_size,
                                                             void* source_map));
  void cyth_wasm_set_link_callback(void (*link_callback)(const char* ref_filename, int ref_line,
                                                         int ref_column, const char* def_filename,
                                                         int def_line, int def_column, int length));
#ifdef __cplusplus
}
#endif
#endif

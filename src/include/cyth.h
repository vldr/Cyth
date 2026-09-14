#ifndef cyth_h
#define cyth_h

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

  // Creates a new VM instance.
  CyVM* cyth_init(void);

  // Loads a string to compile.
  //
  // You MUST call this after "cyth_init" but before "cyth_compile".
  //
  // [filename] is the name to be associated with the provided source code (will appear in the error
  // callback).
  //
  // [source] is the source code to be compiled.
  //
  // This function will return 1 if the string was successfully loaded, or return 0, if an
  // error has occurred (which will also call the error callback).
  int cyth_load_string(CyVM* vm, const char* filename, const char* source);

  // Loads a file to compile.
  //
  // You MUST call this after "cyth_init" but before "cyth_compile".
  //
  // [filename] is the path to a file that contains source code to be compiled.
  //
  // This function will return 1 if the file was successfully loaded, or return 0, if an error has
  // occurred (which will also call the error callback).
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
  // This function will return 1 if the function was successfully loaded, or return 0, if an error
  // has occurred (which will also call the error callback).
  int cyth_load_function(CyVM* vm, const char* signature, uintptr_t func);

  // Compiles the Cyth source code to machine instructions.
  //
  // After calling this function, you can safely run the generated code.
  //
  // This function will return 1 if the program was successfully compiled, or return 0, if an error
  // has occurred (which will also call the error callback).
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
  // This memory is managed by the garbage collector and will be automatically cleaned up.
  //
  // Do not store the returned pointer outside the program as the garbage collector won't be able to
  // find it and might prematurely deallocate it. Only store the returned pointer on the stack.
  //
  // [atomic] is 0, if the memory you're allocating contains pointers to heap allocated strings,
  // arrays and objects.
  //
  // It is 1, if the memory you're allocating does NOT contain any pointers.
  //
  // If you're confused, just pass 0 always (though performance will suffer).
  //
  // [size] is the size in bytes to allocate.
  void* cyth_alloc(int atomic, uintptr_t size);

  // Sets the error callback function.
  //
  // Using this function is optional, Cyth will use a default error callback function that will
  // print error messages to the terminal.
  //
  // [error_callback] will be called when a compilation error occurs.
  void cyth_set_error_callback(CyVM* vm,
                               void (*error_callback)(const char* filename, int start_line,
                                                      int start_column, int end_line,
                                                      int end_column, const char* message));

  // Sets the panic callback function.
  //
  // Using this function is optional, Cyth will use a default panic callback function that will
  // print panics to the terminal.
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

  // Sets the import callback function.
  //
  // Using this function is optional, Cyth will use a default import callback function that will
  // read files from the filesystem.
  //
  // You can disable the ability to import files, by calling "cyth_set_import_callback" with a
  // NULL argument.
  //
  // [import_callback] will be called when importing a file.
  //
  // Inside the callback, you must call either "cyth_load_string" or "cyth_load_file". The callback
  // must return 1, if the import operation was successful, otherwise return 0 (this will also
  // trigger a call to the error callback).
  //
  // The filename passed to "cyth_load_string" or "cyth_load_file" is important. Cyth ignores
  // repeated imports with the same filename. Make sure import paths that refer to the same file are
  // resolved to the same filename.
  void cyth_set_import_callback(CyVM* vm, int (*import_callback)(CyVM* vm, const char* filename,
                                                                 const char* importer_filename));

  // Enable/disable logging.
  //
  // [logging] is 1, logging is enabled. When 0, logging is disabled.
  void cyth_set_logging(CyVM* vm, int logging);

  // Returns the address to a Cyth function.
  //
  // You MUST call "cyth_run" before calling functions obtained from this function, otherwise global
  // variables will be uninitialized.
  //
  // You should call "cyth_error" after calling a Cyth function to check whether an runtime panic
  // occurred (see below). If a runtime panic occurred, the return value of the Cyth function will
  // always be zero or NULL if a pointer.
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
  //    int sum = adder(10, 10);
  //    if (cyth_error(vm))
  //      print("Runtime panic!\n");
  //
  uintptr_t cyth_get_function(CyVM* vm, const char* name);

  // Returns the address to an unsafe Cyth function.
  //
  // This function is similar to "cyth_get_function" except that the function pointer it returns
  // is an unsafe variant, meaning that if you call the Cyth function and a runtime panic occurs,
  // then your C application will crash alongside the Cyth application.
  //
  // This function is useful if you know that the function you are calling will not panic,
  // or if it does panic, you are already inside the Cyth VM, such as in a callback or "cyth_run".
  uintptr_t cyth_get_function_unsafe(CyVM* vm, const char* name);

  // Returns the address to memory that contains a global variable (top-level scope).
  //
  // You MUST call "cyth_run" before accessing global variables, otherwise they will be
  // uninitialized.
  //
  // Note that if a global variable is only read/written within the top-level scope of a program,
  // and not within other functions, it will be demoted from a global to a local variable and will
  // not be accessible by this function.
  //
  // [name] must be in the format: <variable name>.<type name>
  //
  //  For example, if I have the following Cyth code:
  //
  //     int globalVariable = 10
  //
  //  The corresponding C code would look like:
  //
  //     int* myVariable = (int*) cyth_get_variable(vm, "globalVariable.int");
  //
  uintptr_t cyth_get_variable(CyVM* vm, const char* name);

  // Returns 1 if a runtime panic has occurred, or 0 if no runtime panic has occurred.
  //
  // This function should always be called after calling a Cyth function. This includes functions
  // obtained from "cyth_get_function" and function pointers.
  int cyth_error(CyVM* vm);

#ifdef WASM
  // Initializes WASM compilation.
  void cyth_wasm_init(void);

  // Loads a string to compile.
  //
  // You MUST call this after "cyth_wasm_init" but before "cyth_wasm_compile".
  //
  // [filename] is the name to be associated with the provided source code (will appear in the error
  // callback).
  //
  // [string] is the source code to be compiled.
  //
  // This function will return 1 if the file was successfully loaded, or return 0, if an error has
  // occurred (which will also call the error callback).
  int cyth_wasm_load_string(const char* filename, const char* string);

  // Loads an external function to compile.
  //
  // [signature] is the declaration of the function.
  // [module] is the module that the function will be imported from.
  //
  // For example, if I want to import and use the "print" function in Cyth:
  //
  //    print("Hello from Cyth!")
  //
  // The corresponding C code would look like:
  //
  //    cyth_wasm_load_function(vm, "void print(string text)", "env");
  //
  // And the corresponding JS code would look like:
  //
  //    await WebAssembly.instantiate(bytecode, {
  //      env: { "print.void(string)": print },
  //    });
  //
  int cyth_wasm_load_function(const char* signature, const char* module);

  // Compiles the Cyth source code to a WASM binary.
  //
  // This function will return 1 if the program was successfully compiled, or return 0, if an error
  // has occurred (which will also call the error callback).
  //
  // If the function ran successfully, the result callback will be called (see below).
  int cyth_wasm_compile(int compile, int logging);

  // Sets the result callback function.
  //
  // This callback is called after "cyth_wasm_compile" finishes successfully. The callback is
  // provided the compiled WASM binary data and a source map with debug info.
  //
  // [error_callback] will be called when a compilation successfully finishes.
  void cyth_wasm_set_result_callback(void (*result_callback)(size_t size, void* data,
                                                             size_t source_map_size,
                                                             void* source_map));
  // Sets the error callback function.
  //
  // Using this function is optional, Cyth will use a default error callback function.
  //
  // [error_callback] will be called when a compilation error occurs.
  void cyth_wasm_set_error_callback(void (*error_callback)(const char* filename, int start_line,
                                                           int start_column, int end_line,
                                                           int end_column, const char* message));

  // Sets the link callback function.
  //
  // This callback can be used to build a go-to definition table.
  //
  // Using this function is optional, if not set, Cyth will not call the link callback.
  //
  // [link_callback] will be called when there is a link between a reference and definition.
  void cyth_wasm_set_link_callback(void (*link_callback)(const char* ref_filename, int ref_line,
                                                         int ref_column, const char* def_filename,
                                                         int def_line, int def_column, int length));

  // Sets the import callback function.
  //
  // Using this function is optional, by default import functionality is disabled.
  //
  // You can disable the ability to import files, by calling "cyth_wasm_set_import_callback" with a
  // NULL argument.
  //
  // [import_callback] will be called when importing a file.
  //
  // Inside the callback, you must call either "cyth_load_string" or "cyth_load_file". The callback
  // must return 1, if the import operation was successful, otherwise return 0 (this will also
  // trigger a call to the error callback).
  //
  // The filename passed to "cyth_load_string" or "cyth_load_file" is important. Cyth ignores
  // repeated imports with the same filename. Make sure import paths that refer to the same file are
  // resolved to the same filename.
  void cyth_wasm_set_import_callback(int (*import_callback)(const char* filename,
                                                            const char* importer_filename));
#endif
#ifdef __cplusplus
}
#endif
#endif

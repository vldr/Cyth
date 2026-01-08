#include "main.h"
#include "array.h"
#include "checker.h"
#include "jit.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "statement.h"

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static struct
{
  bool error;
  bool logging;
  bool typecheck;
  bool wasm;
  bool io;

  const char* input_path;
  const char* output_path;

  const char* previous_function;
  int previous_line;
  int previous_column;
  int previous_count;

  void (*error_callback)(int start_line, int start_column, int end_line, int end_column,
                         const char* message);
  void (*result_callback)(size_t size, void* data, size_t source_map_size, void* source_map);
} cyth;

#ifdef WASM
#include "codegen.h"
#endif

#ifndef EMSCRIPTEN
#include "jit.h"

static void log_int(int n)
{
  printf("%d\n", n);
}

static void log_float(float n)
{
  printf("%.10g\n", n);
}

static void log_char(char n)
{
  printf("%c\n", n);
}

static void log_string(String* n)
{
  fwrite(n->data, 1, n->size, stdout);
  putchar('\n');
}

static void panic_callback(const char* function, int line, int column)
{
  if (line && column)
  {
    if (function == cyth.previous_function && line == cyth.previous_line &&
        column == cyth.previous_column)
    {
      if (cyth.previous_count == 0)
        fprintf(stderr, "  at ...\n");

      cyth.previous_count++;
    }
    else
    {
      fprintf(stderr, "  at %s:%d:%d\n", function, line, column);
      cyth.previous_count = 0;
    }
  }
  else
  {
    fprintf(stderr, "%s\n", function);
  }

  cyth.previous_line = line;
  cyth.previous_column = column;
  cyth.previous_function = function;
  cyth.error = true;
}
#endif

static void error_callback(int start_line, int start_column, int end_line, int end_column,
                           const char* message)
{
  fprintf(stderr, "%s%s%d:%d-%d:%d: error: %s\n", cyth.input_path ? cyth.input_path : "",
          cyth.input_path ? ":" : "", start_line, start_column, end_line, end_column, message);

  cyth.error = true;
}

static void result_callback(size_t size, void* data, size_t source_map_size, void* source_map)
{
  (void)source_map_size;

  if (!cyth.io && !cyth.output_path)
    goto clean_up;

  FILE* file;

  if (cyth.io)
    file = stdout;
  else
    file = fopen(cyth.output_path, "wb");

  if (!file)
  {
    fprintf(stderr, "error: could not open file: %s\n", cyth.output_path);

    cyth.error = true;
    goto clean_up;
  }

  size_t bytes_written = fwrite(data, sizeof(unsigned char), size, file);
  if (size != bytes_written)
  {
    fprintf(stderr, "error: could not write file: %s\n", cyth.output_path);

    cyth.error = true;
    goto clean_up_fd;
  }

clean_up_fd:
  fclose(file);

clean_up:
  free(data);
  free(source_map);
}

void set_logging(bool logging)
{
  cyth.logging = logging;
}

void set_error_callback(void (*error_callback)(int start_line, int start_column, int end_line,
                                               int end_column, const char* message))
{
  cyth.error_callback = error_callback;
}

void set_result_callback(void (*result_callback)(size_t size, void* data, size_t source_map_size,
                                                 void* source_map))
{
  cyth.result_callback = result_callback;
}

void run(char* source, bool codegen)
{
  if (codegen)
  {
#ifdef EMSCRIPTEN
    if (codegen_init(source, cyth.error_callback, cyth.result_callback))
      codegen_generate(cyth.logging);
#else
#ifdef WASM
    if (cyth.wasm)
    {
      if (codegen_init(source, cyth.error_callback, cyth.result_callback))
        codegen_generate(cyth.logging);
    }
    else
#endif
    {
      Jit* jit = jit_init(source, error_callback, panic_callback);
      if (jit)
      {
        jit_set_function(jit, "env.log", (uintptr_t)log_int);
        jit_set_function(jit, "env.log(int)", (uintptr_t)log_int);
        jit_set_function(jit, "env.log(bool)", (uintptr_t)log_int);
        jit_set_function(jit, "env.log(float)", (uintptr_t)log_float);
        jit_set_function(jit, "env.log(char)", (uintptr_t)log_char);
        jit_set_function(jit, "env.log(string)", (uintptr_t)log_string);
        jit_set_function(jit, "env.log<int>", (uintptr_t)log_int);
        jit_set_function(jit, "env.log<bool>", (uintptr_t)log_int);
        jit_set_function(jit, "env.log<float>", (uintptr_t)log_float);
        jit_set_function(jit, "env.log<char>", (uintptr_t)log_char);
        jit_set_function(jit, "env.log<string>", (uintptr_t)log_string);
        jit_generate(jit, cyth.logging);
        jit_run(jit);
        jit_destroy(jit);
      }
    }
#endif
  }
  else
  {
    lexer_init(source, cyth.error_callback);
    ArrayToken tokens = lexer_scan();

    if (lexer_errors())
      goto clean_up;

    parser_init(tokens, cyth.error_callback);
    ArrayStmt statements = parser_parse();

    if (parser_errors())
      goto clean_up;

    checker_init(statements, cyth.error_callback);
    checker_validate();

  clean_up:
    memory_reset();
  }
}

static void run_file(void)
{
#ifdef _WIN32
  int result = setmode(fileno(stdin), O_BINARY);
  if (result == -1)
  {
    fprintf(stderr, "error: could set 'stdin' mode to binary\n");

    cyth.error = true;
    return;
  }

  result = setmode(fileno(stdout), O_BINARY);
  if (result == -1)
  {
    fprintf(stderr, "error: could set 'stdout' mode to binary\n");

    cyth.error = true;
    return;
  }
#endif

  if (cyth.io)
  {
    ArrayChar source;
    array_init(&source);

    char c;
    while ((c = getchar()) != EOF)
    {
      array_add(&source, c);
    }

    array_add(&source, '\0');

    run(source.elems, !cyth.typecheck);
  }
  else
  {
    if (!cyth.input_path)
    {
      fprintf(stderr, "error: no input file\n");

      cyth.error = true;
      return;
    }

    FILE* file = fopen(cyth.input_path, "rb");
    if (!file)
    {
      fprintf(stderr, "error: could not open file: %s\n", cyth.input_path);

      cyth.error = true;
      return;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* source = memory_alloc(file_size + 1);
    size_t bytes_read = fread(source, sizeof(unsigned char), file_size, file);

    if (file_size != bytes_read)
    {
      fprintf(stderr, "error: could not read file: %s\n", cyth.input_path);

      cyth.error = true;
      return;
    }

    fclose(file);
    source[file_size] = '\0';

    run(source, !cyth.typecheck);
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    printf("usage: cyth [options] <input_file>\n");

#ifdef WASM
    printf("       cyth wasm [options] <input_file> [output_file]\n");
#endif

    printf("\n"
           "Available options are:\n"
           "  -t Parse and type-check only.\n"
           "  -l Print IR.\n"
           "  -  Read from stdin and output to stdout (will ignore options).\n");

    return 0;
  }

  int arg = 1;

#ifdef WASM
  if (strcmp(argv[1], "wasm") == 0)
  {
    cyth.wasm = true;
    arg++;
  }
#endif

  for (; arg < argc; arg++)
  {
    if (strcmp(argv[arg], "-l") == 0)
    {
      cyth.logging = true;
    }
    else if (strcmp(argv[arg], "-t") == 0)
    {
      cyth.typecheck = true;
    }
    else if (strcmp(argv[arg], "-") == 0)
    {
      cyth.io = true;
    }
    else
    {
      if (argv[arg][0] != '-')
      {
        if (cyth.input_path == NULL)
        {
          cyth.input_path = argv[arg];
        }
#ifdef WASM
        else if (cyth.output_path == NULL && cyth.wasm)
        {
          cyth.output_path = argv[arg];
        }
#endif
        else
        {
          fprintf(stderr, "error: unknown non-option: '%s'\n", argv[arg]);
          return -1;
        }
      }
      else
      {
        fprintf(stderr, "error: unknown option: '%s'\n", argv[arg]);
        return -1;
      }
    }
  }

  set_error_callback(error_callback);
  set_result_callback(result_callback);
  set_logging(cyth.logging);

  run_file();
  memory_free();

  return cyth.error ? -1 : 0;
}

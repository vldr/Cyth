#include "main.h"
#include "array.h"
#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "statement.h"

#include <stddef.h>
#include <stdio.h>

typedef void (*error_callback_t)(int start_line, int start_column, int end_line, int end_column,
                                 const char* message);
typedef void (*result_callback_t)(size_t size, void* data, size_t source_map_size,
                                  void* source_map);

static struct
{
  bool error;
  const char* input_path;
  const char* output_path;

  error_callback_t error_callback;
  result_callback_t result_callback;
} cyth;

void set_error_callback(error_callback_t callback)
{
  cyth.error_callback = callback;
}

void set_result_callback(result_callback_t callback)
{
  cyth.result_callback = callback;
}

void error(int start_line, int start_column, int end_line, int end_column, const char* message)
{
  cyth.error_callback(start_line, start_column, end_line, end_column, message);
  cyth.error = true;
}

void run(char* source, bool codegen)
{
  cyth.error = false;

  lexer_init(source);
  ArrayToken tokens = lexer_scan();

  if (cyth.error)
    goto clean_up;

  parser_init(tokens);
  ArrayStmt statements = parser_parse();

  if (cyth.error)
    goto clean_up;

  checker_init(statements);
  checker_validate();

  if (cyth.error)
    goto clean_up;

  if (codegen)
  {
    codegen_init(statements);
    Codegen codegen = codegen_generate();

    cyth.result_callback(codegen.size, codegen.data, codegen.source_map_size, codegen.source_map);
  }

clean_up:
  memory_reset();
}

static void run_file(void)
{
  FILE* file = fopen(cyth.input_path, "rb");
  if (!file)
  {
    fprintf(stderr, "Could not open file: %s\n", cyth.input_path);
    return;
  }

  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char* source = memory_alloc(file_size + 1);
  size_t bytes_read = fread(source, sizeof(unsigned char), file_size, file);

  if (file_size != bytes_read)
  {
    fprintf(stderr, "Could not read file: %s\n", cyth.input_path);
    return;
  }

  fclose(file);
  source[file_size] = '\0';

  run(source, true);
}

static void handle_error(int start_line, int start_column, int end_line, int end_column,
                         const char* message)
{
  fprintf(stderr, "%s:%d:%d-%d:%d: error: %s\n", cyth.input_path, start_line, start_column,
          end_line, end_column, message);
}

static void handle_result(size_t size, void* data, size_t source_map_size, void* source_map)
{
  (void)source_map_size;

  if (!cyth.output_path)
  {
    goto clean_up;
  }

  FILE* file = fopen(cyth.output_path, "wb");
  if (!file)
  {
    fprintf(stderr, "Could not open file: %s\n", cyth.output_path);
    goto clean_up;
  }

  size_t bytes_written = fwrite(data, sizeof(unsigned char), size, file);
  if (size != bytes_written)
  {
    fprintf(stderr, "Could not write file: %s\n", cyth.output_path);
    goto clean_up_fd;
  }

clean_up_fd:
  fclose(file);

clean_up:
  free(data);
  free(source_map);
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: cyth <input_path> [output_path]\n");
    return -1;
  }

  cyth.input_path = argv[1];

  if (argc < 3)
    cyth.output_path = NULL;
  else
    cyth.output_path = argv[2];

  set_error_callback(handle_error);
  set_result_callback(handle_result);

  run_file();
  memory_free();

  return 0;
}

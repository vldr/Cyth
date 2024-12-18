#include "main.h"
#include "array.h"
#include "checker.h"
#include "codegen.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "printer.h"
#include "statement.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static const char* file_name = NULL;
static bool error = false;

void report_error(int start_line, int start_column, int end_line, int end_column,
                  const char* message)
{
  fprintf(stderr, "%s:%d:%d-%d:%d: error: %s\n", file_name, start_line, start_column, end_line,
          end_column, message);
  error = true;
}

void run_source(const char* source)
{
  error = false;

  lexer_init(source);
  ArrayToken tokens = lexer_scan();

  if (error)
    return;

  parser_init(tokens);
  ArrayStmt statements = parser_parse();

  if (error)
    return;

  checker_init(statements);
  checker_validate();

  if (error)
    return;

  codegen_init(statements);
  codegen_generate();
}

void run_file(const char* path)
{
  FILE* file = fopen(path, "rb");
  if (!file)
  {
    fprintf(stderr, "Could not open file: %s\n", path);
    return;
  }

  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char* source = memory_alloc(&memory, file_size + 1);
  size_t bytes_read = fread(source, sizeof(char), file_size, file);

  if (file_size != bytes_read)
  {
    fprintf(stderr, "Could not read file: %s\n", path);
    return;
  }

  fclose(file);

  file_name = path;
  source[file_size] = '\0';

  run_source(source);
}

int main(int argc, char* argv[])
{
  if (argc == 2)
  {
    run_file(argv[1]);
  }
  else
  {
    fprintf(stderr, "Usage: cyth [path]\n");
    return -1;
  }

  memory_free(&memory);
  return 0;
}

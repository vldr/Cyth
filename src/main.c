#include "main.h"
#include "array.h"
#include "checker.h"
#include "memory.h"
#include "parser.h"
#include "printer.h"
#include "scanner.h"
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

static char* read_file(const char* path)
{
  FILE* file = fopen(path, "rb");
  if (!file)
  {
    fprintf(stderr, "Could not open file: %s\n", path);
    return NULL;
  }

  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char* buffer = memory_alloc(&memory, file_size + 1);
  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  if (file_size != bytes_read)
  {
    fprintf(stderr, "Could not read file: %s\n", path);
    return NULL;
  }

  fclose(file);

  file_name = path;
  buffer[file_size] = '\0';

  return buffer;
}

static void run_file(const char* path)
{
  char* source = read_file(path);
  if (!source)
    return;

  scanner_init(source);
  ArrayToken tokens = scanner_scan();

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

  Stmt* stmt;
  array_foreach(&statements, stmt)
  {
    print_ast(stmt->expr.expr);
    printf("\n");
  }
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

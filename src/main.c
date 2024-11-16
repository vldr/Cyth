#include "array.h"
#include "map.h"
#include "memory.h"
#include "scanner.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static char* readFile(const char* path)
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

  char* buffer = memory_alloc(&memory, file_size);
  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  if (file_size != bytes_read)
  {
    fprintf(stderr, "Could not read file: %s\n", path);
    return NULL;
  }

  fclose(file);

  buffer[bytes_read] = '\0';
  return buffer;
}

static void runFile(const char* path)
{
  char* source = readFile(path);
  if (!source)
    return;

  scanner_init(source);
  ArrayToken tokens = scanner_scan();

  Token token;
  array_foreach(&tokens, token)
  {
    printf("%d '%.*s'\n", token.type, token.length, token.start);
  }
}

int main(int argc, char* argv[])
{
  if (argc == 2)
  {
    runFile(argv[1]);
  }
  else
  {
    fprintf(stderr, "Usage: cyth [path]\n");
    return -1;
  }

  memory_free(&memory);
  return 0;
}

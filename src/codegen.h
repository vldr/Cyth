#ifndef codegen_h
#define codegen_h

#include <stddef.h>

typedef struct
{
  size_t size;
  void* data;

  size_t source_map_size;
  char* source_map;
} Codegen;

int codegen_init(char* source,
                 void (*error_callback)(int start_line, int start_column, int end_line,
                                        int end_column, const char* message),
                 void (*result_callback)(size_t size, void* data, size_t source_map_size,
                                         void* source_map));
void codegen_generate(int logging);

#endif

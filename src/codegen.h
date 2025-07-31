#ifndef codegen_h
#define codegen_h

#include "statement.h"

typedef struct
{
  size_t size;
  void* data;

  size_t source_map_size;
  char* source_map;
} Codegen;

void codegen_init(ArrayStmt statements);
Codegen codegen_generate(bool logging);

#endif

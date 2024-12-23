#ifndef codegen_h
#define codegen_h

#include "statement.h"

typedef struct
{
  size_t size;
  void* data;
} Codegen;

void codegen_init(ArrayStmt statements);
Codegen codegen_generate(void);

#endif

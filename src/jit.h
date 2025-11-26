#ifndef jit_h
#define jit_h

#include "statement.h"

void jit_init(ArrayStmt statements);
void jit_run(bool logging);

#endif

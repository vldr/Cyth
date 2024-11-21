#ifndef printer_h
#define printer_h

#include "expression.h"
#include "scanner.h"

void print_tokens(ArrayToken tokens);
void print_ast(Expr* expr);

#endif

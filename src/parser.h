#ifndef parser_h
#define parser_h

#include "scanner.h"

typedef struct _EXPR Expr;

void parser_init(ArrayToken tokens);
Expr* parser_parse();

#endif

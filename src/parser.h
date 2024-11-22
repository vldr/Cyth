#ifndef parser_h
#define parser_h

#include "scanner.h"
#include "statement.h"

void parser_init(ArrayToken tokens);
ArrayStmt parser_parse();

#endif

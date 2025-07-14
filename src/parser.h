#ifndef parser_h
#define parser_h

#include "lexer.h"
#include "statement.h"

void parser_init(ArrayToken tokens);
ArrayStmt parser_parse(void);

Stmt* parser_parse_class_declaration_statement(int offset, Token keyword, Token name);
Stmt* parser_parse_function_declaration_statement(int offset, DataTypeToken type, Token name);

#endif

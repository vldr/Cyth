#ifndef parser_h
#define parser_h

#include "lexer.h"
#include "statement.h"

void parser_init(ArrayToken tokens,
                 void (*error_callback)(int start_line, int start_column, int end_line,
                                        int end_column, const char* message));
int parser_errors(void);
ArrayStmt parser_parse(void);

Stmt* parser_parse_class_declaration_statement(ClassTemplateStmt* template);
Stmt* parser_parse_function_declaration_statement(FuncTemplateStmt* template);
Stmt* parser_parse_import_function_declaration_statement(const void* import);

#endif

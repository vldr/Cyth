#include "printer.h"
#include "scanner.h"

#include <stdio.h>

void print_tokens(ArrayToken tokens)
{
  Token token;
  array_foreach(&tokens, token)
  {
    static const char* types[] = {
      "INDENT",        "DEDENT",      "NEWLINE",      "LEFT_PAREN",    "RIGHT_PAREN",
      "LEFT_BRACE",    "RIGHT_BRACE", "LEFT_BRACKET", "RIGHT_BRACKET", "SEMICOLON",
      "COLON",         "COMMA",       "DOT",          "MINUS",         "MINUS_MINUS",
      "MINUS_EQUAL",   "PLUS",        "PLUS_PLUS",    "PLUS_EQUAL",    "SLASH",
      "SLASH_EQUAL",   "STAR",        "STAR_EQUAL",   "PERCENT",       "PERCENT_EQUAL",
      "BANG",          "BANG_EQUAL",  "EQUAL",        "EQUAL_EQUAL",   "GREATER",
      "GREATER_EQUAL", "LESS",        "LESS_EQUAL",   "IDENTIFIER",    "STRING",
      "INTEGER",       "FLOAT",       "AND",          "CLASS",         "ELSE",
      "FALSE",         "FOR",         "IF",           "NULL",          "OR",
      "NOT",           "RETURN",      "SUPER",        "THIS",          "TRUE",
      "WHILE",         "EOF"};

    printf("%d,%d-%d,%d \t%s    \t'%.*s'  \n", token.start_line, token.start_column, token.end_line,
           token.end_column, types[token.type], token.length, token.start);
  }
}

void print_ast(Expr* expr)
{
  switch (expr->type)
  {
  case EXPR_LITERAL:
    switch (expr->literal.type)
    {
    case TYPE_VOID:
      printf("void");
      break;
    case TYPE_NULL:
      printf("null");
      break;
    case TYPE_BOOL:
      printf("%s", expr->literal.boolean ? "true" : "false");
      break;
    case TYPE_INTEGER:
      printf("%d", expr->literal.integer);
      break;
    case TYPE_FLOAT:
      printf("%f", expr->literal.floating);
      break;
    case TYPE_STRING:
      printf("\"%.*s\"", expr->literal.string.length, expr->literal.string.value);
      break;
    }

    break;
  case EXPR_BINARY:
    printf("(binary ");
    print_ast(expr->binary.left);
    printf(" %c ", *expr->binary.op.start);
    print_ast(expr->binary.right);
    printf(")");
    break;
  case EXPR_UNARY:
    printf("(unary ");
    printf(" %c ", *expr->unary.op.start);
    print_ast(expr->unary.expr);
    printf(")");
    break;
  case EXPR_GROUP:
    printf("(group ");
    print_ast(expr->group.expr);
    printf(")");
    break;
  case EXPR_VAR:
    printf("%.*s", expr->var.name.length, expr->var.name.start);
  case EXPR_ASSIGN:
  case EXPR_CALL:
    break;
  }
}

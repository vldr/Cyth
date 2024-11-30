#include "printer.h"
#include "scanner.h"

#include <stdio.h>

void print_tokens(ArrayToken tokens)
{
  Token token;
  array_foreach(&tokens, token)
  {
    static const char* types[] = {"TOKEN_INDENT",
                                  "TOKEN_DEDENT",
                                  "TOKEN_NEWLINE",
                                  "TOKEN_LEFT_PAREN",
                                  "TOKEN_RIGHT_PAREN",
                                  "TOKEN_LEFT_BRACE",
                                  "TOKEN_RIGHT_BRACE",
                                  "TOKEN_LEFT_BRACKET",
                                  "TOKEN_RIGHT_BRACKET",
                                  "TOKEN_SEMICOLON",
                                  "TOKEN_COLON",
                                  "TOKEN_COMMA",
                                  "TOKEN_DOT",
                                  "TOKEN_MINUS",
                                  "TOKEN_MINUS_MINUS",
                                  "TOKEN_MINUS_EQUAL",
                                  "TOKEN_PLUS",
                                  "TOKEN_PLUS_PLUS",
                                  "TOKEN_PLUS_EQUAL",
                                  "TOKEN_SLASH",
                                  "TOKEN_SLASH_EQUAL",
                                  "TOKEN_STAR",
                                  "TOKEN_STAR_EQUAL",
                                  "TOKEN_PERCENT",
                                  "TOKEN_PERCENT_EQUAL",
                                  "TOKEN_BANG",
                                  "TOKEN_BANG_EQUAL",
                                  "TOKEN_EQUAL",
                                  "TOKEN_EQUAL_EQUAL",
                                  "TOKEN_GREATER",
                                  "TOKEN_GREATER_EQUAL",
                                  "TOKEN_LESS",
                                  "TOKEN_LESS_EQUAL",
                                  "TOKEN_IDENTIFIER",
                                  "TOKEN_STRING",
                                  "TOKEN_INTEGER",
                                  "TOKEN_FLOAT",
                                  "TOKEN_AND",
                                  "TOKEN_CLASS",
                                  "TOKEN_ELSE",
                                  "TOKEN_FALSE",
                                  "TOKEN_FOR",
                                  "TOKEN_IF",
                                  "TOKEN_IN",
                                  "TOKEN_NULL",
                                  "TOKEN_OR",
                                  "TOKEN_NOT",
                                  "TOKEN_RETURN",
                                  "TOKEN_SUPER",
                                  "TOKEN_THIS",
                                  "TOKEN_TRUE",
                                  "TOKEN_WHILE",
                                  "TOKEN_EOF"

    };

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
    printf("%c", *expr->unary.op.start);
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
  case EXPR_CAST:
    break;
  }
}

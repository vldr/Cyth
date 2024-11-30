#ifndef scanner_h
#define scanner_h

#include "array.h"
#include "memory.h"

typedef enum
{
  TOKEN_INDENT,
  TOKEN_DEDENT,
  TOKEN_NEWLINE,

  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_SEMICOLON,
  TOKEN_COLON,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_MINUS_MINUS,
  TOKEN_MINUS_EQUAL,
  TOKEN_PLUS,
  TOKEN_PLUS_PLUS,
  TOKEN_PLUS_EQUAL,
  TOKEN_SLASH,
  TOKEN_SLASH_EQUAL,
  TOKEN_STAR,
  TOKEN_STAR_EQUAL,
  TOKEN_PERCENT,
  TOKEN_PERCENT_EQUAL,

  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,

  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_INTEGER,
  TOKEN_FLOAT,

  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_IF,
  TOKEN_IN,
  TOKEN_NULL,
  TOKEN_OR,
  TOKEN_NOT,
  TOKEN_RETURN,
  TOKEN_SUPER,
  TOKEN_THIS,
  TOKEN_TRUE,
  TOKEN_WHILE,

  TOKEN_EOF
} TokenType;

typedef struct
{
  TokenType type;
  int start_line;
  int start_column;
  int end_line;
  int end_column;
  int length;
  const char* start;
} Token;

array_def(Token, Token);

void scanner_init(const char* source);
ArrayToken scanner_scan();

#endif

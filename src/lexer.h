#ifndef lexer_h
#define lexer_h

#include "array.h"
#include "main.h"
#include "memory.h"

#define TOKEN_EMPTY() ((Token){ 0 })
#define DATA_TYPE_TOKEN_EMPTY() ((DataTypeToken){ 0 })

array_def(struct _TOKEN, Token);
array_def(struct _DATA_TYPE_TOKEN, DataTypeToken);

typedef enum
{
  TOKEN_NONE,

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
  TOKEN_QUESTION,

  TOKEN_TILDE,
  TOKEN_AMPERSAND,
  TOKEN_AMPERSAND_EQUAL,
  TOKEN_PIPE,
  TOKEN_PIPE_EQUAL,
  TOKEN_CARET,
  TOKEN_CARET_EQUAL,
  TOKEN_LESS_LESS,
  TOKEN_LESS_LESS_EQUAL,
  TOKEN_GREATER_GREATER,
  TOKEN_GREATER_GREATER_EQUAL,

  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,

  TOKEN_IDENTIFIER,
  TOKEN_IDENTIFIER_VOID,
  TOKEN_IDENTIFIER_ANY,
  TOKEN_IDENTIFIER_INT,
  TOKEN_IDENTIFIER_FLOAT,
  TOKEN_IDENTIFIER_BOOL,
  TOKEN_IDENTIFIER_CHAR,
  TOKEN_IDENTIFIER_STRING,
  TOKEN_CHAR,
  TOKEN_STRING,
  TOKEN_INTEGER,
  TOKEN_HEX_INTEGER,
  TOKEN_FLOAT,

  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_IF,
  TOKEN_IN,
  TOKEN_IS,
  TOKEN_NULL,
  TOKEN_OR,
  TOKEN_NOT,
  TOKEN_RETURN,
  TOKEN_TRUE,
  TOKEN_WHILE,
  TOKEN_CONTINUE,
  TOKEN_BREAK,
  TOKEN_IMPORT,
  TOKEN_NAN,
  TOKEN_INFINITY,

  TOKEN_EOF
} TokenKind;

typedef struct _TOKEN
{
  TokenKind type;
  int start_line;
  int start_column;
  int end_line;
  int end_column;
  int length;
  const char* lexeme;
} Token;

typedef struct _DATA_TYPE_TOKEN
{
  enum
  {
    DATA_TYPE_TOKEN_NONE,
    DATA_TYPE_TOKEN_PRIMITIVE,
    DATA_TYPE_TOKEN_ARRAY,
    DATA_TYPE_TOKEN_FUNCTION,
  } type;

  Token token;
  struct ArrayDataTypeToken types;

  union {
    struct
    {
      int count;
      struct _DATA_TYPE_TOKEN* type;
    } array;

    struct
    {
      struct _DATA_TYPE_TOKEN* return_value;
      struct ArrayDataTypeToken parameters;
    } function;
  };
} DataTypeToken;

void lexer_init(char* source, void (*error_callback)(int start_line, int start_column, int end_line,
                                                     int end_column, const char* message));
int lexer_errors(void);
void lexer_print(void);
ArrayToken lexer_scan(void);

#endif

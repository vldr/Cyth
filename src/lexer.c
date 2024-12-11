#include "lexer.h"
#include "array.h"
#include "main.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

#define KEYWORD_GROUP(c)                                                                           \
  break;                                                                                           \
  case c:

#define KEYWORD(keyword, token)                                                                    \
  {                                                                                                \
    size_t input_length = lexer.current - lexer.start;                                             \
    size_t keyword_length = sizeof(keyword) - 1;                                                   \
    if (input_length == keyword_length && memcmp(keyword, lexer.start, input_length) == 0)         \
    {                                                                                              \
      type = token;                                                                                \
      break;                                                                                       \
    }                                                                                              \
  }

static struct
{
  int start_line;
  int start_column;
  int current_line;
  int current_column;

  int multi_line;

  const char* start;
  const char* current;

  enum
  {
    INDENTATION_NONE,
    INDENTATION_TAB,
    INDENTATION_SPACE
  } indentation_type;
  ArrayInt indentation;
  ArrayToken tokens;
} lexer;

static void add_custom_token(TokenType type, const char* lexeme, int length)
{
  Token token;
  token.type = type;
  token.start_line = lexer.start_line;
  token.start_column = lexer.start_column;
  token.end_line = lexer.current_line;
  token.end_column = lexer.current_column;
  token.lexeme = memory_strldup(&memory, lexeme, length);

  array_add(&lexer.tokens, token);
}

static void add_token(TokenType type)
{
  add_custom_token(type, lexer.start, (int)(lexer.current - lexer.start));
}

static bool eof(void)
{
  return *lexer.current == '\0';
}

static void newline(void)
{
  lexer.current_column = 1;
  lexer.current_line++;
}

static char advance(void)
{
  lexer.current_column++;
  return *lexer.current++;
}

static char peek(void)
{
  return *lexer.current;
}

static char peek_next(void)
{
  if (eof())
    return '\0';

  return *(lexer.current + 1);
}

static bool match(char c)
{
  if (peek() == c)
  {
    advance();
    return true;
  }

  return false;
}

static void string(void)
{
  while (peek() != '"')
  {
    if (peek() == '\n')
    {
      advance();
      newline();
      continue;
    }

    if (peek() == '\0')
    {
      report_error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                   "Unterminated string");
      return;
    }

    advance();
  }

  add_custom_token(TOKEN_STRING, lexer.start + 1, (int)(lexer.current - lexer.start - 1));
  advance();
}

static void number(void)
{
  TokenType type = TOKEN_INTEGER;

  while (isdigit(peek()))
    advance();

  if (peek() == '.' && isdigit(peek_next()))
  {
    advance();

    while (isdigit(peek()))
      advance();

    type = TOKEN_FLOAT;
  }

  add_token(type);
}

static void literal(void)
{
  while (isalnum(peek()) || peek() == '_')
    advance();

  TokenType type = TOKEN_IDENTIFIER;

  switch (lexer.start[0])
  {
  default:
    KEYWORD_GROUP('a')
    KEYWORD("and", TOKEN_AND)
    KEYWORD_GROUP('b')
    KEYWORD("bool", TOKEN_IDENTIFIER_BOOL)
    KEYWORD_GROUP('c')
    KEYWORD("class", TOKEN_CLASS)
    KEYWORD_GROUP('e')
    KEYWORD("else", TOKEN_ELSE)

    KEYWORD_GROUP('f')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('a')
      KEYWORD("false", TOKEN_FALSE)
      KEYWORD_GROUP('o')
      KEYWORD("for", TOKEN_FOR)
      KEYWORD_GROUP('l')
      KEYWORD("float", TOKEN_IDENTIFIER_FLOAT)
    }

    KEYWORD_GROUP('o')
    KEYWORD("or", TOKEN_OR)
    KEYWORD_GROUP('r')
    KEYWORD("return", TOKEN_RETURN)

    KEYWORD_GROUP('s')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('u')
      KEYWORD("super", TOKEN_SUPER)
      KEYWORD_GROUP('t')
      KEYWORD("string", TOKEN_IDENTIFIER_STRING)
    }

    KEYWORD_GROUP('w')
    KEYWORD("while", TOKEN_WHILE)

    KEYWORD_GROUP('i')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('f')
      KEYWORD("if", TOKEN_IF)
      KEYWORD_GROUP('n')
      KEYWORD("in", TOKEN_IN)
      KEYWORD("int", TOKEN_IDENTIFIER_INT)
    }

    KEYWORD_GROUP('n')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('u')
      KEYWORD("null", TOKEN_NULL)
      KEYWORD_GROUP('o')
      KEYWORD("not", TOKEN_NOT)
    }

    KEYWORD_GROUP('t')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('h')
      KEYWORD("this", TOKEN_THIS)
      KEYWORD_GROUP('r')
      KEYWORD("true", TOKEN_TRUE)
    }

    KEYWORD_GROUP('v')
    KEYWORD("void", TOKEN_IDENTIFIER_VOID)
  }

  add_token(type);
}

static void comment(void)
{
  while (peek() != '\n' && peek() != '\0')
    advance();
}

static void scan_token(void)
{
  char c = advance();

  switch (c)
  {
  case '(':
    lexer.multi_line++;

    add_token(TOKEN_LEFT_PAREN);
    break;
  case ')':
    lexer.multi_line--;

    add_token(TOKEN_RIGHT_PAREN);
    break;
  case '{':
    lexer.multi_line++;

    add_token(TOKEN_LEFT_BRACE);
    break;
  case '}':
    lexer.multi_line--;

    add_token(TOKEN_RIGHT_BRACE);
    break;
  case '[':
    lexer.multi_line++;

    add_token(TOKEN_LEFT_BRACKET);
    break;
  case ']':
    lexer.multi_line--;

    add_token(TOKEN_RIGHT_BRACKET);
    break;

  case ',':
    add_token(TOKEN_COMMA);
    break;
  case '.':
    add_token(TOKEN_DOT);
    break;
  case ':':
    add_token(TOKEN_COLON);
    break;
  case ';':
    add_token(TOKEN_SEMICOLON);
    break;

  case '+':
    if (match('+'))
      add_token(TOKEN_PLUS_PLUS);
    else if (match('='))
      add_token(TOKEN_PLUS_EQUAL);
    else
      add_token(TOKEN_PLUS);

    break;
  case '-':
    if (match('-'))
      add_token(TOKEN_MINUS_MINUS);
    else if (match('='))
      add_token(TOKEN_MINUS_EQUAL);
    else
      add_token(TOKEN_MINUS);

    break;
  case '/':
    if (match('='))
      add_token(TOKEN_SLASH_EQUAL);
    else
      add_token(TOKEN_SLASH);

    break;
  case '%':
    add_token(match('=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
    break;
  case '*':
    add_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
    break;
  case '!':
    add_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    break;
  case '=':
    add_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    break;
  case '<':
    add_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    break;
  case '>':
    add_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    break;

  case '#':
    comment();
    break;

  case '"':
    string();
    break;

  case ' ':
  case '\t':
  case '\r':
    break;

  case '\n':
    if (!lexer.multi_line)
      add_custom_token(TOKEN_NEWLINE, "\\n", sizeof("\\n") - 1);

    newline();
    break;

  default:
    if (isdigit(c))
    {
      number();
      break;
    }
    else if (isalpha(c) || c == '_' || c == '#')
    {
      literal();
      break;
    }

    report_error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                 "Unexpected character");
    break;
  }
}

static void scan_indentation(void)
{
  if (lexer.multi_line || lexer.current_column != 1)
    return;

  int indentation = 0;

  while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n' || peek() == '#')
  {
    switch (peek())
    {
    case '#':
      advance();
      comment();

      break;
    case '\n':
      indentation = 0;

      advance();
      newline();
      break;
    case ' ':
      indentation += 1;
      lexer.indentation_type |= INDENTATION_SPACE;

      advance();
      break;
    case '\t':
      indentation += 4;
      lexer.indentation_type |= INDENTATION_TAB;

      advance();
      break;
    default:
      advance();
      break;
    }
  }

  if (eof())
  {
    return;
  }

  if ((lexer.indentation_type & INDENTATION_SPACE) && (lexer.indentation_type & INDENTATION_TAB))
  {
    report_error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                 "Mixing of tabs and spaces");
    lexer.indentation_type = INDENTATION_NONE;
  }

  if (indentation > array_last(&lexer.indentation))
  {
    array_add(&lexer.indentation, indentation);
    add_custom_token(TOKEN_INDENT, lexer.start + 1, (int)(lexer.current - lexer.start - 1));
  }
  else if (indentation < array_last(&lexer.indentation))
  {
    while (array_last(&lexer.indentation) > indentation)
    {
      add_custom_token(TOKEN_DEDENT, NULL, 0);
      array_del_last(&lexer.indentation);
    }

    if (indentation != array_last(&lexer.indentation))
    {
      report_error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                   "Unexpected deindent");
    }
  }
}

void lexer_init(const char* source)
{
  lexer.start = source;
  lexer.current = source;
  lexer.start_line = 1;
  lexer.start_column = 1;
  lexer.current_line = lexer.start_line;
  lexer.current_column = lexer.start_column;
  lexer.multi_line = 0;
  lexer.indentation_type = INDENTATION_NONE;

  array_init(&lexer.tokens);
  array_init(&lexer.indentation);
  array_add(&lexer.indentation, 0);
}

ArrayToken lexer_scan(void)
{
  for (;;)
  {
    scan_indentation();

    if (eof())
      break;

    lexer.start = lexer.current;
    lexer.start_line = lexer.current_line;
    lexer.start_column = lexer.current_column;

    scan_token();
  }

  if (lexer.multi_line)
  {
    report_error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                 "Reached end-of-file in multi-line mode");
  }

  if (array_size(&lexer.tokens) && array_last(&lexer.tokens).type != TOKEN_NEWLINE)
  {
    add_custom_token(TOKEN_NEWLINE, "\\n", sizeof("\\n") - 1);
  }

  while (array_last(&lexer.indentation))
  {
    add_custom_token(TOKEN_DEDENT, NULL, 0);
    array_del_last(&lexer.indentation);
  }

  add_custom_token(TOKEN_EOF, NULL, 0);

  return lexer.tokens;
}

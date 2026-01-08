#include "lexer.h"
#include "array.h"

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

  char* start;
  char* current;

  bool error;
  int errors;
  void (*error_callback)(int start_line, int start_column, int end_line, int end_column,
                         const char* message);

  enum
  {
    INDENTATION_NONE,
    INDENTATION_TAB,
    INDENTATION_SPACE
  } indentation_type;
  ArrayInt indentation;
  ArrayToken tokens;
} lexer;

static void error(int start_line, int start_column, int end_line, int end_column,
                  const char* message)
{
  lexer.error = true;
  lexer.errors++;

  if (lexer.error_callback)
    lexer.error_callback(start_line, start_column, end_line, end_column, message);
}

static void add_custom_token(TokenKind type, const char* lexeme, int length)
{
  Token token;
  token.type = type;
  token.start_line = lexer.start_line;
  token.start_column = lexer.start_column;
  token.end_line = lexer.current_line;
  token.end_column = lexer.current_column;
  token.length = length;
  token.lexeme = memory_strldup(lexeme, length);

  array_add(&lexer.tokens, token);
}

static void add_token(TokenKind type)
{
  add_custom_token(type, lexer.start, (int)(lexer.current - lexer.start));
}

static int hex_char_to_int(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return -1;
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

static char consume(void)
{
  return *lexer.current++;
}

static char advance(void)
{
  lexer.current_column++;
  return consume();
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

static void text(TokenKind token_type, char terminator)
{
  bool escaping = false;
  int shifts = 0;

  while (true)
  {
    if (peek() == '\n')
    {
      advance();
      newline();
      continue;
    }

    if (peek() == '\0')
    {
      error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
            "Missing terminating character.");
      return;
    }

    if (escaping)
    {
      char character;

      switch (peek())
      {
      case 'n':
        character = '\n';
        break;
      case 'b':
        character = '\b';
        break;
      case 't':
        character = '\t';
        break;
      case 'r':
        character = '\r';
        break;
      case 'f':
        character = '\f';
        break;
      case '\'':
        character = '\'';
        break;
      case '\"':
        character = '\"';
        break;
      case '\\':
        character = '\\';
        break;
      case '0':
        character = '\0';
        break;
      case 'x':
        advance();

        int first = hex_char_to_int(peek());
        if (first == -1)
        {
          error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
                "Expected hexadecimal digit after '\\x'.");
          return;
        }

        int second = hex_char_to_int(peek_next());
        if (second == -1)
        {
          character = (char)first;
          shifts += 1;
        }
        else
        {
          advance();

          character = (char)(first << 4 | second);
          shifts += 2;
        }

        break;
      default:
        error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
              "Invalid escape character.");
        return;
      }

      shifts++;
      escaping = false;

      *(lexer.current - shifts) = character;
    }
    else if (peek() == '\\')
    {
      escaping = true;
    }
    else if (peek() == terminator)
    {
      break;
    }
    else if (shifts)
    {
      *(lexer.current - shifts) = peek();
    }

    advance();
  }

  advance();
  add_custom_token(token_type, lexer.start + 1, (int)(lexer.current - lexer.start - shifts - 2));
}

static void hex(void)
{
  advance();

  while (isxdigit(peek()))
    advance();

  int length = (int)(lexer.current - lexer.start - 2);
  if (!length)
  {
    error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
          "Invalid hexadecimal literal.");
    return;
  }

  add_custom_token(TOKEN_HEX_INTEGER, lexer.start + 2, length);
}

static void number(void)
{
  TokenKind type = TOKEN_INTEGER;

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

  TokenKind type = TOKEN_IDENTIFIER;

  switch (lexer.start[0])
  {
  default:
    KEYWORD_GROUP('a')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('n')
      switch (lexer.start[2])
      {
      default:
        KEYWORD_GROUP('d')
        KEYWORD("and", TOKEN_AND)

        KEYWORD_GROUP('y')
        KEYWORD("any", TOKEN_IDENTIFIER_ANY)
      }
    }

    KEYWORD_GROUP('b')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('o')
      KEYWORD("bool", TOKEN_IDENTIFIER_BOOL)

      KEYWORD_GROUP('r')
      KEYWORD("break", TOKEN_BREAK)
    }

    KEYWORD_GROUP('c')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('l')
      KEYWORD("class", TOKEN_CLASS)

      KEYWORD_GROUP('o')
      KEYWORD("continue", TOKEN_CONTINUE)

      KEYWORD_GROUP('h')
      KEYWORD("char", TOKEN_IDENTIFIER_CHAR)
    }

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

    KEYWORD_GROUP('i')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('f')
      KEYWORD("if", TOKEN_IF)

      KEYWORD_GROUP('m')
      KEYWORD("import", TOKEN_IMPORT)

      KEYWORD_GROUP('n')
      switch (lexer.start[2])
      {
      default:
        KEYWORD("in", TOKEN_IN)

        KEYWORD_GROUP('t')
        KEYWORD("int", TOKEN_IDENTIFIER_INT)

        KEYWORD_GROUP('f')
        KEYWORD("inf", TOKEN_INFINITY)
      }

      KEYWORD_GROUP('s')
      KEYWORD("is", TOKEN_IS)
    }

    KEYWORD_GROUP('n')
    switch (lexer.start[1])
    {
    default:
      KEYWORD_GROUP('u')
      KEYWORD("null", TOKEN_NULL)

      KEYWORD_GROUP('o')
      KEYWORD("not", TOKEN_NOT)

      KEYWORD_GROUP('a')
      KEYWORD("nan", TOKEN_NAN)
    }

    KEYWORD_GROUP('o')
    KEYWORD("or", TOKEN_OR)

    KEYWORD_GROUP('r')
    KEYWORD("return", TOKEN_RETURN)

    KEYWORD_GROUP('s')
    KEYWORD("string", TOKEN_IDENTIFIER_STRING)

    KEYWORD_GROUP('t')
    KEYWORD("true", TOKEN_TRUE)

    KEYWORD_GROUP('v')
    KEYWORD("void", TOKEN_IDENTIFIER_VOID)

    KEYWORD_GROUP('w')
    KEYWORD("while", TOKEN_WHILE)
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
    if (lexer.multi_line < 0)
    {
      error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
            "Unexpected ')' character.");
    }

    add_token(TOKEN_RIGHT_PAREN);
    break;
  case '{':
    lexer.multi_line++;

    add_token(TOKEN_LEFT_BRACE);
    break;
  case '}':
    lexer.multi_line--;
    if (lexer.multi_line < 0)
    {
      error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
            "Unexpected '}' character.");
    }

    add_token(TOKEN_RIGHT_BRACE);
    break;
  case '[':
    lexer.multi_line++;

    add_token(TOKEN_LEFT_BRACKET);
    break;
  case ']':
    lexer.multi_line--;
    if (lexer.multi_line < 0)
    {
      error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
            "Unexpected ']' character.");
    }

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
  case '?':
    add_token(TOKEN_QUESTION);
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
    if (match('<'))
      add_token(match('=') ? TOKEN_LESS_LESS_EQUAL : TOKEN_LESS_LESS);
    else if (match('='))
      add_token(TOKEN_LESS_EQUAL);
    else
      add_token(TOKEN_LESS);
    break;
  case '>':
    if (match('>'))
      add_token(match('=') ? TOKEN_GREATER_GREATER_EQUAL : TOKEN_GREATER_GREATER);
    else if (match('='))
      add_token(TOKEN_GREATER_EQUAL);
    else
      add_token(TOKEN_GREATER);
    break;

  case '~':
    add_token(TOKEN_TILDE);
    break;
  case '&':
    add_token(match('=') ? TOKEN_AMPERSAND_EQUAL : TOKEN_AMPERSAND);
    break;
  case '|':
    add_token(match('=') ? TOKEN_PIPE_EQUAL : TOKEN_PIPE);
    break;
  case '^':
    add_token(match('=') ? TOKEN_CARET_EQUAL : TOKEN_CARET);
    break;

  case '#':
    comment();
    break;

  case '"':
    text(TOKEN_STRING, c);
    break;

  case '\'':
    text(TOKEN_CHAR, c);
    break;

  case ' ':
  case '\t':
    break;

  case '\r':
  case '\n':
    if (c == '\r')
    {
      if (peek() != '\n')
        break;

      c = consume();
    }

    if (!lexer.multi_line)
      add_custom_token(TOKEN_NEWLINE, "\\n", sizeof("\\n") - 1);

    newline();
    break;

  default:
    if (isdigit(c))
    {
      if (peek() == 'x')
        hex();
      else
        number();

      break;
    }
    else if (isalpha(c) || c == '_' || c == '#')
    {
      literal();
      break;
    }

    error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
          "Unexpected character.");
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
    lexer.indentation_type = INDENTATION_NONE;
    error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
          "Mixing of tabs and spaces.");
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
      error(lexer.start_line, lexer.start_column, lexer.current_line, lexer.current_column,
            "Unexpected deindent.");
    }
  }
}

void lexer_init(char* source, void (*error_callback)(int start_line, int start_column, int end_line,
                                                     int end_column, const char* message))
{

  lexer.start = source;
  lexer.current = source;
  lexer.start_line = 1;
  lexer.start_column = 1;
  lexer.current_line = lexer.start_line;
  lexer.current_column = lexer.start_column;
  lexer.multi_line = 0;
  lexer.indentation_type = INDENTATION_NONE;
  lexer.errors = 0;
  lexer.error = false;
  lexer.error_callback = error_callback;

  array_init(&lexer.tokens);
  array_init(&lexer.indentation);
  array_add(&lexer.indentation, 0);
}

int lexer_errors(void)
{
  return lexer.errors;
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

void lexer_print(void)
{
  Token token;
  array_foreach(&lexer.tokens, token)
  {
    static const char* types[] = {
      "TOKEN_NONE",

      "TOKEN_INDENT",
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
      "TOKEN_QUESTION",

      "TOKEN_BANG",
      "TOKEN_BANG_EQUAL",
      "TOKEN_EQUAL",
      "TOKEN_EQUAL_EQUAL",
      "TOKEN_GREATER",
      "TOKEN_GREATER_EQUAL",
      "TOKEN_LESS",
      "TOKEN_LESS_EQUAL",

      "TOKEN_IDENTIFIER",
      "TOKEN_IDENTIFIER_VOID",
      "TOKEN_IDENTIFIER_INT",
      "TOKEN_IDENTIFIER_FLOAT",
      "TOKEN_IDENTIFIER_BOOL",
      "TOKEN_IDENTIFIER_CHAR",
      "TOKEN_IDENTIFIER_STRING",
      "TOKEN_CHAR",
      "TOKEN_STRING",
      "TOKEN_INTEGER",
      "TOKEN_HEX_INTEGER",
      "TOKEN_FLOAT",

      "TOKEN_AND",
      "TOKEN_CLASS",
      "TOKEN_ELSE",
      "TOKEN_FALSE",
      "TOKEN_FOR",
      "TOKEN_IF",
      "TOKEN_IN",
      "TOKEN_IS",
      "TOKEN_NULL",
      "TOKEN_OR",
      "TOKEN_NOT",
      "TOKEN_RETURN",
      "TOKEN_TRUE",
      "TOKEN_WHILE",
      "TOKEN_CONTINUE",
      "TOKEN_BREAK",
      "TOKEN_IMPORT",
      "TOKEN_NAN",
      "TOKEN_INFINITY",

      "TOKEN_EOF",
    };

    printf("%d,%d-%d,%d \t%s    \t'%s'  \n", token.start_line, token.start_column, token.end_line,
           token.end_column, types[token.type], token.lexeme);
  }
}

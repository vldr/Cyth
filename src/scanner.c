#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

struct
{
  const char* start;
  const char* current;
  int line;
} scanner;

static bool eof()
{
  return *scanner.current == '\0';
}

static Token makeToken(TokenType type)
{
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;

  return token;
}

static Token errorToken(const char* message)
{
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;

  return token;
}

static char advance()
{
  return *scanner.current++;
}

static char peek()
{
  return *scanner.current;
}

static char peekNext()
{
  if (eof())
    return '\0';

  return *(scanner.current + 1);
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

static void skipWhitespace()
{
  for (;;)
  {
    switch (peek())
    {
    case '/':
      if (peekNext() == '/')
        while (peek() != '\n' && !eof())
          advance();
      else
        return;

      break;
    case '\n':
      scanner.line++;
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    default:
      return;
    }
  }
}

static Token string()
{
  while (peek() != '"' && !eof())
  {
    if (peek() == '\n')
      scanner.line++;

    advance();
  }

  if (eof())
  {
    return errorToken("Unterminated string.");
  }

  advance();
  return makeToken(TOKEN_STRING);
}

static Token number()
{
  while (isdigit(peek()))
    advance();

  if (peek() == '.' && isdigit(peekNext()))
  {
    advance();

    while (isdigit(peek()))
      advance();
  }

  return makeToken(TOKEN_NUMBER);
}

static TokenType identifierType()
{
  struct
  {
    const char* lexeme;
    int size;
    TokenType type;
  } keywords[] = {{"and", sizeof("and"), TOKEN_AND},
                  {"class", sizeof("class"), TOKEN_CLASS},
                  {"else", sizeof("else"), TOKEN_ELSE},
                  {"false", sizeof("false"), TOKEN_FALSE},
                  {"for", sizeof("for"), TOKEN_FOR},
                  {"fun", sizeof("fun"), TOKEN_FUN},
                  {"if", sizeof("if"), TOKEN_IF},
                  {"nil", sizeof("nil"), TOKEN_NIL},
                  {"or", sizeof("or"), TOKEN_OR},
                  {"print", sizeof("print"), TOKEN_PRINT},
                  {"return", sizeof("return"), TOKEN_RETURN},
                  {"super", sizeof("super"), TOKEN_SUPER},
                  {"this", sizeof("this"), TOKEN_THIS},
                  {"true", sizeof("true"), TOKEN_TRUE},
                  {"var", sizeof("var"), TOKEN_VAR},
                  {"while", sizeof("while"), TOKEN_WHILE}};

  for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
  {
    if (scanner.current - scanner.start != keywords[i].size - 1)
      continue;

    if (memcmp(scanner.start, keywords[i].lexeme, keywords[i].size - 1) == 0)
      return keywords[i].type;
  }

  return TOKEN_IDENTIFIER;
}

static Token literal()
{
  while (isalnum(peek()) || peek() == '_')
    advance();

  return makeToken(identifierType());
}

void initScanner(const char* source)
{
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

Token scanToken()
{
  skipWhitespace();

  scanner.start = scanner.current;

  if (eof())
    return makeToken(TOKEN_EOF);

  char c = advance();

  switch (c)
  {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE);
  case ';':
    return makeToken(TOKEN_SEMICOLON);
  case ',':
    return makeToken(TOKEN_COMMA);
  case '.':
    return makeToken(TOKEN_DOT);
  case '-':
    return makeToken(TOKEN_MINUS);
  case '+':
    return makeToken(TOKEN_PLUS);
  case '/':
    return makeToken(TOKEN_SLASH);
  case '*':
    return makeToken(TOKEN_STAR);
  case '!':
    return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"':
    return string();
  default:
    if (isdigit(c))
    {
      return number();
    }
    else if (isalpha(c) || c == '_' || c == '#')
    {
      return literal();
    }

    break;
  }

  return errorToken("Unexpected character.");
}

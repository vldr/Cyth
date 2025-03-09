#include "parser.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "statement.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static Expr* prefix_unary(void);
static Expr* expression(void);
static Stmt* statement(void);
static ArrayStmt statements(void);

static struct
{
  bool error;
  int current;
  unsigned int classes;
  ArrayToken tokens;
} parser;

static void parser_error(Token token, const char* message)
{
  if (!parser.error)
  {
    error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  }

  parser.error = true;
}

static Token peek(void)
{
  return array_at(&parser.tokens, parser.current);
}

static Token peek_offset(int offset)
{
  return array_at(&parser.tokens, parser.current + offset);
}

static Token previous(void)
{
  return array_at(&parser.tokens, parser.current - 1);
}

static bool check(TokenType type)
{
  return peek().type == type;
}

static bool eof(void)
{
  return peek().type == TOKEN_EOF;
}

static Token advance(void)
{
  if (!eof())
    parser.current++;

  return previous();
}

static bool match(TokenType type)
{
  if (check(type))
  {
    advance();
    return true;
  }

  return false;
}

static Token consume(TokenType type, const char* message)
{
  if (!check(type))
  {
    parser_error(peek(), message);
  }

  return advance();
}

static bool is_data_type(int* offset, int* count)
{
  switch (peek().type)
  {
  case TOKEN_IDENTIFIER:
  case TOKEN_IDENTIFIER_VOID:
  case TOKEN_IDENTIFIER_CHAR:
  case TOKEN_IDENTIFIER_INT:
  case TOKEN_IDENTIFIER_FLOAT:
  case TOKEN_IDENTIFIER_BOOL:
  case TOKEN_IDENTIFIER_STRING:
    break;
  default:
    return false;
  }

  *offset = 1;
  *count = 0;

  while (peek_offset(*offset).type == TOKEN_LEFT_BRACKET)
  {
    if (peek_offset(*offset + 1).type != TOKEN_RIGHT_BRACKET)
    {
      return false;
    }

    *offset += 2;
    *count += 1;
  }

  return true;
}

static bool is_data_type_and_identifier(void)
{
  int offset;
  int count;

  return is_data_type(&offset, &count) && peek_offset(offset).type == TOKEN_IDENTIFIER;
}

static bool is_data_type_and_right_paren(void)
{
  int offset;
  int count;

  return is_data_type(&offset, &count) && peek_offset(offset).type == TOKEN_RIGHT_PAREN;
}

static DataTypeToken consume_data_type(const char* message)
{
  int offset;
  int count;

  if (!is_data_type(&offset, &count))
  {
    parser_error(peek(), message);

    DataTypeToken token = { .token = { .type = TOKEN_NONE }, .count = 0 };
    return token;
  }

  DataTypeToken token = { .token = peek(), .count = count };
  parser.current += offset;

  return token;
}

static void synchronize(void)
{
  while (!eof())
  {
    if (match(TOKEN_INDENT))
    {
      while (!eof() && peek().type != TOKEN_DEDENT)
        advance();

      match(TOKEN_DEDENT);
    }

    if (previous().type == TOKEN_NEWLINE)
      return;

    switch (peek().type)
    {
    case TOKEN_CLASS:
    case TOKEN_FOR:
    case TOKEN_WHILE:
    case TOKEN_IF:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
    case TOKEN_RETURN:
      return;
    default:
      advance();
    }
  }
}

static Expr* primary(void)
{
  Expr* expr = EXPR();
  Token token = peek();

  switch (token.type)
  {
  case TOKEN_TRUE:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_BOOL);
    expr->literal.boolean = true;

    break;
  case TOKEN_FALSE:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_BOOL);
    expr->literal.boolean = false;

    break;
  case TOKEN_NULL:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_OBJECT);

    break;
  case TOKEN_INTEGER:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_INTEGER);
    expr->literal.integer = strtoul(token.lexeme, NULL, 10);

    break;
  case TOKEN_FLOAT:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_FLOAT);
    expr->literal.floating = (float)strtod(token.lexeme, NULL);

    break;
  case TOKEN_CHAR:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_CHAR);
    expr->literal.string = token.lexeme;

    if (strlen(expr->literal.string) > 1)
      parser_error(token, "Character constant cannot have multiple characters.");
    else if (strlen(expr->literal.string) == 0)
      parser_error(token, "Character constant cannot be empty.");

    break;
  case TOKEN_STRING:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_STRING);
    expr->literal.string = token.lexeme;

    break;
  case TOKEN_LEFT_PAREN:
    advance();

    if (is_data_type_and_right_paren())
    {
      DataTypeToken type = consume_data_type("Expected a type.");
      consume(TOKEN_RIGHT_PAREN, "Expected a ')' after type.");

      expr->type = EXPR_CAST;
      expr->cast.expr = prefix_unary();
      expr->cast.type = type;
      expr->cast.to_data_type = DATA_TYPE(TYPE_VOID);
      expr->cast.from_data_type = DATA_TYPE(TYPE_VOID);
    }
    else
    {
      expr->type = EXPR_GROUP;
      expr->group.expr = expression();

      consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }

    break;
  case TOKEN_LEFT_BRACKET:
    advance();

    ArrayToken tokens;
    ArrayExpr values;
    array_init(&tokens);
    array_init(&values);

    if (!check(TOKEN_RIGHT_BRACKET))
    {
      do
      {
        Token start_token = peek();
        Expr* value = expression();
        Token end_token = previous();

        Token token = {
          .type = TOKEN_IDENTIFIER,
          .start_line = start_token.start_line,
          .start_column = start_token.start_column,
          .end_line = end_token.end_line,
          .end_column = end_token.end_column,
          .lexeme = "",
        };

        array_add(&values, value);
        array_add(&tokens, token);
      } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expected ']' at the end of list.");

    expr->type = EXPR_ARRAY;
    expr->array.values = values;
    expr->array.tokens = tokens;

    break;
  case TOKEN_IDENTIFIER:
    advance();

    expr->type = EXPR_VAR;
    expr->var.name = token;
    expr->var.variable = NULL;

    break;
  case TOKEN_INFINITY:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_FLOAT);
    expr->literal.floating = INFINITY;

    break;
  case TOKEN_NAN:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_FLOAT);
    expr->literal.floating = NAN;

    break;
  default:
    parser_error(token, "Expected an expression.");
    break;
  }

  return expr;
}

static Expr* call(void)
{
  Token start_token = peek();
  Expr* expr = primary();

  for (;;)
  {
    Token end_token = previous();

    ArrayDataTypeToken types;
    array_init(&types);

    if (check(TOKEN_LESS))
    {
      bool error = false;
      int current = parser.current;

      advance();

      do
      {
        int offset;
        int count;

        if (is_data_type(&offset, &count))
        {
          DataTypeToken token = { .token = peek(), .count = count };
          parser.current += offset;

          array_add(&types, token);
        }
        else
        {
          error = true;
        }

      } while (match(TOKEN_COMMA));

      if (!match(TOKEN_GREATER))
      {
        error = true;
      }

      if (error)
      {
        array_clear(&types);
        parser.current = current;
      }
    }

    if (match(TOKEN_LEFT_PAREN))
    {
      ArrayExpr arguments;
      array_init(&arguments);

      if (!check(TOKEN_RIGHT_PAREN))
      {
        do
        {
          array_add(&arguments, expression());
        } while (match(TOKEN_COMMA));
      }

      consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

      Expr* call = EXPR();
      call->type = EXPR_CALL;
      call->call.arguments = arguments;
      call->call.types = types;
      call->call.callee = expr;
      call->call.callee_token = (Token){
        TOKEN_IDENTIFIER,   start_token.start_line, start_token.start_column,
        end_token.end_line, end_token.end_column,   "",
      };

      expr = call;
    }
    else if (match(TOKEN_DOT))
    {
      Expr* access = EXPR();
      access->type = EXPR_ACCESS;
      access->access.name = consume(TOKEN_IDENTIFIER, "Expected an identifier.");
      access->access.variable = NULL;
      access->access.expr = expr;
      access->access.expr_token = (Token){
        TOKEN_IDENTIFIER,   start_token.start_line, start_token.start_column,
        end_token.end_line, end_token.end_column,   "",
      };

      expr = access;
    }
    else if (match(TOKEN_LEFT_BRACKET))
    {
      Token start_index_token = peek();
      Expr* index_expr = expression();
      Token end_index_token = previous();

      consume(TOKEN_RIGHT_BRACKET, "Expected ']' after index.");

      Expr* index = EXPR();
      index->type = EXPR_INDEX;
      index->index.index = index_expr;
      index->index.index_token = (Token){
        TOKEN_IDENTIFIER,         start_index_token.start_line, start_index_token.start_column,
        end_index_token.end_line, end_index_token.end_column,   "",
      };

      index->index.expr = expr;
      index->index.expr_token = (Token){
        TOKEN_IDENTIFIER,   start_token.start_line, start_token.start_column,
        end_token.end_line, end_token.end_column,   "",
      };

      expr = index;
    }
    else
    {
      break;
    }
  }

  return expr;
}

static Expr* prefix_unary(void)
{
  if (match(TOKEN_BANG) || match(TOKEN_NOT) || match(TOKEN_MINUS))
  {
    Token op = previous();
    Expr* expr = prefix_unary();

    Expr* unary;
    UNARY_EXPR(unary, op, expr);

    return unary;
  }

  return call();
}

static Expr* factor(void)
{
  Expr* expr = prefix_unary();

  while (match(TOKEN_SLASH) || match(TOKEN_STAR) || match(TOKEN_PERCENT))
  {
    Token op = previous();
    Expr* right = prefix_unary();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* term(void)
{
  Expr* expr = factor();

  while (match(TOKEN_MINUS) || match(TOKEN_PLUS))
  {
    Token op = previous();
    Expr* right = factor();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* comparison(void)
{
  Expr* expr = term();

  while (match(TOKEN_GREATER) || match(TOKEN_GREATER_EQUAL) || match(TOKEN_LESS) ||
         match(TOKEN_LESS_EQUAL))
  {
    Token op = previous();
    Expr* right = term();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* equality(void)
{
  Expr* expr = comparison();

  while (match(TOKEN_BANG_EQUAL) || match(TOKEN_EQUAL_EQUAL))
  {
    Token op = previous();
    Expr* right = comparison();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* logic_and(void)
{
  Expr* expr = equality();

  while (match(TOKEN_AND))
  {
    Token op = previous();
    Expr* right = equality();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* logic_or(void)
{
  Expr* expr = logic_and();

  while (match(TOKEN_OR))
  {
    Token op = previous();
    Expr* right = logic_and();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* assignment(void)
{
  Expr* expr = logic_or();

  if (match(TOKEN_EQUAL))
  {
    Token op = previous();
    Expr* value = assignment();

    Expr* var = EXPR();
    var->type = EXPR_ASSIGN;
    var->assign.op = op;
    var->assign.target = expr;
    var->assign.value = value;
    var->assign.variable = NULL;

    return var;
  }

  return expr;
}

static Expr* expression(void)
{
  return assignment();
}

static Stmt* function_declaration_statement(DataTypeToken type, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_FUNCTION_DECL;
  stmt->func.import = TOKEN_EMPTY();
  stmt->func.type = type;
  stmt->func.name = name;

  array_init(&stmt->func.parameters);
  array_init(&stmt->func.body);
  array_init(&stmt->func.variables);

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN))
  {
    do
    {
      DataTypeToken type = consume_data_type("Expected a type after '('");
      Token name = consume(TOKEN_IDENTIFIER, "Expected a parameter name after type.");

      Stmt* parameter = STMT();
      parameter->type = STMT_VARIABLE_DECL;
      parameter->var.type = type;
      parameter->var.name = name;
      parameter->var.index = -1;
      parameter->var.initializer = NULL;

      array_add(&stmt->func.parameters, &parameter->var);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");
  consume(TOKEN_NEWLINE, "Expected a newline after ')'.");

  if (check(TOKEN_INDENT))
    stmt->func.body = statements();

  return stmt;
}

static Stmt* variable_declaration_statement(DataTypeToken type, Token name, bool newline)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_VARIABLE_DECL;
  stmt->var.type = type;
  stmt->var.name = name;

  if (match(TOKEN_EQUAL))
    stmt->var.initializer = expression();
  else
    stmt->var.initializer = NULL;

  if (newline)
    consume(TOKEN_NEWLINE, "Expected a newline after variable declaration.");
  else
    consume(TOKEN_SEMICOLON, "Expected a semicolon after variable declaration.");

  return stmt;
}

static Stmt* class_template_declaration_statement(Token keyword, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_CLASS_TEMPLATE_DECL;
  stmt->class_template.keyword = keyword;
  stmt->class_template.name = name;

  array_init(&stmt->class_template.types);
  array_init(&stmt->class_template.tokens);
  array_init(&stmt->class_template.classes);

  Token start_token = advance();

  if (!check(TOKEN_GREATER))
  {
    do
    {
      array_add(&stmt->class_template.types, consume_data_type("Expected a type."));
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_GREATER, "Expected a '>'.");

  Token end_token = previous();
  Token types_token = (Token){
    TOKEN_IDENTIFIER,   start_token.start_line, start_token.start_column,
    end_token.end_line, end_token.end_column,   "",
  };

  if (!array_size(&stmt->class_template.types))
  {
    parser_error(types_token, "The types list cannot be empty.");
  }

  consume(TOKEN_NEWLINE, "Expected a newline.");

  const int start = parser.current;

  if (check(TOKEN_INDENT))
  {
    Stmt* body_statement;
    ArrayStmt body_statements = statements();

    array_foreach(&body_statements, body_statement)
    {
      if (body_statement->type != STMT_FUNCTION_DECL && body_statement->type != STMT_VARIABLE_DECL)
      {
        parser_error(stmt->class.keyword,
                     "Only functions and variables can appear inside 'class' declarations.");
      }
    }
  }

  const int end = parser.current;

  array_add(&stmt->class_template.tokens, keyword);
  array_add(&stmt->class_template.tokens, name);
  array_add(&stmt->class_template.tokens, (Token){ .type = TOKEN_NEWLINE });

  for (int i = start; i < end; i++)
  {
    array_add(&stmt->class_template.tokens, parser.tokens.elems[i]);
  }

  return stmt;
}

static Stmt* class_declaration_statement(void)
{
  Token keyword = advance();
  Token name = consume(TOKEN_IDENTIFIER, "Expected class name.");

  if (check(TOKEN_LESS))
    return class_template_declaration_statement(keyword, name);

  Stmt* stmt = STMT();
  stmt->type = STMT_CLASS_DECL;
  stmt->class.id = parser.classes++;
  stmt->class.declared = false;
  stmt->class.keyword = keyword;
  stmt->class.name = name;

  array_init(&stmt->class.variables);
  array_init(&stmt->class.functions);

  consume(TOKEN_NEWLINE, "Expected a newline.");

  if (check(TOKEN_INDENT))
  {
    Stmt* body_statement;
    ArrayStmt body_statements = statements();

    array_foreach(&body_statements, body_statement)
    {
      if (body_statement->type == STMT_FUNCTION_DECL)
      {
        array_add(&stmt->class.functions, &body_statement->func);
      }
      else if (body_statement->type == STMT_VARIABLE_DECL)
      {
        array_add(&stmt->class.variables, &body_statement->var);
      }
      else
      {
        parser_error(stmt->class.keyword,
                     "Only functions and variables can appear inside 'class' declarations.");
      }
    }
  }

  return stmt;
}

static Stmt* import_declaration_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_IMPORT_DECL;
  stmt->import.keyword = advance();
  array_init(&stmt->import.body);

  Token import = consume(TOKEN_STRING, "Expected a string after import keyword.");
  consume(TOKEN_NEWLINE, "Expected a newline.");

  if (check(TOKEN_INDENT))
  {
    Stmt* body_statement;
    ArrayStmt body_statements = statements();

    array_foreach(&body_statements, body_statement)
    {
      if (body_statement->type == STMT_FUNCTION_DECL)
      {
        body_statement->func.import = import;
        array_add(&stmt->import.body, body_statement);
      }
      else
      {
        parser_error(stmt->import.keyword,
                     "Only function signatures can appear inside 'import' declarations.");
      }
    }
  }

  return stmt;
}

static Stmt* expression_statement(bool newline)
{
  Expr* expr = expression();

  if (newline)
    consume(TOKEN_NEWLINE, "Expected a newline after an expression.");
  else
    consume(TOKEN_SEMICOLON, "Expected a semicolon after an expression.");

  Stmt* stmt = STMT();
  stmt->type = STMT_EXPR;
  stmt->expr.expr = expr;

  return stmt;
}

static Stmt* return_statement(void)
{
  Token keyword = advance();
  Expr* expr = NULL;

  if (!match(TOKEN_NEWLINE))
  {
    expr = expression();
    consume(TOKEN_NEWLINE, "Expected a newline after 'return' statement.");
  }

  Stmt* stmt = STMT();
  stmt->type = STMT_RETURN;
  stmt->ret.expr = expr;
  stmt->ret.keyword = keyword;

  return stmt;
}

static Stmt* continue_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_CONTINUE;
  stmt->cont.keyword = advance();

  consume(TOKEN_NEWLINE, "Expected a newline after continue statement.");

  return stmt;
}

static Stmt* break_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_BREAK;
  stmt->cont.keyword = advance();

  consume(TOKEN_NEWLINE, "Expected a newline after break statement.");

  return stmt;
}

static Stmt* if_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_IF;
  stmt->cond.keyword = advance();
  stmt->cond.condition = expression();

  array_init(&stmt->cond.then_branch);
  array_init(&stmt->cond.else_branch);

  consume(TOKEN_NEWLINE, "Expected a newline after condition.");

  if (check(TOKEN_INDENT))
    stmt->cond.then_branch = statements();

  if (match(TOKEN_ELSE))
  {
    if (check(TOKEN_IF))
    {
      array_add(&stmt->cond.else_branch, if_statement());
    }
    else
    {
      consume(TOKEN_NEWLINE, "Expected a newline after else.");

      if (check(TOKEN_INDENT))
        stmt->cond.else_branch = statements();
    }
  }

  return stmt;
}

static Stmt* while_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_WHILE;
  stmt->loop.keyword = advance();
  stmt->loop.condition = expression();
  stmt->loop.initializer = NULL;
  stmt->loop.incrementer = NULL;

  array_init(&stmt->loop.body);

  consume(TOKEN_NEWLINE, "Expected a newline after condition.");

  if (check(TOKEN_INDENT))
    stmt->loop.body = statements();

  return stmt;
}

static Stmt* for_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_WHILE;
  stmt->loop.keyword = advance();

  if (match(TOKEN_SEMICOLON))
  {
    stmt->loop.initializer = NULL;
  }
  else
  {
    if (is_data_type_and_identifier())
    {
      DataTypeToken type = consume_data_type("Expected a type.");
      Token name = consume(TOKEN_IDENTIFIER, "Expected identifier after type.");

      stmt->loop.initializer = variable_declaration_statement(type, name, false);
    }
    else
    {
      stmt->loop.initializer = expression_statement(false);
    }
  }

  if (check(TOKEN_SEMICOLON))
  {
    Expr* expr = EXPR();
    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_BOOL);
    expr->literal.boolean = true;

    stmt->loop.condition = expr;
  }
  else
  {
    stmt->loop.condition = expression();
  }

  consume(TOKEN_SEMICOLON, "Expected a semicolon after condition.");

  if (!check(TOKEN_NEWLINE))
  {
    stmt->loop.incrementer = expression_statement(true);
  }
  else
  {
    stmt->loop.incrementer = NULL;
    consume(TOKEN_NEWLINE, "Expected a newline after incrementer.");
  }

  array_init(&stmt->loop.body);

  if (check(TOKEN_INDENT))
  {
    stmt->loop.body = statements();
  }

  return stmt;
}

static Stmt* statement(void)
{
  if (is_data_type_and_identifier())
  {
    DataTypeToken type = consume_data_type("Expected a type.");
    Token name = consume(TOKEN_IDENTIFIER, "Expected identifier after type.");

    Token token = peek();

    switch (token.type)
    {
    case TOKEN_LEFT_PAREN:
      return function_declaration_statement(type, name);
    default:
      return variable_declaration_statement(type, name, true);
    }
  }
  else
  {
    Token token = peek();

    switch (token.type)
    {
    case TOKEN_RETURN:
      return return_statement();
    case TOKEN_CONTINUE:
      return continue_statement();
    case TOKEN_BREAK:
      return break_statement();
    case TOKEN_IF:
      return if_statement();
    case TOKEN_WHILE:
      return while_statement();
    case TOKEN_FOR:
      return for_statement();
    case TOKEN_CLASS:
      return class_declaration_statement();
    case TOKEN_IMPORT:
      return import_declaration_statement();
    default:
      return expression_statement(true);
    }
  }
}

static ArrayStmt statements(void)
{
  ArrayStmt statements;
  array_init(&statements);

  consume(TOKEN_INDENT, "Expected an indent.");

  while (!eof() && !check(TOKEN_DEDENT))
  {
    array_add(&statements, statement());

    if (parser.error)
    {
      synchronize();

      parser.error = false;
    }
  }

  consume(TOKEN_DEDENT, "Expected a dedent.");

  return statements;
}

void parser_init(ArrayToken tokens)
{
  parser.error = false;
  parser.current = 0;
  parser.tokens = tokens;
}

ArrayStmt parser_parse(void)
{
  ArrayStmt statements;
  array_init(&statements);

  while (!eof())
  {
    array_add(&statements, statement());

    if (parser.error)
    {
      synchronize();

      parser.error = false;
    }
  }

  return statements;
}

Stmt* parser_parse_statement(void)
{
  return statement();
}

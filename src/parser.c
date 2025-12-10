#include "parser.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "statement.h"

#include <math.h>
#include <stdio.h>

static Expr* prefix_unary(void);
static Expr* expression(void);
static Stmt* statement(void);
static ArrayStmt statements(void);

static DataTypeToken data_type_array_function(bool* skip_greater_greater);

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

static Token previous(void)
{
  if (!parser.current)
    return TOKEN_EMPTY();

  return array_at(&parser.tokens, parser.current - 1);
}

static void seek(int position)
{
  parser.current = position;
}

static bool check(TokenKind type)
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

static bool match(TokenKind type)
{
  if (check(type))
  {
    advance();
    return true;
  }

  return false;
}

static Token consume(TokenKind type, const char* message)
{
  if (!check(type))
  {
    parser_error(peek(), message);
  }

  return advance();
}

static DataTypeToken data_type_primary(void)
{
  Token token = advance();

  switch (token.type)
  {
  case TOKEN_IDENTIFIER:
  case TOKEN_IDENTIFIER_VOID:
  case TOKEN_IDENTIFIER_ANY:
  case TOKEN_IDENTIFIER_CHAR:
  case TOKEN_IDENTIFIER_INT:
  case TOKEN_IDENTIFIER_FLOAT:
  case TOKEN_IDENTIFIER_BOOL:
  case TOKEN_IDENTIFIER_STRING: {
    DataTypeToken data_type_token = {
      .type = DATA_TYPE_TOKEN_PRIMITIVE,
      .token = token,
    };

    return data_type_token;
  }
  default:
    return DATA_TYPE_TOKEN_EMPTY();
  }
}

static DataTypeToken data_type_template(bool* skip_greater_greater)
{
  DataTypeToken data_type_token = data_type_primary();
  if (!data_type_token.type)
  {
    return DATA_TYPE_TOKEN_EMPTY();
  }

  if (match(TOKEN_LESS))
  {
    array_init(&data_type_token.types);

    do
    {
      DataTypeToken inner_data_type_token = data_type_array_function(skip_greater_greater);
      if (!inner_data_type_token.type)
      {
        return DATA_TYPE_TOKEN_EMPTY();
      }

      array_add(&data_type_token.types, inner_data_type_token);

    } while (match(TOKEN_COMMA));

    Token closing_tag = peek();

    if (closing_tag.type == TOKEN_GREATER)
    {
      advance();
    }
    else if (closing_tag.type == TOKEN_GREATER_GREATER && *skip_greater_greater)
    {
      advance();

      *skip_greater_greater = false;
    }
    else if (closing_tag.type == TOKEN_GREATER_GREATER && !*skip_greater_greater)
    {
      *skip_greater_greater = true;
    }
    else
    {
      return DATA_TYPE_TOKEN_EMPTY();
    }
  }

  return data_type_token;
}

static Expr* cast_to_array(DataTypeToken type, Expr* expr)
{
  while (expr->type == EXPR_GROUP)
  {
    expr = expr->group.expr;
  }

  if (expr->type == EXPR_ARRAY)
  {
    Expr* cast = EXPR();
    cast->type = EXPR_CAST;
    cast->cast.expr = expr;
    cast->cast.to_data_type = DATA_TYPE(TYPE_VOID);
    cast->cast.from_data_type = DATA_TYPE(TYPE_VOID);
    cast->cast.type = type;

    if (cast->cast.type.type != DATA_TYPE_TOKEN_ARRAY)
    {
      cast->cast.type.type = DATA_TYPE_TOKEN_ARRAY;
      cast->cast.type.array.type = ALLOC(DataTypeToken);
      *cast->cast.type.array.type = type;
      cast->cast.type.array.count = 1;
    }
    else
    {
      cast->cast.type.array.count += 1;
    }

    return cast;
  }
  else
  {
    return expr;
  }
}

static Token combine_tokens(Token start_token, Token end_token)
{
  return (Token){
    TOKEN_IDENTIFIER,
    start_token.start_line,
    start_token.start_column,
    end_token.end_line,
    end_token.end_column,
    0,
    "",
  };
}

static DataTypeToken data_type_array_function(bool* skip_greater_greater)
{
  DataTypeToken data_type_token = data_type_template(skip_greater_greater);
  if (!data_type_token.type)
  {
    return DATA_TYPE_TOKEN_EMPTY();
  }

  for (;;)
  {
    if (match(TOKEN_LEFT_BRACKET))
    {
      int count = 0;

      do
      {
        if (!match(TOKEN_RIGHT_BRACKET))
        {
          return DATA_TYPE_TOKEN_EMPTY();
        }

        count++;
      } while (match(TOKEN_LEFT_BRACKET));

      DataTypeToken* inner_data_type_token = ALLOC(DataTypeToken);
      *inner_data_type_token = data_type_token;

      data_type_token.type = DATA_TYPE_TOKEN_ARRAY;
      data_type_token.array.count = count;
      data_type_token.array.type = inner_data_type_token;
    }
    else if (match(TOKEN_LEFT_PAREN))
    {
      DataTypeToken* inner_data_type_token = ALLOC(DataTypeToken);
      *inner_data_type_token = data_type_token;

      data_type_token.type = DATA_TYPE_TOKEN_FUNCTION;
      data_type_token.function.return_value = inner_data_type_token;
      array_init(&data_type_token.function.parameters);

      if (!check(TOKEN_RIGHT_PAREN))
      {
        do
        {
          DataTypeToken parameter_data_type_token = data_type_array_function(skip_greater_greater);
          if (!parameter_data_type_token.type)
          {
            return DATA_TYPE_TOKEN_EMPTY();
          }

          array_add(&data_type_token.function.parameters, parameter_data_type_token);

        } while (match(TOKEN_COMMA));
      }

      if (!match(TOKEN_RIGHT_PAREN))
      {
        return DATA_TYPE_TOKEN_EMPTY();
      }
    }
    else
    {
      break;
    }
  }

  return data_type_token;
}

static bool is_data_type_and_identifier(void)
{
  int current = parser.current;
  bool skip_greater_greater = false;

  DataTypeToken data_type_token = data_type_array_function(&skip_greater_greater);
  Token next = advance();

  seek(current);

  return data_type_token.type && next.type == TOKEN_IDENTIFIER;
}

static bool is_data_type_and_right_paren(void)
{
  int current = parser.current;
  bool skip_greater_greater = false;

  DataTypeToken data_type_token = data_type_array_function(&skip_greater_greater);
  Token next = advance();

  seek(current);

  return data_type_token.type && next.type == TOKEN_RIGHT_PAREN;
}

static DataTypeToken consume_data_type(const char* message)
{
  int current = parser.current;
  bool skip_greater_greater = false;

  DataTypeToken data_type_token = data_type_array_function(&skip_greater_greater);
  if (!data_type_token.type)
  {
    seek(current);
    parser_error(peek(), message);
  }

  return data_type_token;
}

static ArrayDataTypeToken* consume_template_types(void)
{
  ArrayDataTypeToken* template_types = NULL;

  if (check(TOKEN_LESS))
  {
    ArrayDataTypeToken types;
    array_init(&types);

    bool error = false;
    bool right_shift = false;

    int current = parser.current;
    advance();

    do
    {
      DataTypeToken data_type_token = data_type_array_function(&right_shift);

      if (data_type_token.type)
      {
        array_add(&types, data_type_token);
      }
      else
      {
        error = true;
        break;
      }

    } while (match(TOKEN_COMMA));

    if (!match(TOKEN_GREATER) && !(right_shift && match(TOKEN_GREATER_GREATER)))
    {
      error = true;
    }

    if (error)
    {
      seek(current);
    }
    else
    {
      template_types = ALLOC(ArrayDataTypeToken);
      *template_types = types;
    }
  }

  return template_types;
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
    expr->literal.data_type = DATA_TYPE(TYPE_NULL);
    expr->literal.data_type.null_function = ALLOC(bool);
    *expr->literal.data_type.null_function = false;

    break;
  case TOKEN_INTEGER:
  case TOKEN_HEX_INTEGER:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_INTEGER);
    expr->literal.integer = strtoul(token.lexeme, NULL, token.type == TOKEN_INTEGER ? 10 : 16);

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
    expr->literal.string.data = token.lexeme;
    expr->literal.string.length = token.length;

    if (expr->literal.string.length > 1)
      parser_error(token, "Character constant cannot have multiple characters.");
    else if (expr->literal.string.length == 0)
      parser_error(token, "Character constant cannot be empty.");

    break;
  case TOKEN_STRING:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_STRING);
    expr->literal.string.data = token.lexeme;
    expr->literal.string.length = token.length;

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
  case TOKEN_LEFT_BRACKET: {
    Token start_token = advance();

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
        Token token = combine_tokens(start_token, end_token);

        array_add(&values, value);
        array_add(&tokens, token);
      } while (match(TOKEN_COMMA));
    }

    Token end_token = consume(TOKEN_RIGHT_BRACKET, "Expected ']' at the end of list.");

    expr->type = EXPR_ARRAY;
    expr->array.values = values;
    expr->array.tokens = tokens;
    expr->array.token = combine_tokens(start_token, end_token);

    break;
  }
  case TOKEN_IDENTIFIER:
    advance();

    expr->type = EXPR_VAR;
    expr->var.name = token;
    expr->var.variable = NULL;
    expr->var.template_types = consume_template_types();

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

    if (match(TOKEN_LEFT_PAREN))
    {
      ArrayToken argument_tokens;
      array_init(&argument_tokens);

      ArrayExpr arguments;
      array_init(&arguments);

      if (!check(TOKEN_RIGHT_PAREN))
      {
        do
        {
          Token start_token = peek();

          array_add(&arguments, expression());

          Token end_token = previous();
          Token argument_token = combine_tokens(start_token, end_token);

          array_add(&argument_tokens, argument_token);

        } while (match(TOKEN_COMMA));
      }

      consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

      Expr* call = EXPR();
      call->type = EXPR_CALL;
      call->call.arguments = arguments;
      call->call.argument_tokens = argument_tokens;
      call->call.callee = expr;
      call->call.callee_token = combine_tokens(start_token, end_token);

      expr = call;
    }
    else if (match(TOKEN_DOT))
    {
      Expr* access = EXPR();
      access->type = EXPR_ACCESS;
      access->access.name = consume(TOKEN_IDENTIFIER, "Expected an identifier.");
      access->access.variable = NULL;
      access->access.template_types = consume_template_types();
      access->access.expr = expr;
      access->access.expr_token = combine_tokens(start_token, end_token);

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
      index->index.index_token = combine_tokens(start_index_token, end_index_token);

      index->index.expr = expr;
      index->index.expr_token = combine_tokens(start_token, end_token);

      expr = index;
    }
    else if (match(TOKEN_IS))
    {
      Expr* is = EXPR();
      is->type = EXPR_IS;
      is->is.is_data_type_token = consume_data_type("Expected a type after 'is' keyword.");
      is->is.expr = expr;
      is->is.expr_token = (Token){
        TOKEN_IDENTIFIER,
        start_token.start_line,
        start_token.start_column,
        end_token.end_line,
        end_token.end_column,
        0,
        "",
      };

      expr = is;
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
  if (match(TOKEN_BANG) || match(TOKEN_NOT) || match(TOKEN_TILDE) || match(TOKEN_MINUS))
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

static Expr* bitwise_shift(void)
{
  Expr* expr = term();

  while (match(TOKEN_LESS_LESS) || match(TOKEN_GREATER_GREATER))
  {
    Token op = previous();
    Expr* right = term();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* comparison(void)
{
  Expr* expr = bitwise_shift();

  while (match(TOKEN_GREATER) || match(TOKEN_GREATER_EQUAL) || match(TOKEN_LESS) ||
         match(TOKEN_LESS_EQUAL))
  {
    Token op = previous();
    Expr* right = bitwise_shift();

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

static Expr* bitwise_and(void)
{
  Expr* expr = equality();

  while (match(TOKEN_AMPERSAND))
  {
    Token op = previous();
    Expr* right = equality();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* bitwise_xor(void)
{
  Expr* expr = bitwise_and();

  while (match(TOKEN_CARET))
  {
    Token op = previous();
    Expr* right = bitwise_and();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* bitwise_or(void)
{
  Expr* expr = bitwise_xor();

  while (match(TOKEN_PIPE))
  {
    Token op = previous();
    Expr* right = bitwise_xor();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* logic_and(void)
{
  Expr* expr = bitwise_or();

  while (match(TOKEN_AND))
  {
    Token op = previous();
    Expr* right = bitwise_or();

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
  int current = parser.current;
  Token start_token = peek();
  Expr* expr = logic_or();

  switch (peek().type)
  {
  case TOKEN_EQUAL:
  case TOKEN_PLUS_EQUAL:
  case TOKEN_MINUS_EQUAL:
  case TOKEN_STAR_EQUAL:
  case TOKEN_SLASH_EQUAL:
  case TOKEN_PERCENT_EQUAL:
  case TOKEN_AMPERSAND_EQUAL:
  case TOKEN_PIPE_EQUAL:
  case TOKEN_CARET_EQUAL:
  case TOKEN_LESS_LESS_EQUAL:
  case TOKEN_GREATER_GREATER_EQUAL: {
    advance();
    seek(current);

    Expr* expr_copy = logic_or();

    advance();

    Token op = previous();
    Expr* value = assignment();

    switch (op.type)
    {
    case TOKEN_PLUS_EQUAL:
      op.type = TOKEN_PLUS;
      break;
    case TOKEN_MINUS_EQUAL:
      op.type = TOKEN_MINUS;
      break;
    case TOKEN_STAR_EQUAL:
      op.type = TOKEN_STAR;
      break;
    case TOKEN_SLASH_EQUAL:
      op.type = TOKEN_SLASH;
      break;
    case TOKEN_PERCENT_EQUAL:
      op.type = TOKEN_PERCENT;
      break;

    case TOKEN_AMPERSAND_EQUAL:
      op.type = TOKEN_AMPERSAND;
      break;
    case TOKEN_PIPE_EQUAL:
      op.type = TOKEN_PIPE;
      break;
    case TOKEN_CARET_EQUAL:
      op.type = TOKEN_CARET;
      break;
    case TOKEN_LESS_LESS_EQUAL:
      op.type = TOKEN_LESS_LESS;
      break;
    case TOKEN_GREATER_GREATER_EQUAL:
      op.type = TOKEN_GREATER_GREATER;
      break;

    default:
      break;
    }

    if (op.type != TOKEN_EQUAL)
      BINARY_EXPR(value, op, expr, value);

    Expr* var = EXPR();
    var->type = EXPR_ASSIGN;
    var->assign.op = op;
    var->assign.target = expr_copy;
    var->assign.value = value;
    var->assign.variable = NULL;

    return var;
  }
  case TOKEN_QUESTION: {
    Token condition_token = combine_tokens(start_token, previous());
    start_token = advance();

    Expr* left = assignment();

    consume(TOKEN_COLON, "Expected a colon after expression.");

    Expr* right = assignment();
    Token body_token = combine_tokens(start_token, previous());

    Expr* var = EXPR();
    var->type = EXPR_IF;

    var->cond.body_token = body_token;
    var->cond.condition_token = condition_token;
    var->cond.condition = expr;
    var->cond.left = left;
    var->cond.right = right;

    return var;
  }
  default:
    break;
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
  stmt->func.type = type;
  stmt->func.name = name;
  stmt->func.name_raw = name.lexeme;
  stmt->func.import = NULL;
  stmt->func.item = NULL;
  stmt->func.proto = NULL;
  stmt->func.item_prototype = NULL;
  stmt->func.proto_prototype = NULL;

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
      parameter->var.function = NULL;
      parameter->var.scope = SCOPE_NONE;

      array_add(&stmt->func.parameters, &parameter->var);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");
  consume(TOKEN_NEWLINE, "Expected a newline after ')'.");

  if (check(TOKEN_INDENT))
    stmt->func.body = statements();

  return stmt;
}

static Stmt* function_template_declaration_statement(DataTypeToken type, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_FUNCTION_TEMPLATE_DECL;
  stmt->func_template.type = type;
  stmt->func_template.name = name;
  stmt->func_template.class = NULL;
  stmt->func_template.function = NULL;
  stmt->func_template.loop = NULL;
  stmt->func_template.cond = NULL;
  stmt->func_template.environment = NULL;
  stmt->func_template.import = NULL;

  array_init(&stmt->func_template.types);
  array_init(&stmt->func_template.parameters);
  array_init(&stmt->func_template.functions);

  Token start_token = consume(TOKEN_LESS, "Expected a '<'.");

  if (!check(TOKEN_GREATER))
  {
    do
    {
      array_add(&stmt->func_template.types, consume(TOKEN_IDENTIFIER, "Expected an identifier."));
    } while (match(TOKEN_COMMA));
  }

  Token end_token = consume(TOKEN_GREATER, "Expected a '>'.");

  if (!array_size(&stmt->func_template.types))
  {
    Token types_token = combine_tokens(start_token, end_token);

    parser_error(types_token, "The types list cannot be empty.");
  }

  stmt->func_template.count = 0;
  stmt->func_template.offset = parser.current;

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN))
  {
    do
    {
      DataTypeToken type = consume_data_type("Expected a type after '('");
      consume(TOKEN_IDENTIFIER, "Expected a parameter name after type.");

      array_add(&stmt->func_template.parameters, type);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");
  consume(TOKEN_NEWLINE, "Expected a newline after ')'.");

  if (check(TOKEN_INDENT))
    statements();

  return stmt;
}

static Stmt* variable_declaration_statement(DataTypeToken type, Token name, bool newline)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_VARIABLE_DECL;
  stmt->var.type = type;
  stmt->var.name = name;
  stmt->var.equals = peek();
  stmt->var.function = NULL;
  stmt->var.scope = SCOPE_NONE;

  if (stmt->var.equals.type == TOKEN_EQUAL)
  {
    advance();

    stmt->var.initializer = expression();
  }
  else
  {
    stmt->var.initializer = NULL;
  }

  if (newline)
    consume(TOKEN_NEWLINE, "Expected a newline after variable declaration.");

  return stmt;
}

static Stmt* class_template_declaration_statement(Token keyword, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_CLASS_TEMPLATE_DECL;
  stmt->class_template.keyword = keyword;
  stmt->class_template.name = name;

  array_init(&stmt->class_template.types);
  array_init(&stmt->class_template.classes);

  Token start_token = advance();

  if (!check(TOKEN_GREATER))
  {
    do
    {
      array_add(&stmt->class_template.types, consume(TOKEN_IDENTIFIER, "Expected an identifier."));
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_GREATER, "Expected a '>'.");

  stmt->class_template.count = 0;
  stmt->class_template.offset = parser.current;

  Token end_token = previous();
  Token types_token = combine_tokens(start_token, end_token);

  if (!array_size(&stmt->class_template.types))
  {
    parser_error(types_token, "The types list cannot be empty.");
  }

  consume(TOKEN_NEWLINE, "Expected a newline.");

  if (check(TOKEN_INDENT))
  {
    Stmt* body_statement;
    ArrayStmt body_statements = statements();

    array_foreach(&body_statements, body_statement)
    {
      if (body_statement->type != STMT_FUNCTION_DECL &&
          body_statement->type != STMT_FUNCTION_TEMPLATE_DECL &&
          body_statement->type != STMT_VARIABLE_DECL)
      {
        parser_error(keyword,
                     "Only functions and variables can appear inside 'class' declarations.");
      }
    }
  }

  return stmt;
}

static Stmt* class_declaration_statement(Token keyword, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_CLASS_DECL;
  stmt->class.keyword = keyword;
  stmt->class.name = name;
  stmt->class.ref = 0;
  stmt->class.size = 0;
  stmt->class.alignment = 1;
  stmt->class.default_constructor = ALLOC(FuncStmt);

  array_init(&stmt->class.variables);
  array_init(&stmt->class.functions);
  array_init(&stmt->class.function_templates);

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
      else if (body_statement->type == STMT_FUNCTION_TEMPLATE_DECL)
      {
        array_add(&stmt->class.function_templates, &body_statement->func_template);
      }
      else if (body_statement->type == STMT_VARIABLE_DECL)
      {
        array_add(&stmt->class.variables, &body_statement->var);
      }
      else
      {
        parser_error(keyword,
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
        body_statement->func.import = import.lexeme;
        array_add(&stmt->import.body, body_statement);
      }
      else if (body_statement->type == STMT_FUNCTION_TEMPLATE_DECL)
      {
        body_statement->func_template.import = import.lexeme;
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

  array_init(&stmt->loop.initializer);
  array_init(&stmt->loop.incrementer);
  array_init(&stmt->loop.body);

  consume(TOKEN_NEWLINE, "Expected a newline after condition.");

  if (check(TOKEN_INDENT))
    stmt->loop.body = statements();

  return stmt;
}

static Stmt* for_in_statement(DataTypeToken type, Token name, Stmt* stmt)
{
  Token start_token = peek();
  Expr* list = cast_to_array(type, expression());
  Token end_token = previous();
  Token list_token = combine_tokens(start_token, end_token);

  Stmt* counter_stmt = STMT();
  counter_stmt->type = STMT_VARIABLE_DECL;
  counter_stmt->var.type = DATA_TYPE_TOKEN_EMPTY();
  counter_stmt->var.type.type = DATA_TYPE_TOKEN_PRIMITIVE;
  counter_stmt->var.type.token = TOKEN_EMPTY();
  counter_stmt->var.type.token.type = TOKEN_IDENTIFIER_INT;
  counter_stmt->var.name = name;
  counter_stmt->var.name.lexeme = "it";
  counter_stmt->var.name.length = sizeof("it");
  counter_stmt->var.equals = TOKEN_EMPTY();
  counter_stmt->var.initializer = NULL;
  counter_stmt->var.function = NULL;
  counter_stmt->var.scope = SCOPE_NONE;

  array_add(&stmt->loop.initializer, counter_stmt);

  Stmt* list_stmt = STMT();
  list_stmt->type = STMT_VARIABLE_DECL;
  list_stmt->var.type = DATA_TYPE_TOKEN_EMPTY();
  list_stmt->var.type.token = TOKEN_EMPTY();
  list_stmt->var.name = name;
  list_stmt->var.name.lexeme = "";
  list_stmt->var.name.length = sizeof("");
  list_stmt->var.equals = TOKEN_EMPTY();
  list_stmt->var.initializer = list;
  list_stmt->var.function = NULL;
  list_stmt->var.scope = SCOPE_NONE;

  array_add(&stmt->loop.initializer, list_stmt);

  {
    Expr* counter = EXPR();
    counter->type = EXPR_VAR;
    counter->var.name = counter_stmt->var.name;
    counter->var.variable = NULL;
    counter->var.template_types = NULL;

    Expr* list = EXPR();
    list->type = EXPR_VAR;
    list->var.name = list_stmt->var.name;
    list->var.variable = NULL;
    list->var.template_types = NULL;

    Expr* element_access = EXPR();
    element_access->type = EXPR_ACCESS;
    element_access->access.name = list_token;
    element_access->access.name.lexeme = "length";
    element_access->access.name.length = sizeof("length") - 1;
    element_access->access.variable = NULL;
    element_access->access.template_types = NULL;
    element_access->access.expr = list;
    element_access->access.expr_token = list_token;

    Token op = TOKEN_EMPTY();
    op.type = TOKEN_LESS;

    BINARY_EXPR(stmt->loop.condition, op, counter, element_access);
  }

  {
    Expr* counter = EXPR();
    counter->type = EXPR_VAR;
    counter->var.name = counter_stmt->var.name;
    counter->var.variable = NULL;
    counter->var.template_types = NULL;

    Expr* constant = EXPR();
    constant->type = EXPR_LITERAL;
    constant->literal.data_type = DATA_TYPE(TYPE_INTEGER);
    constant->literal.integer = 1;

    Token op = TOKEN_EMPTY();
    op.type = TOKEN_PLUS;

    Expr* binary;
    BINARY_EXPR(binary, op, counter, constant);

    Expr* target = EXPR();
    target->type = EXPR_VAR;
    target->var.name = counter_stmt->var.name;
    target->var.variable = NULL;
    target->var.template_types = NULL;

    Expr* assignment = EXPR();
    assignment->type = EXPR_ASSIGN;
    assignment->assign.op = op;
    assignment->assign.target = target;
    assignment->assign.value = binary;
    assignment->assign.variable = NULL;

    Stmt* incrementer = STMT();
    incrementer->type = STMT_EXPR;
    incrementer->expr.expr = assignment;

    array_add(&stmt->loop.incrementer, incrementer);
  }

  consume(TOKEN_NEWLINE, "Expected a newline.");

  array_init(&stmt->loop.body);
  if (check(TOKEN_INDENT))
    stmt->loop.body = statements();

  {
    Expr* counter = EXPR();
    counter->type = EXPR_VAR;
    counter->var.name = counter_stmt->var.name;
    counter->var.variable = NULL;
    counter->var.template_types = NULL;

    Expr* list = EXPR();
    list->type = EXPR_VAR;
    list->var.name = list_stmt->var.name;
    list->var.variable = NULL;
    list->var.template_types = NULL;

    Expr* index = EXPR();
    index->type = EXPR_INDEX;
    index->index.index = counter;
    index->index.index_token = list_token;
    index->index.expr = list;
    index->index.expr_token = list_token;

    Stmt* element_stmt = STMT();
    element_stmt->type = STMT_VARIABLE_DECL;
    element_stmt->var.type = type;
    element_stmt->var.name = name;
    element_stmt->var.equals = name;
    element_stmt->var.initializer = index;
    element_stmt->var.function = NULL;
    element_stmt->var.scope = SCOPE_NONE;

    array_prepend(&stmt->loop.body, element_stmt);
  }

  return stmt;
}

static Stmt* for_statement(void)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_WHILE;
  stmt->loop.keyword = advance();

  array_init(&stmt->loop.initializer);
  array_init(&stmt->loop.incrementer);

  if (!check(TOKEN_SEMICOLON))
  {
  add_initializer:

    if (is_data_type_and_identifier())
    {
      DataTypeToken type = consume_data_type("Expected a type.");
      Token name = consume(TOKEN_IDENTIFIER, "Expected identifier after type.");

      if (match(TOKEN_IN))
      {
        return for_in_statement(type, name, stmt);
      }
      else
      {
        array_add(&stmt->loop.initializer, variable_declaration_statement(type, name, false));
      }
    }
    else
    {
      array_add(&stmt->loop.initializer, expression_statement(false));
    }

    if (match(TOKEN_COMMA))
    {
      goto add_initializer;
    }
  }

  consume(TOKEN_SEMICOLON, "Expected a semicolon after initializer.");

  if (!check(TOKEN_SEMICOLON))
  {
    stmt->loop.condition = expression();
  }
  else
  {
    Expr* expr = EXPR();
    expr->type = EXPR_LITERAL;
    expr->literal.data_type = DATA_TYPE(TYPE_BOOL);
    expr->literal.boolean = true;

    stmt->loop.condition = expr;
  }

  consume(TOKEN_SEMICOLON, "Expected a semicolon after condition.");

  if (!check(TOKEN_NEWLINE))
  {
  add_incrementer:

    array_add(&stmt->loop.incrementer, expression_statement(false));

    if (match(TOKEN_COMMA))
    {
      goto add_incrementer;
    }
  }

  consume(TOKEN_NEWLINE, "Expected a newline after incrementer.");

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
    case TOKEN_LESS:
      return function_template_declaration_statement(type, name);
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
    case TOKEN_IMPORT:
      return import_declaration_statement();
    case TOKEN_CLASS: {
      Token keyword = advance();
      Token name = consume(TOKEN_IDENTIFIER, "Expected class name.");

      if (check(TOKEN_LESS))
        return class_template_declaration_statement(keyword, name);
      else
        return class_declaration_statement(keyword, name);
    }
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
  parser.tokens = tokens;
  parser.classes = 0;

  seek(0);
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

Stmt* parser_parse_class_declaration_statement(int offset, Token keyword, Token name)
{
  seek(offset);

  return class_declaration_statement(keyword, name);
}

Stmt* parser_parse_function_declaration_statement(int offset, DataTypeToken type, Token name)
{
  seek(offset);

  return function_declaration_statement(type, name);
}

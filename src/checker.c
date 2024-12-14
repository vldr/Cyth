#include "checker.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "statement.h"

typedef struct _ENVIRONMENT
{
  MapStmt variables;

  struct _ENVIRONMENT* parent;
} Environment;

static struct
{
  bool error;
  ArrayStmt statements;

  Environment* environment;
  Stmt* function;
  Stmt* loop;
} checker;

static DataType check_expression(Expr* expression);
static void check_statement(Stmt* statement);

static Environment* environment_init(Environment* parent)
{
  Environment* environment = ALLOC(Environment);
  environment->parent = parent;
  map_init_stmt(&environment->variables, 0, 0);

  return environment;
}

static bool environment_check_variable(Environment* environment, const char* name)
{
  return map_get_stmt(&environment->variables, name) != NULL;
}

static Stmt* environment_get_variable(Environment* environment, const char* name)
{
  while (environment)
  {
    Stmt* variable = map_get_stmt(&environment->variables, name);
    if (variable)
    {
      return variable;
    }

    environment = environment->parent;
  }

  return NULL;
}

static void environment_set_variable(Environment* environment, const char* name, Stmt* variable)
{
  map_put_stmt(&environment->variables, name, variable);
}

static void error(Token token, const char* message)
{
  if (checker.error)
    return;

  report_error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  checker.error = true;
}

static void error_type_mismatch(Token token)
{
  error(token, "Type mismatch.");
}

static void error_operation_not_defined(Token token, const char* type)
{
  error(token, memory_sprintf(&memory, "Operator '%s' only defined for %s.", token.lexeme, type));
}

static void error_name_already_exists(Token token, const char* name)
{
  error(token, memory_sprintf(&memory, "The name '%s' already exists.", name));
}

static void error_cannot_find_name(Token token, const char* name)
{
  error(token, memory_sprintf(&memory, "Undeclared identifier '%s'.", name));
}

static void error_not_a_variable(Token token, const char* name)
{
  error(token, memory_sprintf(&memory, "The name '%s' is not a variable.", name));
}

static void error_not_a_function(Token token, const char* name)
{
  error(token, memory_sprintf(&memory, "The name '%s' is not a function.", name));
}

static void error_unexpected_function(Token token)
{
  error(token, "A function declaration is not allowed here.");
}

static void error_unexpected_return(Token token)
{
  error(token, "A return statement can only appear inside a function.");
}

static void error_unexpected_continue(Token token)
{
  error(token, "A continue statement can only appear inside a loop.");
}

static void error_unexpected_break(Token token)
{
  error(token, "A break statement can only appear inside a loop.");
}

static void error_condition_is_not_bool(Token token)
{
  error(token, "The condition expression must evaluate to a boolean.");
}

static void error_invalid_return_type(Token token)
{
  error(token, "Type mismatch with return.");
}

static void error_invalid_arity(Token token, int expected, int got)
{
  error(token, memory_sprintf(&memory, "Expected %d parameter(s) but got %d.", expected, got));
}

static bool upcast(Expr* expression, DataType* left, DataType* right, DataType from, DataType to)
{
  Expr** target;
  DataType* target_type;

  if (*left == from && *right == to)
  {
    target_type = left;
    target = &expression->binary.left;
  }
  else if (*left == to && *right == from)
  {
    target_type = right;
    target = &expression->binary.right;
  }
  else
  {
    return false;
  }

  Expr* cast_expression = EXPR();
  cast_expression->type = EXPR_CAST;
  cast_expression->data_type = to;
  cast_expression->cast.expr = *target;

  *target = cast_expression;
  *target_type = to;

  return true;
}

static DataType token_to_data_type(Token token)
{
  switch (token.type)
  {
  case TOKEN_IDENTIFIER_BOOL:
    return TYPE_BOOL;
  case TOKEN_IDENTIFIER_VOID:
    return TYPE_VOID;
  case TOKEN_IDENTIFIER_INT:
    return TYPE_INTEGER;
  case TOKEN_IDENTIFIER_FLOAT:
    return TYPE_FLOAT;
  case TOKEN_IDENTIFIER_STRING:
    return TYPE_STRING;

  default:
    UNREACHABLE("Unhandled data type");
  }
}

static DataType check_cast_expression(Expr* expression)
{
  return expression->data_type;
}

static DataType check_literal_expression(Expr* expression)
{
  expression->data_type = expression->literal.data_type;
  return expression->data_type;
}

static DataType check_group_expression(Expr* expression)
{
  expression->data_type = check_expression(expression->group.expr);
  return expression->data_type;
}

static DataType check_unary_expression(Expr* expression)
{
  Token op = expression->unary.op;
  DataType type = check_expression(expression->unary.expr);

  if (op.type == TOKEN_MINUS)
  {
    if (type != TYPE_INTEGER && type != TYPE_FLOAT)
      error_type_mismatch(op);
  }
  else if (op.type == TOKEN_BANG || op.type == TOKEN_NOT)
  {
    if (type != TYPE_BOOL)
      error_type_mismatch(op);
  }

  expression->data_type = type;
  return type;
}

static DataType check_binary_expression(Expr* expression)
{
  Token op = expression->binary.op;
  DataType left = check_expression(expression->binary.left);
  DataType right = check_expression(expression->binary.right);

  if (left != right)
  {
    if (!upcast(expression, &left, &right, TYPE_INTEGER, TYPE_FLOAT))
    {
      error_type_mismatch(expression->binary.op);
    }
  }

  expression->binary.data_type = left;
  expression->data_type = left;

  switch (op.type)
  {
  case TOKEN_AND:
  case TOKEN_OR:
    if (left != TYPE_BOOL)
      error_operation_not_defined(op, "'bool'");

    break;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (left != TYPE_INTEGER && left != TYPE_FLOAT && left != TYPE_BOOL)
      error_operation_not_defined(op, "'int', 'float', 'bool");

    expression->data_type = TYPE_BOOL;
    break;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    if (left != TYPE_INTEGER && left != TYPE_FLOAT)
      error_operation_not_defined(op, "'int' and 'float'");

    break;

  case TOKEN_PERCENT:
    if (left != TYPE_INTEGER)
      error_operation_not_defined(op, "'int'");

    break;

  default:
    error_operation_not_defined(op, "'unknown'");
  }

  return expression->data_type;
}

static DataType check_variable_expression(Expr* expression)
{
  const char* name = expression->var.name.lexeme;

  Stmt* statement = environment_get_variable(checker.environment, name);
  if (!statement)
  {
    error_cannot_find_name(expression->var.name, name);
    return TYPE_VOID;
  }

  if (statement->type != STMT_VARIABLE_DECL)
  {
    error_not_a_variable(expression->var.name, name);
    return TYPE_VOID;
  }

  expression->var.index = statement->var.index;
  expression->data_type = statement->var.data_type;

  return expression->data_type;
}

static DataType check_assignment_expression(Expr* expression)
{
  const char* name = expression->assign.name.lexeme;

  Stmt* statement = environment_get_variable(checker.environment, name);
  if (!statement)
  {
    error_cannot_find_name(expression->assign.name, name);
    return TYPE_VOID;
  }

  if (statement->type != STMT_VARIABLE_DECL)
  {
    error_not_a_variable(expression->assign.name, name);
    return TYPE_VOID;
  }

  DataType data_type = check_expression(expression->assign.value);

  if (statement->var.data_type != data_type)
  {
    error_type_mismatch(expression->assign.name);
  }

  expression->data_type = data_type;
  expression->assign.index = statement->var.index;

  return expression->data_type;
}

static DataType check_call_expression(Expr* expression)
{
  const char* name = expression->call.name.lexeme;

  Stmt* statement = environment_get_variable(checker.environment, name);
  if (!statement)
  {
    error_cannot_find_name(expression->call.name, name);
    return TYPE_VOID;
  }

  if (statement->type != STMT_FUNCTION_DECL)
  {
    error_not_a_function(expression->call.name, name);
    return TYPE_VOID;
  }

  int number_of_arguments = array_size(&expression->call.arguments);
  int expected_number_of_arguments = array_size(&statement->func.parameters);

  if (number_of_arguments != expected_number_of_arguments)
  {
    error_invalid_arity(expression->call.name, expected_number_of_arguments, number_of_arguments);
    return TYPE_VOID;
  }

  for (int i = 0; i < number_of_arguments; i++)
  {
    Expr* argument = expression->call.arguments.elems[i];
    Stmt* parameter = statement->func.parameters.elems[i];

    if (check_expression(argument) != token_to_data_type(parameter->var.type))
    {
      error_type_mismatch(expression->call.name);
    }
  }

  expression->data_type = statement->func.data_type;
  return expression->data_type;
}

static DataType check_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_CAST:
    return check_cast_expression(expression);
  case EXPR_LITERAL:
    return check_literal_expression(expression);
  case EXPR_GROUP:
    return check_group_expression(expression);
  case EXPR_BINARY:
    return check_binary_expression(expression);
  case EXPR_UNARY:
    return check_unary_expression(expression);
  case EXPR_VAR:
    return check_variable_expression(expression);
  case EXPR_ASSIGN:
    return check_assignment_expression(expression);
  case EXPR_CALL:
    return check_call_expression(expression);

  default:
    UNREACHABLE("Unhandled expression");
  }
}

static void check_expression_statement(Stmt* statement)
{
  check_expression(statement->expr.expr);
}

static void check_return_statement(Stmt* statement)
{
  if (!checker.function)
  {
    error_unexpected_return(statement->ret.keyword);
    return;
  }

  DataType data_type;

  if (statement->ret.expr)
    data_type = check_expression(statement->ret.expr);
  else
    data_type = TYPE_VOID;

  if (checker.function->func.data_type != data_type)
  {
    error_invalid_return_type(checker.function->func.type);
    return;
  }
}

static void check_continue_statement(Stmt* statement)
{
  if (!checker.loop)
  {
    error_unexpected_continue(statement->ret.keyword);
  }
}

static void check_break_statement(Stmt* statement)
{
  if (!checker.loop)
  {
    error_unexpected_break(statement->ret.keyword);
  }
}

static void check_if_statement(Stmt* statement)
{
  DataType data_type = check_expression(statement->cond.condition);
  if (data_type != TYPE_BOOL)
  {
    error_condition_is_not_bool(statement->cond.keyword);
  }

  checker.environment = environment_init(checker.environment);

  Stmt* body_statement;
  array_foreach(&statement->cond.then_branch, body_statement)
  {
    check_statement(body_statement);
  }

  checker.environment = checker.environment->parent;

  if (statement->cond.else_branch.elems)
  {
    checker.environment = environment_init(checker.environment);

    Stmt* body_statement;
    array_foreach(&statement->cond.else_branch, body_statement)
    {
      check_statement(body_statement);
    }

    checker.environment = checker.environment->parent;
  }
}

static void check_while_statement(Stmt* statement)
{
  checker.environment = environment_init(checker.environment);

  if (statement->loop.initializer)
  {
    check_statement(statement->loop.initializer);
  }

  DataType data_type = check_expression(statement->loop.condition);
  if (data_type != TYPE_BOOL)
  {
    error_condition_is_not_bool(statement->loop.keyword);
  }

  Stmt* previous_loop = checker.loop;
  checker.environment = environment_init(checker.environment);
  checker.loop = statement;

  Stmt* body_statement;
  array_foreach(&statement->loop.body, body_statement)
  {
    check_statement(body_statement);
  }

  checker.loop = previous_loop;
  checker.environment = checker.environment->parent;

  if (statement->loop.incrementer)
  {
    check_statement(statement->loop.incrementer);
  }

  checker.environment = checker.environment->parent;
}

static void check_function_declaration(Stmt* statement)
{
  if (checker.function || checker.loop)
  {
    error_unexpected_function(statement->func.name);
    return;
  }

  Stmt* previous_function = checker.function;

  checker.environment = environment_init(checker.environment);
  checker.function = statement;

  int index = 0;
  Stmt* parameter;
  array_foreach(&statement->func.parameters, parameter)
  {
    parameter->var.index = index++;
    parameter->var.data_type = token_to_data_type(parameter->var.type);

    environment_set_variable(checker.environment, parameter->var.name.lexeme, parameter);
  }

  Stmt* body_statement;
  array_foreach(&statement->func.body, body_statement)
  {
    check_statement(body_statement);
  }

  checker.function = previous_function;
  checker.environment = checker.environment->parent;
}

static void check_variable_declaration(Stmt* statement)
{
  const char* name = statement->var.name.lexeme;

  if (checker.function)
  {
    if (environment_check_variable(checker.environment, name))
    {
      error_name_already_exists(statement->var.name, name);
      return;
    }

    statement->var.index = array_size(&checker.function->func.variables) +
                           array_size(&checker.function->func.parameters);
  }
  else
  {
    if (environment_get_variable(checker.environment, name))
    {
      error_name_already_exists(statement->var.name, name);
      return;
    }

    statement->var.index = -1;
  }

  statement->var.data_type = token_to_data_type(statement->var.type);

  if (statement->var.initializer)
  {
    DataType data_type = check_expression(statement->var.initializer);

    if (statement->var.data_type != data_type)
    {
      error_type_mismatch(statement->var.type);
    }
  }

  if (checker.function)
  {
    array_add(&checker.function->func.variables, statement);
  }

  environment_set_variable(checker.environment, name, statement);
}

static void check_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_EXPR:
    check_expression_statement(statement);
    break;
  case STMT_IF:
    check_if_statement(statement);
    break;
  case STMT_WHILE:
    check_while_statement(statement);
    break;
  case STMT_RETURN:
    check_return_statement(statement);
    break;
  case STMT_CONTINUE:
    check_continue_statement(statement);
    break;
  case STMT_BREAK:
    check_break_statement(statement);
    break;
  case STMT_FUNCTION_DECL:
    check_function_declaration(statement);
    break;
  case STMT_VARIABLE_DECL:
    check_variable_declaration(statement);
    break;

  default:
    UNREACHABLE("Unhandled statement");
  }
}

static void init_function_declaration(Stmt* statement)
{
  const char* name = statement->func.name.lexeme;
  if (environment_get_variable(checker.environment, name))
  {
    error_name_already_exists(statement->func.name, name);
    return;
  }

  statement->func.data_type = token_to_data_type(statement->func.type);
  environment_set_variable(checker.environment, name, statement);
}

static void init_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_FUNCTION_DECL:
    init_function_declaration(statement);
    break;

  default:
    break;
  }
}

void checker_init(ArrayStmt statements)
{
  checker.error = false;
  checker.function = NULL;
  checker.loop = NULL;
  checker.statements = statements;
  checker.environment = environment_init(NULL);
}

void checker_validate(void)
{
  Stmt* statement;
  array_foreach(&checker.statements, statement)
  {
    init_statement(statement);
  }

  array_foreach(&checker.statements, statement)
  {
    check_statement(statement);
  }
}

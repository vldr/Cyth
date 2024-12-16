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
  MapVarStmt variables;

  struct _ENVIRONMENT* parent;
} Environment;

static struct
{
  bool error;
  ArrayStmt statements;

  Environment* environment;
  FuncStmt* function;
  ClassStmt* class;
  WhileStmt* loop;
} checker;

static DataType check_expression(Expr* expression);
static void check_statement(Stmt* statement);

static Environment* environment_init(Environment* parent)
{
  Environment* environment = ALLOC(Environment);
  environment->parent = parent;
  map_init_var_stmt(&environment->variables, 0, 0);

  return environment;
}

static bool environment_check_variable(Environment* environment, const char* name)
{
  return map_get_var_stmt(&environment->variables, name) != NULL;
}

static VarStmt* environment_get_variable(Environment* environment, const char* name)
{
  while (environment)
  {
    VarStmt* variable = map_get_var_stmt(&environment->variables, name);
    if (variable)
    {
      return variable;
    }

    environment = environment->parent;
  }

  return NULL;
}

static void environment_set_variable(Environment* environment, const char* name, VarStmt* variable)
{
  map_put_var_stmt(&environment->variables, name, variable);
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

static void error_unexpected_class(Token token)
{
  error(token, "A class declaration is not allowed here.");
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

bool equal_data_type(DataType left, DataType right)
{
  return left.type == right.type;
}

static DataType token_to_data_type(Token token)
{
  switch (token.type)
  {
  case TOKEN_IDENTIFIER_BOOL:
    return DATA_TYPE(TYPE_BOOL);
  case TOKEN_IDENTIFIER_VOID:
    return DATA_TYPE(TYPE_VOID);
  case TOKEN_IDENTIFIER_INT:
    return DATA_TYPE(TYPE_INTEGER);
  case TOKEN_IDENTIFIER_FLOAT:
    return DATA_TYPE(TYPE_FLOAT);
  case TOKEN_IDENTIFIER_STRING:
    return DATA_TYPE(TYPE_STRING);

  default:
    UNREACHABLE("Unhandled data type");
  }
}

static bool upcast(BinaryExpr* expression, DataType* left, DataType* right, DataType from,
                   DataType to)
{
  Expr** target;
  DataType* target_type;

  if (equal_data_type(*left, from) && equal_data_type(*right, to))
  {
    target_type = left;
    target = &expression->left;
  }
  else if (equal_data_type(*left, to) && equal_data_type(*right, from))
  {
    target_type = right;
    target = &expression->right;
  }
  else
  {
    return false;
  }

  Expr* cast_expression = EXPR();
  cast_expression->type = EXPR_CAST;
  cast_expression->cast.from_data_type = from;
  cast_expression->cast.to_data_type = to;
  cast_expression->cast.expr = *target;

  *target = cast_expression;
  *target_type = to;

  return true;
}

static DataType check_cast_expression(CastExpr* expression)
{
  return expression->to_data_type;
}

static DataType check_literal_expression(LiteralExpr* expression)
{
  return expression->data_type;
}

static DataType check_group_expression(GroupExpr* expression)
{
  expression->data_type = check_expression(expression->expr);

  return expression->data_type;
}

static DataType check_unary_expression(UnaryExpr* expression)
{
  Token op = expression->op;
  DataType type = check_expression(expression->expr);

  if (op.type == TOKEN_MINUS)
  {
    if (!equal_data_type(type, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(type, DATA_TYPE(TYPE_FLOAT)))
      error_type_mismatch(op);
  }
  else if (op.type == TOKEN_BANG || op.type == TOKEN_NOT)
  {
    if (!equal_data_type(type, DATA_TYPE(TYPE_BOOL)))
      error_type_mismatch(op);
  }

  expression->data_type = type;
  return type;
}

static DataType check_binary_expression(BinaryExpr* expression)
{
  Token op = expression->op;
  DataType left = check_expression(expression->left);
  DataType right = check_expression(expression->right);

  if (!equal_data_type(left, right))
  {
    if (!upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_FLOAT)))
    {
      error_type_mismatch(expression->op);
    }
  }

  expression->data_type = left;
  expression->operand_data_type = left;

  switch (op.type)
  {
  case TOKEN_AND:
  case TOKEN_OR:
    if (!equal_data_type(left, DATA_TYPE(TYPE_BOOL)))
      error_operation_not_defined(op, "'bool'");

    break;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_BOOL)))
      error_operation_not_defined(op, "'int', 'float', 'bool");

    expression->data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)))
      error_operation_not_defined(op, "'int' and 'float'");

    break;

  case TOKEN_PERCENT:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)))
      error_operation_not_defined(op, "'int'");

    break;

  default:
    error_operation_not_defined(op, "'unknown'");
  }

  return expression->data_type;
}

static DataType check_variable_expression(VarExpr* expression)
{
  const char* name = expression->name.lexeme;

  VarStmt* variable = environment_get_variable(checker.environment, name);
  if (!variable)
  {
    error_cannot_find_name(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  if (equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION)))
  {
    error_not_a_variable(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  expression->index = variable->index;
  expression->data_type = variable->data_type;

  return expression->data_type;
}

static DataType check_assignment_expression(AssignExpr* expression)
{
  const char* name = expression->name.lexeme;

  VarStmt* variable = environment_get_variable(checker.environment, name);
  if (!variable)
  {
    error_cannot_find_name(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  if (equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION)))
  {
    error_not_a_variable(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  DataType data_type = check_expression(expression->value);

  if (!equal_data_type(variable->data_type, data_type))
  {
    error_type_mismatch(expression->name);
  }

  expression->data_type = data_type;
  expression->index = variable->index;

  return expression->data_type;
}

static DataType check_call_expression(CallExpr* expression)
{
  const char* name = expression->name.lexeme;
  VarStmt* variable = environment_get_variable(checker.environment, name);

  if (!variable)
  {
    error_cannot_find_name(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  if (!equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION)))
  {
    error_not_a_function(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  FuncStmt* function = variable->data_type.function;
  int number_of_arguments = array_size(&expression->arguments);
  int expected_number_of_arguments = array_size(&function->parameters);

  if (number_of_arguments != expected_number_of_arguments)
  {
    error_invalid_arity(expression->name, expected_number_of_arguments, number_of_arguments);
    return DATA_TYPE(TYPE_VOID);
  }

  for (int i = 0; i < number_of_arguments; i++)
  {
    Expr* argument = expression->arguments.elems[i];
    VarStmt* parameter = function->parameters.elems[i];

    if (!equal_data_type(check_expression(argument), token_to_data_type(parameter->type)))
    {
      error_type_mismatch(expression->name);
    }
  }

  expression->data_type = function->data_type;
  return expression->data_type;
}

static DataType check_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_CAST:
    return check_cast_expression(&expression->cast);
  case EXPR_LITERAL:
    return check_literal_expression(&expression->literal);
  case EXPR_GROUP:
    return check_group_expression(&expression->group);
  case EXPR_BINARY:
    return check_binary_expression(&expression->binary);
  case EXPR_UNARY:
    return check_unary_expression(&expression->unary);
  case EXPR_VAR:
    return check_variable_expression(&expression->var);
  case EXPR_ASSIGN:
    return check_assignment_expression(&expression->assign);
  case EXPR_CALL:
    return check_call_expression(&expression->call);

  default:
    UNREACHABLE("Unhandled expression");
  }
}

static void check_expression_statement(ExprStmt* statement)
{
  statement->data_type = check_expression(statement->expr);
}

static void check_return_statement(ReturnStmt* statement)
{
  if (!checker.function)
  {
    error_unexpected_return(statement->keyword);
    return;
  }

  DataType data_type;

  if (statement->expr)
    data_type = check_expression(statement->expr);
  else
    data_type = DATA_TYPE(TYPE_VOID);

  if (!equal_data_type(checker.function->data_type, data_type))
  {
    error_invalid_return_type(checker.function->type);
    return;
  }
}

static void check_continue_statement(ContinueStmt* statement)
{
  if (!checker.loop)
  {
    error_unexpected_continue(statement->keyword);
  }
}

static void check_break_statement(BreakStmt* statement)
{
  if (!checker.loop)
  {
    error_unexpected_break(statement->keyword);
  }
}

static void check_if_statement(IfStmt* statement)
{
  DataType data_type = check_expression(statement->condition);
  if (!equal_data_type(data_type, DATA_TYPE(TYPE_BOOL)))
  {
    error_condition_is_not_bool(statement->keyword);
  }

  checker.environment = environment_init(checker.environment);

  Stmt* body_statement;
  array_foreach(&statement->then_branch, body_statement)
  {
    check_statement(body_statement);
  }

  checker.environment = checker.environment->parent;

  if (statement->else_branch.elems)
  {
    checker.environment = environment_init(checker.environment);

    Stmt* body_statement;
    array_foreach(&statement->else_branch, body_statement)
    {
      check_statement(body_statement);
    }

    checker.environment = checker.environment->parent;
  }
}

static void check_while_statement(WhileStmt* statement)
{
  checker.environment = environment_init(checker.environment);

  if (statement->initializer)
  {
    check_statement(statement->initializer);
  }

  DataType data_type = check_expression(statement->condition);
  if (!equal_data_type(data_type, DATA_TYPE(TYPE_BOOL)))
  {
    error_condition_is_not_bool(statement->keyword);
  }

  WhileStmt* previous_loop = checker.loop;
  checker.environment = environment_init(checker.environment);
  checker.loop = statement;

  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    check_statement(body_statement);
  }

  checker.loop = previous_loop;
  checker.environment = checker.environment->parent;

  if (statement->incrementer)
  {
    check_statement(statement->incrementer);
  }

  checker.environment = checker.environment->parent;
}

static void check_variable_declaration(VarStmt* statement)
{
  const char* name = statement->name.lexeme;

  if (checker.function)
  {
    if (environment_check_variable(checker.environment, name))
    {
      error_name_already_exists(statement->name, name);
      return;
    }

    statement->index =
      array_size(&checker.function->variables) + array_size(&checker.function->parameters);
  }
  else
  {
    if (environment_get_variable(checker.environment, name))
    {
      error_name_already_exists(statement->name, name);
      return;
    }

    statement->index = -1;
  }

  statement->data_type = token_to_data_type(statement->type);

  if (statement->initializer)
  {
    DataType data_type = check_expression(statement->initializer);

    if (!equal_data_type(statement->data_type, data_type))
    {
      error_type_mismatch(statement->type);
    }
  }

  if (checker.function)
  {
    array_add(&checker.function->variables, statement);
  }

  environment_set_variable(checker.environment, name, statement);
}

static void check_function_declaration(FuncStmt* statement)
{
  if (checker.function || checker.loop)
  {
    error_unexpected_function(statement->name);
    return;
  }

  FuncStmt* previous_function = checker.function;

  checker.environment = environment_init(checker.environment);
  checker.function = statement;

  int index = 0;
  VarStmt* parameter;

  array_foreach(&statement->parameters, parameter)
  {
    const char* name = parameter->name.lexeme;
    if (environment_check_variable(checker.environment, name))
    {
      error_name_already_exists(parameter->name, name);
      continue;
    }

    parameter->index = index++;
    parameter->data_type = token_to_data_type(parameter->type);

    environment_set_variable(checker.environment, name, parameter);
  }

  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    check_statement(body_statement);
  }

  checker.function = previous_function;
  checker.environment = checker.environment->parent;
}

static void check_class_declaration(ClassStmt* statement)
{
  if (checker.function || checker.loop || checker.class)
  {
    error_unexpected_class(statement->name);
    return;
  }

  checker.environment = environment_init(checker.environment);
  checker.class = statement;

  FuncStmt* function_statement;
  array_foreach(&statement->functions, function_statement)
  {
    const char* class_name = statement->name.lexeme;
    const char* function_name = function_statement->name.lexeme;
    function_statement->name.lexeme = memory_sprintf(&memory, "%s~%s", class_name, function_name);

    check_function_declaration(function_statement);
  }

  checker.class = NULL;
  checker.environment = checker.environment->parent;
}

static void check_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_EXPR:
    check_expression_statement(&statement->expr);
    break;
  case STMT_IF:
    check_if_statement(&statement->cond);
    break;
  case STMT_WHILE:
    check_while_statement(&statement->loop);
    break;
  case STMT_RETURN:
    check_return_statement(&statement->ret);
    break;
  case STMT_CONTINUE:
    check_continue_statement(&statement->cont);
    break;
  case STMT_BREAK:
    check_break_statement(&statement->brk);
    break;
  case STMT_FUNCTION_DECL:
    check_function_declaration(&statement->func);
    break;
  case STMT_VARIABLE_DECL:
    check_variable_declaration(&statement->var);
    break;
  case STMT_CLASS_DECL:
    check_class_declaration(&statement->class);
    break;

  default:
    UNREACHABLE("Unhandled statement");
  }
}

static void init_function_declaration(FuncStmt* statement)
{
  const char* name = statement->name.lexeme;
  if (environment_get_variable(checker.environment, name))
  {
    error_name_already_exists(statement->name, name);
    return;
  }

  statement->data_type = token_to_data_type(statement->type);

  Stmt* function = STMT();
  function->type = STMT_VARIABLE_DECL;
  function->var.name = statement->name;
  function->var.type = statement->type;
  function->var.initializer = NULL;
  function->var.index = -1;
  function->var.data_type = DATA_TYPE(TYPE_FUNCTION);
  function->var.data_type.function = statement;

  environment_set_variable(checker.environment, name, &function->var);
}

static void init_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_FUNCTION_DECL:
    init_function_declaration(&statement->func);
    break;

  default:
    break;
  }
}

void checker_init(ArrayStmt statements)
{
  checker.error = false;
  checker.function = NULL;
  checker.class = NULL;
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

#include "checker.h"
#include "array.h"
#include "environment.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "memory.h"
#include "statement.h"

static struct
{
  bool error;
  ArrayStmt statements;

  Environment* global_environment;
  Environment* environment;
  FuncStmt* function;
  ClassStmt* class;
  WhileStmt* loop;
} checker;

static DataType check_expression(Expr* expression);
static void check_statement(Stmt* statement);

static void checker_error(Token token, const char* message)
{
  if (checker.error)
    return;

  error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  checker.error = true;
}

static void error_type_mismatch(Token token)
{
  checker_error(token, "Type mismatch.");
}

static void error_operation_not_defined(Token token, const char* type)
{
  checker_error(token,
                memory_sprintf(&memory, "Operator '%s' only defined for %s.", token.lexeme, type));
}

static void error_name_already_exists(Token token, const char* name)
{
  checker_error(token, memory_sprintf(&memory, "The name '%s' already exists.", name));
}

static void error_type_cannot_be_void(Token token, const char* name)
{
  checker_error(token, memory_sprintf(&memory, "The type %s cannot be used here", name));
}

static void error_cannot_find_name(Token token, const char* name)
{
  checker_error(token, memory_sprintf(&memory, "Undeclared identifier '%s'.", name));
}

static void error_cannot_find_member_name(Token token, const char* name, const char* class_name)
{
  checker_error(token, memory_sprintf(&memory, "No member named '%s' in '%s'.", name, class_name));
}

static void error_cannot_find_type(Token token, const char* name)
{
  checker_error(token, memory_sprintf(&memory, "Undeclared type '%s'.", name));
}

static void error_not_a_type(Token token, const char* name)
{
  checker_error(token, memory_sprintf(&memory, "The name '%s' is not a type.", name));
}

static void error_not_a_function(Token token)
{
  checker_error(token, "The expression is not a function.");
}

static void error_not_an_object(Token token)
{
  checker_error(token, "The expression is not an object.");
}

static void error_not_assignable(Token token)
{
  checker_error(token, "The expression is not assignable.");
}

static void error_unexpected_function(Token token)
{
  checker_error(token, "A function declaration is not allowed here.");
}

static void error_unexpected_class(Token token)
{
  checker_error(token, "A class declaration is not allowed here.");
}

static void error_unexpected_return(Token token)
{
  checker_error(token, "A return statement can only appear inside a function.");
}

static void error_unexpected_continue(Token token)
{
  checker_error(token, "A continue statement can only appear inside a loop.");
}

static void error_unexpected_break(Token token)
{
  checker_error(token, "A break statement can only appear inside a loop.");
}

static void error_condition_is_not_bool(Token token)
{
  checker_error(token, "The condition expression must evaluate to a boolean.");
}

static void error_invalid_return_type(Token token)
{
  checker_error(token, "Type mismatch with return.");
}

static void error_invalid_initializer_return_type(Token token)
{
  checker_error(token, "The return type of '__init__' must be 'void'.");
}

static void error_invalid_arity(Token token, int expected, int got)
{
  checker_error(token,
                memory_sprintf(&memory, "Expected %d parameter(s) but got %d.", expected, got));
}

bool equal_data_type(DataType left, DataType right)
{
  if (left.type == TYPE_OBJECT && right.type == TYPE_OBJECT && left.class && right.class)
    return left.class == right.class;

  return left.type == right.type;
}

static DataType token_to_data_type(Token token, bool ignore_undeclared)
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
  case TOKEN_IDENTIFIER: {
    VarStmt* variable = environment_get_variable(checker.environment, token.lexeme);
    if (!variable)
    {
      error_cannot_find_type(token, token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    if (!equal_data_type(variable->data_type, DATA_TYPE(TYPE_PROTOTYPE)))
    {
      error_not_a_type(token, token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    if (!ignore_undeclared && !variable->data_type.class->declared)
    {
      error_cannot_find_type(token, token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    DataType object = DATA_TYPE(TYPE_OBJECT);
    object.class = variable->data_type.class;

    return object;
  }
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

static void init_function_declaration(FuncStmt* statement)
{
  const char* name = statement->name.lexeme;
  if (environment_check_variable(checker.environment, name))
  {
    error_name_already_exists(statement->name, name);
    return;
  }

  if (checker.class)
  {
    if (strcmp(name, "__init__") == 0 && statement->type.type != TOKEN_IDENTIFIER_VOID)
    {
      error_invalid_initializer_return_type(statement->name);
      return;
    }

    VarStmt* parameter = ALLOC(VarStmt);
    parameter->name = (Token){ .lexeme = "this" };
    parameter->type = checker.class->name;
    parameter->initializer = NULL;
    parameter->index = 0;
    parameter->scope = SCOPE_LOCAL;
    parameter->data_type = DATA_TYPE(TYPE_OBJECT);
    parameter->data_type.class = checker.class;

    ArrayVarStmt parameters;
    array_init(&parameters);
    array_add(&parameters, parameter);
    array_foreach(&statement->parameters, parameter)
    {
      array_add(&parameters, parameter);
    }

    statement->parameters = parameters;
  }

  statement->data_type = token_to_data_type(statement->type, true);

  VarStmt* variable = ALLOC(VarStmt);
  variable->name = statement->name;
  variable->type = statement->type;
  variable->initializer = NULL;

  if (checker.class)
  {
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_MEMBER);
    variable->data_type.function_member.function = statement;
    variable->data_type.function_member.this = NULL;
  }
  else
  {
    variable->data_type = DATA_TYPE(TYPE_FUNCTION);
    variable->data_type.function = statement;
  }

  environment_set_variable(checker.environment, name, variable);
}

static void init_class_declaration(ClassStmt* statement)
{
  const char* name = statement->name.lexeme;
  if (environment_check_variable(checker.environment, name))
  {
    error_name_already_exists(statement->name, name);
  }

  VarStmt* variable = ALLOC(VarStmt);
  variable->name = statement->name;
  variable->type = statement->name;
  variable->initializer = NULL;
  variable->scope = SCOPE_GLOBAL;
  variable->index = -1;
  variable->data_type = DATA_TYPE(TYPE_PROTOTYPE);
  variable->data_type.class = statement;

  environment_set_variable(checker.environment, name, variable);
}

static void init_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_FUNCTION_DECL:
    init_function_declaration(&statement->func);
    break;
  case STMT_CLASS_DECL:
    init_class_declaration(&statement->class);
    break;

  default:
    break;
  }
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

  expression->return_data_type = left;
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
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_BOOL)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_OBJECT)))
      error_operation_not_defined(op, "'int', 'float', 'bool', 'class'");

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_BOOL)))
      error_operation_not_defined(op, "'int', 'float', 'bool'");

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
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

  return expression->return_data_type;
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

  expression->variable = variable;
  expression->data_type = variable->data_type;

  return expression->data_type;
}

static DataType check_assignment_expression(AssignExpr* expression)
{
  Expr* target = expression->target;
  DataType target_data_type = check_expression(target);
  DataType value_data_type = check_expression(expression->value);

  if (equal_data_type(target_data_type, DATA_TYPE(TYPE_VOID)) ||
      equal_data_type(target_data_type, DATA_TYPE(TYPE_PROTOTYPE)) ||
      equal_data_type(target_data_type, DATA_TYPE(TYPE_FUNCTION)) ||
      equal_data_type(target_data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
  {
    error_not_assignable(expression->op);
    return DATA_TYPE(TYPE_VOID);
  }

  if (!equal_data_type(target_data_type, value_data_type))
  {
    error_type_mismatch(expression->op);
    return DATA_TYPE(TYPE_VOID);
  }

  if (target->type == EXPR_VAR)
  {
    VarStmt* variable = target->var.variable;
    expression->variable = variable;
    expression->data_type = variable->data_type;

    return expression->data_type;
  }
  else if (target->type == EXPR_ACCESS)
  {
    VarStmt* variable = target->access.variable;
    expression->variable = variable;
    expression->data_type = variable->data_type;

    return expression->data_type;
  }

  error_not_assignable(expression->op);
  return DATA_TYPE(TYPE_VOID);
}

static DataType check_call_expression(CallExpr* expression)
{
  Expr* callee = expression->callee;
  DataType callee_data_type = check_expression(callee);

  if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
  {
    FuncStmt* function = callee_data_type.function_member.function;
    Expr* argument;

    if (callee_data_type.function_member.this)
    {
      argument = callee_data_type.function_member.this;
    }
    else
    {
      argument = EXPR();
      argument->type = EXPR_VAR;
      argument->var.name = (Token){ .lexeme = "this" };
      argument->var.variable = array_at(&function->parameters, 0);
      argument->var.data_type = DATA_TYPE(TYPE_OBJECT);
      argument->var.data_type.class = checker.class;
    }

    ArrayExpr arguments;
    array_init(&arguments);
    array_add(&arguments, argument);
    array_foreach(&expression->arguments, argument)
    {
      array_add(&arguments, argument);
    }

    expression->arguments = arguments;

    int number_of_arguments = array_size(&expression->arguments);
    int expected_number_of_arguments = array_size(&function->parameters);

    if (number_of_arguments != expected_number_of_arguments)
    {
      error_invalid_arity(expression->callee_token, expected_number_of_arguments - 1,
                          number_of_arguments - 1);
      return DATA_TYPE(TYPE_VOID);
    }

    for (int i = 1; i < number_of_arguments; i++)
    {
      Expr* argument = expression->arguments.elems[i];
      VarStmt* parameter = function->parameters.elems[i];

      if (!equal_data_type(check_expression(argument), token_to_data_type(parameter->type, false)))
      {
        error_type_mismatch(expression->callee_token);
      }
    }

    expression->return_data_type = function->data_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_FUNCTION)))
  {
    FuncStmt* function = callee_data_type.function;
    int number_of_arguments = array_size(&expression->arguments);
    int expected_number_of_arguments = array_size(&function->parameters);

    if (number_of_arguments != expected_number_of_arguments)
    {
      error_invalid_arity(expression->callee_token, expected_number_of_arguments,
                          number_of_arguments);
      return DATA_TYPE(TYPE_VOID);
    }

    for (int i = 0; i < number_of_arguments; i++)
    {
      Expr* argument = expression->arguments.elems[i];
      VarStmt* parameter = function->parameters.elems[i];

      if (!equal_data_type(check_expression(argument), token_to_data_type(parameter->type, false)))
      {
        error_type_mismatch(expression->callee_token);
      }
    }

    expression->return_data_type = function->data_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_PROTOTYPE)))
  {
    ClassStmt* class = callee_data_type.class;
    VarStmt* variable = map_get_var_stmt(&class->members, "__init__");

    Expr* argument = EXPR();
    argument->type = EXPR_LITERAL;
    argument->literal.data_type = DATA_TYPE(TYPE_OBJECT);

    ArrayExpr arguments;
    array_init(&arguments);
    array_add(&arguments, argument);
    array_foreach(&expression->arguments, argument)
    {
      array_add(&arguments, argument);
    }

    expression->arguments = arguments;

    if (variable)
    {
      if (!equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
      {
        error_not_a_function(expression->callee_token);
        return DATA_TYPE(TYPE_VOID);
      }

      FuncStmt* function = variable->data_type.function_member.function;
      int number_of_arguments = array_size(&expression->arguments);
      int expected_number_of_arguments = array_size(&function->parameters);

      if (number_of_arguments != expected_number_of_arguments)
      {
        error_invalid_arity(expression->callee_token, expected_number_of_arguments - 1,
                            number_of_arguments - 1);
        return DATA_TYPE(TYPE_VOID);
      }

      for (int i = 1; i < number_of_arguments; i++)
      {
        Expr* argument = expression->arguments.elems[i];
        VarStmt* parameter = function->parameters.elems[i];

        if (!equal_data_type(check_expression(argument),
                             token_to_data_type(parameter->type, false)))
        {
          error_type_mismatch(expression->callee_token);
        }
      }
    }
    else
    {
      int number_of_arguments = array_size(&expression->arguments);
      if (number_of_arguments > 1)
      {
        error_invalid_arity(expression->callee_token, 0, number_of_arguments);
        return DATA_TYPE(TYPE_VOID);
      }
    }

    expression->callee_data_type = callee_data_type;
    expression->return_data_type = token_to_data_type(class->name, false);

    return expression->return_data_type;
  }
  else
  {
    error_not_a_function(expression->callee_token);
    return DATA_TYPE(TYPE_VOID);
  }
}

static DataType check_access_expression(AccessExpr* expression)
{
  DataType data_type = check_expression(expression->expr);

  if (equal_data_type(data_type, DATA_TYPE(TYPE_OBJECT)))
  {
    const char* name = expression->name.lexeme;

    ClassStmt* class = data_type.class;
    VarStmt* variable = map_get_var_stmt(&class->members, name);

    if (!variable)
    {
      error_cannot_find_member_name(expression->name, name, class->name.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    if (equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
    {
      variable->data_type.function_member.this = expression->expr;
    }

    expression->variable = variable;
    expression->data_type = variable->data_type;

    return variable->data_type;
  }
  else
  {
    error_not_an_object(expression->expr_token);
    return DATA_TYPE(TYPE_VOID);
  }
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
  case EXPR_ACCESS:
    return check_access_expression(&expression->access);

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
    error_invalid_return_type(statement->keyword);
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
  Environment* environment = checker.environment;
  const char* name = statement->name.lexeme;

  if (checker.function)
  {
    statement->scope = SCOPE_LOCAL;
    statement->index =
      array_size(&checker.function->variables) + array_size(&checker.function->parameters);
  }
  else if (checker.class)
  {
    statement->scope = SCOPE_CLASS;
  }
  else
  {
    environment = checker.global_environment;

    statement->scope = SCOPE_GLOBAL;
  }

  if (environment_check_variable(environment, name))
  {
    error_name_already_exists(statement->name, name);
    return;
  }

  statement->data_type = token_to_data_type(statement->type, false);

  if (equal_data_type(statement->data_type, DATA_TYPE(TYPE_VOID)))
  {
    error_type_cannot_be_void(statement->type, statement->type.lexeme);
    return;
  }

  if (statement->initializer)
  {
    DataType initializer_data_type = check_expression(statement->initializer);

    if (!equal_data_type(statement->data_type, initializer_data_type))
    {
      error_type_mismatch(statement->name);
    }
  }

  if (checker.function)
  {
    array_add(&checker.function->variables, statement);
  }

  environment_set_variable(environment, name, statement);
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

    parameter->scope = SCOPE_LOCAL;
    parameter->index = index++;
    parameter->data_type = token_to_data_type(parameter->type, false);

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
  checker.class->declared = true;

  FuncStmt* function_statement;
  array_foreach(&statement->functions, function_statement)
  {
    init_function_declaration(function_statement);

    const char* class_name = statement->name.lexeme;
    const char* function_name = function_statement->name.lexeme;

    function_statement->data_type = token_to_data_type(function_statement->type, false);
    function_statement->name.lexeme = memory_sprintf(&memory, "%s.%s", class_name, function_name);
  }

  int count = 0;
  VarStmt* variable_statement;
  array_foreach(&statement->variables, variable_statement)
  {
    variable_statement->index = count++;

    check_variable_declaration(variable_statement);
  }

  checker.class->members = checker.environment->variables;

  array_foreach(&statement->functions, function_statement)
  {
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

void checker_init(ArrayStmt statements)
{
  checker.error = false;
  checker.function = NULL;
  checker.class = NULL;
  checker.loop = NULL;
  checker.statements = statements;

  checker.environment = environment_init(NULL);
  checker.global_environment = checker.environment;
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

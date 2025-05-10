#include "checker.h"
#include "array.h"
#include "environment.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "parser.h"
#include "statement.h"

static struct
{
  bool error;
  ArrayStmt statements;

  Environment* environment;
  Environment* global_environment;
  ArrayVarStmt global_locals;

  FuncStmt* function;
  ClassStmt* class;
  WhileStmt* loop;

  ArrayLiteralArrayExpr* array;
} checker;

static void init_statement(Stmt* statement);
static void check_statement(Stmt* statement, bool synchronize);
static void check_class_declaration(ClassStmt* statement);
static DataType check_expression(Expr* expression);

static const char* data_type_to_string(DataType data_type);
static const char* data_type_token_to_string(DataTypeToken type, ArrayChar* string);
static DataType data_type_token_to_data_type(DataTypeToken type);

static void checker_error(Token token, const char* message)
{
  if (checker.error)
    return;

  error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  checker.error = true;
}

static void error_type_mismatch(Token token, DataType expected, DataType got)
{
  checker_error(token, memory_sprintf("Mismatched types '%s' and '%s'",
                                      data_type_to_string(expected), data_type_to_string(got)));
}

static void error_operation_not_defined(Token token, const char* type)
{
  checker_error(token, memory_sprintf("Operator '%s' is only defined for %s.", token.lexeme, type));
}

static void error_missing_operator_overload(Token token, const char* function_name)
{
  checker_error(token, memory_sprintf("Cannot use operator '%s', missing %s method.", token.lexeme,
                                      function_name));
}

static void error_name_already_exists(Token token, const char* name)
{
  checker_error(token, memory_sprintf("The name '%s' already exists.", name));
}

static void error_type_cannot_be_void(Token token, const char* name)
{
  checker_error(token, memory_sprintf("The type %s cannot be used here", name));
}

static void error_cannot_find_name(Token token, const char* name)
{
  checker_error(token, memory_sprintf("Undeclared identifier '%s'.", name));
}

static void error_cannot_find_member_name(Token token, const char* name, const char* class_name)
{
  checker_error(token, memory_sprintf("No member named '%s' in '%s'.", name, class_name));
}

static void error_cannot_find_type(Token token, const char* name)
{
  checker_error(token, memory_sprintf("Undeclared type '%s'.", name));
}

static void error_not_a_template_type(Token token, const char* name)
{
  checker_error(token, memory_sprintf("'%s' is not a template type.", name));
}

static void error_not_a_type(Token token, const char* name)
{
  checker_error(token, memory_sprintf("The name '%s' is not a type.", name));
}

static void error_invalid_template_arity(Token token, int expected, int got)
{
  checker_error(token,
                memory_sprintf("Expected %d template argument(s) but got %d.", expected, got));
}

static void error_recursive_template_type(Token token, const char* name)
{
  checker_error(
    token, memory_sprintf("Cannot instiantiate '%s' template, recursion limit reached.", name));
}

static void error_not_a_function(Token token)
{
  checker_error(token, "The expression is not a function.");
}

static void error_not_an_object(Token token)
{
  checker_error(token, "The expression is not an object.");
}

static void error_not_indexable(Token token)
{
  checker_error(token, "The expression cannot be indexed.");
}

static void error_not_indexable_missing_overload(Token token)
{
  checker_error(token, "The object cannot be indexed, missing '__get__' method.");
}

static void error_not_indexable_and_assignable_missing_overload(Token token)
{
  checker_error(token, "The object cannot be indexed and assigned to, missing '__set__' method.");
}

static void error_index_not_an_int(Token token)
{
  checker_error(token, "The index must be of type 'int'.");
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

static void error_unexpected_import(Token token)
{
  checker_error(token, "An import declaration is not allowed here.");
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

static void error_invalid_initializer_return_type(Token token)
{
  checker_error(token, "The return type of '__init__' must be 'void'.");
}

static void error_invalid_set_return_type(Token token)
{
  checker_error(token, "The return type of '__set__' must be 'void'.");
}

static void error_invalid_get_set_function(Token token)
{
  checker_error(
    token,
    "The return type of '__get__' must match the type of the second parameter of '__set__'.");
}
static void error_invalid_get_set_first_parameter_function(Token token)
{
  checker_error(token,
                "The '__get__' and  '__set__' methods must have the same first parameter type.");
}

static void error_invalid_get_arity(Token token)
{
  checker_error(token, "The '__get__' method must have one argument.");
}

static void error_invalid_set_arity(Token token)
{
  checker_error(token, "The '__set__' method must have two arguments.");
}

static void error_invalid_binary_arity(Token token, const char* name)
{
  checker_error(token, memory_sprintf("The '%s' method must have one argument.", name));
}

static void error_invalid_arity(Token token, int expected, int got)
{
  checker_error(token, memory_sprintf("Expected %d parameter(s) but got %d.", expected, got));
}

static void error_invalid_type_conversion(Token token)
{
  checker_error(token, "Invalid type conversion.");
}

static void error_imported_functions_cannot_have_bodies(Token token)
{
  checker_error(token, "An imported function cannot have a body.");
}

static void error_cannot_duduce_conflicting_type(Token token)
{
  checker_error(token, "This type conflicts with the other deduced types.");
}

static void error_incomplete_type(Token token)
{
  checker_error(token, "This type is incomplete.");
}

VarStmt* get_class_member(ClassStmt* class, Token token, const char* key, bool ignore_undeclared)
{
  if (!ignore_undeclared && !class->declared)
  {
    error_incomplete_type(token);
    return NULL;
  }

  return map_get_var_stmt(class->members, key);
}

ArrayVarStmt global_locals(void)
{
  return checker.global_locals;
}

bool equal_data_type(DataType left, DataType right)
{
  if (left.type == TYPE_OBJECT && right.type == TYPE_OBJECT && left.class && right.class)
    return left.class == right.class;

  if (left.type == TYPE_ARRAY && right.type == TYPE_ARRAY && left.array.data_type &&
      right.array.data_type)
    return left.array.count == right.array.count &&
           equal_data_type(*left.array.data_type, *right.array.data_type);

  return left.type == right.type;
}

bool assignable_data_type(DataType destination, DataType source)
{
  if (destination.type == TYPE_ANY)
    return source.type == TYPE_OBJECT || source.type == TYPE_STRING || source.type == TYPE_ARRAY;

  return false;
}

unsigned int array_data_type_hash(DataType array_data_type)
{
  assert(array_data_type.type == TYPE_ARRAY);
  assert(array_data_type.array.count >= 1);

  unsigned int hash = array_data_type.array.data_type->type << 8 | array_data_type.array.count;
  if (array_data_type.array.data_type->type == TYPE_OBJECT)
    hash |= array_data_type.array.data_type->class->id << 16;

  return hash;
}

DataType array_data_type_element(DataType array_data_type)
{
  assert(array_data_type.type == TYPE_ARRAY);
  assert(array_data_type.array.count >= 1);

  if (array_data_type.array.count == 1)
  {
    return *array_data_type.array.data_type;
  }
  else
  {
    DataType element_data_type = DATA_TYPE(TYPE_ARRAY);
    element_data_type.array.data_type = array_data_type.array.data_type;
    element_data_type.array.count = array_data_type.array.count - 1;

    return element_data_type;
  }
}

static void array_data_type_inference(DataType* source, DataType* target)
{
  if (source->type != TYPE_ARRAY || target->type != TYPE_ARRAY)
  {
    return;
  }

  if (source->array.data_type->type == TYPE_VOID ||
      source->array.data_type->type == target->array.data_type->type)
  {
    if (source->array.list)
    {
      LiteralArrayExpr* expression;
      array_foreach(source->array.list, expression)
      {
        expression->data_type.array.data_type->type = target->array.data_type->type;
      }
    }

    source->array.data_type->type = target->array.data_type->type;
  }
}

static DataType token_to_data_type(Token token)
{
  switch (token.type)
  {
  case TOKEN_IDENTIFIER_BOOL:
    return DATA_TYPE(TYPE_BOOL);
  case TOKEN_IDENTIFIER_VOID:
    return DATA_TYPE(TYPE_VOID);
  case TOKEN_IDENTIFIER_ANY:
    return DATA_TYPE(TYPE_ANY);
  case TOKEN_IDENTIFIER_INT:
    return DATA_TYPE(TYPE_INTEGER);
  case TOKEN_IDENTIFIER_FLOAT:
    return DATA_TYPE(TYPE_FLOAT);
  case TOKEN_IDENTIFIER_CHAR:
    return DATA_TYPE(TYPE_CHAR);
  case TOKEN_IDENTIFIER_STRING:
    return DATA_TYPE(TYPE_STRING);
  case TOKEN_IDENTIFIER: {
    VarStmt* variable = environment_get_variable(checker.environment, token.lexeme);
    if (!variable)
    {
      error_cannot_find_type(token, token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    if (equal_data_type(variable->data_type, DATA_TYPE(TYPE_PROTOTYPE)))
    {
      if (!variable->data_type.class->declared)
      {
        check_class_declaration(variable->data_type.class);
      }

      DataType object = DATA_TYPE(TYPE_OBJECT);
      object.class = variable->data_type.class;

      return object;
    }
    else if (equal_data_type(variable->data_type, DATA_TYPE(TYPE_ALIAS)))
    {
      return *variable->data_type.alias.data_type;
    }
    else
    {
      error_not_a_type(token, token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }
  }
  default:
    UNREACHABLE("Unhandled data type");
  }
}

static ClassStmt* template_to_data_type(DataType template, DataTypeToken template_type)
{
  template_type.count = 0;

  const char* name = data_type_token_to_string(template_type, NULL);
  VarStmt* variable = environment_get_variable(checker.environment, name);

  if (variable && equal_data_type(variable->data_type, DATA_TYPE(TYPE_PROTOTYPE)))
  {
    return variable->data_type.class;
  }

  static const int RECURSION_LIMIT = 32;
  if (template.class_template->count >= RECURSION_LIMIT)
  {
    error_recursive_template_type(template_type.token, name);
    return NULL;
  }

  template.class_template->count++;

  Stmt* statement = parser_parse_class_declaration_statement(template.class_template->offset,
                                                             template.class_template->keyword,
                                                             template.class_template->name);

  ClassStmt* class_statement = &statement->class;
  class_statement->name.lexeme = name;

  ClassStmt* previous_class = checker.class;
  FuncStmt* previous_function = checker.function;
  WhileStmt* previous_loop = checker.loop;
  Environment* previous_environment = checker.environment;

  checker.class = NULL;
  checker.function = NULL;
  checker.loop = NULL;
  checker.environment = checker.global_environment;

  init_statement(statement);

  checker.environment = environment_init(previous_environment);

  for (unsigned int i = 0; i < template.class_template->types.size; i++)
  {
    Token name = template.class_template->types.elems[i];
    DataTypeToken actual_data_type_token = array_at(template_type.types, i);

    VarStmt* variable = ALLOC(VarStmt);
    variable->name = name;
    variable->type = (DataTypeToken){ .token = name, .count = 0 };
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type.type = TYPE_ALIAS;
    variable->data_type.alias.token = actual_data_type_token;
    variable->data_type.alias.data_type = ALLOC(DataType);
    *variable->data_type.alias.data_type = data_type_token_to_data_type(actual_data_type_token);

    environment_set_variable(checker.environment, variable->name.lexeme, variable);
  }

  check_statement(statement, false);

  template.class_template->count--;

  checker.class = previous_class;
  checker.function = previous_function;
  checker.loop = previous_loop;
  checker.environment = previous_environment;

  array_add(&template.class_template->classes, class_statement);

  return class_statement;
}

static const char* data_type_to_string(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
    return "void";
  case TYPE_ANY:
    return "any";
  case TYPE_BOOL:
    return "bool";
  case TYPE_CHAR:
    return "char";
  case TYPE_INTEGER:
    return "int";
  case TYPE_FLOAT:
    return "float";
  case TYPE_STRING:
    return "string";
  case TYPE_ALIAS:
    return data_type_to_string(*data_type.alias.data_type);
  case TYPE_FUNCTION_INTERNAL:
    return data_type.function_internal.name;
  case TYPE_OBJECT:
    return data_type.class->name.lexeme;
  case TYPE_PROTOTYPE:
    return memory_sprintf("class %s", data_type.class->name.lexeme);
  case TYPE_PROTOTYPE_TEMPLATE: {
    ArrayChar string;
    array_init(&string);

    const char* c = "class ";
    while (*c)
      array_add(&string, *c++);

    c = data_type.class_template->name.lexeme;
    while (*c)
      array_add(&string, *c++);

    array_add(&string, '<');

    for (unsigned i = 0; i < data_type.class_template->types.size; i++)
    {
      const char* c = data_type.class_template->types.elems[i].lexeme;
      while (*c)
        array_add(&string, *c++);

      if (i < data_type.class_template->types.size - 1)
      {
        array_add(&string, ',');
        array_add(&string, ' ');
      }
    }

    array_add(&string, '>');
    array_add(&string, '\0');

    return string.elems;
  }
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER: {
    FuncStmt* function =
      data_type.type == TYPE_FUNCTION ? data_type.function : data_type.function_member.function;

    ArrayChar string;
    array_init(&string);

    const char* c = data_type_to_string(function->data_type);
    while (*c)
      array_add(&string, *c++);

    array_add(&string, '(');

    for (unsigned int i = 0; i < function->parameters.size; i++)
    {
      const char* c = data_type_to_string(function->parameters.elems[i]->data_type);
      while (*c)
        array_add(&string, *c++);

      if (i < function->parameters.size - 1)
      {
        array_add(&string, ',');
        array_add(&string, ' ');
      }
    }

    array_add(&string, ')');
    array_add(&string, '\0');

    return string.elems;
  }
  case TYPE_ARRAY: {
    ArrayChar string;
    array_init(&string);

    const char* c = data_type_to_string(*data_type.array.data_type);
    while (*c)
      array_add(&string, *c++);

    for (int i = 0; i < data_type.array.count; i++)
    {
      array_add(&string, '[');
      array_add(&string, ']');
    }

    array_add(&string, '\0');

    return string.elems;
  }
  default:
    UNREACHABLE("Unexpected data type to string");
  }
}

static const char* data_type_token_to_string(DataTypeToken type, ArrayChar* string)
{
  if (string == NULL)
  {
    ArrayChar string;
    array_init(&string);
    data_type_token_to_string(type, &string);
    array_add(&string, '\0');

    return string.elems;
  }
  else
  {
    const char* c = type.token.lexeme;
    while (*c)
      array_add(string, *c++);

    if (array_size(type.types))
    {
      array_add(string, '<');

      for (unsigned int i = 0; i < array_size(type.types); i++)
      {
        data_type_token_to_string(array_at(type.types, i), string);

        if (i != array_size(type.types) - 1)
        {
          array_add(string, ',');
          array_add(string, ' ');
        }
      }

      array_add(string, '>');
    }

    for (int i = 0; i < type.count; i++)
    {
      array_add(string, '[');
      array_add(string, ']');
    }

    return string->elems;
  }
}

static void data_type_token_unalias(ArrayDataTypeToken* types)
{
  for (unsigned int i = 0; i < types->size; i++)
  {
    VarStmt* variable =
      environment_get_variable(checker.environment, array_at(types, i).token.lexeme);

    if (variable && equal_data_type(variable->data_type, DATA_TYPE(TYPE_ALIAS)))
    {
      array_at(types, i) = variable->data_type.alias.token;
    }

    data_type_token_unalias(array_at(types, i).types);
  }
}

static DataType data_type_token_to_data_type(DataTypeToken type)
{
  if (type.types && array_size(type.types))
  {
    if (type.token.type != TOKEN_IDENTIFIER)
    {
      error_not_a_template_type(type.token, type.token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    VarStmt* variable = environment_get_variable(checker.environment, type.token.lexeme);
    if (!variable)
    {
      error_cannot_find_type(type.token, type.token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    if (!equal_data_type(variable->data_type, DATA_TYPE(TYPE_PROTOTYPE_TEMPLATE)))
    {
      error_not_a_template_type(type.token, type.token.lexeme);
      return DATA_TYPE(TYPE_VOID);
    }

    int expected = array_size(&variable->data_type.class_template->types);
    int got = array_size(type.types);

    if (expected != got)
    {
      error_invalid_template_arity(type.token, expected, got);
      return DATA_TYPE(TYPE_VOID);
    }

    data_type_token_unalias(type.types);

    ClassStmt* class_statement = template_to_data_type(variable->data_type, type);
    if (!class_statement)
    {
      return DATA_TYPE(TYPE_VOID);
    }

    type.token.lexeme = class_statement->name.lexeme;
  }

  if (type.count > 0)
  {
    DataType element_data_type = token_to_data_type(type.token);

    DataType data_type = DATA_TYPE(TYPE_ARRAY);
    data_type.array.data_type = ALLOC(DataType);
    data_type.array.count = type.count;

    if (equal_data_type(element_data_type, DATA_TYPE(TYPE_ARRAY)))
    {
      data_type.array.count += element_data_type.array.count;
      *data_type.array.data_type = *element_data_type.array.data_type;
    }
    else
    {
      *data_type.array.data_type = element_data_type;
    }

    return data_type;
  }
  else
  {
    return token_to_data_type(type.token);
  }
}

static Expr* cast_to_bool(Expr* expression, DataType data_type)
{
  if (equal_data_type(data_type, DATA_TYPE(TYPE_OBJECT)) ||
      equal_data_type(data_type, DATA_TYPE(TYPE_INTEGER)))
  {
    Expr* cast_expression = EXPR();
    cast_expression->type = EXPR_CAST;
    cast_expression->cast.type = (DataTypeToken){ .token = TOKEN_EMPTY(), .count = 0 };
    cast_expression->cast.from_data_type = data_type;
    cast_expression->cast.to_data_type = DATA_TYPE(TYPE_BOOL);
    cast_expression->cast.expr = expression;

    return cast_expression;
  }

  return NULL;
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
  cast_expression->cast.type = (DataTypeToken){ .token = TOKEN_EMPTY(), .count = 0 };
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
    if (strcmp(name, "__init__") == 0 && statement->type.token.type != TOKEN_IDENTIFIER_VOID)
    {
      error_invalid_initializer_return_type(statement->name);
      return;
    }

    VarStmt* parameter = ALLOC(VarStmt);
    parameter->name = (Token){ .lexeme = "this" };
    parameter->type = (DataTypeToken){ .token = checker.class->name, .count = 0 };
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

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    parameter->data_type = data_type_token_to_data_type(parameter->type);
  }

  statement->data_type = data_type_token_to_data_type(statement->type);

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

static void init_class_template_declaration(ClassTemplateStmt* statement)
{
  const char* name = statement->name.lexeme;
  if (environment_check_variable(checker.environment, name))
  {
    error_name_already_exists(statement->name, name);
  }

  MapSInt type_set;
  map_init_sint(&type_set, 0, 0);

  Token type;
  array_foreach(&statement->types, type)
  {
    if (map_get_sint(&type_set, type.lexeme))
      error_name_already_exists(type, type.lexeme);
    else
      map_put_sint(&type_set, type.lexeme, 1);
  }

  VarStmt* variable = ALLOC(VarStmt);
  variable->name = statement->name;
  variable->type = (DataTypeToken){ .token = statement->name, .count = 0 };
  variable->initializer = NULL;
  variable->scope = SCOPE_GLOBAL;
  variable->index = -1;
  variable->data_type = DATA_TYPE(TYPE_PROTOTYPE_TEMPLATE);
  variable->data_type.class_template = statement;

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
  variable->type = (DataTypeToken){ .token = statement->name, .count = 0 };
  variable->initializer = NULL;
  variable->scope = SCOPE_GLOBAL;
  variable->index = -1;
  variable->data_type = DATA_TYPE(TYPE_PROTOTYPE);
  variable->data_type.class = statement;

  environment_set_variable(checker.environment, name, variable);
}

static void init_import_declaration(ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    init_statement(body_statement);
  }
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
  case STMT_CLASS_TEMPLATE_DECL:
    init_class_template_declaration(&statement->class_template);
    break;
  case STMT_IMPORT_DECL:
    init_import_declaration(&statement->import);
    break;

  default:
    break;
  }
}

static DataType check_cast_expression(CastExpr* expression)
{
  if (equal_data_type(expression->from_data_type, DATA_TYPE(TYPE_VOID)) &&
      equal_data_type(expression->to_data_type, DATA_TYPE(TYPE_VOID)))
  {
    expression->from_data_type = check_expression(expression->expr);
    expression->to_data_type = data_type_token_to_data_type(expression->type);

    bool valid = false;

    switch (expression->from_data_type.type)
    {
    case TYPE_CHAR:
      switch (expression->to_data_type.type)
      {
      case TYPE_CHAR:
      case TYPE_INTEGER:
      case TYPE_STRING:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_INTEGER:
      switch (expression->to_data_type.type)
      {
      case TYPE_INTEGER:
      case TYPE_BOOL:
      case TYPE_FLOAT:
      case TYPE_STRING:
      case TYPE_CHAR:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_FLOAT:
      switch (expression->to_data_type.type)
      {
      case TYPE_FLOAT:
      case TYPE_BOOL:
      case TYPE_INTEGER:
      case TYPE_STRING:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_BOOL:
      switch (expression->to_data_type.type)
      {
      case TYPE_BOOL:
      case TYPE_FLOAT:
      case TYPE_INTEGER:
      case TYPE_STRING:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_STRING:
      switch (expression->to_data_type.type)
      {
      case TYPE_STRING:
      case TYPE_ANY:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_ARRAY:
      switch (expression->to_data_type.type)
      {
      case TYPE_ARRAY:
      case TYPE_ANY:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_OBJECT:
      switch (expression->to_data_type.type)
      {
      case TYPE_OBJECT:
      case TYPE_ANY:
        valid = true;

      default:
        break;
      }

      break;
    case TYPE_ANY:
      switch (expression->to_data_type.type)
      {
      case TYPE_ANY:
      case TYPE_STRING:
      case TYPE_ARRAY:
      case TYPE_OBJECT:
        valid = true;

      default:
        break;
      }

      break;
    default:
      break;
    }

    if (!valid)
    {
      error_invalid_type_conversion(expression->type.token);
    }
  }

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
  DataType data_type = check_expression(expression->expr);
  Token op = expression->op;

  switch (op.type)
  {
  case TOKEN_MINUS:
    if (!equal_data_type(data_type, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(data_type, DATA_TYPE(TYPE_FLOAT)))
      error_operation_not_defined(op, "'int' and 'float'");

    expression->data_type = data_type;
    break;

  case TOKEN_TILDE:
    if (!equal_data_type(data_type, DATA_TYPE(TYPE_INTEGER)))
      error_operation_not_defined(op, "'int'");

    expression->data_type = data_type;
    break;

  case TOKEN_NOT:
  case TOKEN_BANG:
    if (!equal_data_type(data_type, DATA_TYPE(TYPE_BOOL)))
    {
      Expr* cast_expression = cast_to_bool(expression->expr, data_type);
      if (cast_expression)
        expression->expr = cast_expression;
      else
        error_type_mismatch(op, DATA_TYPE(TYPE_BOOL), data_type);
    }

    expression->data_type = DATA_TYPE(TYPE_BOOL);
    break;

  default:
    UNREACHABLE("Unexpected unary operator");
  }

  return expression->data_type;
}

static DataType check_binary_expression(BinaryExpr* expression)
{
  Token op = expression->op;
  DataType left = check_expression(expression->left);
  DataType right = check_expression(expression->right);

  if (equal_data_type(left, DATA_TYPE(TYPE_OBJECT)))
  {
    ClassStmt* class = left.class;
    char* name;

    switch (op.type)
    {
    case TOKEN_PLUS:
      name = "__add__";
      break;
    case TOKEN_MINUS:
      name = "__sub__";
      break;
    case TOKEN_SLASH:
      name = "__div__";
      break;
    case TOKEN_STAR:
      name = "__mul__";
      break;

    case TOKEN_PERCENT:
      name = "__mod__";
      break;
    case TOKEN_AMPERSAND:
      name = "__and__";
      break;
    case TOKEN_PIPE:
      name = "__or__";
      break;
    case TOKEN_CARET:
      name = "__xor__";
      break;
    case TOKEN_LESS_LESS:
      name = "__lshift__";
      break;
    case TOKEN_GREATER_GREATER:
      name = "__rshift__";
      break;

    case TOKEN_LESS:
      name = "__lt__";
      break;
    case TOKEN_LESS_EQUAL:
      name = "__le__";
      break;
    case TOKEN_GREATER:
      name = "__gt__";
      break;
    case TOKEN_GREATER_EQUAL:
      name = "__ge__";
      break;
    case TOKEN_EQUAL_EQUAL:
      name = "__eq__";
      break;
    case TOKEN_BANG_EQUAL:
      name = "__ne__";
      break;
    default:
      goto skip;
    }

    VarStmt* variable = get_class_member(class, op, name, false);
    if (!variable || !equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
    {
      if (op.type == TOKEN_EQUAL_EQUAL || op.type == TOKEN_BANG_EQUAL)
      {
        goto skip;
      }

      error_missing_operator_overload(op, name);
      return DATA_TYPE(TYPE_VOID);
    }

    FuncStmt* function = variable->data_type.function_member.function;
    if (!equal_data_type(right, array_at(&function->parameters, 1)->data_type))
    {
      error_type_mismatch(op, array_at(&function->parameters, 1)->data_type, right);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->return_data_type = function->data_type;
    expression->operand_data_type = left;

    return expression->return_data_type;
  }

skip:

  if (!equal_data_type(left, right))
  {
    if (!upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_FLOAT)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_CHAR), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FLOAT), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_BOOL), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_BOOL)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_OBJECT), DATA_TYPE(TYPE_BOOL)))
    {
      error_type_mismatch(expression->op, left, right);
    }
  }

  expression->return_data_type = left;
  expression->operand_data_type = left;

  switch (op.type)
  {
  case TOKEN_AND:
  case TOKEN_OR:
    if (!equal_data_type(left, DATA_TYPE(TYPE_BOOL)))
    {
      Expr* left_cast_expression = cast_to_bool(expression->left, left);
      Expr* right_cast_expression = cast_to_bool(expression->right, right);

      if (!left_cast_expression || !right_cast_expression)
      {
        error_operation_not_defined(op, "'bool'");
      }
      else
      {
        expression->left = left_cast_expression;
        expression->right = right_cast_expression;
      }
    }

    expression->operand_data_type = DATA_TYPE(TYPE_BOOL);
    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_BOOL)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_OBJECT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_CHAR)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_STRING)))
      error_operation_not_defined(op, "'int', 'float', 'bool', 'object', 'char' and 'string'");

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_BOOL)))
      error_operation_not_defined(op, "'int', 'float' and 'bool'");

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_PLUS:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_STRING)))
      error_operation_not_defined(op, "'int', 'float' and 'string'");

    break;
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    if (!equal_data_type(left, DATA_TYPE(TYPE_INTEGER)) &&
        !equal_data_type(left, DATA_TYPE(TYPE_FLOAT)))
      error_operation_not_defined(op, "'int' and 'float'");

    break;

  case TOKEN_PERCENT:
  case TOKEN_AMPERSAND:
  case TOKEN_PIPE:
  case TOKEN_CARET:
  case TOKEN_LESS_LESS:
  case TOKEN_GREATER_GREATER:
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

  array_data_type_inference(&value_data_type, &target_data_type);

  if (!equal_data_type(target_data_type, value_data_type) &&
      !assignable_data_type(target_data_type, value_data_type))
  {
    error_type_mismatch(expression->op, target_data_type, value_data_type);
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
    if (!target->access.variable)
    {
      error_not_assignable(expression->op);
      return DATA_TYPE(TYPE_VOID);
    }

    VarStmt* variable = target->access.variable;
    expression->variable = variable;
    expression->data_type = variable->data_type;

    return expression->data_type;
  }
  else if (target->type == EXPR_INDEX)
  {
    if (equal_data_type(target->index.expr_data_type, DATA_TYPE(TYPE_STRING)))
    {
      error_not_assignable(expression->op);
      return DATA_TYPE(TYPE_VOID);
    }

    if (equal_data_type(target->index.expr_data_type, DATA_TYPE(TYPE_OBJECT)))
    {
      ClassStmt* class = target->index.expr_data_type.class;
      VarStmt* variable = get_class_member(class, target->index.expr_token, "__set__", false);

      if (!variable || !equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
      {
        error_not_indexable_and_assignable_missing_overload(expression->op);
        return DATA_TYPE(TYPE_VOID);
      }
    }

    expression->variable = NULL;
    expression->data_type = value_data_type;

    return expression->data_type;
  }

  error_not_assignable(expression->op);
  return DATA_TYPE(TYPE_VOID);
}

static DataType check_call_expression(CallExpr* expression)
{
  Expr* callee = expression->callee;
  DataType callee_data_type = check_expression(callee);

  if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_PROTOTYPE_TEMPLATE)))
  {
    int expected = array_size(&callee_data_type.class_template->types);
    int got = array_size(&expression->types);

    if (expected != got)
    {
      error_invalid_template_arity(expression->callee_token, expected, got);
      return DATA_TYPE(TYPE_VOID);
    }

    data_type_token_unalias(&expression->types);

    DataTypeToken class_type = {
      .token = callee_data_type.class_template->name,
      .types = &expression->types,
      .count = 0,
    };

    ClassStmt* class_statement = template_to_data_type(callee_data_type, class_type);
    if (!class_statement)
    {
      return DATA_TYPE(TYPE_VOID);
    }

    callee_data_type.type = TYPE_PROTOTYPE;
    callee_data_type.class = class_statement;
  }

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

      DataType argument_data_type = check_expression(argument);
      DataType parameter_data_type = parameter->data_type;

      array_data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(parameter_data_type, argument_data_type))
      {
        error_type_mismatch(expression->argument_tokens.elems[i - 1], argument_data_type,
                            parameter_data_type);
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

      DataType argument_data_type = check_expression(argument);
      DataType parameter_data_type = parameter->data_type;

      array_data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(parameter_data_type, argument_data_type))
      {
        error_type_mismatch(expression->argument_tokens.elems[i], argument_data_type,
                            parameter_data_type);
      }
    }

    expression->return_data_type = function->data_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_FUNCTION_INTERNAL)))
  {
    if (callee_data_type.function_internal.this)
    {
      ArrayExpr arguments;
      Expr* argument = callee_data_type.function_member.this;

      array_init(&arguments);
      array_add(&arguments, argument);
      array_foreach(&expression->arguments, argument)
      {
        array_add(&arguments, argument);
      }

      expression->arguments = arguments;
    }

    int number_of_arguments = array_size(&expression->arguments);
    int expected_number_of_arguments =
      array_size(&callee_data_type.function_internal.parameter_types);

    if (number_of_arguments != expected_number_of_arguments)
    {
      error_invalid_arity(expression->callee_token, expected_number_of_arguments,
                          number_of_arguments);
      return DATA_TYPE(TYPE_VOID);
    }

    for (int i = 0; i < number_of_arguments; i++)
    {
      Expr* argument = expression->arguments.elems[i];

      DataType argument_data_type = check_expression(argument);
      DataType parameter_data_type = callee_data_type.function_internal.parameter_types.elems[i];

      array_data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(parameter_data_type, argument_data_type))
      {
        Token argument_token;

        if (callee_data_type.function_internal.this)
          argument_token = expression->argument_tokens.elems[i - 1];
        else
          argument_token = expression->argument_tokens.elems[i];

        error_type_mismatch(argument_token, argument_data_type, parameter_data_type);
      }
    }

    expression->return_data_type = *callee_data_type.function_internal.return_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_PROTOTYPE)))
  {
    ClassStmt* class = callee_data_type.class;
    VarStmt* variable = get_class_member(class, expression->callee_token, "__init__", true);

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

        DataType argument_data_type = check_expression(argument);
        DataType parameter_data_type = parameter->data_type;

        array_data_type_inference(&argument_data_type, &parameter_data_type);

        if (!equal_data_type(argument_data_type, parameter_data_type) &&
            !assignable_data_type(parameter_data_type, argument_data_type))
        {
          error_type_mismatch(expression->argument_tokens.elems[i - 1], argument_data_type,
                              parameter_data_type);
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
    expression->return_data_type = token_to_data_type(class->name);

    return expression->return_data_type;
  }
  else if (equal_data_type(callee_data_type, DATA_TYPE(TYPE_ALIAS)))
  {
    int number_of_arguments = array_size(&expression->arguments);
    if (number_of_arguments > 1)
    {
      error_invalid_arity(expression->callee_token, 0, number_of_arguments);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->callee_data_type = callee_data_type;
    expression->return_data_type = *callee_data_type.alias.data_type;

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
    VarStmt* variable = get_class_member(class, expression->name, name, false);

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
    expression->expr_data_type = data_type;

    return expression->data_type;
  }
  else if (equal_data_type(data_type, DATA_TYPE(TYPE_ARRAY)))
  {
    const char* name = expression->name.lexeme;
    if (strcmp("length", name) == 0 || strcmp("capacity", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_INTEGER);
      expression->expr_data_type = data_type;
      expression->variable = NULL;

      return expression->data_type;
    }
    else if (strcmp("push", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.push";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_VOID;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types,
                array_data_type_element(data_type));

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("pop", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.pop";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      *expression->data_type.function_internal.return_type = array_data_type_element(data_type);

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, "array");
    return DATA_TYPE(TYPE_VOID);
  }
  else if (equal_data_type(data_type, DATA_TYPE(TYPE_STRING)))
  {
    const char* name = expression->name.lexeme;
    if (strcmp("length", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_INTEGER);
      expression->expr_data_type = data_type;
      expression->variable = NULL;

      return expression->data_type;
    }
    else if (strcmp("hash", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.hash";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_INTEGER;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, "string");
    return DATA_TYPE(TYPE_VOID);
  }
  else if (equal_data_type(data_type, DATA_TYPE(TYPE_INTEGER)) ||
           equal_data_type(data_type, DATA_TYPE(TYPE_FLOAT)) ||
           equal_data_type(data_type, DATA_TYPE(TYPE_CHAR)) ||
           equal_data_type(data_type, DATA_TYPE(TYPE_BOOL)))
  {
    const char* name = expression->name.lexeme;

    if (strcmp("hash", name) == 0)
    {
      const char* function_name;
      switch (data_type.type)
      {

      case TYPE_BOOL:
      case TYPE_CHAR:
      case TYPE_INTEGER:
        function_name = "int.hash";
        break;
      case TYPE_FLOAT:
        function_name = "float.hash";
        break;
      default:
        UNREACHABLE("Unknown data type hash");
      }

      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = function_name;
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_INTEGER;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, "string");
    return DATA_TYPE(TYPE_VOID);
  }
  else
  {
    error_not_an_object(expression->expr_token);
    return DATA_TYPE(TYPE_VOID);
  }
}

static DataType check_index_expression(IndexExpr* expression)
{
  DataType index_data_type = check_expression(expression->index);
  DataType expr_data_type = check_expression(expression->expr);

  if (equal_data_type(expr_data_type, DATA_TYPE(TYPE_STRING)))
  {
    if (!equal_data_type(index_data_type, DATA_TYPE(TYPE_INTEGER)))
    {
      error_index_not_an_int(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->data_type = DATA_TYPE(TYPE_CHAR);
    expression->expr_data_type = expr_data_type;

    return expression->data_type;
  }
  else if (equal_data_type(expr_data_type, DATA_TYPE(TYPE_ARRAY)))
  {
    if (!equal_data_type(index_data_type, DATA_TYPE(TYPE_INTEGER)))
    {
      error_index_not_an_int(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->data_type = array_data_type_element(expr_data_type);
    expression->expr_data_type = expr_data_type;

    return expression->data_type;
  }
  else if (equal_data_type(expr_data_type, DATA_TYPE(TYPE_OBJECT)))
  {
    ClassStmt* class = expr_data_type.class;
    VarStmt* variable = get_class_member(class, expression->expr_token, "__get__", false);

    if (!variable || !equal_data_type(variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
    {
      error_not_indexable_missing_overload(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    FuncStmt* function = variable->data_type.function_member.function;
    if (!equal_data_type(index_data_type, array_at(&function->parameters, 1)->data_type))
    {
      error_type_mismatch(expression->index_token, index_data_type,
                          array_at(&function->parameters, 1)->data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->data_type = function->data_type;
    expression->expr_data_type = expr_data_type;

    return expression->data_type;
  }

  error_not_indexable(expression->expr_token);
  return DATA_TYPE(TYPE_VOID);
}

NO_OPTIMIZATION static DataType check_array_expression(LiteralArrayExpr* expression)
{
  DataType data_type = DATA_TYPE(TYPE_ARRAY);
  DataType element_data_type = DATA_TYPE(TYPE_VOID);
  bool initializer = false;

  if (!checker.array)
  {
    checker.array = ALLOC(ArrayLiteralArrayExpr);
    initializer = true;

    array_init(checker.array);
  }

  array_add(checker.array, expression);

  for (unsigned int i = 0; i < expression->values.size; i++)
  {
    Expr* value = expression->values.elems[i];
    Token token = expression->tokens.elems[i];

    DataType value_data_type = check_expression(value);
    if (equal_data_type(element_data_type, DATA_TYPE(TYPE_VOID)))
    {
      element_data_type = value_data_type;
    }

    if (value_data_type.type == TYPE_ARRAY && element_data_type.type == TYPE_ARRAY &&
        value_data_type.array.data_type->type != TYPE_VOID &&
        element_data_type.array.data_type->type == TYPE_VOID &&
        element_data_type.array.count == value_data_type.array.count)
    {
      element_data_type.array.data_type->type = value_data_type.array.data_type->type;
    }

    if (element_data_type.type == TYPE_ARRAY && value_data_type.type == TYPE_ARRAY &&
        element_data_type.array.data_type->type != TYPE_VOID &&
        value_data_type.array.data_type->type == TYPE_VOID &&
        value_data_type.array.count == element_data_type.array.count)
    {
      value_data_type.array.data_type->type = element_data_type.array.data_type->type;
    }

    if (!equal_data_type(element_data_type, value_data_type))
    {
      error_cannot_duduce_conflicting_type(token);
      data_type = DATA_TYPE(TYPE_VOID);

      goto finish;
    }
  }

  if (equal_data_type(element_data_type, DATA_TYPE(TYPE_ARRAY)))
  {
    data_type.array.count = element_data_type.array.count + 1;
    data_type.array.data_type = element_data_type.array.data_type;
    data_type.array.list = checker.array;
  }
  else
  {
    data_type.array.count = 1;
    data_type.array.data_type = ALLOC(DataType);
    *data_type.array.data_type = element_data_type;
    data_type.array.list = checker.array;
  }

finish:
  if (initializer)
  {
    checker.array = NULL;
  }

  expression->data_type = data_type;
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
  case EXPR_ACCESS:
    return check_access_expression(&expression->access);
  case EXPR_INDEX:
    return check_index_expression(&expression->index);
  case EXPR_ARRAY:
    return check_array_expression(&expression->array);

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

  DataType data_type = DATA_TYPE(TYPE_VOID);

  if (statement->expr)
  {
    data_type = check_expression(statement->expr);

    array_data_type_inference(&data_type, &checker.function->data_type);
  }

  if (!equal_data_type(checker.function->data_type, data_type) &&
      !assignable_data_type(checker.function->data_type, data_type))
  {
    error_type_mismatch(statement->keyword, checker.function->data_type, data_type);
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
    Expr* cast_expression = cast_to_bool(statement->condition, data_type);
    if (cast_expression)
      statement->condition = cast_expression;
    else
      error_condition_is_not_bool(statement->keyword);
  }

  checker.environment = environment_init(checker.environment);

  Stmt* body_statement;
  array_foreach(&statement->then_branch, body_statement)
  {
    check_statement(body_statement, true);
  }

  checker.environment = checker.environment->parent;

  if (statement->else_branch.elems)
  {
    checker.environment = environment_init(checker.environment);

    Stmt* body_statement;
    array_foreach(&statement->else_branch, body_statement)
    {
      check_statement(body_statement, true);
    }

    checker.environment = checker.environment->parent;
  }
}

static void check_while_statement(WhileStmt* statement)
{
  checker.environment = environment_init(checker.environment);

  if (statement->initializer)
  {
    check_statement(statement->initializer, true);
  }

  DataType data_type = check_expression(statement->condition);
  if (!equal_data_type(data_type, DATA_TYPE(TYPE_BOOL)))
  {
    Expr* cast_expression = cast_to_bool(statement->condition, data_type);
    if (cast_expression)
      statement->condition = cast_expression;
    else
      error_condition_is_not_bool(statement->keyword);
  }

  WhileStmt* previous_loop = checker.loop;
  checker.environment = environment_init(checker.environment);
  checker.loop = statement;

  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    check_statement(body_statement, true);
  }

  checker.loop = previous_loop;
  checker.environment = checker.environment->parent;

  if (statement->incrementer)
  {
    check_statement(statement->incrementer, true);
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
    if (environment == checker.global_environment)
    {
      statement->scope = SCOPE_GLOBAL;
    }
    else
    {
      statement->scope = SCOPE_LOCAL;
      statement->index = array_size(&checker.global_locals);
      array_add(&checker.global_locals, statement);
    }
  }

  if (environment_check_variable(environment, name))
  {
    error_name_already_exists(statement->name, name);
    return;
  }

  statement->data_type = data_type_token_to_data_type(statement->type);

  if (equal_data_type(statement->data_type, DATA_TYPE(TYPE_VOID)))
  {
    error_type_cannot_be_void(statement->type.token, statement->type.token.lexeme);
    return;
  }

  if (statement->initializer)
  {
    DataType initializer_data_type = check_expression(statement->initializer);

    array_data_type_inference(&initializer_data_type, &statement->data_type);

    if (!equal_data_type(statement->data_type, initializer_data_type) &&
        !assignable_data_type(statement->data_type, initializer_data_type))
    {
      error_type_mismatch(statement->equals, statement->data_type, initializer_data_type);
    }
  }

  if (checker.function)
  {
    array_add(&checker.function->variables, statement);
  }

  environment_set_variable(environment, name, statement);
}

static void check_get_function_declaration(FuncStmt* function)
{
  if (strcmp(function->name.lexeme + checker.class->name.length + 1, "__get__") == 0)
  {
    int got = array_size(&function->parameters);
    int expected = 2;

    if (expected != got)
    {
      error_invalid_get_arity(function->name);
      return;
    }
  }
}

static void check_set_function_declaration(FuncStmt* function)
{
  if (strcmp(function->name.lexeme + checker.class->name.length + 1, "__set__") == 0)
  {
    int got = array_size(&function->parameters);
    int expected = 3;

    if (expected != got)
    {
      error_invalid_set_arity(function->name);
      return;
    }

    if (!equal_data_type(function->data_type, DATA_TYPE(TYPE_VOID)))
    {
      error_invalid_set_return_type(function->name);
      return;
    }
  }
}

static void check_binary_overload_function_declaration(FuncStmt* function, const char* name)
{
  if (strcmp(function->name.lexeme + checker.class->name.length + 1, name) == 0)
  {
    int got = array_size(&function->parameters);
    int expected = 2;

    if (expected != got)
    {
      error_invalid_binary_arity(function->name, name);
      return;
    }
  }
}

static void check_function_declaration(FuncStmt* statement)
{
  if (checker.function || checker.loop)
  {
    error_unexpected_function(statement->name);
    return;
  }

  if (statement->body.size && statement->import.type == TOKEN_STRING)
  {
    error_imported_functions_cannot_have_bodies(statement->name);
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

    environment_set_variable(checker.environment, name, parameter);
  }

  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    check_statement(body_statement, true);
  }

  if (checker.class)
  {
    check_get_function_declaration(statement);
    check_set_function_declaration(statement);

    check_binary_overload_function_declaration(statement, "__add__");
    check_binary_overload_function_declaration(statement, "__sub__");
    check_binary_overload_function_declaration(statement, "__div__");
    check_binary_overload_function_declaration(statement, "__mul__");

    check_binary_overload_function_declaration(statement, "__mod__");
    check_binary_overload_function_declaration(statement, "__and__");
    check_binary_overload_function_declaration(statement, "__or_");
    check_binary_overload_function_declaration(statement, "__xor__");
    check_binary_overload_function_declaration(statement, "__lshift__");
    check_binary_overload_function_declaration(statement, "__rshift__");

    check_binary_overload_function_declaration(statement, "__lt__");
    check_binary_overload_function_declaration(statement, "__le__");
    check_binary_overload_function_declaration(statement, "__gt__");
    check_binary_overload_function_declaration(statement, "__ge__");
    check_binary_overload_function_declaration(statement, "__eq__");
    check_binary_overload_function_declaration(statement, "__ne__");
  }

  checker.function = previous_function;
  checker.environment = checker.environment->parent;
}

static void check_set_get_function_declarations(ClassStmt* statement)
{
  VarStmt* set_variable = map_get_var_stmt(statement->members, "__set__");
  VarStmt* get_variable = map_get_var_stmt(statement->members, "__get__");

  if (set_variable && get_variable &&
      equal_data_type(set_variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)) &&
      equal_data_type(get_variable->data_type, DATA_TYPE(TYPE_FUNCTION_MEMBER)))
  {
    FuncStmt* set_function = set_variable->data_type.function_member.function;
    FuncStmt* get_function = get_variable->data_type.function_member.function;

    if (set_function->parameters.size != 3 || get_function->parameters.size != 2)
      return;

    if (!equal_data_type(get_function->data_type, set_function->parameters.elems[2]->data_type))
    {
      error_invalid_get_set_function(get_function->name);
      return;
    }

    if (!equal_data_type(get_function->parameters.elems[1]->data_type,
                         set_function->parameters.elems[1]->data_type))
    {
      error_invalid_get_set_first_parameter_function(set_function->parameters.elems[1]->type.token);
      return;
    }
  }
}

static void check_class_declaration(ClassStmt* statement)
{
  if (checker.function || checker.loop)
  {
    error_unexpected_class(statement->name);
    return;
  }

  if (statement->initializing)
  {
    return;
  }

  statement->initializing = true;

  ClassStmt* previous_class = checker.class;
  checker.environment = environment_init(checker.environment);
  checker.class = statement;
  checker.class->members = &checker.environment->variables;

  FuncStmt* function_statement;
  array_foreach(&statement->functions, function_statement)
  {
    init_function_declaration(function_statement);

    const char* class_name = statement->name.lexeme;
    const char* function_name = function_statement->name.lexeme;

    function_statement->data_type = data_type_token_to_data_type(function_statement->type);
    function_statement->name.lexeme = memory_sprintf("%s.%s", class_name, function_name);
  }

  int count = 0;
  VarStmt* variable_statement;
  array_foreach(&statement->variables, variable_statement)
  {
    variable_statement->index = count++;

    check_variable_declaration(variable_statement);
  }

  statement->declared = true;

  array_foreach(&statement->functions, function_statement)
  {
    check_function_declaration(function_statement);
  }

  check_set_get_function_declarations(statement);

  checker.class = previous_class;
  checker.environment = checker.environment->parent;
}

static void check_import_declaration(ImportStmt* statement)
{
  if (checker.function || checker.loop || checker.class)
  {
    error_unexpected_import(statement->keyword);
    return;
  }

  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    check_statement(body_statement, true);
  }
}

static void check_statement(Stmt* statement, bool synchronize)
{
  if (synchronize)
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
  case STMT_IMPORT_DECL:
    check_import_declaration(&statement->import);
    break;

  case STMT_CLASS_TEMPLATE_DECL:
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
  checker.array = NULL;
  checker.statements = statements;

  checker.environment = environment_init(NULL);
  checker.global_environment = checker.environment;

  array_init(&checker.global_locals);
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
    check_statement(statement, true);
  }
}

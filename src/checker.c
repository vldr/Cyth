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
  ArrayStmt statements;

  Environment* environment;
  Environment* global_environment;
  ArrayVarStmt global_locals;

  FuncStmt* function;
  ClassStmt* class;
  IfStmt* cond;
  Token* template;
  WhileStmt* loop;
  AssignExpr* assignment;

  bool error;
  int errors;
  void (*error_callback)(int start_line, int start_column, int end_line, int end_column,
                         const char* message);

} checker;

static void check_statement(Stmt* statement, bool synchronize);
static DataType check_expression(Expr* expression);

static void init_function_declaration(FuncStmt* statement);
static void check_function_declaration(FuncStmt* statement);

static void init_class_declaration(ClassStmt* statement);
static void init_class_declaration_body(ClassStmt* statement);

static bool analyze_statement(Stmt* statement);
static bool analyze_statements(ArrayStmt statements);

static bool autocast(Expr** expression, DataType from, DataType to);
static bool autocast_int_literal_to_float_literal(Expr** expression);

static const char* data_type_token_to_string(DataTypeToken type, ArrayChar* string);
static DataType data_type_token_to_data_type(DataTypeToken type);

static void error(Token token, const char* message)
{
  if (checker.template)
    message =
      memory_sprintf("%s (occurred when creating %s at %d:%d)", message, checker.template->lexeme,
                     checker.template->start_line, checker.template->start_column);

  if (!checker.error)
    checker.error_callback(token.start_line, token.start_column, token.end_line, token.end_column,
                           message);

  checker.error = true;
  checker.errors++;
}

static void error_type_mismatch(Token token, DataType expected, DataType got)
{
  error(token, memory_sprintf("Mismatched types, expected '%s' but got '%s'.",
                              data_type_to_string(expected), data_type_to_string(got)));
}

static void error_should_not_return_value_in_top_level(Token token)
{
  error(token, "Should not return a value here.");
}

static void error_should_not_return_value(Token token, const char* function_name)
{
  error(token, memory_sprintf("Void function '%s' should not return a value.", function_name));
}

static void error_should_return_value(Token token, const char* function_name)
{
  error(token, memory_sprintf("Non-void function '%s' should return a value.", function_name));
}

static void error_operation_not_defined(Token token, DataType data_type)
{
  error(token, memory_sprintf("Operator '%s' is not defined for '%s'.", token.lexeme,
                              data_type_to_string(data_type)));
}

static void error_unknown_operation(Token token)
{
  error(token, memory_sprintf("Operator '%s' is not valid.", token.lexeme));
}

static void error_missing_operator_overload(Token token, DataType left_data_type,
                                            DataType right_data_type, const char* function_name)
{
  error(token, memory_sprintf("Operator '%s' is not defined for '%s' and '%s' (missing %s method).",
                              token.lexeme, data_type_to_string(left_data_type),
                              data_type_to_string(right_data_type), function_name));
}

static void error_name_already_exists(Token token, const char* name)
{
  error(token, memory_sprintf("The name '%s' already exists.", name));
}

static void error_missing_template_types(Token token)
{
  error(token, "Missing template types here.");
}

static void error_type_cannot_be_void(Token token)
{
  error(token, "The type cannot be void here.");
}

static void error_cannot_find_name(Token token, const char* name)
{
  error(token, memory_sprintf("Undeclared identifier '%s'.", name));
}

static void error_cannot_access_name_outside_function(Token token, const char* name)
{
  error(token, memory_sprintf("Cannot access '%s' because it is outside of the function.", name));
}

static void error_cannot_find_member_name(Token token, const char* name, DataType data_type)
{
  error(token,
        memory_sprintf("No member named '%s' in '%s'.", name, data_type_to_string(data_type)));
}

static void error_cannot_find_type(Token token, const char* name)
{
  error(token, memory_sprintf("Undeclared type '%s'.", name));
}

static void error_not_a_template_type(Token token, const char* name)
{
  error(token, memory_sprintf("'%s' is not a template type.", name));
}

static void error_not_a_type(Token token, const char* name)
{
  error(token, memory_sprintf("The name '%s' is not a type.", name));
}

static void error_invalid_template_arity(Token token, int expected, int got)
{
  error(token, memory_sprintf("Expected %d template %s but got %d.", expected,
                              expected == 1 ? "argument" : "arguments", got));
}

static void error_recursive_template_type(Token token, const char* name)
{
  error(token, memory_sprintf("Cannot instiantiate '%s' template, recursion limit reached.", name));
}

static void error_not_a_function(Token token)
{
  error(token, "The expression is not a function.");
}

static void error_not_an_object(Token token)
{
  error(token, "The expression is not an object.");
}

static void error_not_indexable(Token token, DataType data_type)
{
  error(token, memory_sprintf("Cannot index into type '%s'.", data_type_to_string(data_type)));
}

static void error_not_indexable_missing_overload(Token token, DataType index_data_type,
                                                 DataType value_data_type)
{
  if (checker.assignment)
    error(token, memory_sprintf("The object cannot be indexed and assigned to, missing "
                                "'__set__' method that takes '%s' and '%s'.",
                                data_type_to_string(index_data_type),
                                data_type_to_string(value_data_type)));
  else
    error(token,
          memory_sprintf("The object cannot be indexed, missing '__get__' method that takes '%s'.",
                         data_type_to_string(index_data_type)));
}

static void error_index_not_an_int(Token token)
{
  error(token, "The index must be of type 'int'.");
}

static void error_not_assignable(Token token)
{
  error(token, "The expression is not assignable.");
}

static void error_unexpected_class(Token token)
{
  error(token, "A class declaration is not allowed here.");
}

static void error_unexpected_import(Token token)
{
  error(token, "An import declaration is not allowed here.");
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

static void error_invalid_initializer_return_type(Token token)
{
  error(token, "The return type of '__init__' must be 'void'.");
}

static void error_invalid_str_return_type(Token token)
{
  error(token, "The return type of '__str__' must be 'string'.");
}

static void error_invalid_get_arity(Token token)
{
  error(token, "The '__get__' method must have one argument.");
}

static void error_invalid_set_arity(Token token)
{
  error(token, "The '__set__' method must have two arguments.");
}

static void error_invalid_str_arity(Token token)
{
  error(token, "The '__str__' method must have no arguments.");
}

static void error_invalid_binary_arity(Token token, const char* name)
{
  error(token, memory_sprintf("The '%s' method must have one argument.", name));
}

static void error_invalid_arity(Token token, int expected, int got)
{
  error(token, memory_sprintf("Expected %d %s but got %d.", expected,
                              expected == 1 ? "parameter" : "parameters", got));
}

static void error_invalid_type_conversion(Token token, DataType from, DataType to)
{
  error(token, memory_sprintf("Invalid type conversion from '%s' to '%s'.",
                              data_type_to_string(from), data_type_to_string(to)));
}

static void error_imported_functions_cannot_have_bodies(Token token)
{
  error(token, "An imported function cannot have a body.");
}

static void error_no_return(Token token)
{
  error(token, "Non-void function must return a value.");
}

static void error_array_type_is_unresolved(Token token)
{
  error(token, "The array type is unresolved; add a cast to declare its type.");
}

static void error_type_is_not_assignable(Token token, DataType from, DataType to)
{
  error(token, memory_sprintf("The type '%s' is not assignable to '%s'.", data_type_to_string(from),
                              data_type_to_string(to)));
}

static void error_ternary_type_mismatch(Token token, DataType left, DataType right)
{
  error(token, memory_sprintf("The ternary true type '%s' doesn't match the false type '%s'.",
                              data_type_to_string(left), data_type_to_string(right)));
}

static void error_cannot_find_suitable_overload(Token name, DataType data_type)
{
  error(name,
        memory_sprintf("Cannot find a suitable overload: %s", data_type_to_string(data_type)));
}

static void error_overload_conflict(Token token, const char* name)
{
  error(token, memory_sprintf(
                 "Cannot overload '%s', parameter types conflict with another function.", name));
}

static void error_ambiguous_call(Token token, const char* name)
{
  error(token, memory_sprintf("Call of overloaded '%s' is ambiguous; "
                              "parameter types conflict with another function.",
                              name));
}

static inline int align(int value, int alignment)
{
  return ((value + alignment - 1) / alignment) * alignment;
}

ArrayVarStmt global_locals(void)
{
  return checker.global_locals;
}

const char* function_data_type_to_string(const char* name, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER: {
    ArrayChar string;
    array_init(&string);

    const char* c = name;
    while (*c)
      array_add(&string, *c++);

    unsigned int offset = data_type.type == TYPE_FUNCTION_MEMBER ? 1 : 0;
    FuncStmt* function = data_type.type == TYPE_FUNCTION_MEMBER ? data_type.function_member.function
                                                                : data_type.function;

    array_add(&string, '(');

    for (unsigned int i = offset; i < function->parameters.size; i++)
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
  default:
    UNREACHABLE("Unexpected function data type");
  }
}

const char* data_type_to_string(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
    return "void";
  case TYPE_NULL:
    return "null";
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
  case TYPE_OBJECT:
    return data_type.class->name.lexeme;
  case TYPE_PROTOTYPE:
    return memory_sprintf("class %s", data_type.class->name.lexeme);
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_PROTOTYPE_TEMPLATE: {
    ArrayChar string;
    array_init(&string);

    ArrayToken types;
    const char* name;
    const char* prefix;

    if (data_type.type == TYPE_PROTOTYPE_TEMPLATE)
    {
      types = data_type.class_template->types;
      name = data_type.class_template->name.lexeme;
      prefix = "class ";
    }
    else
    {
      types = data_type.function_template.function->types;
      name = "";
      prefix = data_type_token_to_string(data_type.function_template.function->type, NULL);
    }

    const char* c = prefix;
    while (*c)
      array_add(&string, *c++);

    c = name;
    while (*c)
      array_add(&string, *c++);

    array_add(&string, '<');

    for (unsigned i = 0; i < types.size; i++)
    {
      const char* c = types.elems[i].lexeme;
      while (*c)
        array_add(&string, *c++);

      if (i < types.size - 1)
      {
        array_add(&string, ',');
        array_add(&string, ' ');
      }
    }

    array_add(&string, '>');

    if (data_type.type == TYPE_FUNCTION_TEMPLATE)
    {
      array_add(&string, '(');

      DataTypeToken data_type_token;
      array_foreach(&data_type.function_template.function->parameters, data_type_token)
      {
        const char* c = data_type_token_to_string(data_type_token, NULL);
        while (*c)
          array_add(&string, *c++);

        if (_i + 1 != data_type.function_template.function->parameters.size)
        {
          array_add(&string, ',');
          array_add(&string, ' ');
        }
      }

      array_add(&string, ')');
    }

    array_add(&string, '\0');

    return string.elems;
  }
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER: {
    DataType return_data_type;
    ArrayDataType parameter_types;
    expand_function_data_type(data_type, &return_data_type, &parameter_types);

    ArrayChar string;
    array_init(&string);

    const char* c = data_type_to_string(return_data_type);
    while (*c)
      array_add(&string, *c++);

    array_add(&string, '(');

    for (unsigned int i = 0; i < parameter_types.size; i++)
    {
      const char* c = data_type_to_string(parameter_types.elems[i]);
      while (*c)
        array_add(&string, *c++);

      if (i < parameter_types.size - 1)
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

    for (int i = 0; i < *data_type.array.count; i++)
    {
      array_add(&string, '[');
      array_add(&string, ']');
    }

    array_add(&string, '\0');

    return string.elems;
  }
  case TYPE_FUNCTION_GROUP: {
    ArrayChar string;
    array_init(&string);

    for (unsigned int i = 0; i < data_type.function_group.size; i++)
    {
      const char* c = data_type_to_string(data_type.function_group.elems[i]);
      while (*c)
        array_add(&string, *c++);

      if (i + 1 != data_type.function_group.size)
      {
        array_add(&string, ',');
        array_add(&string, ' ');
      }
    }

    array_add(&string, '\0');

    return string.elems;
  }
  default:
    UNREACHABLE("Unexpected data type to string");
  }
}

void expand_function_data_type(DataType data_type, DataType* return_data_type,
                               ArrayDataType* parameter_types)
{
  assert(data_type.type == TYPE_FUNCTION || data_type.type == TYPE_FUNCTION_INTERNAL ||
         data_type.type == TYPE_FUNCTION_MEMBER || data_type.type == TYPE_FUNCTION_POINTER);

  array_init(parameter_types);

  if (data_type.type == TYPE_FUNCTION || data_type.type == TYPE_FUNCTION_MEMBER)
  {
    FuncStmt* function =
      data_type.type == TYPE_FUNCTION ? data_type.function : data_type.function_member.function;

    for (unsigned int i = 0; i < function->parameters.size; i++)
      array_add(parameter_types, function->parameters.elems[i]->data_type);

    *return_data_type = function->data_type;
  }
  else
  {
    *parameter_types = data_type.function_internal.parameter_types;
    *return_data_type = *data_type.function_internal.return_type;
  }
}

bool equal_data_type(DataType left, DataType right)
{
  if (left.type == TYPE_OBJECT && right.type == TYPE_OBJECT)
    return left.class == right.class;

  if (left.type == TYPE_ARRAY && right.type == TYPE_ARRAY)
    return *left.array.count == *right.array.count &&
           equal_data_type(*left.array.data_type, *right.array.data_type);

  if ((left.type == TYPE_FUNCTION || left.type == TYPE_FUNCTION_MEMBER ||
       left.type == TYPE_FUNCTION_INTERNAL || left.type == TYPE_FUNCTION_POINTER) &&
      (right.type == TYPE_FUNCTION || right.type == TYPE_FUNCTION_MEMBER ||
       right.type == TYPE_FUNCTION_INTERNAL || right.type == TYPE_FUNCTION_POINTER))
  {
    DataType left_return_data_type;
    ArrayDataType left_parameter_types;
    expand_function_data_type(left, &left_return_data_type, &left_parameter_types);

    DataType right_return_data_type;
    ArrayDataType right_parameter_types;
    expand_function_data_type(right, &right_return_data_type, &right_parameter_types);

    if (!equal_data_type(left_return_data_type, right_return_data_type))
      return false;

    if (left_parameter_types.size != right_parameter_types.size)
      return false;

    for (unsigned int i = 0; i < left_parameter_types.size; i++)
      if (!equal_data_type(array_at(&left_parameter_types, i), array_at(&right_parameter_types, i)))
        return false;

    return true;
  }

  return left.type == right.type;
}

bool nullable_data_type(DataType data_type)
{
  return data_type.type == TYPE_ANY || data_type.type == TYPE_OBJECT ||
         data_type.type == TYPE_NULL || data_type.type == TYPE_FUNCTION_POINTER;
}

bool boolable_data_type(DataType data_type)
{
  return nullable_data_type(data_type) || data_type.type == TYPE_ARRAY ||
         data_type.type == TYPE_INTEGER || data_type.type == TYPE_STRING ||
         data_type.type == TYPE_BOOL;
}

bool assignable_data_type(Expr** expression, DataType destination, DataType source)
{
  if (destination.type == TYPE_ANY)
    return (source.type == TYPE_OBJECT || source.type == TYPE_STRING || source.type == TYPE_ARRAY ||
            source.type == TYPE_NULL || source.type == TYPE_ANY) &&
           autocast(expression, source, destination);

  if (destination.type == TYPE_FLOAT)
    return source.type == TYPE_INTEGER && autocast_int_literal_to_float_literal(expression);

  if (destination.type == TYPE_OBJECT)
    return source.type == TYPE_NULL;

  if (destination.type == TYPE_FUNCTION_POINTER)
    return source.type == TYPE_NULL;

  if (destination.type == TYPE_INTEGER)
    return source.type == TYPE_CHAR;

  return false;
}

int size_data_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
    return 1;
  case TYPE_BOOL:
    return sizeof(bool);
  case TYPE_CHAR:
    return sizeof(char);
  case TYPE_INTEGER:
    return sizeof(int);
  case TYPE_FLOAT:
    return sizeof(float);
  case TYPE_STRING:
  case TYPE_OBJECT:
  case TYPE_ARRAY:
  case TYPE_ANY:
  case TYPE_FUNCTION_POINTER:
    return sizeof(uintptr_t);
  default:
    UNREACHABLE("Unexpected data type size");
  }
}

DataType array_data_type_element(DataType array_data_type)
{
  assert(array_data_type.type == TYPE_ARRAY);
  assert(*array_data_type.array.count >= 1);

  if (*array_data_type.array.count == 1)
  {
    return *array_data_type.array.data_type;
  }
  else
  {
    DataType element_data_type = DATA_TYPE(TYPE_ARRAY);
    element_data_type.array.data_type = array_data_type.array.data_type;
    element_data_type.array.count = ALLOC(unsigned char);
    *element_data_type.array.count = *array_data_type.array.count - 1;

    return element_data_type;
  }
}

static void data_type_inference(DataType* source, DataType* target)
{
  if (source->type == TYPE_ARRAY && source->array.data_type->type == TYPE_VOID)
  {
    if (target->type != TYPE_ARRAY)
    {
      error_array_type_is_unresolved(source->array.token);
      return;
    }

    *source->array.data_type = *target->array.data_type;
    *source->array.count = *target->array.count;

    DataType element_data_type = array_data_type_element(*source);

    for (unsigned int i = 0; i < source->array.values.size; i++)
    {
      Expr* value = source->array.values.elems[i];
      Token token = source->array.tokens.elems[i];

      DataType data_type = check_expression(value);

      data_type_inference(&data_type, &element_data_type);

      if (!equal_data_type(element_data_type, data_type) &&
          !assignable_data_type(&source->array.values.elems[i], element_data_type, data_type))
      {
        error_type_mismatch(token, element_data_type, data_type);
      }
    }
  }
  else if (source->type == TYPE_NULL && target->type == TYPE_FUNCTION_POINTER)
  {
    *source->null_function = true;
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

    if (variable->data_type.type == TYPE_PROTOTYPE)
    {
      DataType object = DATA_TYPE(TYPE_OBJECT);
      object.class = variable->data_type.class;

      return object;
    }
    else if (variable->data_type.type == TYPE_ALIAS)
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

static DataType class_template_to_data_type(DataType template, DataTypeToken template_type)
{
  const char* name = data_type_token_to_string(template_type, NULL);
  VarStmt* variable = environment_get_variable(checker.environment, name);

  if (variable && variable->data_type.type == TYPE_PROTOTYPE)
  {
    return variable->data_type;
  }

  static const int RECURSION_LIMIT = 32;
  if (template.class_template->count >= RECURSION_LIMIT)
  {
    error_recursive_template_type(template_type.token, name);
    return DATA_TYPE(TYPE_VOID);
  }

  template.class_template->count++;

  Stmt* statement = parser_parse_class_declaration_statement(template.class_template->offset,
                                                             template.class_template->keyword,
                                                             template.class_template->name);

  ClassStmt* class_statement = &statement->class;
  class_statement->name.lexeme = name;
  class_statement->name.length = strlen(name);
  class_statement->name.start_line = template_type.token.start_line;
  class_statement->name.end_line = template_type.token.end_line;
  class_statement->name.start_column = template_type.token.start_column;
  class_statement->name.end_column = template_type.token.end_column;

  ClassStmt* previous_class = checker.class;
  FuncStmt* previous_function = checker.function;
  WhileStmt* previous_loop = checker.loop;
  IfStmt* previous_cond = checker.cond;
  Environment* previous_environment = checker.environment;

  checker.class = NULL;
  checker.function = NULL;
  checker.loop = NULL;
  checker.cond = NULL;
  checker.environment = checker.global_environment;

  Token* previous_template = checker.template;
  if (!previous_template)
  {
    checker.template = ALLOC(Token);
    *checker.template = class_statement->name;
  }

  init_class_declaration(class_statement);

  checker.environment = environment_init(previous_environment);

  for (unsigned int i = 0; i < template.class_template->types.size; i++)
  {
    Token name = template.class_template->types.elems[i];
    DataTypeToken actual_data_type_token = array_at(&template_type.types, i);

    VarStmt* variable = ALLOC(VarStmt);
    variable->name = name;
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type.type = TYPE_ALIAS;
    variable->data_type.alias.token = actual_data_type_token;
    variable->data_type.alias.data_type = ALLOC(DataType);
    *variable->data_type.alias.data_type = data_type_token_to_data_type(actual_data_type_token);

    environment_set_variable(checker.environment, variable->name.lexeme, variable);
  }

  init_class_declaration_body(class_statement);

  template.class_template->count--;

  checker.class = previous_class;
  checker.function = previous_function;
  checker.loop = previous_loop;
  checker.cond = previous_cond;
  checker.environment = previous_environment;

  if (!previous_template)
    checker.template = NULL;

  array_add(&template.class_template->classes, class_statement);

  variable = environment_get_variable(checker.environment, name);
  assert(variable && variable->data_type.type == TYPE_PROTOTYPE);

  return variable->data_type;
}

static DataType function_template_to_data_type(DataType template, DataTypeToken function_type,
                                               bool overloaded)
{
  const char* name = data_type_token_to_string(function_type, NULL);
  VarStmt* variable =
    environment_get_variable(template.function_template.function->environment, name);

  if (variable && (variable->data_type.type == TYPE_FUNCTION ||
                   variable->data_type.type == TYPE_FUNCTION_MEMBER))
  {
    return variable->data_type;
  }

  DataType** template_data_types =
    alloca(template.function_template.function->types.size * sizeof(DataType*));

  for (unsigned int i = 0; i < template.function_template.function->types.size; i++)
  {
    DataType template_data_type = data_type_token_to_data_type(array_at(&function_type.types, i));

    if (checker.error)
      return DATA_TYPE(TYPE_VOID);

    template_data_types[i] = ALLOC(DataType);
    *template_data_types[i] = template_data_type;
  }

  Stmt* statement = parser_parse_function_declaration_statement(
    template.function_template.function->offset, template.function_template.function->type,
    template.function_template.function->name);

  FuncStmt* function_statement = &statement->func;
  function_statement->name.lexeme = name;
  function_statement->name.length = strlen(name);
  function_statement->name.start_line = function_type.token.start_line;
  function_statement->name.end_line = function_type.token.end_line;
  function_statement->name.start_column = function_type.token.start_column;
  function_statement->name.end_column = function_type.token.end_column;
  function_statement->import = template.function_template.function->import;

  ClassStmt* previous_class = checker.class;
  FuncStmt* previous_function = checker.function;
  WhileStmt* previous_loop = checker.loop;
  IfStmt* previous_cond = checker.cond;

  Environment* previous_environment = checker.environment;

  checker.class = template.function_template.function->class;
  checker.function = template.function_template.function->function;
  checker.loop = template.function_template.function->loop;
  checker.cond = template.function_template.function->cond;
  checker.environment = template.function_template.function->environment;

  Token* previous_template = checker.template;
  if (!previous_template)
  {
    checker.template = ALLOC(Token);
    *checker.template = function_statement->name;
  }

  for (unsigned int i = 0; i < template.function_template.function->types.size; i++)
  {
    Token name = template.function_template.function->types.elems[i];
    DataTypeToken data_type_token = array_at(&function_type.types, i);

    VarStmt* variable = ALLOC(VarStmt);
    variable->name = name;
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type.type = TYPE_ALIAS;
    variable->data_type.alias.token = data_type_token;
    variable->data_type.alias.data_type = template_data_types[i];

    environment_set_variable(checker.environment, variable->name.lexeme, variable);
  }

  if (!checker.function && !checker.loop && !checker.cond)
    init_function_declaration(function_statement);

  check_function_declaration(function_statement);

  if (overloaded)
  {
    function_statement->name.lexeme = function_data_type_to_string(
      function_statement->name.lexeme, function_statement->function_data_type);
    function_statement->name.length = strlen(function_statement->name.lexeme);
  }

  checker.class = previous_class;
  checker.function = previous_function;
  checker.loop = previous_loop;
  checker.cond = previous_cond;
  checker.environment = previous_environment;

  if (!previous_template)
    checker.template = NULL;

  array_add(&template.function_template.function->functions, function_statement);

  variable = environment_get_variable(template.function_template.function->environment, name);
  assert(variable && (variable->data_type.type == TYPE_FUNCTION ||
                      variable->data_type.type == TYPE_FUNCTION_MEMBER));

  return variable->data_type;
}

static const char* data_type_token_to_string(DataTypeToken data_type_token, ArrayChar* string)
{
  if (string == NULL)
  {
    ArrayChar string;
    array_init(&string);
    data_type_token_to_string(data_type_token, &string);
    array_add(&string, '\0');

    return string.elems;
  }
  else
  {
    switch (data_type_token.type)
    {
    case DATA_TYPE_TOKEN_PRIMITIVE: {
      const char* c = data_type_token.token.lexeme;
      while (*c)
        array_add(string, *c++);

      if (array_size(&data_type_token.types))
      {
        array_add(string, '<');

        for (unsigned int i = 0; i < array_size(&data_type_token.types); i++)
        {
          data_type_token_to_string(array_at(&data_type_token.types, i), string);

          if (i != array_size(&data_type_token.types) - 1)
          {
            array_add(string, ',');
            array_add(string, ' ');
          }
        }

        array_add(string, '>');
      }

      break;
    }

    case DATA_TYPE_TOKEN_ARRAY: {
      data_type_token_to_string(*data_type_token.array.type, string);

      for (int i = 0; i < data_type_token.array.count; i++)
      {
        array_add(string, '[');
        array_add(string, ']');
      }

      break;
    }

    case DATA_TYPE_TOKEN_FUNCTION: {
      data_type_token_to_string(*data_type_token.function.return_value, string);

      array_add(string, '(');

      for (unsigned int i = 0; i < data_type_token.function.parameters.size; i++)
      {
        data_type_token_to_string(data_type_token.function.parameters.elems[i], string);

        if (i + 1 < data_type_token.function.parameters.size)
        {
          array_add(string, ',');
          array_add(string, ' ');
        }
      }

      array_add(string, ')');
      break;
    }

    default:
      UNREACHABLE("Unexpected data type token");
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

    if (variable && variable->data_type.type == TYPE_ALIAS)
    {
      array_at(types, i) = variable->data_type.alias.token;
    }

    data_type_token_unalias(&array_at(types, i).types);
  }
}

static DataType data_type_token_to_data_type(DataTypeToken data_type_token)
{
  switch (data_type_token.type)
  {
  case DATA_TYPE_TOKEN_PRIMITIVE: {
    Token token = data_type_token.token;
    ArrayDataTypeToken* types = &data_type_token.types;

    if (array_size(types))
    {
      if (token.type != TOKEN_IDENTIFIER)
      {
        error_not_a_template_type(token, token.lexeme);
        return DATA_TYPE(TYPE_VOID);
      }

      VarStmt* variable = environment_get_variable(checker.environment, token.lexeme);
      if (!variable)
      {
        error_cannot_find_type(token, token.lexeme);
        return DATA_TYPE(TYPE_VOID);
      }

      if (variable->data_type.type != TYPE_PROTOTYPE_TEMPLATE)
      {
        error_not_a_template_type(token, token.lexeme);
        return DATA_TYPE(TYPE_VOID);
      }

      int expected = array_size(&variable->data_type.class_template->types);
      int got = array_size(types);

      if (expected != got)
      {
        error_invalid_template_arity(token, expected, got);
        return DATA_TYPE(TYPE_VOID);
      }

      data_type_token_unalias(types);

      DataType data_type = class_template_to_data_type(variable->data_type, data_type_token);
      if (data_type.type == TYPE_VOID)
      {
        return DATA_TYPE(TYPE_VOID);
      }

      token.lexeme = data_type.class->name.lexeme;
      token.length = data_type.class->name.length;
    }

    return token_to_data_type(token);
  }

  case DATA_TYPE_TOKEN_ARRAY: {
    DataType data_type = DATA_TYPE(TYPE_ARRAY);
    data_type.array.data_type = ALLOC(DataType);
    data_type.array.count = ALLOC(unsigned char);
    *data_type.array.count = data_type_token.array.count;

    DataType element_data_type = data_type_token_to_data_type(*data_type_token.array.type);
    if (element_data_type.type == TYPE_VOID)
    {
      error_type_cannot_be_void(data_type_token.token);
      return DATA_TYPE(TYPE_VOID);
    }

    if (element_data_type.type == TYPE_ARRAY)
    {
      *data_type.array.count += *element_data_type.array.count;
      *data_type.array.data_type = *element_data_type.array.data_type;
    }
    else
    {
      *data_type.array.data_type = element_data_type;
    }

    return data_type;
  }

  case DATA_TYPE_TOKEN_FUNCTION: {
    DataType data_type = DATA_TYPE(TYPE_FUNCTION_POINTER);
    array_init(&data_type.function_internal.parameter_types);

    DataTypeToken parameter_data_type_token;
    array_foreach(&data_type_token.function.parameters, parameter_data_type_token)
    {
      array_add(&data_type.function_internal.parameter_types,
                data_type_token_to_data_type(parameter_data_type_token));
    }

    data_type.function_internal.return_type = ALLOC(DataType);
    *data_type.function_internal.return_type =
      data_type_token_to_data_type(*data_type_token.function.return_value);

    data_type.function_internal.name = data_type_to_string(data_type);

    return data_type;
  }

  default:
    UNREACHABLE("Unexpected data type token");
  }
}

static Expr* cast_to_bool(Expr* expression, DataType data_type)
{
  if (boolable_data_type(data_type))
  {
    Expr* cast_expression = EXPR();
    cast_expression->type = EXPR_CAST;
    cast_expression->cast.type = DATA_TYPE_TOKEN_EMPTY();
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

  if (left->type == from.type && right->type == to.type)
  {
    from = *left;
    target_type = left;
    target = &expression->left;
  }
  else if (left->type == to.type && right->type == from.type)
  {
    from = *right;
    target_type = right;
    target = &expression->right;
  }
  else
  {
    return false;
  }

  Expr* cast_expression = EXPR();
  cast_expression->type = EXPR_CAST;
  cast_expression->cast.type = DATA_TYPE_TOKEN_EMPTY();
  cast_expression->cast.from_data_type = from;
  cast_expression->cast.to_data_type = to;
  cast_expression->cast.expr = *target;

  *target = cast_expression;
  *target_type = to;

  return true;
}

static bool autocast(Expr** expression, DataType from, DataType to)
{
  if (expression == NULL)
    return true;

  Expr* cast_expression = EXPR();
  cast_expression->type = EXPR_CAST;
  cast_expression->cast.type = DATA_TYPE_TOKEN_EMPTY();
  cast_expression->cast.from_data_type = from;
  cast_expression->cast.to_data_type = to;
  cast_expression->cast.expr = *expression;

  *expression = cast_expression;
  return true;
}

static bool autocast_int_literal_to_float_literal(Expr** expression)
{
  if (expression == NULL)
    return true;

  Expr* expr = *expression;
  if (expr->type == EXPR_LITERAL)
  {
    float value = (float)expr->literal.integer;
    expr->literal.data_type = DATA_TYPE(TYPE_FLOAT);
    expr->literal.floating = value;

    return true;
  }
  else if (expr->type == EXPR_UNARY && expr->unary.expr->type == EXPR_LITERAL)
  {
    if (autocast_int_literal_to_float_literal(&expr->unary.expr))
    {
      expr->unary.data_type = DATA_TYPE(TYPE_FLOAT);
      return true;
    }
  }
  else if (expr->type == EXPR_GROUP && expr->group.expr->type == EXPR_LITERAL)
  {
    if (autocast_int_literal_to_float_literal(&expr->group.expr))
    {
      expr->group.data_type = DATA_TYPE(TYPE_FLOAT);
      return true;
    }
  }

  return false;
}

static bool upcast_boolable_to_bool(BinaryExpr* expression, DataType* left, DataType* right,
                                    DataType from)
{
  if (expression->op.type != TOKEN_AND && expression->op.type != TOKEN_OR)
    return false;

  if (!boolable_data_type(*left) && !boolable_data_type(*right))
    return false;

  return upcast(expression, left, right, from, DATA_TYPE(TYPE_BOOL));
}

static void expand_function_group(DataType* data_type, DataType* argument_data_types,
                                  unsigned int number_of_arguments)
{
  if (data_type->type == TYPE_FUNCTION_GROUP)
  {
    ArrayDataType function_group = data_type->function_group;
    DataType function_data_type;

    array_foreach(&function_group, function_data_type)
    {
      if (function_data_type.type == TYPE_FUNCTION ||
          function_data_type.type == TYPE_FUNCTION_MEMBER)
      {
        FuncStmt* function = function_data_type.type == TYPE_FUNCTION
                               ? function_data_type.function
                               : function_data_type.function_member.function;

        int offset = function_data_type.type == TYPE_FUNCTION_MEMBER ? 1 : 0;
        bool match = true;

        if (function->parameters.size - offset == number_of_arguments)
        {
          for (unsigned int i = offset; i < function->parameters.size; i++)
          {
            DataType argument_data_type = argument_data_types[i - offset];
            DataType parameter_data_type = function->parameters.elems[i]->data_type;

            if (!equal_data_type(parameter_data_type, argument_data_type) &&
                !assignable_data_type(NULL, parameter_data_type, argument_data_type))
            {
              match = false;
              break;
            }
          }
        }
        else
        {
          match = false;
        }

        if (match)
          *data_type = function_data_type;
      }
    }

    array_foreach(&function_group, function_data_type)
    {
      if (function_data_type.type == TYPE_FUNCTION ||
          function_data_type.type == TYPE_FUNCTION_MEMBER)
      {
        FuncStmt* function = function_data_type.type == TYPE_FUNCTION
                               ? function_data_type.function
                               : function_data_type.function_member.function;

        int offset = function_data_type.type == TYPE_FUNCTION_MEMBER ? 1 : 0;
        bool match = true;

        if (function->parameters.size - offset == number_of_arguments)
        {
          for (unsigned int i = offset; i < function->parameters.size; i++)
          {
            DataType argument_data_type = argument_data_types[i - offset];
            DataType parameter_data_type = function->parameters.elems[i]->data_type;

            if (!equal_data_type(parameter_data_type, argument_data_type))
            {
              match = false;
              break;
            }
          }
        }
        else
        {
          match = false;
        }

        if (match)
          *data_type = function_data_type;
      }
    }
  }
}

static void expand_exact_function_group(DataType* data_type, DataType* argument_data_types,
                                        unsigned int number_of_arguments)
{
  if (data_type->type == TYPE_FUNCTION_GROUP)
  {
    ArrayDataType function_group = data_type->function_group;
    DataType function_data_type;

    array_foreach(&function_group, function_data_type)
    {
      if (function_data_type.type == TYPE_FUNCTION ||
          function_data_type.type == TYPE_FUNCTION_MEMBER)
      {
        FuncStmt* function = function_data_type.type == TYPE_FUNCTION
                               ? function_data_type.function
                               : function_data_type.function_member.function;

        bool match = true;

        if (function->parameters.size == number_of_arguments)
        {
          for (unsigned int i = 0; i < function->parameters.size; i++)
          {
            DataType argument_data_type = argument_data_types[i];
            DataType parameter_data_type = function->parameters.elems[i]->data_type;

            if (!equal_data_type(parameter_data_type, argument_data_type))
            {
              match = false;
              break;
            }
          }
        }
        else
        {
          match = false;
        }

        if (match)
          *data_type = function_data_type;
      }
    }
  }
}

static void expand_template_types(ArrayDataTypeToken* types, DataType* data_type, Token name)
{
  if (!types)
    return;

  if (data_type->type == TYPE_PROTOTYPE_TEMPLATE)
  {
    int expected = array_size(&data_type->class_template->types);
    int got = array_size(types);

    if (expected != got)
    {
      error_invalid_template_arity(name, expected, got);

      *data_type = DATA_TYPE(TYPE_VOID);
      return;
    }

    data_type_token_unalias(types);

    DataTypeToken class_type = {
      .type = DATA_TYPE_TOKEN_PRIMITIVE,
      .token = data_type->class_template->name,
      .types = *types,
    };

    class_type.token.start_line = name.start_line;
    class_type.token.start_column = name.start_column;
    class_type.token.end_line = name.end_line;
    class_type.token.end_column = name.end_column;

    *data_type = class_template_to_data_type(*data_type, class_type);
  }
  else if (data_type->type == TYPE_FUNCTION_TEMPLATE)
  {
    int expected = array_size(&data_type->function_template.function->types);
    int got = array_size(types);

    if (expected != got)
    {
      error_invalid_template_arity(name, expected, got);

      *data_type = DATA_TYPE(TYPE_VOID);
      return;
    }

    data_type_token_unalias(types);

    DataTypeToken function_type = {
      .type = DATA_TYPE_TOKEN_PRIMITIVE,
      .token = data_type->function_template.function->name,
      .types = *types,
    };

    function_type.token.start_line = name.start_line;
    function_type.token.start_column = name.start_column;
    function_type.token.end_line = name.end_line;
    function_type.token.end_column = name.end_column;

    *data_type = function_template_to_data_type(*data_type, function_type, false);
  }
  else if (data_type->type == TYPE_FUNCTION_GROUP)
  {
    data_type_token_unalias(types);

    DataType function_group_data_type = DATA_TYPE(TYPE_FUNCTION_GROUP);
    array_init(&function_group_data_type.function_group);

    MapSInt function_set;
    map_init_sint(&function_set, 0, 0);

    DataType function_template_data_type;
    array_foreach(&data_type->function_group, function_template_data_type)
    {
      if (function_template_data_type.type == TYPE_FUNCTION_TEMPLATE)
      {
        int expected = array_size(&function_template_data_type.function_template.function->types);
        int got = array_size(types);

        if (expected == got)
        {
          DataTypeToken function_type = {
            .type = DATA_TYPE_TOKEN_PRIMITIVE,
            .token = function_template_data_type.function_template.function->name,
            .types = *types,
          };

          function_type.token.start_line = name.start_line;
          function_type.token.start_column = name.start_column;
          function_type.token.end_line = name.end_line;
          function_type.token.end_column = name.end_column;

          DataType function_data_type =
            function_template_to_data_type(function_template_data_type, function_type, true);

          if (checker.error)
          {
            checker.error = false;
            continue;
          }

          FuncStmt* function = function_data_type.type == TYPE_FUNCTION_MEMBER
                                 ? function_data_type.function_member.function
                                 : function_data_type.function;

          if (map_get_sint(&function_set, function->name.lexeme))
          {
            error_ambiguous_call(name, function->name.lexeme);
          }
          else
          {
            map_put_sint(&function_set, function->name.lexeme, true);
            array_add(&function_group_data_type.function_group, function_data_type);
          }
        }
      }
    }

    *data_type = function_group_data_type;
    return;
  }
  else
  {
    error_not_a_template_type(name, data_type_to_string(*data_type));

    *data_type = DATA_TYPE(TYPE_VOID);
    return;
  }
}

static void init_function_declaration(FuncStmt* statement)
{
  const char* name = statement->name.lexeme;

  if (checker.function)
  {
    statement->name.lexeme =
      memory_sprintf("%s.%s:%d:%d", checker.function->name.lexeme, statement->name.lexeme,
                     statement->name.start_line, statement->name.start_column);
    statement->name.length = strlen(statement->name.lexeme);
  }
  else if (checker.loop || checker.cond)
  {
    statement->name.lexeme = memory_sprintf(
      "%s:%d:%d", statement->name.lexeme, statement->name.start_line, statement->name.start_column);
    statement->name.length = strlen(statement->name.lexeme);
  }

  if (checker.class)
  {
    if (strcmp(name, "__init__") == 0 && statement->type.token.type != TOKEN_IDENTIFIER_VOID)
    {
      error_invalid_initializer_return_type(statement->name);
      return;
    }

    VarStmt* parameter = ALLOC(VarStmt);
    parameter->name = (Token){ .lexeme = "this", .length = sizeof("this") - 1 };
    parameter->type = (DataTypeToken){
      .type = DATA_TYPE_TOKEN_PRIMITIVE,
      .token = checker.class->name,
    };
    parameter->function = NULL;
    parameter->initializer = NULL;
    parameter->index = 0;
    parameter->scope = SCOPE_LOCAL;
    parameter->data_type = DATA_TYPE(TYPE_OBJECT);
    parameter->data_type.class = checker.class;

    array_prepend(&statement->parameters, parameter);
  }

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    parameter->data_type = data_type_token_to_data_type(parameter->type);

    if (parameter->data_type.type == TYPE_VOID)
    {
      error_type_cannot_be_void(parameter->type.token);
    }
  }

  statement->data_type = data_type_token_to_data_type(statement->type);

  if (checker.class)
  {
    statement->function_data_type = DATA_TYPE(TYPE_FUNCTION_MEMBER);
    statement->function_data_type.function_member.function = statement;
    statement->function_data_type.function_member.this = NULL;
  }
  else
  {
    statement->function_data_type = DATA_TYPE(TYPE_FUNCTION);
    statement->function_data_type.function = statement;
  }

  if (environment_check_variable(checker.environment, name))
  {
    VarStmt* variable = environment_get_variable(checker.environment, name);

    if (variable->data_type.type == TYPE_FUNCTION ||
        variable->data_type.type == TYPE_FUNCTION_MEMBER ||
        variable->data_type.type == TYPE_FUNCTION_TEMPLATE ||
        variable->data_type.type == TYPE_FUNCTION_GROUP)
    {
      if (variable->data_type.type != TYPE_FUNCTION_GROUP)
      {
        DataType function_data_type = variable->data_type;

        if (variable->data_type.type == TYPE_FUNCTION ||
            variable->data_type.type == TYPE_FUNCTION_MEMBER)
        {
          FuncStmt* function = function_data_type.type == TYPE_FUNCTION_MEMBER
                                 ? function_data_type.function_member.function
                                 : function_data_type.function;

          function->name.lexeme =
            function_data_type_to_string(function->name.lexeme, function_data_type);
          function->name.length = strlen(function->name.lexeme);
        }

        variable->data_type = DATA_TYPE(TYPE_FUNCTION_GROUP);
        array_init(&variable->data_type.function_group);
        array_add(&variable->data_type.function_group, function_data_type);
      }

      DataType function_data_type;
      array_foreach(&variable->data_type.function_group, function_data_type)
      {
        if (function_data_type.type == TYPE_FUNCTION ||
            function_data_type.type == TYPE_FUNCTION_MEMBER)
        {
          FuncStmt* function = function_data_type.type == TYPE_FUNCTION
                                 ? function_data_type.function
                                 : function_data_type.function_member.function;

          if (function->parameters.size == statement->parameters.size)
          {
            bool equal = true;

            for (unsigned int i = 0; i < function->parameters.size; i++)
            {
              if (!equal_data_type(function->parameters.elems[i]->data_type,
                                   statement->parameters.elems[i]->data_type))
              {
                equal = false;
                break;
              }
            }

            if (equal)
            {
              error_overload_conflict(statement->name, name);
              return;
            }
          }
        }
      }

      array_add(&variable->data_type.function_group, statement->function_data_type);

      statement->name.lexeme = function_data_type_to_string(name, statement->function_data_type);
      statement->name.length = strlen(statement->name.lexeme);
    }
    else
    {
      error_name_already_exists(statement->name, name);
    }
  }
  else
  {
    VarStmt* variable = ALLOC(VarStmt);
    variable->name = statement->name;
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->scope = SCOPE_GLOBAL;
    variable->initializer = NULL;
    variable->function = NULL;
    variable->data_type = statement->function_data_type;

    environment_set_variable(checker.environment, name, variable);
  }
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
      map_put_sint(&type_set, type.lexeme, true);
  }

  VarStmt* variable = ALLOC(VarStmt);
  variable->name = statement->name;
  variable->type = (DataTypeToken){
    .type = DATA_TYPE_TOKEN_PRIMITIVE,
    .token = statement->name,
  };
  variable->initializer = NULL;
  variable->function = NULL;
  variable->scope = SCOPE_GLOBAL;
  variable->index = -1;
  variable->data_type = DATA_TYPE(TYPE_PROTOTYPE_TEMPLATE);
  variable->data_type.class_template = statement;

  environment_set_variable(checker.environment, name, variable);
}

static void init_function_template_declaration(FuncTemplateStmt* statement)
{
  const char* name = statement->name.lexeme;
  statement->function = checker.function;
  statement->class = checker.class;
  statement->loop = checker.loop;
  statement->cond = checker.cond;
  statement->environment = environment_init(checker.environment);

  MapSInt type_set;
  map_init_sint(&type_set, 0, 0);

  Token type;
  array_foreach(&statement->types, type)
  {
    if (map_get_sint(&type_set, type.lexeme))
      error_name_already_exists(type, type.lexeme);
    else
      map_put_sint(&type_set, type.lexeme, true);
  }

  if (environment_check_variable(checker.environment, name))
  {
    VarStmt* variable = environment_get_variable(checker.environment, name);

    if (variable->data_type.type == TYPE_FUNCTION ||
        variable->data_type.type == TYPE_FUNCTION_MEMBER ||
        variable->data_type.type == TYPE_FUNCTION_TEMPLATE ||
        variable->data_type.type == TYPE_FUNCTION_GROUP)
    {
      if (variable->data_type.type != TYPE_FUNCTION_GROUP)
      {
        DataType function_data_type = variable->data_type;

        if (variable->data_type.type == TYPE_FUNCTION ||
            variable->data_type.type == TYPE_FUNCTION_MEMBER)
        {
          FuncStmt* function = function_data_type.type == TYPE_FUNCTION_MEMBER
                                 ? function_data_type.function_member.function
                                 : function_data_type.function;

          function->name.lexeme =
            function_data_type_to_string(function->name.lexeme, function_data_type);
          function->name.length = strlen(function->name.lexeme);
        }

        variable->data_type = DATA_TYPE(TYPE_FUNCTION_GROUP);
        array_init(&variable->data_type.function_group);
        array_add(&variable->data_type.function_group, function_data_type);
      }

      DataType function_template_data_type = DATA_TYPE(TYPE_FUNCTION_TEMPLATE);
      function_template_data_type.function_template.function = statement;
      function_template_data_type.function_template.this = NULL;

      array_add(&variable->data_type.function_group, function_template_data_type);
    }
    else
    {
      error_name_already_exists(statement->name, name);
    }
  }
  else
  {
    VarStmt* variable = ALLOC(VarStmt);
    variable->name = statement->name;
    variable->type = (DataTypeToken){
      .type = DATA_TYPE_TOKEN_PRIMITIVE,
      .token = statement->name,
    };
    variable->initializer = NULL;
    variable->function = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_TEMPLATE);
    variable->data_type.function_template.function = statement;
    variable->data_type.function_template.this = NULL;

    environment_set_variable(checker.environment, name, variable);
  }
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
  variable->type = (DataTypeToken){
    .type = DATA_TYPE_TOKEN_PRIMITIVE,
    .token = statement->name,
  };
  variable->initializer = NULL;
  variable->function = NULL;
  variable->scope = SCOPE_GLOBAL;
  variable->index = -1;
  variable->data_type = DATA_TYPE(TYPE_PROTOTYPE);
  variable->data_type.class = statement;

  environment_set_variable(checker.environment, name, variable);
}

static void init_variable_declaration(VarStmt* statement)
{
  Environment* environment = checker.environment;
  const char* name = statement->name.lexeme;

  if (checker.function)
  {
    statement->function = checker.function;
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
  }

  if (statement->type.type == DATA_TYPE_TOKEN_NONE)
  {
    statement->data_type = statement->initializer_data_type;
  }
  else
  {
    statement->data_type = data_type_token_to_data_type(statement->type);

    if (statement->data_type.type == TYPE_VOID)
    {
      error_type_cannot_be_void(statement->type.token);
    }
  }

  if (checker.function)
  {
    array_add(&checker.function->variables, statement);
  }

  environment_set_variable(environment, name, statement);
}

static void init_class_declaration_body(ClassStmt* statement)
{
  Environment* previous_environment = checker.environment;
  checker.environment = environment_init(checker.environment);

  ClassStmt* previous_class = checker.class;
  checker.class = statement;
  checker.class->environment = checker.environment;
  checker.class->members = &checker.environment->variables;

  FuncStmt* function_statement;
  array_foreach(&statement->functions, function_statement)
  {
    init_function_declaration(function_statement);

    const char* class_name = statement->name.lexeme;
    const char* function_name = function_statement->name.lexeme;

    function_statement->name.lexeme = memory_sprintf("%s.%s", class_name, function_name);
    function_statement->name.length = strlen(statement->name.lexeme);
  }

  FuncTemplateStmt* function_template;
  array_foreach(&statement->function_templates, function_template)
  {
    init_function_template_declaration(function_template);

    const char* class_name = statement->name.lexeme;
    const char* function_name = function_template->name.lexeme;

    function_template->name.lexeme = memory_sprintf("%s.%s", class_name, function_name);
    function_template->name.length = strlen(statement->name.lexeme);
  }

  int count = 0;
  VarStmt* variable_statement;
  array_foreach(&statement->variables, variable_statement)
  {
    variable_statement->index = count++;

    init_variable_declaration(variable_statement);
  }

  checker.class = previous_class;
  checker.environment = previous_environment;
}

static void init_import_declaration(ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    if (body_statement->type == STMT_FUNCTION_DECL)
      init_function_declaration(&body_statement->func);
    else if (body_statement->type == STMT_FUNCTION_TEMPLATE_DECL)
      init_function_template_declaration(&body_statement->func_template);
  }
}

static DataType check_cast_expression(CastExpr* expression)
{
  if (expression->from_data_type.type == TYPE_VOID && expression->to_data_type.type == TYPE_VOID)
  {
    expression->from_data_type = check_expression(expression->expr);
    expression->to_data_type = data_type_token_to_data_type(expression->type);

    data_type_inference(&expression->from_data_type, &expression->to_data_type);

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
        break;

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
        break;

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
        break;

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
        break;

      default:
        break;
      }

      break;
    case TYPE_STRING:
      switch (expression->to_data_type.type)
      {
      case TYPE_STRING:
      case TYPE_BOOL:
      case TYPE_ANY:
        valid = true;
        break;

      default:
        break;
      }

      break;
    case TYPE_ARRAY:
      switch (expression->to_data_type.type)
      {
      case TYPE_ARRAY:
        valid = equal_data_type(expression->from_data_type, expression->to_data_type);
        break;
      case TYPE_STRING:
      case TYPE_ANY:
        valid = true;
        break;

      default:
        break;
      }

      break;
    case TYPE_OBJECT:
      switch (expression->to_data_type.type)
      {
      case TYPE_OBJECT:
        valid = equal_data_type(expression->from_data_type, expression->to_data_type);
        break;
      case TYPE_STRING:
      case TYPE_ANY:
        valid = true;
        break;

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
        break;

      default:
        break;
      }

      break;
    case TYPE_NULL:
      switch (expression->to_data_type.type)
      {
      case TYPE_ANY:
      case TYPE_OBJECT:
      case TYPE_FUNCTION_POINTER:
        valid = true;
        break;

      default:
        break;
      }

      break;
    case TYPE_FUNCTION_POINTER:
      switch (expression->to_data_type.type)
      {
      case TYPE_NULL:
      case TYPE_STRING:
      case TYPE_BOOL:
        valid = true;
        break;

      default:
        break;
      }

      break;

    case TYPE_FUNCTION_GROUP:
      switch (expression->to_data_type.type)
      {
      case TYPE_FUNCTION_POINTER: {
        DataType function_data_type = expression->from_data_type;
        expand_exact_function_group(
          &function_data_type, expression->to_data_type.function_internal.parameter_types.elems,
          expression->to_data_type.function_internal.parameter_types.size);

        if (function_data_type.type != TYPE_FUNCTION_GROUP)
        {
          expression->from_data_type = function_data_type;
          valid = true;
        }
        else
        {
          valid = false;
        }

        break;
      }

      case TYPE_STRING:
        valid = true;
        break;

      default:
        break;
      }

      break;

    case TYPE_ALIAS:
    case TYPE_FUNCTION_TEMPLATE:
    case TYPE_PROTOTYPE:
    case TYPE_PROTOTYPE_TEMPLATE:
      switch (expression->to_data_type.type)
      {
      case TYPE_STRING:
        valid = true;
        break;

      default:
        break;
      }

      break;

    case TYPE_FUNCTION:
    case TYPE_FUNCTION_MEMBER:
    case TYPE_FUNCTION_INTERNAL:
      switch (expression->to_data_type.type)
      {
      case TYPE_STRING:
        valid = true;
        break;

      case TYPE_FUNCTION_POINTER:
        valid = equal_data_type(expression->from_data_type, expression->to_data_type);
        break;

      default:
        break;
      }

      break;

    default:
      break;
    }

    if (!valid)
    {
      error_invalid_type_conversion(expression->type.token, expression->from_data_type,
                                    expression->to_data_type);
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
    if (data_type.type != TYPE_INTEGER && data_type.type != TYPE_FLOAT)
      error_operation_not_defined(op, data_type);

    expression->data_type = data_type;
    break;

  case TOKEN_TILDE:
    if (data_type.type != TYPE_INTEGER)
      error_operation_not_defined(op, data_type);

    expression->data_type = data_type;
    break;

  case TOKEN_NOT:
  case TOKEN_BANG:
    if (data_type.type != TYPE_BOOL)
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

  bool left_nullable = left.type == TYPE_NULL && nullable_data_type(right);
  bool right_nullable = right.type == TYPE_NULL && nullable_data_type(left);

  if ((op.type == TOKEN_EQUAL_EQUAL || op.type == TOKEN_BANG_EQUAL) &&
      (left_nullable || right_nullable))
  {
    Expr* expr;

    if (left.type == TYPE_NULL)
      expr = cast_to_bool(expression->right, right);
    else
      expr = cast_to_bool(expression->left, left);

    expression->left = expr;

    expression->right = EXPR();
    expression->right->type = EXPR_LITERAL;
    expression->right->literal.data_type = DATA_TYPE(TYPE_BOOL);
    expression->right->literal.boolean = false;

    expression->operand_data_type = DATA_TYPE(TYPE_BOOL);
    expression->return_data_type = DATA_TYPE(TYPE_BOOL);

    return DATA_TYPE(TYPE_BOOL);
  }

  if (left.type == TYPE_OBJECT)
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

    VarStmt* variable = map_get_var_stmt(class->members, name);
    if (!variable || (variable->data_type.type != TYPE_FUNCTION_MEMBER &&
                      variable->data_type.type != TYPE_FUNCTION_GROUP))
    {
      if (op.type == TOKEN_EQUAL_EQUAL || op.type == TOKEN_BANG_EQUAL)
      {
        goto skip;
      }

      error_missing_operator_overload(op, left, right, name);
      return DATA_TYPE(TYPE_VOID);
    }

    DataType function_data_type = variable->data_type;
    expand_function_group(&function_data_type, &right, 1);

    if (function_data_type.type == TYPE_FUNCTION_GROUP)
    {
      error_missing_operator_overload(op, left, right, name);
      return DATA_TYPE(TYPE_VOID);
    }

    FuncStmt* function = function_data_type.function_member.function;
    if (function->parameters.size != 2)
    {
      error_missing_operator_overload(op, left, right, name);
      return DATA_TYPE(TYPE_VOID);
    }

    if (!equal_data_type(right, array_at(&function->parameters, 1)->data_type) &&
        !assignable_data_type(&expression->right, array_at(&function->parameters, 1)->data_type,
                              right))
    {
      error_type_mismatch(op, array_at(&function->parameters, 1)->data_type, right);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->function = function;
    expression->return_data_type = function->data_type;
    expression->operand_data_type = left;

    return expression->return_data_type;
  }

skip:

  if (!equal_data_type(left, right))
  {
    if (!upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_FLOAT)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_CHAR), DATA_TYPE(TYPE_INTEGER)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_CHAR), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_INTEGER), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FLOAT), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_BOOL), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_ARRAY), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_OBJECT), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_ALIAS), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_POINTER),
                DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_MEMBER),
                DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_INTERNAL),
                DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_TEMPLATE),
                DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_GROUP),
                DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_PROTOTYPE), DATA_TYPE(TYPE_STRING)) &&
        !upcast(expression, &left, &right, DATA_TYPE(TYPE_PROTOTYPE_TEMPLATE),
                DATA_TYPE(TYPE_STRING)) &&

        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_INTEGER)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_STRING)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_FUNCTION_POINTER)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_OBJECT)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_ARRAY)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_ANY)) &&
        !upcast_boolable_to_bool(expression, &left, &right, DATA_TYPE(TYPE_NULL)))
    {
      error_type_mismatch(expression->op, left, right);
    }
  }

  expression->function = NULL;
  expression->return_data_type = left;
  expression->operand_data_type = left;

  switch (op.type)
  {
  case TOKEN_AND:
  case TOKEN_OR:
    if (left.type != TYPE_BOOL)
    {
      Expr* left_cast_expression = cast_to_bool(expression->left, left);
      Expr* right_cast_expression = cast_to_bool(expression->right, right);

      if (!left_cast_expression)
      {
        error_operation_not_defined(op, left);
      }
      else if (!right_cast_expression)
      {
        error_operation_not_defined(op, right);
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
    if (left.type != TYPE_INTEGER && left.type != TYPE_FLOAT && left.type != TYPE_BOOL &&
        left.type != TYPE_OBJECT && left.type != TYPE_CHAR && left.type != TYPE_STRING)
      error_operation_not_defined(op, left);

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (left.type != TYPE_CHAR && left.type != TYPE_INTEGER && left.type != TYPE_FLOAT &&
        left.type != TYPE_BOOL)
      error_operation_not_defined(op, left);

    expression->return_data_type = DATA_TYPE(TYPE_BOOL);
    break;
  case TOKEN_PLUS:
    if (left.type == TYPE_CHAR)
      expression->return_data_type = DATA_TYPE(TYPE_INTEGER);
    else if (left.type != TYPE_INTEGER && left.type != TYPE_FLOAT && left.type != TYPE_STRING)
      error_operation_not_defined(op, left);

    break;
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    if (left.type == TYPE_CHAR)
      expression->return_data_type = DATA_TYPE(TYPE_INTEGER);
    else if (left.type != TYPE_INTEGER && left.type != TYPE_FLOAT)
      error_operation_not_defined(op, left);

    break;

  case TOKEN_PERCENT:
  case TOKEN_AMPERSAND:
  case TOKEN_PIPE:
  case TOKEN_CARET:
  case TOKEN_LESS_LESS:
  case TOKEN_GREATER_GREATER:
    if (left.type == TYPE_CHAR)
      expression->return_data_type = DATA_TYPE(TYPE_INTEGER);
    else if (left.type != TYPE_INTEGER)
      error_operation_not_defined(op, left);

    break;

  default:
    error_unknown_operation(op);
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

  if (variable->scope != SCOPE_GLOBAL && variable->scope != SCOPE_CLASS &&
      variable->function != checker.function)
  {
    error_cannot_access_name_outside_function(expression->name, name);
    return DATA_TYPE(TYPE_VOID);
  }

  expression->variable = variable;
  expression->data_type = variable->data_type;

  expand_template_types(expression->template_types, &expression->data_type, expression->name);

  return expression->data_type;
}

static DataType check_assignment_expression(AssignExpr* expression)
{
  DataType value_data_type = check_expression(expression->value);
  expression->value_data_type = value_data_type;

  AssignExpr* previous_assignment = checker.assignment;
  checker.assignment = expression;

  Expr* target = expression->target;
  DataType target_data_type = check_expression(target);

  checker.assignment = previous_assignment;

  if (target_data_type.type == TYPE_VOID || target_data_type.type == TYPE_PROTOTYPE ||
      target_data_type.type == TYPE_PROTOTYPE_TEMPLATE ||
      target_data_type.type == TYPE_FUNCTION_TEMPLATE || target_data_type.type == TYPE_FUNCTION ||
      target_data_type.type == TYPE_FUNCTION_MEMBER ||
      target_data_type.type == TYPE_FUNCTION_GROUP ||
      target_data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    error_not_assignable(expression->op);
    return DATA_TYPE(TYPE_VOID);
  }

  data_type_inference(&value_data_type, &target_data_type);

  if (!equal_data_type(target_data_type, value_data_type) &&
      !assignable_data_type(&expression->value, target_data_type, value_data_type))
  {
    error_type_mismatch(expression->op, target_data_type, value_data_type);
    return DATA_TYPE(TYPE_VOID);
  }

  expression->value_data_type = value_data_type;

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
    if (target->index.expr_data_type.type == TYPE_STRING)
    {
      error_not_assignable(expression->op);
      return DATA_TYPE(TYPE_VOID);
    }

    if (target->index.expr_data_type.type == TYPE_OBJECT)
    {
      expression->function = target->index.function;
      expression->data_type = target->index.data_type;
    }
    else
    {
      expression->data_type = value_data_type;
    }

    expression->variable = NULL;

    return expression->data_type;
  }

  error_not_assignable(expression->op);
  return DATA_TYPE(TYPE_VOID);
}

static DataType check_call_expression(CallExpr* expression)
{
  Expr* callee = expression->callee;
  DataType callee_data_type = check_expression(callee);

  DataType* argument_data_types = alloca(expression->arguments.size * sizeof(DataType));
  for (unsigned int i = 0; i < expression->arguments.size; i++)
    argument_data_types[i] = check_expression(expression->arguments.elems[i]);

  expand_function_group(&callee_data_type, argument_data_types, expression->arguments.size);

  if (callee_data_type.type == TYPE_FUNCTION_GROUP)
  {
    error_cannot_find_suitable_overload(expression->callee_token, callee_data_type);
    return DATA_TYPE(TYPE_VOID);
  }
  else if (callee_data_type.type == TYPE_FUNCTION_TEMPLATE ||
           callee_data_type.type == TYPE_PROTOTYPE_TEMPLATE)
  {
    error_missing_template_types(expression->callee_token);
    return DATA_TYPE(TYPE_VOID);
  }
  else if (callee_data_type.type == TYPE_FUNCTION_MEMBER)
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
      argument->var.name = (Token){ .lexeme = "this", .length = sizeof("this") - 1 };
      argument->var.variable = array_at(&function->parameters, 0);
      argument->var.template_types = NULL;
      argument->var.data_type = DATA_TYPE(TYPE_OBJECT);
      argument->var.data_type.class = checker.class;
    }

    array_prepend(&expression->arguments, argument);

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
      VarStmt* parameter = function->parameters.elems[i];

      DataType argument_data_type = argument_data_types[i - 1];
      DataType parameter_data_type = parameter->data_type;

      data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(&expression->arguments.elems[i], parameter_data_type,
                                argument_data_type))
      {
        error_type_mismatch(expression->argument_tokens.elems[i - 1], parameter_data_type,
                            argument_data_type);
      }
    }

    expression->function = function;
    expression->return_data_type = function->data_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (callee_data_type.type == TYPE_FUNCTION)
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
      VarStmt* parameter = function->parameters.elems[i];

      DataType argument_data_type = argument_data_types[i];
      DataType parameter_data_type = parameter->data_type;

      data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(&expression->arguments.elems[i], parameter_data_type,
                                argument_data_type))
      {
        error_type_mismatch(expression->argument_tokens.elems[i], parameter_data_type,
                            argument_data_type);
      }
    }

    expression->function = function;
    expression->return_data_type = function->data_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (callee_data_type.type == TYPE_FUNCTION_INTERNAL ||
           callee_data_type.type == TYPE_FUNCTION_POINTER)
  {
    if (callee_data_type.function_internal.this)
    {
      Expr* argument = callee_data_type.function_internal.this;

      array_prepend(&expression->arguments, argument);
    }

    int number_of_arguments = array_size(&expression->arguments);
    int expected_number_of_arguments =
      array_size(&callee_data_type.function_internal.parameter_types);

    if (number_of_arguments != expected_number_of_arguments)
    {
      if (callee_data_type.function_internal.this)
      {
        number_of_arguments--;
        expected_number_of_arguments--;
      }

      error_invalid_arity(expression->callee_token, expected_number_of_arguments,
                          number_of_arguments);
      return DATA_TYPE(TYPE_VOID);
    }

    int offset = 0;
    if (callee_data_type.type == TYPE_FUNCTION_INTERNAL && callee_data_type.function_internal.this)
      offset = 1;

    for (int i = offset; i < number_of_arguments; i++)
    {
      DataType argument_data_type = argument_data_types[i - offset];
      DataType parameter_data_type = callee_data_type.function_internal.parameter_types.elems[i];

      data_type_inference(&argument_data_type, &parameter_data_type);

      if (!equal_data_type(argument_data_type, parameter_data_type) &&
          !assignable_data_type(&expression->arguments.elems[i], parameter_data_type,
                                argument_data_type))
      {
        Token argument_token;

        if (callee_data_type.function_internal.this)
          argument_token = expression->argument_tokens.elems[i - 1];
        else
          argument_token = expression->argument_tokens.elems[i];

        error_type_mismatch(argument_token, parameter_data_type, argument_data_type);
      }
    }

    if (callee_data_type.type == TYPE_FUNCTION_INTERNAL)
      expression->function_name = callee_data_type.function_internal.name;

    expression->return_data_type = *callee_data_type.function_internal.return_type;
    expression->callee_data_type = callee_data_type;

    return expression->return_data_type;
  }
  else if (callee_data_type.type == TYPE_PROTOTYPE)
  {
    ClassStmt* class = callee_data_type.class;
    VarStmt* variable = map_get_var_stmt(class->members, "__init__");

    Expr* argument = EXPR();
    argument->type = EXPR_LITERAL;
    argument->literal.data_type = DATA_TYPE(TYPE_NULL);
    argument->literal.data_type.null_function = ALLOC(bool);
    *argument->literal.data_type.null_function = false;

    array_prepend(&expression->arguments, argument);

    if (variable && (variable->data_type.type == TYPE_FUNCTION_MEMBER ||
                     variable->data_type.type == TYPE_FUNCTION_GROUP))
    {
      DataType function_data_type = variable->data_type;
      expand_function_group(&function_data_type, argument_data_types,
                            expression->arguments.size - 1);

      if (function_data_type.type == TYPE_FUNCTION_GROUP)
      {
        error_cannot_find_suitable_overload(expression->callee_token, function_data_type);
        return DATA_TYPE(TYPE_VOID);
      }

      FuncStmt* function = function_data_type.function;
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
        VarStmt* parameter = function->parameters.elems[i];

        DataType argument_data_type = argument_data_types[i - 1];
        DataType parameter_data_type = parameter->data_type;

        data_type_inference(&argument_data_type, &parameter_data_type);

        if (!equal_data_type(argument_data_type, parameter_data_type) &&
            !assignable_data_type(&expression->arguments.elems[i], parameter_data_type,
                                  argument_data_type))
        {
          error_type_mismatch(expression->argument_tokens.elems[i - 1], parameter_data_type,
                              argument_data_type);
        }
      }

      expression->function_name =
        variable->data_type.type == TYPE_FUNCTION_GROUP
          ? function_data_type_to_string(class->name.lexeme, function_data_type)
          : class->name.lexeme;
      expression->function = function;
    }
    else
    {
      int number_of_arguments = array_size(&expression->arguments);
      if (number_of_arguments > 1)
      {
        error_invalid_arity(expression->callee_token, 0, number_of_arguments - 1);
        return DATA_TYPE(TYPE_VOID);
      }

      expression->function_name = class->name.lexeme;
      expression->function = class->default_constructor;
    }

    expression->callee_data_type = callee_data_type;
    expression->return_data_type = token_to_data_type(class->name);

    return expression->return_data_type;
  }
  else if (callee_data_type.type == TYPE_ALIAS)
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

  if (data_type.type == TYPE_OBJECT || data_type.type == TYPE_PROTOTYPE)
  {
    const char* name = expression->name.lexeme;

    ClassStmt* class = data_type.class;
    VarStmt* variable = map_get_var_stmt(class->members, name);

    if (!variable)
    {
      error_cannot_find_member_name(expression->name, name, data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    if (data_type.type == TYPE_PROTOTYPE && (variable->data_type.type != TYPE_FUNCTION_TEMPLATE &&
                                             variable->data_type.type != TYPE_FUNCTION_MEMBER &&
                                             variable->data_type.type != TYPE_FUNCTION_GROUP))
    {
      error_cannot_find_member_name(expression->name, name, data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->variable = variable;
    expression->data_type = variable->data_type;
    expression->expr_data_type = data_type;

    expand_template_types(expression->template_types, &expression->data_type, expression->name);

    if (expression->data_type.type == TYPE_FUNCTION_MEMBER)
    {
      expression->data_type.function_member.this = expression->expr;
    }
    else if (expression->data_type.type == TYPE_FUNCTION_GROUP)
    {
      for (unsigned int i = 0; i < expression->data_type.function_group.size; i++)
      {
        if (expression->data_type.function_group.elems[i].type == TYPE_FUNCTION_MEMBER)
        {
          expression->data_type.function_group.elems[i].function_member.this = expression->expr;
        }
      }
    }

    return expression->data_type;
  }
  else if (data_type.type == TYPE_ARRAY)
  {
    if (data_type.array.data_type->type == TYPE_VOID)
    {
      error_array_type_is_unresolved(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

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
    else if (strcmp("pushString", name) == 0)
    {
      if (array_data_type_element(data_type).type != TYPE_CHAR)
      {
        error_cannot_find_member_name(expression->name, name, data_type);
        return DATA_TYPE(TYPE_VOID);
      }

      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.push_string";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_VOID;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_STRING));

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
    else if (strcmp("clear", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.clear";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_VOID;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("reserve", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.reserve";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_VOID;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      for (int i = 0; i < *data_type.array.count; i++)
      {
        array_add(&expression->data_type.function_internal.parameter_types,
                  DATA_TYPE(TYPE_INTEGER));
      }

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("remove", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.remove";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      *expression->data_type.function_internal.return_type = array_data_type_element(data_type);

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_INTEGER));

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("toString", name) == 0)
    {
      if (array_data_type_element(data_type).type != TYPE_CHAR)
      {
        error_cannot_find_member_name(expression->name, name, data_type);
        return DATA_TYPE(TYPE_VOID);
      }

      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "array.to_string";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_STRING;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("join", name) == 0)
    {
      if (array_data_type_element(data_type).type != TYPE_STRING)
      {
        error_cannot_find_member_name(expression->name, name, data_type);
        return DATA_TYPE(TYPE_VOID);
      }

      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.join";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_STRING;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_STRING));

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, data_type);
    return DATA_TYPE(TYPE_VOID);
  }
  else if (data_type.type == TYPE_STRING)
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
    else if (strcmp("indexOf", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.index_of";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_INTEGER;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("split", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.split";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_ARRAY;
      expression->data_type.function_internal.return_type->array.data_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->array.data_type->type = TYPE_STRING;
      expression->data_type.function_internal.return_type->array.count = ALLOC(unsigned char);
      *expression->data_type.function_internal.return_type->array.count = 1;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_STRING));

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("trim", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.trim";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_STRING;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("contains", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.contains";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_BOOL;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("count", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.count";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_INTEGER;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("replace", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.replace";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_STRING;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("startsWith", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.starts_with";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_BOOL;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("endsWith", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.ends_with";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_BOOL;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }
    else if (strcmp("toArray", name) == 0)
    {
      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = "string.to_array";
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_ARRAY;
      expression->data_type.function_internal.return_type->array.data_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->array.data_type->type = TYPE_CHAR;
      expression->data_type.function_internal.return_type->array.count = ALLOC(unsigned char);
      *expression->data_type.function_internal.return_type->array.count = 1;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, data_type);
    return DATA_TYPE(TYPE_VOID);
  }
  else if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_FLOAT ||
           data_type.type == TYPE_CHAR || data_type.type == TYPE_BOOL)
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
    else if (strcmp("sqrt", name) == 0)
    {
      const char* function_name;
      switch (data_type.type)
      {

      case TYPE_BOOL:
      case TYPE_CHAR:
      case TYPE_INTEGER:
        error_cannot_find_member_name(expression->name, name, data_type);
        return DATA_TYPE(TYPE_VOID);
      case TYPE_FLOAT:
        function_name = "float.sqrt";
        break;
      default:
        UNREACHABLE("Unknown data type hash");
      }

      expression->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
      expression->data_type.function_internal.name = function_name;
      expression->data_type.function_internal.this = expression->expr;
      expression->data_type.function_internal.return_type = ALLOC(DataType);
      expression->data_type.function_internal.return_type->type = TYPE_FLOAT;

      array_init(&expression->data_type.function_internal.parameter_types);
      array_add(&expression->data_type.function_internal.parameter_types, data_type);

      expression->variable = NULL;
      expression->expr_data_type = data_type;

      return expression->data_type;
    }

    error_cannot_find_member_name(expression->name, name, data_type);
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

  if (expr_data_type.type == TYPE_STRING)
  {
    if (index_data_type.type != TYPE_INTEGER)
    {
      error_index_not_an_int(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->data_type = DATA_TYPE(TYPE_CHAR);
    expression->expr_data_type = expr_data_type;
    expression->index_data_type = index_data_type;

    return expression->data_type;
  }
  else if (expr_data_type.type == TYPE_ARRAY)
  {
    if (index_data_type.type != TYPE_INTEGER)
    {
      error_index_not_an_int(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    if (expr_data_type.array.data_type->type == TYPE_VOID)
    {
      error_array_type_is_unresolved(expression->expr_token);
      return DATA_TYPE(TYPE_VOID);
    }

    expression->data_type = array_data_type_element(expr_data_type);
    expression->expr_data_type = expr_data_type;
    expression->index_data_type = index_data_type;

    return expression->data_type;
  }
  else if (expr_data_type.type == TYPE_OBJECT)
  {
    ClassStmt* class = expr_data_type.class;
    VarStmt* variable =
      map_get_var_stmt(class->members, checker.assignment ? "__set__" : "__get__");
    DataType value_data_type =
      checker.assignment ? checker.assignment->value_data_type : DATA_TYPE(TYPE_VOID);

    if (!variable || (variable->data_type.type != TYPE_FUNCTION_MEMBER &&
                      variable->data_type.type != TYPE_FUNCTION_GROUP))
    {
      error_not_indexable_missing_overload(expression->expr_token, index_data_type,
                                           value_data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    DataType function_data_type = variable->data_type;

    if (checker.assignment)
      expand_function_group(&function_data_type, (DataType[]){ index_data_type, value_data_type },
                            2);
    else
      expand_function_group(&function_data_type, &index_data_type, 1);

    if (function_data_type.type == TYPE_FUNCTION_GROUP)
    {
      error_not_indexable_missing_overload(expression->expr_token, index_data_type,
                                           value_data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    FuncStmt* function = function_data_type.function_member.function;
    if (!equal_data_type(index_data_type, array_at(&function->parameters, 1)->data_type) &&
        !assignable_data_type(&expression->index, array_at(&function->parameters, 1)->data_type,
                              index_data_type))
    {
      error_not_indexable_missing_overload(expression->expr_token, index_data_type,
                                           value_data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    if (checker.assignment)
    {
      if (!equal_data_type(value_data_type, array_at(&function->parameters, 2)->data_type) &&
          !assignable_data_type(&checker.assignment->value,
                                array_at(&function->parameters, 2)->data_type,
                                checker.assignment->value_data_type))
      {
        error_not_indexable_missing_overload(expression->expr_token, index_data_type,
                                             value_data_type);
        return DATA_TYPE(TYPE_VOID);
      }
    }

    expression->function = function;
    expression->data_type = function->data_type;
    expression->expr_data_type = expr_data_type;
    expression->index_data_type = index_data_type;

    return checker.assignment ? value_data_type : function->data_type;
  }

  error_not_indexable(expression->expr_token, expr_data_type);
  return DATA_TYPE(TYPE_VOID);
}

static DataType check_array_expression(LiteralArrayExpr* expression)
{
  expression->data_type = DATA_TYPE(TYPE_ARRAY);
  expression->data_type.array.data_type = ALLOC(DataType);
  expression->data_type.array.count = ALLOC(unsigned char);
  expression->data_type.array.token = expression->token;
  expression->data_type.array.values = expression->values;
  expression->data_type.array.tokens = expression->tokens;

  *expression->data_type.array.count = 1;
  *expression->data_type.array.data_type = DATA_TYPE(TYPE_VOID);

  return expression->data_type;
}

static DataType check_is_expression(IsExpr* expression)
{
  expression->expr_data_type = check_expression(expression->expr);
  expression->is_data_type = data_type_token_to_data_type(expression->is_data_type_token);

  switch (expression->expr_data_type.type)
  {
  case TYPE_ANY:
    if (!assignable_data_type(NULL, expression->expr_data_type, expression->is_data_type))
    {
      error_type_is_not_assignable(expression->is_data_type_token.token, expression->is_data_type,
                                   expression->expr_data_type);
      return DATA_TYPE(TYPE_VOID);
    }

    break;
  default:
    error_type_mismatch(expression->expr_token, DATA_TYPE(TYPE_ANY), expression->expr_data_type);
    return DATA_TYPE(TYPE_VOID);
  }

  return DATA_TYPE(TYPE_BOOL);
}

static DataType check_if_expression(IfExpr* expression)
{
  DataType data_type = check_expression(expression->condition);
  if (data_type.type != TYPE_BOOL)
  {
    Expr* cast_expression = cast_to_bool(expression->condition, data_type);
    if (cast_expression)
      expression->condition = cast_expression;
    else
      error_condition_is_not_bool(expression->condition_token);
  }

  DataType left_data_type = check_expression(expression->left);
  DataType right_data_type = check_expression(expression->right);

  if (!equal_data_type(left_data_type, right_data_type) &&
      !assignable_data_type(&expression->right, left_data_type, right_data_type))
  {
    error_ternary_type_mismatch(expression->body_token, left_data_type, right_data_type);
  }

  return left_data_type;
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
  case EXPR_IS:
    return check_is_expression(&expression->is);
  case EXPR_IF:
    return check_if_expression(&expression->cond);

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
    if (statement->expr)
    {
      error_should_not_return_value_in_top_level(statement->keyword);
      return;
    }

    return;
  }

  if (statement->expr)
  {
    if (checker.function->data_type.type == TYPE_VOID)
    {
      error_should_not_return_value(statement->keyword, checker.function->name.lexeme);
      return;
    }

    DataType data_type = check_expression(statement->expr);
    data_type_inference(&data_type, &checker.function->data_type);

    if (!equal_data_type(checker.function->data_type, data_type) &&
        !assignable_data_type(&statement->expr, checker.function->data_type, data_type))
    {
      error_type_mismatch(statement->keyword, checker.function->data_type, data_type);
      return;
    }
  }
  else
  {
    if (checker.function->data_type.type != TYPE_VOID)
    {
      error_should_return_value(statement->keyword, checker.function->name.lexeme);
      return;
    }
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
  if (data_type.type != TYPE_BOOL)
  {
    Expr* cast_expression = cast_to_bool(statement->condition, data_type);
    if (cast_expression)
      statement->condition = cast_expression;
    else
      error_condition_is_not_bool(statement->keyword);
  }

  IfStmt* previous_cond = checker.cond;

  checker.environment = environment_init(checker.environment);
  checker.cond = statement;

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

  checker.cond = previous_cond;
}

static void check_while_statement(WhileStmt* statement)
{
  checker.environment = environment_init(checker.environment);

  Stmt* initializer_statement;
  array_foreach(&statement->initializer, initializer_statement)
  {
    check_statement(initializer_statement, true);
  }

  DataType data_type = check_expression(statement->condition);
  if (data_type.type != TYPE_BOOL)
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

  Stmt* incrementer_statement;
  array_foreach(&statement->incrementer, incrementer_statement)
  {
    check_statement(incrementer_statement, true);
  }

  checker.environment = checker.environment->parent;
}

static void check_variable_declaration(VarStmt* statement)
{
  if (statement->initializer)
  {
    statement->initializer_data_type = check_expression(statement->initializer);
  }

  if (statement->scope == SCOPE_NONE)
  {
    init_variable_declaration(statement);
  }

  if (statement->initializer)
  {
    data_type_inference(&statement->initializer_data_type, &statement->data_type);

    if (!equal_data_type(statement->data_type, statement->initializer_data_type) &&
        !assignable_data_type(&statement->initializer, statement->data_type,
                              statement->initializer_data_type))
    {
      error_type_mismatch(statement->equals, statement->data_type,
                          statement->initializer_data_type);
    }
  }
}

static void check_get_function_declaration(FuncStmt* function)
{
  if (strcmp(function->name_raw, "__get__") == 0)
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
  if (strcmp(function->name_raw, "__set__") == 0)
  {
    int got = array_size(&function->parameters);
    int expected = 3;

    if (expected != got)
    {
      error_invalid_set_arity(function->name);
      return;
    }
  }
}

static void check_str_function_declaration(FuncStmt* function)
{
  if (strcmp(function->name_raw, "__str__") == 0)
  {
    int got = array_size(&function->parameters);
    int expected = 1;

    if (expected != got)
    {
      error_invalid_str_arity(function->name);
      return;
    }

    if (function->data_type.type != TYPE_STRING)
    {
      error_invalid_str_return_type(function->name);
      return;
    }
  }
}

static void check_binary_overload_function_declaration(FuncStmt* function, const char* name)
{
  if (strcmp(function->name_raw, name) == 0)
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
  if (checker.function || checker.loop || checker.cond)
  {
    init_function_declaration(statement);
  }

  if (statement->body.size && statement->import)
  {
    error_imported_functions_cannot_have_bodies(statement->name);
    return;
  }

  FuncStmt* previous_function = checker.function;
  checker.function = statement;

  Environment* previous_environment = checker.environment;
  checker.environment = environment_init(checker.environment);

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
    parameter->function = statement;

    environment_set_variable(checker.environment, name, parameter);
  }

  if (statement->import == NULL && statement->data_type.type != TYPE_VOID &&
      !analyze_statements(statement->body))
  {
    error_no_return(statement->name);
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
    check_str_function_declaration(statement);

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

  checker.environment = previous_environment;
  checker.function = previous_function;
}

static void check_class_declaration(ClassStmt* statement)
{
  if (checker.function || checker.loop || checker.class || checker.cond)
  {
    error_unexpected_class(statement->name);
    return;
  }

  ClassStmt* previous_class = checker.class;
  checker.class = statement;

  Environment* previous_environment = checker.environment;
  checker.environment = statement->environment;

  int offset = 0;
  VarStmt* variable_statement;
  array_foreach(&statement->variables, variable_statement)
  {
    check_variable_declaration(variable_statement);

    const int size = size_data_type(variable_statement->data_type);
    if (size > statement->alignment)
      statement->alignment = size;

    variable_statement->offset = align(offset, size);
    offset = size + variable_statement->offset;
  }

  statement->size = align(offset, statement->alignment);

  FuncStmt* function_statement;
  array_foreach(&statement->functions, function_statement)
  {
    check_function_declaration(function_statement);
  }

  checker.environment = previous_environment;
  checker.class = previous_class;
}

static void check_class_template_declaration(ClassTemplateStmt* statement)
{
  ClassStmt* class_statement;
  array_foreach(&statement->classes, class_statement)
  {
    check_class_declaration(class_statement);
  }
}

static void check_import_declaration(ImportStmt* statement)
{
  if (checker.function || checker.loop || checker.class || checker.cond)
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

  case STMT_FUNCTION_TEMPLATE_DECL:
    if (checker.function || checker.loop || checker.cond)
      init_function_template_declaration(&statement->func_template);

    break;

  case STMT_CLASS_TEMPLATE_DECL:
    break;

  default:
    UNREACHABLE("Unhandled statement");
  }
}

static bool analyze_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_RETURN:
    return true;

  case STMT_IF:
    return analyze_statements(statement->cond.then_branch) &&
           analyze_statements(statement->cond.else_branch);

  case STMT_WHILE:
  case STMT_EXPR:
  case STMT_CONTINUE:
  case STMT_BREAK:
  case STMT_FUNCTION_DECL:
  case STMT_VARIABLE_DECL:
  case STMT_CLASS_DECL:
  case STMT_IMPORT_DECL:
  case STMT_CLASS_TEMPLATE_DECL:
  case STMT_FUNCTION_TEMPLATE_DECL:
    return false;

  default:
    UNREACHABLE("Unexpected statement to analyze");
  }
}

static bool analyze_statements(ArrayStmt statements)
{
  Stmt* statement;
  array_foreach(&statements, statement)
  {
    if (analyze_statement(statement))
      return true;
  }

  return false;
}

void checker_init_globals(void)
{
  {
    VarStmt* variable = ALLOC(VarStmt);
    variable->name = TOKEN_EMPTY();
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
    variable->data_type.function_internal.name = "alloc";
    variable->data_type.function_internal.this = NULL;
    variable->data_type.function_internal.return_type = ALLOC(DataType);
    variable->data_type.function_internal.return_type->type = TYPE_INTEGER;

    array_init(&variable->data_type.function_internal.parameter_types);
    array_add(&variable->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_INTEGER));

    environment_set_variable(checker.environment, variable->data_type.function_internal.name,
                             variable);
  }

  {
    VarStmt* variable = ALLOC(VarStmt);
    variable->name = TOKEN_EMPTY();
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
    variable->data_type.function_internal.name = "memory";
    variable->data_type.function_internal.this = NULL;
    variable->data_type.function_internal.return_type = ALLOC(DataType);
    variable->data_type.function_internal.return_type->type = TYPE_INTEGER;

    array_init(&variable->data_type.function_internal.parameter_types);

    environment_set_variable(checker.environment, variable->data_type.function_internal.name,
                             variable);
  }

  {
    VarStmt* variable = ALLOC(VarStmt);
    variable->name = TOKEN_EMPTY();
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
    variable->data_type.function_internal.name = "allocReset";
    variable->data_type.function_internal.this = NULL;
    variable->data_type.function_internal.return_type = ALLOC(DataType);
    variable->data_type.function_internal.return_type->type = TYPE_VOID;

    array_init(&variable->data_type.function_internal.parameter_types);

    environment_set_variable(checker.environment, variable->data_type.function_internal.name,
                             variable);
  }

  const char* writers[] = {
    "writeInt",
    "writeFloat",
    "writeChar",
    "writeBool",
  };
  DataType writer_param_types[] = {
    DATA_TYPE(TYPE_INTEGER),
    DATA_TYPE(TYPE_FLOAT),
    DATA_TYPE(TYPE_CHAR),
    DATA_TYPE(TYPE_BOOL),
  };

  for (unsigned int i = 0; i < sizeof(writers) / sizeof_ptr(writers); i++)
  {
    const char* name = writers[i];
    DataType param_type = writer_param_types[i];

    VarStmt* variable = ALLOC(VarStmt);
    variable->name = TOKEN_EMPTY();
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
    variable->data_type.function_internal.name = name;
    variable->data_type.function_internal.this = NULL;
    variable->data_type.function_internal.return_type = ALLOC(DataType);
    variable->data_type.function_internal.return_type->type = TYPE_VOID;

    array_init(&variable->data_type.function_internal.parameter_types);
    array_add(&variable->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_INTEGER));
    array_add(&variable->data_type.function_internal.parameter_types, param_type);

    environment_set_variable(checker.environment, variable->data_type.function_internal.name,
                             variable);
  }

  const char* readers[] = {
    "readInt",
    "readFloat",
    "readChar",
    "readBool",
  };
  DataType reader_return_types[] = {
    DATA_TYPE(TYPE_INTEGER),
    DATA_TYPE(TYPE_FLOAT),
    DATA_TYPE(TYPE_CHAR),
    DATA_TYPE(TYPE_BOOL),
  };

  for (unsigned int i = 0; i < sizeof(readers) / sizeof_ptr(readers); i++)
  {
    const char* name = readers[i];
    DataType return_type = reader_return_types[i];

    VarStmt* variable = ALLOC(VarStmt);
    variable->name = TOKEN_EMPTY();
    variable->type = DATA_TYPE_TOKEN_EMPTY();
    variable->function = NULL;
    variable->initializer = NULL;
    variable->scope = SCOPE_GLOBAL;
    variable->index = -1;
    variable->data_type = DATA_TYPE(TYPE_FUNCTION_INTERNAL);
    variable->data_type.function_internal.name = name;
    variable->data_type.function_internal.this = NULL;
    variable->data_type.function_internal.return_type = ALLOC(DataType);
    *variable->data_type.function_internal.return_type = return_type;

    array_init(&variable->data_type.function_internal.parameter_types);
    array_add(&variable->data_type.function_internal.parameter_types, DATA_TYPE(TYPE_INTEGER));

    environment_set_variable(checker.environment, variable->data_type.function_internal.name,
                             variable);
  }
}

void checker_init(ArrayStmt statements,
                  void (*error_callback)(int start_line, int start_column, int end_line,
                                         int end_column, const char* message))
{
  checker.function = NULL;
  checker.template = NULL;
  checker.class = NULL;
  checker.loop = NULL;
  checker.cond = NULL;
  checker.assignment = NULL;
  checker.statements = statements;

  checker.errors = 0;
  checker.error = false;
  checker.error_callback = error_callback;

  checker.environment = environment_init(NULL);
  checker.global_environment = checker.environment;

  array_init(&checker.global_locals);

  checker_init_globals();
}

int checker_errors(void)
{
  return checker.errors;
}

void checker_validate(void)
{
  Stmt* statement;

  array_foreach(&checker.statements, statement)
  {
    checker.error = false;

    switch (statement->type)
    {
    case STMT_CLASS_DECL:
      init_class_declaration(&statement->class);
      break;
    case STMT_CLASS_TEMPLATE_DECL:
      init_class_template_declaration(&statement->class_template);
      break;
    case STMT_FUNCTION_TEMPLATE_DECL:
      init_function_template_declaration(&statement->func_template);
      break;

    default:
      break;
    }
  }

  array_foreach(&checker.statements, statement)
  {
    checker.error = false;

    switch (statement->type)
    {
    case STMT_FUNCTION_DECL:
      init_function_declaration(&statement->func);
      break;
    case STMT_CLASS_DECL:
      init_class_declaration_body(&statement->class);
      break;
    case STMT_IMPORT_DECL:
      init_import_declaration(&statement->import);
      break;
    case STMT_VARIABLE_DECL:
      init_variable_declaration(&statement->var);
      break;

    default:
      break;
    }
  }

  array_foreach(&checker.statements, statement)
  {
    check_statement(statement, true);
  }

  array_foreach(&checker.statements, statement)
  {
    checker.error = false;

    switch (statement->type)
    {
    case STMT_CLASS_TEMPLATE_DECL:
      check_class_template_declaration(&statement->class_template);
      break;

    default:
      break;
    }
  }
}

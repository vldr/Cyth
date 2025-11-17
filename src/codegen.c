#include "codegen.h"
#include "array.h"
#include "checker.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "mir.h"
#include "statement.h"

#include <mir-gen.h>
#include <setjmp.h>

typedef void (*Start)(void);
typedef struct _STRING
{
  unsigned int size;
  char data[];
} String;

typedef struct _ARRAY
{
  unsigned int size;
  unsigned int capacity;
  void* data;
} Array;

typedef struct _FUNCTION
{
  MIR_item_t func;
  MIR_item_t proto;
} Function;

array_def(MIR_type_t, MIR_type_t);
array_def(MIR_var_t, MIR_var_t);
array_def(MIR_op_t, MIR_op_t);
array_def(MIR_reg_t, MIR_reg_t);

static struct
{
  jmp_buf jmp;
  MIR_context_t ctx;
  MIR_module_t module;
  MIR_item_t function;
  ArrayStmt statements;
  MIR_label_t continue_label;
  MIR_label_t break_label;
  MapMIR_item string_constants;
  MapFunction functions;
  int strings;

  Function panic;
  Function malloc;
  Function realloc;
  Function string_concat;
  Function string_bool_cast;
  Function string_int_cast;
  Function string_float_cast;
  Function string_char_cast;
} codegen;

static void generate_expression(MIR_reg_t dest, Expr* expression);
static void generate_statement(Stmt* statement);
static void generate_statements(ArrayStmt* statements);

static void log_int(int n)
{
  printf("%d\n", n);
}

static void log_float(float n)
{
  printf("%f\n", n);
}

static void log_char(char n)
{
  printf("%c\n", n);
}

static void log_string(String* n)
{
  printf("%.*s\n", n->size, n->data);
}

static String* string_concat(String* left, String* right)
{
  uintptr_t size = sizeof(String) + left->size + right->size;

  String* result = malloc(size);
  result->size = left->size + right->size;

  memcpy(result->data, left->data, left->size);
  memcpy(result->data + left->size, right->data, right->size);

  return result;
}

static String* string_int_cast(int n)
{
  int length = snprintf(NULL, 0, "%d", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length;

  snprintf(result->data, length, "%d", n);

  return result;
}

static String* string_float_cast(float n)
{
  int length = snprintf(NULL, 0, "%g", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length;

  snprintf(result->data, length, "%g", n);

  return result;
}

static String* string_char_cast(char n)
{
  int length = snprintf(NULL, 0, "%c", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length;

  snprintf(result->data, length, "%c", n);

  return result;
}

static String* string_bool_cast(bool n)
{
  static String true_string = { .size = sizeof("true") - 1, .data = "true" };
  static String false_string = { .size = sizeof("false") - 1, .data = "false" };

  return n ? &true_string : &false_string;
}

static void panic(String* n)
{
  printf("%.*s\n", n->size, n->data);
  longjmp(codegen.jmp, 1);
}

static MIR_insn_code_t data_type_to_mov_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FLOAT:
    return MIR_FMOV;
  default:
    return MIR_MOV;
  }
}

static MIR_type_t data_type_to_mir_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_ALIAS:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_FUNCTION_GROUP:
    return MIR_T_UNDEF;
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER:
  case TYPE_NULL:
  case TYPE_ANY:
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
  case TYPE_STRING:
  case TYPE_OBJECT:
  case TYPE_ARRAY:
    return MIR_T_I64;
  case TYPE_FLOAT:
    return MIR_T_F;
  }

  UNREACHABLE("Unhandled data type");
}

static MIR_type_t data_type_to_mir_array_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_ALIAS:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_FUNCTION_GROUP:
    return MIR_T_UNDEF;
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER:
  case TYPE_NULL:
  case TYPE_ANY:
  case TYPE_STRING:
  case TYPE_OBJECT:
  case TYPE_ARRAY:
    return MIR_T_I64;
  case TYPE_BOOL:
  case TYPE_CHAR:
    return MIR_T_I8;
  case TYPE_INTEGER:
    return MIR_T_I32;
  case TYPE_FLOAT:
    return MIR_T_F;
  }

  UNREACHABLE("Unhandled data type");
}

static MIR_item_t data_type_to_proto(DataType data_type)
{
  if (data_type.type == TYPE_FUNCTION || data_type.type == TYPE_FUNCTION_INTERNAL ||
      data_type.type == TYPE_FUNCTION_MEMBER || data_type.type == TYPE_FUNCTION_POINTER)
  {
    DataType return_data_type;
    ArrayDataType parameter_types;
    expand_function_data_type(data_type, &return_data_type, &parameter_types);

    ArrayMIR_var_t vars;
    array_init(&vars);

    DataType parameter_type;
    array_foreach(&parameter_types, parameter_type)
    {
      MIR_var_t var;
      var.name = memory_sprintf("%d", _i);
      var.type = data_type_to_mir_type(parameter_type);

      array_add(&vars, var);
    }

    return MIR_new_proto_arr(
      codegen.ctx, memory_sprintf("%s.proto", data_type_to_string(data_type)),
      return_data_type.type != TYPE_VOID, (MIR_type_t[]){ data_type_to_mir_type(return_data_type) },
      vars.size, vars.elems);
  }

  UNREACHABLE("Unhandled data type");
}

static void generate_malloc_expression(MIR_reg_t dest, MIR_op_t size)
{
  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_call_insn(codegen.ctx, 4,
                                    MIR_new_ref_op(codegen.ctx, codegen.malloc.proto),
                                    MIR_new_ref_op(codegen.ctx, codegen.malloc.func),
                                    MIR_new_reg_op(codegen.ctx, dest), size));
}

static void generate_realloc_expression(MIR_op_t dest, MIR_op_t ptr, MIR_op_t size)
{
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.realloc.proto),
                      MIR_new_ref_op(codegen.ctx, codegen.realloc.func), dest, ptr, size));
}

static void generate_string_literal_expression(MIR_reg_t dest, const char* literal, int length)
{
  MIR_item_t item = map_get_mir_item(&codegen.string_constants, literal);
  if (!item)
  {
    uintptr_t size = sizeof(String) + length;
    String* string = malloc(size);
    string->size = length;
    memcpy(string->data, literal, length);

    const char* name = memory_sprintf("string.%d", map_size_mir_item(&codegen.string_constants));
    item = MIR_new_data(codegen.ctx, name, MIR_T_U8, size, string);

    map_put_mir_item(&codegen.string_constants, literal, item);
  }

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, data_type_to_mov_type(DATA_TYPE(TYPE_STRING)),
                               MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_ref_op(codegen.ctx, item)));
}

static MIR_op_t generate_array_length_op(MIR_reg_t base)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U32, 0, base, 0, 1);
}

static MIR_op_t generate_array_capacity_op(MIR_reg_t base)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U32, sizeof(unsigned int), base, 0, 1);
}

static MIR_op_t generate_array_data_op(MIR_reg_t base)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_I64, sizeof(unsigned int) + sizeof(unsigned int), base,
                        0, 1);
}

static void generate_default_array_initialization(MIR_reg_t dest)
{
  generate_malloc_expression(dest, MIR_new_int_op(codegen.ctx, sizeof(Array)));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_length_op(dest),
                               MIR_new_int_op(codegen.ctx, 0)));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_capacity_op(dest),
                               MIR_new_int_op(codegen.ctx, 0)));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_data_op(dest),
                               MIR_new_int_op(codegen.ctx, 0)));
}

static Function* generate_array_push_function(DataType data_type, DataType element_data_type)
{
  const char* name = memory_sprintf("array.push.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = MIR_T_UNDEF;
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) },
                           { .name = "value", .type = data_type_to_mir_type(element_data_type) } };

    MIR_item_t previous_funcion = codegen.function;
    MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(codegen.ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    codegen.function = function->func;

    MIR_reg_t ptr = MIR_reg(codegen.ctx, "ptr", codegen.function->u.func);
    MIR_reg_t value = MIR_reg(codegen.ctx, "value", codegen.function->u.func);

    {
      MIR_label_t push_label = MIR_new_label(codegen.ctx);
      MIR_label_t resize_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BEQS,
                                   MIR_new_label_op(codegen.ctx, resize_label),
                                   generate_array_length_op(ptr), generate_array_capacity_op(ptr)));
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, push_label)));

      MIR_append_insn(codegen.ctx, codegen.function, resize_label);

      MIR_reg_t new_size = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_UMULOS, generate_array_capacity_op(ptr),
                                   generate_array_capacity_op(ptr),
                                   MIR_new_int_op(codegen.ctx, 2)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADDS, generate_array_capacity_op(ptr),
                                   generate_array_capacity_op(ptr),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_UMULOS, MIR_new_reg_op(codegen.ctx, new_size),
                                   generate_array_capacity_op(ptr),
                                   MIR_new_int_op(codegen.ctx, size_data_type(element_data_type))));

      generate_realloc_expression(generate_array_data_op(ptr), generate_array_data_op(ptr),
                                  MIR_new_reg_op(codegen.ctx, new_size));

      MIR_append_insn(codegen.ctx, codegen.function, push_label);
    }

    {
      MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, array_ptr),
                                   generate_array_data_op(ptr)));

      MIR_reg_t index = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, index),
                                   generate_array_length_op(ptr)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type)),
                     MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADDS, generate_array_length_op(ptr),
                                   generate_array_length_op(ptr), MIR_new_int_op(codegen.ctx, 1)));
    }

    codegen.function = previous_funcion;
    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static void generate_panic(Token token, const char* what)
{
  const char* message =
    memory_sprintf("%s at a.cy:%d:%d\n", what, token.start_line, token.start_column);

  MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
  generate_string_literal_expression(ptr, message, strlen(message));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_call_insn(codegen.ctx, 3,
                                    MIR_new_ref_op(codegen.ctx, codegen.panic.proto),
                                    MIR_new_ref_op(codegen.ctx, codegen.panic.func),
                                    MIR_new_reg_op(codegen.ctx, ptr)));
}

static MIR_op_t generate_string_length_op(MIR_reg_t base)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_I32, 0, base, 0, 1);
}

static MIR_op_t generate_string_at_op(MIR_reg_t base, MIR_reg_t index)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U8, sizeof(unsigned int), base, index, 1);
}

static void generate_default_initialization(MIR_reg_t dest, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_INTEGER:
  case TYPE_CHAR:
  case TYPE_BOOL:
  case TYPE_NULL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, 0)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_float_op(codegen.ctx, 0.0)));
    return;
  case TYPE_STRING:
    generate_string_literal_expression(dest, "", 0);
    return;
  case TYPE_ARRAY:
    generate_default_array_initialization(dest);
    return;
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static Function* generate_function_internal(DataType data_type)
{
  assert(data_type.type == TYPE_FUNCTION_INTERNAL);

  const char* name = data_type.function_internal.name;

  if (strcmp(name, "array.push") == 0)
    return generate_array_push_function(array_at(&data_type.function_internal.parameter_types, 0),
                                        array_at(&data_type.function_internal.parameter_types, 1));
  // if (strcmp(name, "array.push_string") == 0)
  //   return generate_array_push_string_function(
  //     array_at(&data_type.function_internal.parameter_types, 0));
  // else if (strcmp(name, "array.to_string") == 0)
  //   return generate_array_to_string_function(
  //     array_at(&data_type.function_internal.parameter_types, 0));
  // else if (strcmp(name, "array.pop") == 0)
  //   return generate_array_pop_function(array_at(&data_type.function_internal.parameter_types,
  //   0));
  // else if (strcmp(name, "array.clear") == 0)
  //   return generate_array_clear_function(array_at(&data_type.function_internal.parameter_types,
  //   0));
  // else if (strcmp(name, "array.reserve") == 0)
  //   return generate_array_reserve_function(
  //     array_at(&data_type.function_internal.parameter_types, 0));
  // else if (strcmp(name, "int.hash") == 0)
  //   return generate_int_hash_function();
  // else if (strcmp(name, "float.sqrt") == 0)
  //   return generate_float_sqrt_function();
  // else if (strcmp(name, "float.hash") == 0)
  //   return generate_float_hash_function();
  // else if (strcmp(name, "string.hash") == 0)
  //   return generate_string_hash_function();
  // else if (strcmp(name, "string.index_of") == 0)
  //   return generate_string_index_of_function();
  // else if (strcmp(name, "string.count") == 0)
  //   return generate_string_count_function();
  // else if (strcmp(name, "string.replace") == 0)
  //   return generate_string_replace_function();
  // else if (strcmp(name, "string.trim") == 0)
  //   return generate_string_trim_function();
  // else if (strcmp(name, "string.starts_with") == 0)
  //   return generate_string_starts_with_function();
  // else if (strcmp(name, "string.ends_with") == 0)
  //   return generate_string_ends_with_function();
  // else if (strcmp(name, "string.contains") == 0)
  //   return generate_string_contains_function();
  // else if (strcmp(name, "string.split") == 0)
  //   return generate_string_split_function(*data_type.function_internal.return_type);
  // else if (strcmp(name, "string.join") == 0)
  //   return generate_string_join_function(array_at(&data_type.function_internal.parameter_types,
  //   0));
  // else if (strcmp(name, "string.to_array") == 0)
  //   return generate_string_to_array_function(*data_type.function_internal.return_type);

  // else if (strcmp(name, "alloc") == 0)
  //   return generate_alloc_function();
  // else if (strcmp(name, "allocReset") == 0)
  //   return generate_alloc_reset_function();
  // else if (strcmp(name, "memory") == 0)
  //   return generate_memory_function();

  // else if (strcmp(name, "writeInt") == 0)
  //   return generate_write_function(name, BinaryenTypeInt32(), 4);
  // else if (strcmp(name, "writeFloat") == 0)
  //   return generate_write_function(name, BinaryenTypeFloat32(), 4);
  // else if (strcmp(name, "writeChar") == 0)
  //   return generate_write_function(name, BinaryenTypeInt32(), 1);
  // else if (strcmp(name, "writeBool") == 0)
  //   return generate_write_function(name, BinaryenTypeInt32(), 1);

  // else if (strcmp(name, "readInt") == 0)
  //   return generate_read_function(name, BinaryenTypeInt32(), 4);
  // else if (strcmp(name, "readFloat") == 0)
  //   return generate_read_function(name, BinaryenTypeFloat32(), 4);
  // else if (strcmp(name, "readChar") == 0)
  //   return generate_read_function(name, BinaryenTypeInt32(), 1);
  // else if (strcmp(name, "readBool") == 0)
  //   return generate_read_function(name, BinaryenTypeInt32(), 1);

  else
    UNREACHABLE("Unexpected internal function");
}

static void generate_function_pointer(MIR_reg_t dest, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FUNCTION:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_ref_op(codegen.ctx, data_type.function->item)));
    return;
  case TYPE_FUNCTION_MEMBER:
    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type), MIR_new_reg_op(codegen.ctx, dest),
                   MIR_new_ref_op(codegen.ctx, data_type.function_member.function->item)));
    return;
  case TYPE_FUNCTION_INTERNAL:
  default:
    UNREACHABLE("Unknown function type");
  }
}

static void generate_group_expression(MIR_reg_t dest, GroupExpr* expression)
{
  generate_expression(dest, expression->expr);
}

static void generate_literal_expression(MIR_reg_t dest, LiteralExpr* expression)
{
  switch (expression->data_type.type)
  {
  case TYPE_INTEGER:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->integer)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_float_op(codegen.ctx, expression->floating)));
    return;
  case TYPE_BOOL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->boolean)));
    return;
  case TYPE_NULL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, 0)));
    return;
  case TYPE_CHAR:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->string.data[0])));
    return;
  case TYPE_STRING:
    generate_string_literal_expression(dest, expression->string.data, expression->string.length);
    return;
  default:
    UNREACHABLE("Unhandled literal value");
  }
}

static void generate_binary_expression(MIR_reg_t dest, BinaryExpr* expression)
{
  MIR_reg_t left = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->operand_data_type), codegen.function->u.func);
  MIR_reg_t right = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->operand_data_type), codegen.function->u.func);

  generate_expression(left, expression->left);
  generate_expression(right, expression->right);
  int op = 0;

  DataType data_type = expression->operand_data_type;

  switch (expression->op.type)
  {
  case TOKEN_PLUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_ADD;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FADD;
    else if (data_type.type == TYPE_STRING)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.string_concat.proto),
                          MIR_new_ref_op(codegen.ctx, codegen.string_concat.func),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_SUB;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FSUB;
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_MULS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FMUL;
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_DIVS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FDIV;
    else
      UNREACHABLE("Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
  case TOKEN_AMPERSAND:
  case TOKEN_PIPE:
  case TOKEN_CARET:
  case TOKEN_LESS_LESS:
  case TOKEN_GREATER_GREATER: {
    switch (expression->op.type)
    {
    case TOKEN_PERCENT:
      op = MIR_MODS;
      break;
    case TOKEN_AMPERSAND:
      op = MIR_ANDS;
      break;
    case TOKEN_PIPE:
      op = MIR_ORS;
      break;
    case TOKEN_CARET:
      op = MIR_XORS;
      break;
    case TOKEN_LESS_LESS:
      op = MIR_LSHS;
      break;
    case TOKEN_GREATER_GREATER:
      op = MIR_URSHS;
      break;
    default:
      UNREACHABLE("Unknown operator");
    }

    if (data_type.type != TYPE_INTEGER && data_type.type != TYPE_CHAR)
      UNREACHABLE("Unsupported binary type for %, &, |, ^, <<, >>");

    break;
  }

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_EQS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FEQ;

    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_NES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FNE;
    else
      UNREACHABLE("Unsupported binary type for !=");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLE;
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGE;
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLT;
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGT;
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, left), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, left), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for OR");

    break;

  default:
    UNREACHABLE("Unhandled binary operation");
  }

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, op, MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_reg_op(codegen.ctx, left),
                               MIR_new_reg_op(codegen.ctx, right)));
}

static void generate_unary_expression(MIR_reg_t dest, UnaryExpr* expression)
{
  MIR_reg_t expr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->data_type),
                                     codegen.function->u.func);
  generate_expression(expr, expression->expr);

  switch (expression->op.type)
  {
  case TOKEN_TILDE:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_XORS, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFFFFFFFF)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for ~");

  case TOKEN_MINUS:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_NEGS, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      break;
    }
    else if (expression->data_type.type == TYPE_FLOAT)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_FNEG, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BFS,
                                   MIR_new_label_op(codegen.ctx, if_false_label),
                                   MIR_new_reg_op(codegen.ctx, expr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      break;
    }
    else
      UNREACHABLE("Unsupported unary type for !");

  default:
    UNREACHABLE("Unhandled unary expression");
  }
}

static void generate_cast_expression(MIR_reg_t dest, CastExpr* expression)
{
  MIR_reg_t expr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->from_data_type),
                                     codegen.function->u.func);
  generate_expression(expr, expression->expr);

  if (expression->to_data_type.type == TYPE_FLOAT &&
      expression->from_data_type.type == TYPE_INTEGER)
  {
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_I2F, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_reg_op(codegen.ctx, expr)));
    return;
  }
  else if (expression->to_data_type.type == TYPE_STRING)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_BOOL:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_call_insn(
                        codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, codegen.string_bool_cast.proto),
                        MIR_new_ref_op(codegen.ctx, codegen.string_bool_cast.func),
                        MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, expr)));
      return;
    case TYPE_FLOAT:
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 4,
                          MIR_new_ref_op(codegen.ctx, codegen.string_float_cast.proto),
                          MIR_new_ref_op(codegen.ctx, codegen.string_float_cast.func),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, expr)));
      return;
    case TYPE_INTEGER:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_call_insn(
                        codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, codegen.string_int_cast.proto),
                        MIR_new_ref_op(codegen.ctx, codegen.string_int_cast.func),
                        MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, expr)));
      return;
    case TYPE_CHAR:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_call_insn(
                        codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, codegen.string_char_cast.proto),
                        MIR_new_ref_op(codegen.ctx, codegen.string_char_cast.func),
                        MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, expr)));
      return;
    case TYPE_ANY:
    case TYPE_ARRAY:
    case TYPE_OBJECT:
    case TYPE_ALIAS:
    case TYPE_FUNCTION:
    case TYPE_FUNCTION_MEMBER:
    case TYPE_FUNCTION_INTERNAL:
    case TYPE_FUNCTION_POINTER:
    case TYPE_FUNCTION_TEMPLATE:
    case TYPE_FUNCTION_GROUP:
    case TYPE_PROTOTYPE:
    case TYPE_PROTOTYPE_TEMPLATE: {
      const char* name = data_type_to_string(expression->from_data_type);
      generate_string_literal_expression(dest, name, strlen(name));

      return;
    }
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_FLOAT)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_BOOL:
    case TYPE_INTEGER:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_I2F, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      return;
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_CHAR)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_INTEGER:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ANDS, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFF)));
      return;
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_INTEGER)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_CHAR:
    case TYPE_BOOL:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      return;
    case TYPE_FLOAT:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_F2I, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      return;
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_BOOL)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_FLOAT: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_FBNE, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, expr), MIR_new_float_op(codegen.ctx, 0.0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    case TYPE_INTEGER: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, expr), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    case TYPE_STRING: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     generate_string_length_op(expr), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    case TYPE_ANY:
    case TYPE_NULL:
    case TYPE_OBJECT:
    case TYPE_FUNCTION_POINTER: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, expr), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_ANY)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_ARRAY:
    case TYPE_STRING:
    case TYPE_OBJECT:
    case TYPE_NULL:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      return;

    default:
      break;
    }
  }
  // else if (expression->to_data_type.type == TYPE_ARRAY)
  // {
  //   switch (expression->from_data_type.type)
  //   {
  //   case TYPE_ANY: {
  //     BinaryenExpressionRef result =
  //       BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, value),
  //                  BinaryenUnreachable(codegen.module),
  //                  BinaryenRefCast(codegen.module, BinaryenExpressionCopy(value, codegen.module),
  //                                  data_type_to_binaryen_type(expression->to_data_type)));

  //     generate_debug_info(expression->type.token, BinaryenIfGetIfTrue(result), codegen.function);
  //     generate_debug_info(expression->type.token, BinaryenIfGetIfFalse(result),
  //     codegen.function); return result;
  //   }
  //   default:
  //     break;
  //   }
  // }
  // else if (expression->to_data_type.type == TYPE_OBJECT)
  // {
  //   switch (expression->from_data_type.type)
  //   {
  //   case TYPE_NULL:
  //     return generate_default_initialization(expression->to_data_type);
  //   case TYPE_ANY: {
  //     BinaryenExpressionRef result =
  //       BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, value),
  //                  BinaryenUnreachable(codegen.module),
  //                  BinaryenRefCast(codegen.module, BinaryenExpressionCopy(value, codegen.module),
  //                                  expression->to_data_type.class->ref));

  //     generate_debug_info(expression->type.token, BinaryenIfGetIfTrue(result), codegen.function);
  //     generate_debug_info(expression->type.token, BinaryenIfGetIfFalse(result),
  //     codegen.function); return result;
  //   }
  //   default:
  //     break;
  //   }
  // }
  else if (expression->to_data_type.type == TYPE_FUNCTION_POINTER)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_FUNCTION:
    case TYPE_FUNCTION_MEMBER:
    case TYPE_FUNCTION_INTERNAL:
      generate_function_pointer(dest, expression->from_data_type);
      return;
    case TYPE_NULL:
      generate_default_initialization(dest, expression->to_data_type);
      return;

    default:
      break;
    }
  }

  if (equal_data_type(expression->from_data_type, expression->to_data_type))
  {
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->to_data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_reg_op(codegen.ctx, expr)));
    return;
  }

  UNREACHABLE("Unsupported cast type");
}

static void generate_variable_expression(MIR_reg_t dest, VarExpr* expression)
{
  MIR_type_t type = data_type_to_mir_type(expression->data_type);
  if (type == MIR_T_UNDEF)
    return;

  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    generate_function_pointer(dest, expression->data_type);
    return;
  }

  Scope scope = expression->variable->scope;
  switch (scope)
  {
  case SCOPE_LOCAL: {
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_reg_op(codegen.ctx, expression->variable->reg)));

    return;
  }
  case SCOPE_GLOBAL: {
    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                 MIR_new_ref_op(codegen.ctx, expression->variable->item)));
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_mem_op(codegen.ctx, type, 0, ptr, 0, 1)));

    return;
  }
  // case SCOPE_CLASS: {
  //   BinaryenExpressionRef get =
  //     BinaryenStructGet(codegen.module, expression->variable->index,
  //                       BinaryenLocalGet(codegen.module, 0, codegen.class), type, false);
  //   generate_debug_info(expression->name, get, codegen.function);

  //   return get;
  // }
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static void generate_assignment_expression(MIR_reg_t dest, AssignExpr* expression)
{
  MIR_reg_t value = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->data_type),
                                      codegen.function->u.func);
  generate_expression(value, expression->value);

  VarStmt* variable = expression->variable;
  if (variable)
  {
    switch (variable->scope)
    {
    case SCOPE_LOCAL: {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, variable->reg),
                                   MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      return;
    }

    case SCOPE_GLOBAL: {
      MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                   MIR_new_ref_op(codegen.ctx, expression->variable->item)));
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(
          codegen.ctx, data_type_to_mov_type(expression->data_type),
          MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(expression->data_type), 0, ptr, 0, 1),
          MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      return;
    }

      // case SCOPE_CLASS: {
      //   BinaryenExpressionRef ref;
      //   if (expression->target->type == EXPR_ACCESS)
      //     ref = generate_expression(expression->target->access.expr);
      //   else
      //     ref = BinaryenLocalGet(codegen.module, 0, codegen.class);

      //   BinaryenExpressionRef list[] = {
      //     BinaryenStructSet(codegen.module, variable->index, ref, value),
      //     BinaryenStructGet(codegen.module, variable->index,
      //                       BinaryenExpressionCopy(ref, codegen.module), codegen.class, false),
      //   };

      //   generate_debug_info(expression->op, list[0], codegen.function);

      //   return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list),
      //   type);
      // }

    default:
      UNREACHABLE("Unhandled scope type");
    }
  }
  else
  {
    if (expression->target->type != EXPR_INDEX)
    {
      UNREACHABLE("Unhandled expression type");
    }

    MIR_reg_t ptr = _MIR_new_temp_reg(
      codegen.ctx, data_type_to_mir_type(expression->target->index.expr_data_type),
      codegen.function->u.func);
    generate_expression(ptr, expression->target->index.expr);

    MIR_reg_t index = _MIR_new_temp_reg(
      codegen.ctx, data_type_to_mir_type(expression->target->index.index_data_type),
      codegen.function->u.func);
    generate_expression(index, expression->target->index.index);

    // if (expression->target->index.expr_data_type.type == TYPE_OBJECT)
    // {
    //   BinaryenExpressionRef operands[] = { ref, index, value };
    //   BinaryenExpressionRef call = BinaryenCall(
    //     codegen.module, expression->function, operands, sizeof(operands) /
    //     sizeof_ptr(operands), data_type_to_binaryen_type(expression->target->index.data_type));

    //   generate_debug_info(expression->target->index.index_token, call, codegen.function);
    //   return call;
    // }
    // else
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_UBGES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, index), generate_array_length_op(ptr)));

      MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, array_ptr),
                                   generate_array_data_op(ptr)));

      DataType element_data_type =
        array_data_type_element(expression->target->index.expr_data_type);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type)),
                     MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(element_data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      generate_panic(expression->target->index.index_token, "Out of bounds access");

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);
    }
  }
}

static void generate_call_expression(MIR_reg_t dest, CallExpr* expression)
{
  MIR_item_t proto;
  MIR_item_t func;

  if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    Function* internal = generate_function_internal(expression->callee_data_type);
    proto = internal->proto;
    func = internal->func;
  }
  else
  {
    if (!expression->function->proto)
    {
      ArrayMIR_var_t vars;
      array_init(&vars);

      VarStmt* parameter;
      array_foreach(&expression->function->parameters, parameter)
      {
        MIR_var_t var;
        var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
        var.type = data_type_to_mir_type(parameter->data_type);

        array_add(&vars, var);
      }

      expression->function->proto = MIR_new_proto_arr(
        codegen.ctx, memory_sprintf("%s.proto", expression->function->name.lexeme),
        expression->function->data_type.type != TYPE_VOID,
        (MIR_type_t[]){ data_type_to_mir_type(expression->function->data_type) }, vars.size,
        vars.elems);

      if (!expression->function->item)
      {
        MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
        MIR_set_curr_func(codegen.ctx, NULL);

        expression->function->item =
          MIR_new_func_arr(codegen.ctx, expression->function->name.lexeme,
                           expression->function->data_type.type != TYPE_VOID,
                           (MIR_type_t[]){ data_type_to_mir_type(expression->function->data_type) },
                           vars.size, vars.elems);

        MIR_set_curr_func(codegen.ctx, previous_func);
      }
    }

    proto = expression->function->proto;
    func = expression->function->item;
  }

  // MIR_item_t proto = data_type_to_proto(expression->callee_data_type);

  // MIR_reg_t callee = _MIR_new_temp_reg(
  //   codegen.ctx, data_type_to_mir_type(expression->callee_data_type), codegen.function->u.func);

  // generate_expression(callee, expression->callee);

  ArrayMIR_op_t arguments;
  array_init(&arguments);

  array_add(&arguments, MIR_new_ref_op(codegen.ctx, proto));
  array_add(&arguments, MIR_new_ref_op(codegen.ctx, func));

  if (expression->return_data_type.type != TYPE_VOID)
    array_add(&arguments, MIR_new_reg_op(codegen.ctx, dest));

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    MIR_reg_t temp =
      _MIR_new_temp_reg(codegen.ctx, proto->u.proto->args->varr[_i].type, codegen.function->u.func);
    generate_expression(temp, argument);

    array_add(&arguments, MIR_new_reg_op(codegen.ctx, temp));
  }

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn_arr(codegen.ctx, MIR_CALL, arguments.size, arguments.elems));

  // BinaryenExpressionRef call;

  // if (expression->callee_data_type.type == TYPE_ALIAS)
  // {
  //   return generate_default_initialization(*expression->callee_data_type.alias.data_type);
  // }
  // else if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  // {
  //   call = BinaryenCallRef(codegen.module, generate_expression(expression->callee),
  //   arguments.elems,
  //                          arguments.size, return_type, false);
  // }
  // else
  // {
  //   const char* name;
  //   if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
  //     name = generate_function_internal(expression->callee_data_type);
  //   else
  //     name = expression->function;

  //   call = BinaryenCall(codegen.module, name, arguments.elems, arguments.size, return_type);
  // }

  // generate_debug_info(expression->callee_token, call, codegen.function);
  // return call;
}

static void generate_access_expression(MIR_reg_t dest, AccessExpr* expression)
{
  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    generate_function_pointer(dest, expression->data_type);
    return;
  }

  MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->expr_data_type),
                                    codegen.function->u.func);
  generate_expression(ptr, expression->expr);

  if (expression->expr_data_type.type == TYPE_STRING)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   generate_string_length_op(ptr)));

      return;
    }

    UNREACHABLE("Unhandled string access name");
  }
  else if (expression->expr_data_type.type == TYPE_ARRAY)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   generate_array_length_op(ptr)));
      return;
    }
    else if (strcmp(expression->name.lexeme, "capacity") == 0)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   generate_array_capacity_op(ptr)));
      return;
    }

    UNREACHABLE("Unhandled array access name");
  }
  // else
  // {
  //   BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  //   BinaryenExpressionRef access =
  //     BinaryenStructGet(codegen.module, expression->variable->index, ref, type, false);
  //   generate_debug_info(expression->name, access, codegen.function);

  //   return access;
  // }
}

static void generate_index_expression(MIR_reg_t dest, IndexExpr* expression)
{
  MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->expr_data_type),
                                    codegen.function->u.func);
  generate_expression(ptr, expression->expr);

  MIR_reg_t index = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->index_data_type), codegen.function->u.func);
  generate_expression(index, expression->index);

  switch (expression->expr_data_type.type)
  {
  case TYPE_STRING: {
    MIR_label_t cont_label = MIR_new_label(codegen.ctx);
    MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(codegen.ctx, MIR_UBGES, MIR_new_label_op(codegen.ctx, if_false_label),
                   MIR_new_reg_op(codegen.ctx, index), generate_string_length_op(ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 generate_string_at_op(ptr, index)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

    MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

    generate_panic(expression->index_token, "Out of bounds access");

    MIR_append_insn(codegen.ctx, codegen.function, cont_label);
    return;
  }
  case TYPE_ARRAY: {
    MIR_label_t cont_label = MIR_new_label(codegen.ctx);
    MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(codegen.ctx, MIR_UBGES, MIR_new_label_op(codegen.ctx, if_false_label),
                   MIR_new_reg_op(codegen.ctx, index), generate_array_length_op(ptr)));

    MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, array_ptr),
                                 generate_array_data_op(ptr)));

    DataType element_data_type = array_data_type_element(expression->expr_data_type);

    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(codegen.ctx, data_type_to_mov_type(element_data_type),
                   MIR_new_reg_op(codegen.ctx, dest),
                   MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type), 0,
                                  array_ptr, index, size_data_type(element_data_type))));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

    MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

    generate_panic(expression->index_token, "Out of bounds access");

    MIR_append_insn(codegen.ctx, codegen.function, cont_label);
    return;
  }
  // case TYPE_OBJECT: {
  //   BinaryenExpressionRef operands[] = {
  //     ref,
  //     index,
  //   };

  //   BinaryenExpressionRef call = BinaryenCall(codegen.module, expression->function, operands,
  //                                             sizeof(operands) / sizeof_ptr(operands), type);

  //   generate_debug_info(expression->index_token, call, codegen.function);
  //   return call;
  // }
  default:
    UNREACHABLE("Unhandled index type");
  }
}

static void generate_array_expression(MIR_reg_t dest, LiteralArrayExpr* expression)
{
  if (expression->values.size)
  {
    MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    DataType element_data_type = array_data_type_element(expression->data_type);

    generate_malloc_expression(dest, MIR_new_int_op(codegen.ctx, sizeof(Array)));
    generate_malloc_expression(
      array_ptr,
      MIR_new_int_op(codegen.ctx, size_data_type(element_data_type) * expression->values.size));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_length_op(dest),
                                 MIR_new_int_op(codegen.ctx, expression->values.size)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_capacity_op(dest),
                                 MIR_new_int_op(codegen.ctx, expression->values.size)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_data_op(dest),
                                 MIR_new_reg_op(codegen.ctx, array_ptr)));

    MIR_reg_t item = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(element_data_type),
                                       codegen.function->u.func);
    Expr* value;
    array_foreach(&expression->values, value)
    {
      generate_expression(item, value);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type),
                                    _i * size_data_type(element_data_type), array_ptr, 0, 1),
                     MIR_new_reg_op(codegen.ctx, item)));
    }
  }
  else
  {
    generate_default_array_initialization(dest);
  }
}

static void generate_is_expression(IsExpr* expression)
{
}

static void generate_if_expression(IfExpr* expression)
{
}

static void generate_expression(MIR_reg_t dest, Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    generate_literal_expression(dest, &expression->literal);
    return;
  case EXPR_BINARY:
    generate_binary_expression(dest, &expression->binary);
    return;
  case EXPR_GROUP:
    generate_group_expression(dest, &expression->group);
    return;
  case EXPR_UNARY:
    generate_unary_expression(dest, &expression->unary);
    return;
  case EXPR_VAR:
    generate_variable_expression(dest, &expression->var);
    return;
  case EXPR_ASSIGN:
    generate_assignment_expression(dest, &expression->assign);
    return;
  case EXPR_CALL:
    generate_call_expression(dest, &expression->call);
    return;
  case EXPR_CAST:
    generate_cast_expression(dest, &expression->cast);
    return;
  case EXPR_ACCESS:
    generate_access_expression(dest, &expression->access);
    return;
  case EXPR_INDEX:
    generate_index_expression(dest, &expression->index);
    return;
  case EXPR_ARRAY:
    generate_array_expression(dest, &expression->array);
    return;
  }

  UNREACHABLE("Unhandled expression");
}

static void generate_expression_statement(ExprStmt* statement)
{
  MIR_reg_t temp = 0;

  MIR_type_t type = data_type_to_mir_type(statement->data_type);
  if (type != MIR_T_UNDEF)
    temp = _MIR_new_temp_reg(codegen.ctx, type, codegen.function->u.func);

  generate_expression(temp, statement->expr);
}

static void generate_if_statement(IfStmt* statement)
{
  MIR_reg_t condition = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)),
                                          codegen.function->u.func);
  generate_expression(condition, statement->condition);

  MIR_label_t cont_label = MIR_new_label(codegen.ctx);
  MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_BFS, MIR_new_label_op(codegen.ctx, if_false_label),
                               MIR_new_reg_op(codegen.ctx, condition)));

  generate_statements(&statement->then_branch);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

  MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

  if (statement->else_branch.elems)
    generate_statements(&statement->else_branch);

  MIR_append_insn(codegen.ctx, codegen.function, cont_label);
}

static void generate_while_statement(WhileStmt* statement)
{
  MIR_label_t previous_continue_label = codegen.continue_label;
  MIR_label_t previous_break_label = codegen.break_label;

  codegen.continue_label = MIR_new_label(codegen.ctx);
  codegen.break_label = MIR_new_label(codegen.ctx);

  MIR_label_t loop_label = MIR_new_label(codegen.ctx);

  generate_statements(&statement->initializer);

  MIR_append_insn(codegen.ctx, codegen.function, loop_label);

  MIR_reg_t condition = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)),
                                          codegen.function->u.func);
  generate_expression(condition, statement->condition);
  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_BFS,
                               MIR_new_label_op(codegen.ctx, codegen.break_label),
                               MIR_new_reg_op(codegen.ctx, condition)));

  generate_statements(&statement->body);

  MIR_append_insn(codegen.ctx, codegen.function, codegen.continue_label);

  generate_statements(&statement->incrementer);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, loop_label)));

  MIR_append_insn(codegen.ctx, codegen.function, codegen.break_label);

  codegen.continue_label = previous_continue_label;
  codegen.break_label = previous_break_label;
}

static void generate_return_statement(ReturnStmt* statement)
{
  if (statement->expr)
  {
    MIR_reg_t temp = _MIR_new_temp_reg(codegen.ctx, *codegen.function->u.func->res_types,
                                       codegen.function->u.func);
    generate_expression(temp, statement->expr);

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, temp)));
  }
  else
  {
    MIR_append_insn(codegen.ctx, codegen.function, MIR_new_ret_insn(codegen.ctx, 0));
  }
}

static void generate_continue_statement(void)
{
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, codegen.continue_label)));
}

static void generate_break_statement(void)
{
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, codegen.break_label)));
}

static void generate_variable_declaration(VarStmt* statement)
{
  if (statement->scope == SCOPE_GLOBAL)
  {
    const char* name = statement->name.lexeme;
    uint64_t init = 0;

    MIR_item_t data =
      MIR_new_data(codegen.ctx, name, data_type_to_mir_type(statement->data_type), 1, &init);
    statement->item = data;

    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                 MIR_new_ref_op(codegen.ctx, data)));
    MIR_reg_t initializer = _MIR_new_temp_reg(
      codegen.ctx, data_type_to_mir_type(statement->data_type), codegen.function->u.func);

    if (statement->initializer)
    {
      generate_expression(initializer, statement->initializer);
    }
    else
    {
      generate_default_initialization(initializer, statement->data_type);
    }

    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(
        codegen.ctx, data_type_to_mov_type(statement->data_type),
        MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(statement->data_type), 0, ptr, 0, 1),
        MIR_new_reg_op(codegen.ctx, initializer)));
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    if (statement->initializer)
    {
      generate_expression(statement->reg, statement->initializer);
    }
    else
    {
      generate_default_initialization(statement->reg, statement->data_type);
    }
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void generate_function_declaration(FuncStmt* statement)
{
  if (statement->import)
  {
    statement->item = MIR_new_import(codegen.ctx, statement->name.lexeme);
    return;
  }

  ArrayMIR_var_t vars;
  array_init(&vars);

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    MIR_var_t var;
    var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
    var.type = data_type_to_mir_type(parameter->data_type);

    array_add(&vars, var);
  }

  MIR_item_t previous_function = codegen.function;
  MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
  MIR_set_curr_func(codegen.ctx, NULL);

  if (statement->item)
  {
    codegen.function = statement->item;
    MIR_set_curr_func(codegen.ctx, statement->item->u.func);
  }
  else
  {
    codegen.function = MIR_new_func_arr(
      codegen.ctx, statement->name.lexeme, statement->data_type.type != TYPE_VOID,
      (MIR_type_t[]){ data_type_to_mir_type(statement->data_type) }, vars.size, vars.elems);
  }

  statement->item = codegen.function;

  array_foreach(&statement->parameters, parameter)
  {
    parameter->reg = MIR_reg(codegen.ctx, vars.elems[_i].name, codegen.function->u.func);
  }

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    variable->reg = MIR_new_func_reg(
      codegen.ctx, codegen.function->u.func, data_type_to_mir_type(variable->data_type),
      memory_sprintf("%s.%d", variable->name.lexeme, variable->index));
  }

  generate_statements(&statement->body);

  MIR_new_export(codegen.ctx, statement->name.lexeme);
  MIR_finish_func(codegen.ctx);

  MIR_set_curr_func(codegen.ctx, previous_func);
  codegen.function = previous_function;

  // const char* name = statement->name.lexeme;

  // BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  // BinaryenType results = data_type_to_binaryen_type(statement->data_type);

  // if (statement->import)
  // {
  //   BinaryenAddFunctionImport(codegen.module, name, statement->import, name, params, results);

  //   if (parameters_contain_string)
  //     generate_string_export_functions();

  //   ArrayTypeBuilderSubtype subtypes;
  //   array_init(&subtypes);

  //   generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);
  // }
  // else
  // {
  //   BinaryenExpressionRef body = generate_statements(&statement->body);
  //   BinaryenFunctionRef func = BinaryenAddFunction(codegen.module, name, params, results,
  //                                                  variable_types.elems, variable_types.size,
  //                                                  body);
  //   BinaryenAddFunctionExport(codegen.module, name, name);

  //   ArrayTypeBuilderSubtype subtypes;
  //   array_init(&subtypes);

  //   BinaryenHeapType heap_type =
  //     generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);

  //   BinaryenFunctionSetType(func, heap_type);
  // }
}

static void generate_function_template_declaration(FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    generate_function_declaration(function_declaration);
  }
}

static void generate_class_body_declaration(ClassStmt* statement)
{
}

static void generate_class_template_declaration(ClassTemplateStmt* statement)
{
}

static void generate_import_declaration(ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    generate_statement(body_statement);
  }
}

static void generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    generate_expression_statement(&statement->expr);
    return;
  case STMT_IF:
    generate_if_statement(&statement->cond);
    return;
  case STMT_WHILE:
    generate_while_statement(&statement->loop);
    return;
  case STMT_RETURN:
    generate_return_statement(&statement->ret);
    return;
  case STMT_CONTINUE:
    generate_continue_statement();
    return;
  case STMT_BREAK:
    generate_break_statement();
    return;
  case STMT_VARIABLE_DECL:
    generate_variable_declaration(&statement->var);
    return;
  case STMT_FUNCTION_DECL:
    generate_function_declaration(&statement->func);
    return;
  case STMT_IMPORT_DECL:
    generate_import_declaration(&statement->import);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    generate_function_template_declaration(&statement->func_template);
    return;
  case STMT_CLASS_DECL: {
  }
  }

  UNREACHABLE("Unhandled statement");
}

static void generate_statements(ArrayStmt* statements)
{
  Stmt* statement;
  array_foreach(statements, statement)
  {
    generate_statement(statement);
  }
}

void codegen_init(ArrayStmt statements)
{
  codegen.statements = statements;
  codegen.ctx = MIR_init();
  codegen.module = MIR_new_module(codegen.ctx, "main");
  codegen.function = MIR_new_func(codegen.ctx, "<start>", 0, 0, 0);
  codegen.continue_label = NULL;
  codegen.break_label = NULL;

  MIR_load_external(codegen.ctx, "panic", (void*)panic);
  codegen.panic.proto = MIR_new_proto_arr(codegen.ctx, "panic.proto", 0, NULL, 1,
                                          (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.panic.func = MIR_new_import(codegen.ctx, "panic");

  MIR_load_external(codegen.ctx, "malloc", (void*)malloc);
  codegen.malloc.proto =
    MIR_new_proto_arr(codegen.ctx, "malloc.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.malloc.func = MIR_new_import(codegen.ctx, "malloc");

  MIR_load_external(codegen.ctx, "realloc", (void*)realloc);
  codegen.realloc.proto = MIR_new_proto_arr(
    codegen.ctx, "realloc.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 2,
    (MIR_var_t[]){ { .name = "ptr", .type = MIR_T_I64 }, { .name = "size", .type = MIR_T_I64 } });
  codegen.realloc.func = MIR_new_import(codegen.ctx, "realloc");

  MIR_load_external(codegen.ctx, "string.concat", (void*)string_concat);
  codegen.string_concat.proto = MIR_new_proto_arr(
    codegen.ctx, "string.concat.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 2,
    (MIR_var_t[]){ { .name = "left", .type = MIR_T_I64 }, { .name = "right", .type = MIR_T_I64 } });
  codegen.string_concat.func = MIR_new_import(codegen.ctx, "string.concat");

  MIR_load_external(codegen.ctx, "string.bool_cast", (void*)string_bool_cast);
  codegen.string_bool_cast.proto =
    MIR_new_proto_arr(codegen.ctx, "string.bool_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.string_bool_cast.func = MIR_new_import(codegen.ctx, "string.bool_cast");

  MIR_load_external(codegen.ctx, "string.int_cast", (void*)string_int_cast);
  codegen.string_int_cast.proto =
    MIR_new_proto_arr(codegen.ctx, "string.int_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.string_int_cast.func = MIR_new_import(codegen.ctx, "string.int_cast");

  MIR_load_external(codegen.ctx, "string.float_cast", (void*)string_float_cast);
  codegen.string_float_cast.proto =
    MIR_new_proto_arr(codegen.ctx, "string.float_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_F } });
  codegen.string_float_cast.func = MIR_new_import(codegen.ctx, "string.float_cast");

  MIR_load_external(codegen.ctx, "string.char_cast", (void*)string_char_cast);
  codegen.string_char_cast.proto =
    MIR_new_proto_arr(codegen.ctx, "string.char_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.string_char_cast.func = MIR_new_import(codegen.ctx, "string.char_cast");

  MIR_load_external(codegen.ctx, "log", (void*)log_int);
  MIR_load_external(codegen.ctx, "log(int)", (void*)log_int);
  MIR_load_external(codegen.ctx, "log(bool)", (void*)log_int);
  MIR_load_external(codegen.ctx, "log(float)", (void*)log_float);
  MIR_load_external(codegen.ctx, "log(char)", (void*)log_char);
  MIR_load_external(codegen.ctx, "log(string)", (void*)log_string);

  map_init_function(&codegen.functions, 0, 0);
  map_init_mir_item(&codegen.string_constants, 0, 0);

  VarStmt* global_local;
  ArrayVarStmt global_local_statements = global_locals();
  array_foreach(&global_local_statements, global_local)
  {
    global_local->reg = MIR_new_func_reg(
      codegen.ctx, codegen.function->u.func, data_type_to_mir_type(global_local->data_type),
      memory_sprintf("%s.%d", global_local->name.lexeme, global_local->index));
  }
}

Codegen codegen_generate(bool logging)
{
  Codegen result = { 0 };
  generate_statements(&codegen.statements);

  MIR_finish_func(codegen.ctx);
  MIR_finish_module(codegen.ctx);

  if (logging)
    MIR_output(codegen.ctx, stderr);

  MIR_load_module(codegen.ctx, codegen.module);
  MIR_gen_init(codegen.ctx);
  MIR_gen_set_optimize_level(codegen.ctx, 4);

  MIR_link(codegen.ctx, MIR_set_gen_interface, NULL);

  Start start = (Start)MIR_gen(codegen.ctx, codegen.function);

  if (setjmp(codegen.jmp) == 0)
    start();
  else
    printf("Recovered from runtime error!\n");

  MIR_gen_finish(codegen.ctx);
  MIR_finish(codegen.ctx);

  return result;
}

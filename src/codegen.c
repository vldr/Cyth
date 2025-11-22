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

#include <math.h>
#include <mir-gen.h>
#include <setjmp.h>

#ifndef _WIN32
#include <execinfo.h>
#endif

typedef void (*Start)(void);
typedef struct _STRING
{
  int size;
  char data[];
} String;

typedef struct _ARRAY
{
  int size;
  int capacity;
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
array_def(MIR_item_t, MIR_item_t);

static struct
{
  jmp_buf jmp;
  MIR_context_t ctx;
  MIR_module_t module;
  MIR_item_t function;
  MIR_label_t continue_label;
  MIR_label_t break_label;
  ArrayStmt statements;
  MapS64 typeids;
  MapMIR_item string_constants;
  MapMIR_item items;
  ArrayMIR_item_t function_items;
  MapFunction functions;
  int strings;

  Function panic;
  Function malloc;
  Function memcpy;
  Function realloc;
  Function string_concat;
  Function string_equals;
  Function string_bool_cast;
  Function string_int_cast;
  Function string_float_cast;
  Function string_char_cast;
} codegen;

static void generate_default_initialization(MIR_reg_t dest, DataType data_type);
static void generate_expression(MIR_reg_t dest, Expr* expression);
static void generate_statement(Stmt* statement);
static void generate_statements(ArrayStmt* statements);
static void init_statement(Stmt* statement);
static void init_statements(ArrayStmt* statements);
static void init_function_declaration(FuncStmt* statement);

static void log_int(int n)
{
  printf("%d\n", n);
}

static void log_float(float n)
{
  printf("%g\n", n);
}

static void log_char(char n)
{
  printf("%c\n", n);
}

static void log_string(String* n)
{
  fwrite(n->data, 1, n->size, stdout);
  putchar('\n');
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

static bool string_equals(String* left, String* right)
{
  return left == right ||
         (left->size == right->size && memcmp(left->data, right->data, left->size) == 0);
}

static String* string_int_cast(int n)
{
  int length = snprintf(NULL, 0, "%d", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length - 1;

  snprintf(result->data, length, "%d", n);

  return result;
}

static String* string_float_cast(float n)
{
  int length = snprintf(NULL, 0, "%g", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length - 1;

  snprintf(result->data, length, "%g", n);

  return result;
}

static String* string_char_cast(char n)
{
  int length = snprintf(NULL, 0, "%c", n) + 1;
  uintptr_t size = sizeof(String) + length;

  String* result = malloc(size);
  result->size = length - 1;

  snprintf(result->data, length, "%c", n);

  return result;
}

static String* string_bool_cast(bool n)
{
  static String true_string = { .size = sizeof("true") - 1, .data = "true" };
  static String false_string = { .size = sizeof("false") - 1, .data = "false" };

  return n ? &true_string : &false_string;
}

static void panic(String* n, int line, int column)
{
  void* array[10];
  int size = 0;

#ifdef _WIN32
  size = RtlCaptureStackBackTrace(0, sizeof(array) / sizeof_ptr(array), array, NULL);
#else
  size = backtrace(array, sizeof(array) / sizeof_ptr(array));
#endif

  if (line && column)
    printf("%.*s, on line %d:%d\n", n->size, n->data, line, column);
  else
    printf("%.*s\n", n->size, n->data);

  for (int i = 0; i < size; i++)
  {
    MIR_item_t item;

    array_foreach(&codegen.function_items, item)
    {
      uint64_t distance = (uint64_t)array[i] - (uint64_t)item->u.func->machine_code;
      if (distance <= item->u.func->length)
      {
        printf("  at %s\n", item->u.func->name);
      }
    }
  }

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
  const char* name = data_type_to_string(data_type);
  MIR_item_t item = map_get_mir_item(&codegen.items, name);

  if (!item)
  {
    assert(data_type.type == TYPE_FUNCTION || data_type.type == TYPE_FUNCTION_INTERNAL ||
           data_type.type == TYPE_FUNCTION_MEMBER || data_type.type == TYPE_FUNCTION_POINTER);

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

    item = MIR_new_proto_arr(
      codegen.ctx, memory_sprintf("%s.proto", data_type_to_string(data_type)),
      return_data_type.type != TYPE_VOID, (MIR_type_t[]){ data_type_to_mir_type(return_data_type) },
      vars.size, vars.elems);

    map_put_mir_item(&codegen.items, name, item);
  }

  return item;
}

static uint64_t data_type_to_typeid(DataType data_type)
{
  const char* name = data_type_to_string(data_type);
  int id = map_get_s64(&codegen.typeids, name);

  if (!id)
  {
    id = map_size_s64(&codegen.typeids) + 1;
    map_put_s64(&codegen.typeids, name, id);
  }

  return id;
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

static void generate_string_literal_expression(MIR_op_t dest, const char* literal, int length)
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
                  MIR_new_insn(codegen.ctx, data_type_to_mov_type(DATA_TYPE(TYPE_STRING)), dest,
                               MIR_new_ref_op(codegen.ctx, item)));
}

static void generate_panic(Token token, const char* what)
{
  MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
  generate_string_literal_expression(MIR_new_reg_op(codegen.ctx, ptr), what, strlen(what));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_call_insn(
                    codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.panic.proto),
                    MIR_new_ref_op(codegen.ctx, codegen.panic.func),
                    MIR_new_reg_op(codegen.ctx, ptr), MIR_new_int_op(codegen.ctx, token.start_line),
                    MIR_new_int_op(codegen.ctx, token.start_column)));
}

static MIR_op_t generate_array_length_op(MIR_reg_t ptr)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U32, 0, ptr, 0, 1);
}

static MIR_op_t generate_array_capacity_op(MIR_reg_t ptr)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U32, sizeof(unsigned int), ptr, 0, 1);
}

static MIR_op_t generate_array_data_op(MIR_reg_t ptr)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_I64, sizeof(unsigned int) + sizeof(unsigned int), ptr, 0,
                        1);
}

static MIR_op_t generate_string_length_op(MIR_reg_t base)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_I32, 0, base, 0, 1);
}

static MIR_op_t generate_string_at_op(MIR_reg_t base, MIR_reg_t index)
{
  return MIR_new_mem_op(codegen.ctx, MIR_T_U8, sizeof(unsigned int), base, index, 1);
}

static MIR_op_t generate_object_field_op(VarStmt* field, MIR_reg_t ptr)
{
  return MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(field->data_type), field->index,
                        ptr, 0, 1);
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
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) },
                           { .name = "value", .type = data_type_to_mir_type(element_data_type) } };

    MIR_item_t previous_function = codegen.function;
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

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static Function* generate_array_push_string_function(DataType data_type)
{
  const char* name = "array.pushString";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    DataType element_data_type = DATA_TYPE(TYPE_CHAR);

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) },
                           { .name = "string_ptr",
                             .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) } };

    MIR_item_t previous_function = codegen.function;
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
    MIR_reg_t string_ptr = MIR_reg(codegen.ctx, "string_ptr", codegen.function->u.func);

    {
      MIR_reg_t new_size = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADDS, MIR_new_reg_op(codegen.ctx, new_size),
                                   generate_array_length_op(ptr),
                                   generate_string_length_op(string_ptr)));

      MIR_label_t push_label = MIR_new_label(codegen.ctx);
      MIR_label_t resize_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_UBGES, MIR_new_label_op(codegen.ctx, resize_label),
                     MIR_new_reg_op(codegen.ctx, new_size), generate_array_capacity_op(ptr)));
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, push_label)));

      MIR_append_insn(codegen.ctx, codegen.function, resize_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_capacity_op(ptr),
                                   MIR_new_reg_op(codegen.ctx, new_size)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_UMULOS, generate_array_capacity_op(ptr),
                     generate_array_capacity_op(ptr),
                     MIR_new_int_op(codegen.ctx, 2 * size_data_type(element_data_type))));

      generate_realloc_expression(generate_array_data_op(ptr), generate_array_data_op(ptr),
                                  generate_array_capacity_op(ptr));

      MIR_append_insn(codegen.ctx, codegen.function, push_label);
    }

    {
      MIR_reg_t dest_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   generate_array_length_op(ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_UMULO, MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   MIR_new_int_op(codegen.ctx, size_data_type(element_data_type))));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   generate_array_data_op(ptr)));

      MIR_reg_t source_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, source_ptr),
                                   MIR_new_reg_op(codegen.ctx, string_ptr),
                                   MIR_new_int_op(codegen.ctx, sizeof(unsigned int))));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(
          codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.memcpy.proto),
          MIR_new_ref_op(codegen.ctx, codegen.memcpy.func), MIR_new_reg_op(codegen.ctx, dest_ptr),
          MIR_new_reg_op(codegen.ctx, source_ptr), generate_string_length_op(string_ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADDS, generate_array_length_op(ptr),
                                   generate_array_length_op(ptr),
                                   generate_string_length_op(string_ptr)));
    }

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static Function* generate_array_pop_function(DataType data_type)
{
  const char* name = memory_sprintf("array.pop.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);

    MIR_type_t return_type = data_type_to_mir_type(element_data_type);
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = codegen.function;
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

    {
      MIR_label_t finish_label = MIR_new_label(codegen.ctx);
      MIR_label_t panic_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BEQS,
                                   MIR_new_label_op(codegen.ctx, panic_label),
                                   generate_array_length_op(ptr), MIR_new_int_op(codegen.ctx, 0)));

      MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, array_ptr),
                                   generate_array_data_op(ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_SUBS, generate_array_length_op(ptr),
                                   generate_array_length_op(ptr), MIR_new_int_op(codegen.ctx, 1)));

      MIR_reg_t index = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, index),
                                   generate_array_length_op(ptr)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_ret_insn(codegen.ctx, 1,
                         MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type),
                                        0, array_ptr, index, size_data_type(element_data_type))));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, finish_label)));

      MIR_append_insn(codegen.ctx, codegen.function, panic_label);

      generate_panic((Token){ 0 }, "Out of bounds access");

      MIR_append_insn(codegen.ctx, codegen.function, finish_label);
    }

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static Function* generate_array_to_string_function(DataType data_type)
{
  const char* name = "array.toString";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    DataType return_data_type = DATA_TYPE(TYPE_STRING);

    MIR_type_t return_type = data_type_to_mir_type(return_data_type);
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = codegen.function;
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

    {
      MIR_reg_t string_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_reg_t size = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, size),
                                   MIR_new_int_op(codegen.ctx, sizeof(unsigned int)),
                                   generate_array_length_op(ptr)));

      generate_malloc_expression(string_ptr, MIR_new_reg_op(codegen.ctx, size));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_string_length_op(string_ptr),
                                   generate_array_length_op(ptr)));

      MIR_reg_t dest_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, dest_ptr),
                                   MIR_new_reg_op(codegen.ctx, string_ptr),
                                   MIR_new_int_op(codegen.ctx, sizeof(unsigned int))));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.memcpy.proto),
                          MIR_new_ref_op(codegen.ctx, codegen.memcpy.func),
                          MIR_new_reg_op(codegen.ctx, dest_ptr), generate_array_data_op(ptr),
                          generate_array_length_op(ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, string_ptr)));
    }

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static Function* generate_array_clear_function(DataType data_type)
{
  const char* name = "array.clear";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = { { .name = "ptr", .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = codegen.function;
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

    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_length_op(ptr),
                                   MIR_new_int_op(codegen.ctx, 0)));
    }

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static Function* generate_array_reserve_function(DataType data_type)
{
  const char* name = memory_sprintf("array.reserve.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    ArrayMIR_var_t params;
    array_init(&params);

    MIR_var_t this = { .name = "ptr", .type = data_type_to_mir_type(data_type) };
    array_add(&params, this);

    for (int i = 0; i < *data_type.array.count; i++)
    {
      MIR_var_t n = { .name = memory_sprintf("n.%d", i), .type = data_type_to_mir_type(data_type) };
      array_add(&params, n);
    }

    MIR_item_t previous_function = codegen.function;
    MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, params.size, params.elems);
    function->func = MIR_new_func_arr(codegen.ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      params.size, params.elems);

    codegen.function = function->func;

    MIR_reg_t ptr = MIR_reg(codegen.ctx, "ptr", codegen.function->u.func);
    MIR_reg_t n = MIR_reg(codegen.ctx, "n.0", codegen.function->u.func);
    MIR_reg_t array_ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

    {
      MIR_label_t continue_label = MIR_new_label(codegen.ctx);
      MIR_label_t panic_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BLTS,
                                   MIR_new_label_op(codegen.ctx, panic_label),
                                   MIR_new_reg_op(codegen.ctx, n), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, continue_label)));

      MIR_append_insn(codegen.ctx, codegen.function, panic_label);

      generate_panic((Token){ 0 }, "Invalid reservation amount");

      MIR_append_insn(codegen.ctx, codegen.function, continue_label);
    }

    {
      MIR_reg_t size = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MUL, MIR_new_reg_op(codegen.ctx, size),
                                   MIR_new_reg_op(codegen.ctx, n),
                                   MIR_new_int_op(codegen.ctx, size_data_type(element_data_type))));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, array_ptr),
                                   generate_array_data_op(ptr)));

      generate_malloc_expression(array_ptr, MIR_new_reg_op(codegen.ctx, size));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_data_op(ptr),
                                   MIR_new_reg_op(codegen.ctx, array_ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_length_op(ptr),
                                   MIR_new_reg_op(codegen.ctx, n)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, generate_array_capacity_op(ptr),
                                   MIR_new_reg_op(codegen.ctx, n)));
    }

    {
      MIR_reg_t i = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, i),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_label_t break_label = MIR_new_label(codegen.ctx);
      MIR_label_t continue_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function, continue_label);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BGE, MIR_new_label_op(codegen.ctx, break_label),
                                   MIR_new_reg_op(codegen.ctx, i), MIR_new_reg_op(codegen.ctx, n)));

      MIR_reg_t dest = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(element_data_type),
                                         codegen.function->u.func);
      generate_default_initialization(dest, element_data_type);

      if (element_data_type.type == TYPE_ARRAY)
      {
        Function* function = generate_array_reserve_function(element_data_type);

        ArrayMIR_op_t arguments;
        array_init(&arguments);
        array_add(&arguments, MIR_new_ref_op(codegen.ctx, function->proto));
        array_add(&arguments, MIR_new_ref_op(codegen.ctx, function->func));
        array_add(&arguments, MIR_new_reg_op(codegen.ctx, dest));

        for (int i = 1; i < *data_type.array.count; i++)
        {
          MIR_reg_t n = MIR_reg(codegen.ctx, memory_sprintf("n.%d", i), codegen.function->u.func);
          array_add(&arguments, MIR_new_reg_op(codegen.ctx, n));
        }

        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_insn_arr(codegen.ctx, MIR_INLINE, arguments.size, arguments.elems));
      }

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_MOV,
                     MIR_new_mem_op(codegen.ctx, data_type_to_mir_array_type(element_data_type), 0,
                                    array_ptr, i, size_data_type(element_data_type)),
                     MIR_new_reg_op(codegen.ctx, dest)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, i),
                                   MIR_new_reg_op(codegen.ctx, i), MIR_new_int_op(codegen.ctx, 1)));
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, continue_label)));
      MIR_append_insn(codegen.ctx, codegen.function, break_label);
    }

    map_put_function(&codegen.functions, name, function);

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  }

  return function;
}

static int int_hash(int n)
{
  return n;
}

static Function* generate_int_hash_function(void)
{
  const char* name = "int.hash";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)int_hash);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static int float_hash(float n)
{
  return *(int*)&n;
}

static Function* generate_float_hash_function(void)
{
  const char* name = "float.hash";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)float_hash);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static float float_sqrt(float n)
{
  return sqrtf(n);
}

static Function* generate_float_sqrt_function(void)
{
  const char* name = "float.sqrt";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT));
    MIR_var_t params[] = {
      { .name = "n", .type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)float_sqrt);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static int string_hash(String* n)
{
  uint32_t hash = 0x811c9dc5;

  for (int i = 0; i < n->size; i++)
  {
    hash ^= n->data[i];
    hash *= 0x01000193;
  }

  return hash;
}

static Function* generate_string_hash_function(void)
{
  const char* name = "string.hash";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_hash);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static int string_index_of(String* haystack, String* needle)
{
  if (needle->size == 0)
    return 0;

  for (int i = 0; i <= haystack->size - needle->size; i++)
  {
    bool match = true;

    for (int j = 0; j < needle->size; j++)
    {
      if (haystack->data[i + j] != needle->data[j])
      {
        match = false;
        break;
      }
    }

    if (match)
      return i;
  }

  return -1;
}

static Function* generate_string_index_of_function(void)
{
  const char* name = "string.index_of";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "haystack", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "needle", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_index_of);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static int string_count(String* haystack, String* needle)
{
  if (needle->size == 0)
    return haystack->size + 1;

  int count = 0;

  for (int i = 0; i <= haystack->size - needle->size; i++)
  {
    bool match = true;

    for (int j = 0; j < needle->size; j++)
    {
      if (haystack->data[i + j] != needle->data[j])
      {
        match = false;
        break;
      }
    }

    if (match)
    {
      count++;
      i += needle->size - 1;
    }
  }

  return count;
}

static Function* generate_string_count_function(void)
{
  const char* name = "string.count";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "haystack", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "needle", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_count);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static String* string_replace(String* input, String* old, String* new)
{
  if (old == new)
    return input;

  int count = string_count(input, old);
  if (count == 0)
    return input;

  int size = input->size + count * (new->size - old->size);

  String* result = malloc(sizeof(String) + size);
  result->size = size;

  if (old->size > 0)
  {
    for (int i = 0, k = 0; i < input->size;)
    {
      bool match = true;

      for (int j = 0; j < old->size; j++)
      {
        if (i + j >= input->size || input->data[i + j] != old->data[j])
        {
          match = false;
          break;
        }
      }

      if (match)
      {
        for (int j = 0; j < new->size; j++)
          result->data[k + j] = new->data[j];

        i += old->size;
        k += new->size;
      }
      else
      {
        result->data[k] = input->data[i];

        i++;
        k++;
      }
    }
  }
  else
  {
    for (int i = 0, k = 0; i <= input->size;)
    {
      for (int j = 0; j < new->size; j++)
        result->data[k + j] = new->data[j];

      if (i < input->size)
      {
        k += new->size;
        result->data[k] = input->data[i];
      }

      k++;
      i++;
    }
  }

  return result;
}

static Function* generate_string_replace_function(void)
{
  const char* name = "string.replace";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "old", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "new", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_replace);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static String* string_trim(String* input)
{
  if (!input->size)
    return input;

  int start = 0;
  int end = input->size - 1;

  while (start < input->size && isspace(input->data[start]))
    start++;

  while (end >= start && isspace(input->data[end]))
    end--;

  int size = end - start + 1;

  String* result = malloc(sizeof(String) + size);
  result->size = size;

  for (int i = start, j = 0; i <= end; i++, j++)
    result->data[j] = input->data[i];

  return result;
}

static Function* generate_string_trim_function(void)
{
  const char* name = "string.trim";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_trim);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static bool string_starts_with(String* input, String* target)
{
  if (target->size == 0)
    return true;

  if (input->size < target->size)
    return false;

  for (int i = 0; i < target->size; i++)
  {
    if (input->data[i] != target->data[i])
      return false;
  }

  return true;
}

static Function* generate_string_starts_with_function(void)
{
  const char* name = "string.starts_with";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_starts_with);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static bool string_ends_with(String* input, String* target)
{
  if (target->size == 0)
    return true;

  if (input->size < target->size)
    return false;

  for (int i = 0; i < target->size; i++)
  {
    if (input->data[input->size - 1 - i] != target->data[target->size - 1 - i])
      return false;
  }

  return true;
}

static Function* generate_string_ends_with_function(void)
{
  const char* name = "string.ends_with";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_ends_with);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static bool string_contains(String* input, String* target)
{
  return string_index_of(input, target) != -1;
}

static Function* generate_string_contains_function(void)
{
  const char* name = "string.contains";

  Function* function = map_get_function(&codegen.functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(codegen.ctx, name);

    MIR_load_external(codegen.ctx, name, (void*)string_contains);
    map_put_function(&codegen.functions, name, function);
  }

  return function;
}

static void generate_default_initialization(MIR_reg_t dest, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_ANY:
  case TYPE_FUNCTION_POINTER:
  case TYPE_INTEGER:
  case TYPE_CHAR:
  case TYPE_BOOL:
  case TYPE_NULL:
  case TYPE_OBJECT:
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
    generate_string_literal_expression(MIR_new_reg_op(codegen.ctx, dest), "", 0);
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
  else if (strcmp(name, "array.push_string") == 0)
    return generate_array_push_string_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.pop") == 0)
    return generate_array_pop_function(array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.to_string") == 0)
    return generate_array_to_string_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.clear") == 0)
    return generate_array_clear_function(array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.reserve") == 0)
    return generate_array_reserve_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "int.hash") == 0)
    return generate_int_hash_function();
  else if (strcmp(name, "float.hash") == 0)
    return generate_float_hash_function();
  else if (strcmp(name, "float.sqrt") == 0)
    return generate_float_sqrt_function();
  else if (strcmp(name, "string.hash") == 0)
    return generate_string_hash_function();
  else if (strcmp(name, "string.index_of") == 0)
    return generate_string_index_of_function();
  else if (strcmp(name, "string.count") == 0)
    return generate_string_count_function();
  else if (strcmp(name, "string.replace") == 0)
    return generate_string_replace_function();
  else if (strcmp(name, "string.trim") == 0)
    return generate_string_trim_function();
  else if (strcmp(name, "string.starts_with") == 0)
    return generate_string_starts_with_function();
  else if (strcmp(name, "string.ends_with") == 0)
    return generate_string_ends_with_function();
  else if (strcmp(name, "string.contains") == 0)
    return generate_string_contains_function();
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
    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type), MIR_new_reg_op(codegen.ctx, dest),
                   MIR_new_ref_op(codegen.ctx, generate_function_internal(data_type)->func)));
    return;
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
    generate_string_literal_expression(MIR_new_reg_op(codegen.ctx, dest), expression->string.data,
                                       expression->string.length);
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

  if (expression->op.type != TOKEN_OR && expression->op.type != TOKEN_AND)
  {
    generate_expression(left, expression->left);
    generate_expression(right, expression->right);
  }

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
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
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
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_MULS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FMUL;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_DIVS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FDIV;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
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

    if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else if (data_type.type != TYPE_INTEGER && data_type.type != TYPE_CHAR)
      UNREACHABLE("Unsupported binary type for %, &, |, ^, <<, >>");

    break;
  }

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_EQS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FEQ;
    else if (data_type.type == TYPE_STRING)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.string_equals.proto),
                          MIR_new_ref_op(codegen.ctx, codegen.string_equals.func),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      if (expression->function)
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_call_insn(
                          codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      else
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_insn(codegen.ctx, MIR_EQ, MIR_new_reg_op(codegen.ctx, dest),
                                     MIR_new_reg_op(codegen.ctx, left),
                                     MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_NES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FNE;
    else if (data_type.type == TYPE_STRING)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, codegen.string_equals.proto),
                          MIR_new_ref_op(codegen.ctx, codegen.string_equals.func),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_EQ, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));
      return;
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      if (expression->function)
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_call_insn(
                          codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      else
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_insn(codegen.ctx, MIR_NE, MIR_new_reg_op(codegen.ctx, dest),
                                     MIR_new_reg_op(codegen.ctx, left),
                                     MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for !=");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLE;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGE;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLT;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGT;
    else if (data_type.type == TYPE_OBJECT)
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, left),
                          MIR_new_reg_op(codegen.ctx, right)));
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      generate_expression(left, expression->left);

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

      generate_expression(right, expression->right);

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

      generate_expression(left, expression->left);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, left), MIR_new_int_op(codegen.ctx, 0)));

      generate_expression(right, expression->right);

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
      generate_string_literal_expression(MIR_new_reg_op(codegen.ctx, dest), name, strlen(name));

      return;
    }
    case TYPE_ANY: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_reg_t id = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_URSH, MIR_new_reg_op(codegen.ctx, id),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 48)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNE, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, id),
                     MIR_new_int_op(codegen.ctx, data_type_to_typeid(expression->to_data_type))));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_AND, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      generate_panic(expression->type.token, "Invalid type cast");

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);
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
    case TYPE_FLOAT:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_FNE, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_float_op(codegen.ctx, 0.0f)));

      return;
    case TYPE_INTEGER:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_NES, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0)));
      return;
    case TYPE_STRING:
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_NES, MIR_new_reg_op(codegen.ctx, dest),
                                   generate_string_length_op(expr),
                                   MIR_new_int_op(codegen.ctx, 0)));
      return;
    case TYPE_ANY:
    case TYPE_NULL:
    case TYPE_OBJECT:
    case TYPE_FUNCTION_POINTER: {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_NES, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0)));

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
    case TYPE_STRING:
    case TYPE_ARRAY:
    case TYPE_OBJECT: {
      uint64_t id = data_type_to_typeid(expression->from_data_type) << 48;

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_OR, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, id)));
      return;
    }
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
  else if (expression->to_data_type.type == TYPE_ARRAY)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_ANY: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_reg_t id = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_URSH, MIR_new_reg_op(codegen.ctx, id),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 48)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNE, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, id),
                     MIR_new_int_op(codegen.ctx, data_type_to_typeid(expression->to_data_type))));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_AND, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      generate_panic(expression->type.token, "Invalid type cast");

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);
      return;
    }
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_OBJECT)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_NULL:
      generate_default_initialization(dest, expression->to_data_type);
      return;
    case TYPE_ANY: {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_reg_t id = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_URSH, MIR_new_reg_op(codegen.ctx, id),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 48)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNE, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, id),
                     MIR_new_int_op(codegen.ctx, data_type_to_typeid(expression->to_data_type))));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_AND, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      generate_panic(expression->type.token, "Invalid type cast");

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);
      return;
    }
    default:
      break;
    }
  }
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
  case SCOPE_CLASS: {
    MIR_label_t cont_label = MIR_new_label(codegen.ctx);
    MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

    MIR_reg_t ptr = MIR_reg(codegen.ctx, "this.0", codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_BF, MIR_new_label_op(codegen.ctx, if_false_label),
                                 MIR_new_reg_op(codegen.ctx, ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 generate_object_field_op(expression->variable, ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
    MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

    generate_panic(expression->name, "Null pointer access");

    MIR_append_insn(codegen.ctx, codegen.function, cont_label);

    return;
  }
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static void generate_assignment_expression(MIR_reg_t dest, AssignExpr* expression)
{
  MIR_reg_t value = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->value_data_type), codegen.function->u.func);
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

    case SCOPE_CLASS: {
      MIR_reg_t ptr;

      if (expression->target->type == EXPR_ACCESS)
      {
        ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
        generate_expression(ptr, expression->target->access.expr);
      }
      else
      {
        ptr = MIR_reg(codegen.ctx, "this.0", codegen.function->u.func);
      }

      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BF,
                                   MIR_new_label_op(codegen.ctx, if_false_label),
                                   MIR_new_reg_op(codegen.ctx, ptr)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   generate_object_field_op(expression->variable, ptr),
                                   MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      generate_panic(expression->op, "Null pointer access");

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }

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

    if (expression->target->index.expr_data_type.type == TYPE_OBJECT)
    {
      if (expression->function->data_type.type == TYPE_VOID)
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_call_insn(
                          codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, ptr), MIR_new_reg_op(codegen.ctx, index),
                          MIR_new_reg_op(codegen.ctx, value)));
      else
        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_call_insn(
                          codegen.ctx, 6, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, ptr),
                          MIR_new_reg_op(codegen.ctx, index), MIR_new_reg_op(codegen.ctx, value)));
    }
    else
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
  MIR_item_t proto = NULL;
  MIR_item_t func = NULL;

  if (expression->callee_data_type.type == TYPE_ALIAS)
  {
    generate_default_initialization(dest, *expression->callee_data_type.alias.data_type);
    return;
  }
  else if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    Function* internal = generate_function_internal(expression->callee_data_type);
    proto = internal->proto;
    func = internal->func;
  }
  else if (expression->callee_data_type.type == TYPE_PROTOTYPE)
  {
    proto = expression->function->proto_prototype;
    func = expression->function->item_prototype;
  }
  else if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  {
    proto = data_type_to_proto(expression->callee_data_type);
  }
  else
  {
    proto = expression->function->proto;
    func = expression->function->item;
  }

  assert(proto != NULL);

  ArrayMIR_op_t arguments;
  array_init(&arguments);
  array_add(&arguments, MIR_new_ref_op(codegen.ctx, proto));

  if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  {
    MIR_reg_t callee_ptr = _MIR_new_temp_reg(
      codegen.ctx, data_type_to_mir_type(expression->callee_data_type), codegen.function->u.func);
    generate_expression(callee_ptr, expression->callee);

    MIR_label_t cont_label = MIR_new_label(codegen.ctx);
    MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_BF, MIR_new_label_op(codegen.ctx, if_false_label),
                                 MIR_new_reg_op(codegen.ctx, callee_ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

    MIR_append_insn(codegen.ctx, codegen.function, if_false_label);
    generate_panic(expression->callee_token, "Null pointer call");
    MIR_append_insn(codegen.ctx, codegen.function, cont_label);

    array_add(&arguments, MIR_new_reg_op(codegen.ctx, callee_ptr));
  }
  else
  {
    assert(func != NULL);

    array_add(&arguments, MIR_new_ref_op(codegen.ctx, func));
  }

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
  else
  {
    MIR_label_t cont_label = MIR_new_label(codegen.ctx);
    MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_BF, MIR_new_label_op(codegen.ctx, if_false_label),
                                 MIR_new_reg_op(codegen.ctx, ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 generate_object_field_op(expression->variable, ptr)));

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));
    MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

    generate_panic(expression->name, "Null pointer access");

    MIR_append_insn(codegen.ctx, codegen.function, cont_label);

    return;
  }
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
  case TYPE_OBJECT: {
    if (expression->function->data_type.type == TYPE_VOID)
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, ptr), MIR_new_reg_op(codegen.ctx, index)));
    else
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_call_insn(codegen.ctx, 5, MIR_new_ref_op(codegen.ctx, expression->function->proto),
                          MIR_new_ref_op(codegen.ctx, expression->function->item),
                          MIR_new_reg_op(codegen.ctx, dest), MIR_new_reg_op(codegen.ctx, ptr),
                          MIR_new_reg_op(codegen.ctx, index)));

    return;
  }
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

static void generate_is_expression(MIR_reg_t dest, IsExpr* expression)
{
  generate_expression(dest, expression->expr);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_URSH, MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_reg_op(codegen.ctx, dest), MIR_new_int_op(codegen.ctx, 48)));

  uint64_t id = data_type_to_typeid(expression->is_data_type);
  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_EQ, MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_reg_op(codegen.ctx, dest), MIR_new_int_op(codegen.ctx, id)));
}

static void generate_if_expression(MIR_reg_t dest, IfExpr* expression)
{
  MIR_reg_t condition = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)),
                                          codegen.function->u.func);
  generate_expression(condition, expression->condition);

  MIR_label_t cont_label = MIR_new_label(codegen.ctx);
  MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_BFS, MIR_new_label_op(codegen.ctx, if_false_label),
                               MIR_new_reg_op(codegen.ctx, condition)));

  generate_expression(dest, expression->left);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

  MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

  generate_expression(dest, expression->right);

  MIR_append_insn(codegen.ctx, codegen.function, cont_label);
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
  case EXPR_IF:
    generate_if_expression(dest, &expression->cond);
    return;
  case EXPR_IS:
    generate_is_expression(dest, &expression->is);
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
    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                 MIR_new_ref_op(codegen.ctx, statement->item)));
    MIR_reg_t initializer = _MIR_new_temp_reg(
      codegen.ctx, data_type_to_mir_type(statement->data_type), codegen.function->u.func);

    if (statement->initializer)
      generate_expression(initializer, statement->initializer);
    else
      generate_default_initialization(initializer, statement->data_type);

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
    return;

  if (statement->item == NULL || statement->proto == NULL)
    init_function_declaration(statement);

  MIR_item_t previous_function = codegen.function;
  codegen.function = statement->item;

  MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
  MIR_set_curr_func(codegen.ctx, codegen.function->u.func);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    variable->reg = MIR_new_func_reg(
      codegen.ctx, codegen.function->u.func, data_type_to_mir_type(variable->data_type),
      memory_sprintf("%s.%d", variable->name.lexeme, variable->index));
  }

  generate_statements(&statement->body);

  if (statement->data_type.type == TYPE_VOID)
    MIR_append_insn(codegen.ctx, codegen.function, MIR_new_ret_insn(codegen.ctx, 0));

  MIR_finish_func(codegen.ctx);
  MIR_set_curr_func(codegen.ctx, previous_func);
  codegen.function = previous_function;
}

static void generate_function_template_declaration(FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    generate_function_declaration(function_declaration);
  }
}

static void generate_class_declaration(ClassStmt* statement)
{
  ArrayFuncStmt initializer_functions = {
    .size = 0,
    .cap = 1,
    .elems = alloca(sizeof(FuncStmt)),
  };

  FuncStmt* function;
  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name_raw, "__init__") == 0)
      array_add(&initializer_functions, function);

    generate_function_declaration(function);
  }

  FuncTemplateStmt* function_template;
  array_foreach(&statement->function_templates, function_template)
  {
    generate_function_template_declaration(function_template);
  }

  unsigned int index = 0;

  do
  {
    FuncStmt* initializer_function =
      initializer_functions.size ? initializer_functions.elems[index] : NULL;

    MIR_item_t previous_function = codegen.function;
    MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);

    if (initializer_function)
    {
      codegen.function = initializer_function->item_prototype;
      MIR_set_curr_func(codegen.ctx, codegen.function->u.func);
    }
    else
    {
      codegen.function = statement->default_constructor->item_prototype;
      MIR_set_curr_func(codegen.ctx, codegen.function->u.func);
    }

    MIR_reg_t ptr = MIR_reg(codegen.ctx, "this.0", codegen.function->u.func);
    generate_malloc_expression(ptr, MIR_new_int_op(codegen.ctx, statement->size));

    VarStmt* variable;
    array_foreach(&statement->variables, variable)
    {
      MIR_reg_t initializer = _MIR_new_temp_reg(
        codegen.ctx, data_type_to_mir_type(variable->data_type), codegen.function->u.func);

      generate_default_initialization(initializer, variable->data_type);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(variable->data_type),
                                   generate_object_field_op(variable, ptr),
                                   MIR_new_reg_op(codegen.ctx, initializer)));
    }

    array_foreach(&statement->variables, variable)
    {
      if (variable->initializer)
      {
        MIR_reg_t initializer = _MIR_new_temp_reg(
          codegen.ctx, data_type_to_mir_type(variable->data_type), codegen.function->u.func);

        generate_expression(initializer, variable->initializer);

        MIR_append_insn(codegen.ctx, codegen.function,
                        MIR_new_insn(codegen.ctx, data_type_to_mov_type(variable->data_type),
                                     generate_object_field_op(variable, ptr),
                                     MIR_new_reg_op(codegen.ctx, initializer)));
      }
    }

    if (initializer_function)
    {
      ArrayMIR_op_t arguments;
      array_init(&arguments);

      array_add(&arguments, MIR_new_ref_op(codegen.ctx, initializer_function->proto));
      array_add(&arguments, MIR_new_ref_op(codegen.ctx, initializer_function->item));

      for (unsigned int i = 0; i < initializer_function->parameters.size; i++)
      {
        VarStmt* parameter = array_at(&initializer_function->parameters, i);
        const char* name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);

        MIR_reg_t var_reg = MIR_reg(codegen.ctx, name, codegen.function->u.func);
        array_add(&arguments, MIR_new_reg_op(codegen.ctx, var_reg));
      }

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn_arr(codegen.ctx, MIR_CALL, arguments.size, arguments.elems));
    }

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, ptr)));

    index++;

    MIR_finish_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, previous_func);
    codegen.function = previous_function;
  } while (index < initializer_functions.size);
}

static void generate_class_template_declaration(ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    generate_class_declaration(class_declaration);
  }
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
  case STMT_CLASS_DECL:
    generate_class_declaration(&statement->class);
    return;
  case STMT_CLASS_TEMPLATE_DECL:
    generate_class_template_declaration(&statement->class_template);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    generate_function_template_declaration(&statement->func_template);
    return;
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

static void init_function_declaration(FuncStmt* statement)
{
  MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
  MIR_set_curr_func(codegen.ctx, NULL);

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

  statement->proto = MIR_new_proto_arr(
    codegen.ctx, memory_sprintf("%s.proto", statement->name.lexeme),
    statement->data_type.type != TYPE_VOID,
    (MIR_type_t[]){ data_type_to_mir_type(statement->data_type) }, vars.size, vars.elems);

  if (statement->import)
  {
    statement->item = MIR_new_import(codegen.ctx, statement->name.lexeme);
  }
  else
  {
    statement->item = MIR_new_func_arr(
      codegen.ctx, statement->name.lexeme, statement->data_type.type != TYPE_VOID,
      (MIR_type_t[]){ data_type_to_mir_type(statement->data_type) }, vars.size, vars.elems);

    array_foreach(&statement->parameters, parameter)
    {
      parameter->reg = MIR_reg(codegen.ctx, vars.elems[_i].name, statement->item->u.func);
    }

    array_add(&codegen.function_items, statement->item);
  }

  MIR_set_curr_func(codegen.ctx, previous_func);
}

static void init_function_template_declaration(FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    init_function_declaration(function_declaration);
  }
}

static void init_class_declaration(ClassStmt* statement)
{
  ArrayFuncStmt initializer_functions = {
    .size = 0,
    .cap = 1,
    .elems = alloca(sizeof(FuncStmt)),
  };

  FuncStmt* function;
  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name_raw, "__init__") == 0)
      array_add(&initializer_functions, function);

    init_function_declaration(function);
  }

  FuncTemplateStmt* function_template;
  array_foreach(&statement->function_templates, function_template)
  {
    init_function_template_declaration(function_template);
  }

  unsigned int index = 0;

  do
  {
    MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
    MIR_set_curr_func(codegen.ctx, NULL);

    FuncStmt* initializer_function =
      initializer_functions.size ? initializer_functions.elems[index] : NULL;

    const char* initalizer_name =
      initializer_functions.size > 1
        ? function_data_type_to_string(statement->name.lexeme,
                                       initializer_function->function_data_type)
        : statement->name.lexeme;

    ArrayMIR_var_t vars;
    array_init(&vars);

    MIR_var_t var;
    var.name = "this.0";
    var.type = data_type_to_mir_type(DATA_TYPE(TYPE_OBJECT));
    array_add(&vars, var);

    if (initializer_function)
    {
      for (unsigned int i = 1; i < initializer_function->parameters.size; i++)
      {
        VarStmt* parameter = array_at(&initializer_function->parameters, i);

        MIR_var_t var;
        var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
        var.type = data_type_to_mir_type(parameter->data_type);

        array_add(&vars, var);
      }
    }

    MIR_item_t item = MIR_new_func_arr(
      codegen.ctx, initalizer_name, 1,
      (MIR_type_t[]){ data_type_to_mir_type(DATA_TYPE(TYPE_OBJECT)) }, vars.size, vars.elems);

    MIR_item_t proto = MIR_new_proto_arr(
      codegen.ctx, memory_sprintf("%s.proto", initalizer_name), 1,
      (MIR_type_t[]){ data_type_to_mir_type(DATA_TYPE(TYPE_OBJECT)) }, vars.size, vars.elems);

    if (initializer_function)
    {
      initializer_function->item_prototype = item;
      initializer_function->proto_prototype = proto;
    }
    else
    {
      statement->default_constructor->item_prototype = item;
      statement->default_constructor->proto_prototype = proto;
    }

    array_add(&codegen.function_items, item);

    MIR_set_curr_func(codegen.ctx, previous_func);
    index++;

  } while (index < initializer_functions.size);
}

static void init_import_declaration(ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    init_statement(body_statement);
  }
}

static void init_class_template_declaration(ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    init_class_declaration(class_declaration);
  }
}

static void init_variable_declaration(VarStmt* statement)
{
  if (statement->scope == SCOPE_GLOBAL)
  {
    uint64_t init = 0;
    statement->item = MIR_new_data(codegen.ctx, statement->name.lexeme,
                                   data_type_to_mir_type(statement->data_type), 1, &init);
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void init_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_VARIABLE_DECL:
    init_variable_declaration(&statement->var);
    return;
  case STMT_FUNCTION_DECL:
    init_function_declaration(&statement->func);
    return;
  case STMT_IMPORT_DECL:
    init_import_declaration(&statement->import);
    return;
  case STMT_CLASS_DECL:
    init_class_declaration(&statement->class);
    return;
  case STMT_CLASS_TEMPLATE_DECL:
    init_class_template_declaration(&statement->class_template);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    init_function_template_declaration(&statement->func_template);
    return;
  default:
    return;
  }
}

static void init_statements(ArrayStmt* statements)
{
  Stmt* statement;
  array_foreach(statements, statement)
  {
    init_statement(statement);
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

  array_init(&codegen.function_items);
  array_add(&codegen.function_items, codegen.function);

  MIR_load_external(codegen.ctx, "panic", (void*)panic);
  codegen.panic.proto = MIR_new_proto_arr(codegen.ctx, "panic.proto", 0, NULL, 3,
                                          (MIR_var_t[]){ { .name = "ptr", .type = MIR_T_I64 },
                                                         { .name = "line", .type = MIR_T_I64 },
                                                         { .name = "column", .type = MIR_T_I64 } });
  codegen.panic.func = MIR_new_import(codegen.ctx, "panic");

  MIR_load_external(codegen.ctx, "malloc", (void*)malloc);
  codegen.malloc.proto =
    MIR_new_proto_arr(codegen.ctx, "malloc.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .type = MIR_T_I64 } });
  codegen.malloc.func = MIR_new_import(codegen.ctx, "malloc");

  MIR_load_external(codegen.ctx, "memcpy", (void*)memcpy);
  codegen.memcpy.proto =
    MIR_new_proto_arr(codegen.ctx, "memcpy.proto", 0, (MIR_type_t[]){ MIR_T_I64 }, 3,
                      (MIR_var_t[]){ { .name = "dest", .type = MIR_T_I64 },
                                     { .name = "soruce", .type = MIR_T_I64 },
                                     { .name = "n", .type = MIR_T_I64 } });
  codegen.memcpy.func = MIR_new_import(codegen.ctx, "memcpy");

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

  MIR_load_external(codegen.ctx, "string.equals", (void*)string_equals);
  codegen.string_equals.proto = MIR_new_proto_arr(
    codegen.ctx, "string.equals.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 2,
    (MIR_var_t[]){ { .name = "left", .type = MIR_T_I64 }, { .name = "right", .type = MIR_T_I64 } });
  codegen.string_equals.func = MIR_new_import(codegen.ctx, "string.equals");

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
  MIR_load_external(codegen.ctx, "log<int>", (void*)log_int);
  MIR_load_external(codegen.ctx, "log<bool>", (void*)log_int);
  MIR_load_external(codegen.ctx, "log<float>", (void*)log_float);
  MIR_load_external(codegen.ctx, "log<char>", (void*)log_char);
  MIR_load_external(codegen.ctx, "log<string>", (void*)log_string);

  map_init_function(&codegen.functions, 0, 0);
  map_init_mir_item(&codegen.string_constants, 0, 0);
  map_init_mir_item(&codegen.items, 0, 0);
  map_init_s64(&codegen.typeids, 0, 0);

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

  init_statements(&codegen.statements);
  generate_statements(&codegen.statements);

  MIR_finish_func(codegen.ctx);
  MIR_finish_module(codegen.ctx);

  if (logging)
    MIR_output(codegen.ctx, stdout);

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

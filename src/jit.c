#include "array.h"
#include "checker.h"
#include "expression.h"
#include "include/cyth.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "parser.h"
#include "statement.h"

#include <ctype.h>
#include <gc.h>
#include <mir-gen.h>
#include <mir.h>

#ifdef _WIN32
#include <Windows.h>
#include <math.h>
#else
#define __USE_GNU
#include <pthread.h>
#include <signal.h>

#if defined(__linux__)
#include <ucontext.h>
#elif defined(__APPLE__)
#include <sys/ucontext.h>
#endif
#endif

array_def(MIR_type_t, MIR_type_t);
array_def(MIR_var_t, MIR_var_t);
array_def(MIR_op_t, MIR_op_t);
array_def(MIR_reg_t, MIR_reg_t);
array_def(MIR_item_t, MIR_item_t);

typedef void (*Start)(void);
typedef struct _FUNCTION
{
  MIR_item_t func;
  MIR_item_t proto;
} Function;

struct _CY_VM
{
  jmp_buf* jmp;
  MIR_context_t ctx;
  MIR_module_t module;
  MIR_item_t function;
  MIR_label_t continue_label;
  MIR_label_t break_label;
  Start start;

  ArrayStmt statements;
  MapS64 typeids;
  MapMIR_item string_constants;
  MapMIR_item items;
  MapFunction functions;

  Function panic;
  Function malloc;
  Function malloc_atomic;
  Function realloc;
  Function memcpy;
  Function memmove;
  Function string_equals;
  Function string_bool_cast;
  Function string_int_cast;
  Function string_float_cast;
  Function string_char_cast;

  int logging;
  void (*error_callback)(int start_line, int start_column, int end_line, int end_column,
                         const char* message);
  void (*panic_callback)(const char* function, int line, int column);
};

static void generate_default_initialization(CyVM* vm, MIR_reg_t dest, DataType data_type);
static void generate_string_cast(CyVM* vm, MIR_reg_t dest, MIR_reg_t expr, MIR_reg_t depth,
                                 MIR_reg_t list, DataType data_type);
static void generate_expression(CyVM* vm, MIR_reg_t dest, Expr* expression);
static void generate_statement(CyVM* vm, Stmt* statement);
static void generate_statements(CyVM* vm, ArrayStmt* statements);
static void init_statement(CyVM* vm, Stmt* statement);
static void init_statements(CyVM* vm, ArrayStmt* statements);
static void init_function_declaration(CyVM* vm, FuncStmt* statement);
static void panic(CyVM* vm, const char* what, uintptr_t pc, uintptr_t fp);

static int string_equals(CyString* left, CyString* right)
{
  return left == right ||
         (left->size == right->size && memcmp(left->data, right->data, left->size) == 0);
}

static CyString* string_int_cast(int n)
{
  int length = snprintf(NULL, 0, "%d", n) + 1;
  uintptr_t size = sizeof(CyString) + length;

  CyString* result = GC_malloc_atomic(size);
  result->size = length - 1;

  snprintf(result->data, length, "%d", n);

  return result;
}

static CyString* string_float_cast(float n)
{
  int length = snprintf(NULL, 0, "%.10g", n) + 1;
  uintptr_t size = sizeof(CyString) + length;

  CyString* result = GC_malloc_atomic(size);
  result->size = length - 1;

  snprintf(result->data, length, "%.10g", n);

  return result;
}

static CyString* string_char_cast(char n)
{
  int length = snprintf(NULL, 0, "%c", n) + 1;
  uintptr_t size = sizeof(CyString) + length;

  CyString* result = GC_malloc_atomic(size);
  result->size = length - 1;

  snprintf(result->data, length, "%c", n);

  return result;
}

static CyString* string_bool_cast(bool n)
{
  cyth_static_string(true_string, "true");
  cyth_static_string(false_string, "false");

  return n ? (CyString*)&true_string : (CyString*)&false_string;
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

static bool data_type_is_pointer(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_ALIAS:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_FUNCTION_GROUP:
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER:
  case TYPE_NULL:
  case TYPE_FLOAT:
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
    return false;
  case TYPE_ANY:
  case TYPE_STRING:
  case TYPE_OBJECT:
  case TYPE_ARRAY:
    return true;
  }

  UNREACHABLE("Unexpected data type");
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

static MIR_type_t data_type_to_sized_mir_type(DataType data_type)
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

static MIR_type_t sized_mir_type_to_mir_type(MIR_type_t type)
{
  switch (type)
  {
  case MIR_T_F:
    return MIR_T_F;
  default:
    return MIR_T_I64;
  }

  UNREACHABLE("Unexpected mir type");
}

static MIR_item_t data_type_to_proto(CyVM* vm, DataType data_type)
{
  const char* name = data_type_to_string(data_type);
  MIR_item_t item = map_get_mir_item(&vm->items, name);

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

    item = MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", data_type_to_string(data_type)),
                             return_data_type.type != TYPE_VOID,
                             (MIR_type_t[]){ data_type_to_mir_type(return_data_type) }, vars.size,
                             vars.elems);

    map_put_mir_item(&vm->items, name, item);
  }

  return item;
}

static uint64_t data_type_to_typeid(CyVM* vm, DataType data_type)
{
  const char* name = data_type_to_string(data_type);
  uint64_t id = map_get_s64(&vm->typeids, name);

  if (!id)
  {
    id = map_size_s64(&vm->typeids) + 1;
    map_put_s64(&vm->typeids, name, id);
  }

  return id;
}

static FuncStmt* get_function_member(DataType data_type, const char* name)
{
  if (data_type.type != TYPE_OBJECT)
    return NULL;

  ClassStmt* class = data_type.class;
  VarStmt* variable = map_get_var_stmt(class->members, name);

  if (!variable || variable->data_type.type != TYPE_FUNCTION_MEMBER)
    return NULL;

  FuncStmt* function = variable->data_type.function_member.function;
  return function;
}

static MIR_insn_t generate_debug_info(Token token, MIR_insn_t insn)
{
  insn->line = token.start_line;
  insn->column = token.start_column;

  return insn;
}

static void generate_malloc_expression(CyVM* vm, MIR_reg_t dest, MIR_op_t size)
{
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->malloc.proto),
                                    MIR_new_ref_op(vm->ctx, vm->malloc.func),
                                    MIR_new_reg_op(vm->ctx, dest), size));
}

static void generate_malloc_atomic_expression(CyVM* vm, MIR_reg_t dest, MIR_op_t size)
{
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->malloc_atomic.proto),
                                    MIR_new_ref_op(vm->ctx, vm->malloc_atomic.func),
                                    MIR_new_reg_op(vm->ctx, dest), size));
}

static void generate_realloc_expression(CyVM* vm, MIR_op_t dest, MIR_op_t ptr, MIR_op_t size)
{
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->realloc.proto),
                                    MIR_new_ref_op(vm->ctx, vm->realloc.func), dest, ptr, size));
}

static void generate_string_literal_expression(CyVM* vm, MIR_op_t dest, const char* literal,
                                               int length)
{
  MIR_item_t item = map_get_mir_item(&vm->string_constants, literal);
  if (!item)
  {
    if (length == -1)
      length = strlen(literal);

    uintptr_t size = sizeof(CyString) + length + 1;
    CyString* string = memory_alloc(size);
    string->size = length;
    string->data[length] = '\0';
    memcpy(string->data, literal, length);

    const char* name = memory_sprintf("string.%d", map_size_mir_item(&vm->string_constants));
    item = MIR_new_data(vm->ctx, name, MIR_T_U8, size, string);

    map_put_mir_item(&vm->string_constants, literal, item);
  }

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, data_type_to_mov_type(DATA_TYPE(TYPE_STRING)), dest,
                               MIR_new_ref_op(vm->ctx, item)));
}

static void generate_panic(CyVM* vm, const char* what, Token token)
{
  MIR_append_insn(vm->ctx, vm->function,
                  generate_debug_info(
                    token, MIR_new_call_insn(vm->ctx, 6, MIR_new_ref_op(vm->ctx, vm->panic.proto),
                                             MIR_new_ref_op(vm->ctx, vm->panic.func),
                                             MIR_new_int_op(vm->ctx, (uint64_t)vm),
                                             MIR_new_int_op(vm->ctx, (uint64_t)what),
                                             MIR_new_int_op(vm->ctx, (uint64_t)0),
                                             MIR_new_int_op(vm->ctx, (uint64_t)0))));
}

static MIR_op_t generate_array_length_op(CyVM* vm, MIR_reg_t ptr)
{
  return MIR_new_mem_op(vm->ctx, MIR_T_U32, 0, ptr, 0, 1);
}

static MIR_op_t generate_array_capacity_op(CyVM* vm, MIR_reg_t ptr)
{
  return MIR_new_mem_op(vm->ctx, MIR_T_U32, sizeof(unsigned int), ptr, 0, 1);
}

static MIR_op_t generate_array_data_op(CyVM* vm, MIR_reg_t ptr)
{
  return MIR_new_mem_op(vm->ctx, MIR_T_I64, sizeof(unsigned int) + sizeof(unsigned int), ptr, 0, 1);
}

static MIR_op_t generate_string_length_op(CyVM* vm, MIR_reg_t base)
{
  return MIR_new_mem_op(vm->ctx, MIR_T_I32, 0, base, 0, 1);
}

static MIR_op_t generate_string_at_op(CyVM* vm, MIR_reg_t base, MIR_reg_t index)
{
  return MIR_new_mem_op(vm->ctx, MIR_T_U8, sizeof(unsigned int), base, index, 1);
}

static MIR_op_t generate_object_field_op(CyVM* vm, VarStmt* field, MIR_reg_t ptr)
{
  return MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(field->data_type), field->offset, ptr,
                        0, 1);
}

static void generate_default_array_initialization(CyVM* vm, MIR_reg_t dest)
{
  generate_malloc_expression(vm, dest, MIR_new_int_op(vm->ctx, sizeof(CyArray)));

  MIR_append_insn(
    vm->ctx, vm->function,
    MIR_new_insn(vm->ctx, MIR_MOV, generate_array_length_op(vm, dest), MIR_new_int_op(vm->ctx, 0)));

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_MOV, generate_array_capacity_op(vm, dest),
                               MIR_new_int_op(vm->ctx, 0)));

  MIR_append_insn(
    vm->ctx, vm->function,
    MIR_new_insn(vm->ctx, MIR_MOV, generate_array_data_op(vm, dest), MIR_new_int_op(vm->ctx, 0)));
}

static Function* generate_array_push_function(CyVM* vm, DataType data_type,
                                              DataType element_data_type)
{
  const char* name = memory_sprintf("array.push.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = {
      { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) },
      { .name = "value", .size = 0, .type = data_type_to_mir_type(element_data_type) }
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t value = MIR_reg(vm->ctx, "value", vm->function->u.func);

    {
      MIR_label_t push_label = MIR_new_label(vm->ctx);
      MIR_label_t resize_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BEQS, MIR_new_label_op(vm->ctx, resize_label),
                                   generate_array_length_op(vm, ptr),
                                   generate_array_capacity_op(vm, ptr)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, push_label)));

      MIR_append_insn(vm->ctx, vm->function, resize_label);

      MIR_reg_t new_size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_UMULOS, generate_array_capacity_op(vm, ptr),
                                   generate_array_capacity_op(vm, ptr),
                                   MIR_new_int_op(vm->ctx, 2)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADDS, generate_array_capacity_op(vm, ptr),
                                   generate_array_capacity_op(vm, ptr),
                                   MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_UMULOS, MIR_new_reg_op(vm->ctx, new_size),
                                   generate_array_capacity_op(vm, ptr),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      generate_realloc_expression(vm, generate_array_data_op(vm, ptr),
                                  generate_array_data_op(vm, ptr),
                                  MIR_new_reg_op(vm->ctx, new_size));

      MIR_append_insn(vm->ctx, vm->function, push_label);
    }

    {
      MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                   generate_array_data_op(vm, ptr)));

      MIR_reg_t index = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, index),
                                   generate_array_length_op(vm, ptr)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type)),
                     MIR_new_reg_op(vm->ctx, value)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADDS, generate_array_length_op(vm, ptr),
                                   generate_array_length_op(vm, ptr), MIR_new_int_op(vm->ctx, 1)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_push_string_function(CyVM* vm, DataType data_type)
{
  const char* name = "array.pushString";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType element_data_type = DATA_TYPE(TYPE_CHAR);

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = {
      { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) },
      { .name = "string_ptr", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) }
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t string_ptr = MIR_reg(vm->ctx, "string_ptr", vm->function->u.func);

    {
      MIR_reg_t new_size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADDS, MIR_new_reg_op(vm->ctx, new_size),
                                   generate_array_length_op(vm, ptr),
                                   generate_string_length_op(vm, string_ptr)));

      MIR_label_t push_label = MIR_new_label(vm->ctx);
      MIR_label_t resize_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_UBGES, MIR_new_label_op(vm->ctx, resize_label),
                                   MIR_new_reg_op(vm->ctx, new_size),
                                   generate_array_capacity_op(vm, ptr)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, push_label)));

      MIR_append_insn(vm->ctx, vm->function, resize_label);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_array_capacity_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, new_size)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_UMULOS, generate_array_capacity_op(vm, ptr),
                                   generate_array_capacity_op(vm, ptr),
                                   MIR_new_int_op(vm->ctx, 2 * size_data_type(element_data_type))));

      generate_realloc_expression(vm, generate_array_data_op(vm, ptr),
                                  generate_array_data_op(vm, ptr),
                                  generate_array_capacity_op(vm, ptr));

      MIR_append_insn(vm->ctx, vm->function, push_label);
    }

    {
      MIR_reg_t dest_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, dest_ptr),
                                   generate_array_length_op(vm, ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_UMULO, MIR_new_reg_op(vm->ctx, dest_ptr),
                                   MIR_new_reg_op(vm->ctx, dest_ptr),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, dest_ptr),
                                   MIR_new_reg_op(vm->ctx, dest_ptr),
                                   generate_array_data_op(vm, ptr)));

      MIR_reg_t source_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, source_ptr),
                                   MIR_new_reg_op(vm->ctx, string_ptr),
                                   MIR_new_int_op(vm->ctx, sizeof(unsigned int))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->memcpy.proto),
                                        MIR_new_ref_op(vm->ctx, vm->memcpy.func),
                                        MIR_new_reg_op(vm->ctx, dest_ptr),
                                        MIR_new_reg_op(vm->ctx, source_ptr),
                                        generate_string_length_op(vm, string_ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADDS, generate_array_length_op(vm, ptr),
                                   generate_array_length_op(vm, ptr),
                                   generate_string_length_op(vm, string_ptr)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_pop_function(CyVM* vm, DataType data_type)
{
  const char* name = memory_sprintf("array.pop.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);

    MIR_type_t return_type = data_type_to_mir_type(element_data_type);
    MIR_var_t params[] = { { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);

    {
      MIR_label_t finish_label = MIR_new_label(vm->ctx);
      MIR_label_t panic_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BEQS, MIR_new_label_op(vm->ctx, panic_label),
                                   generate_array_length_op(vm, ptr), MIR_new_int_op(vm->ctx, 0)));

      MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                   generate_array_data_op(vm, ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_SUBS, generate_array_length_op(vm, ptr),
                                   generate_array_length_op(vm, ptr), MIR_new_int_op(vm->ctx, 1)));

      MIR_reg_t index = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, index),
                                   generate_array_length_op(vm, ptr)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_ret_insn(vm->ctx, 1,
                         MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                        array_ptr, index, size_data_type(element_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, finish_label)));

      MIR_append_insn(vm->ctx, vm->function, panic_label);

      generate_panic(vm, "Out of bounds access", (Token){ 0 });

      MIR_append_insn(vm->ctx, vm->function, finish_label);
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_to_string_function(CyVM* vm, DataType data_type)
{
  const char* name = "array.toString";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType return_data_type = DATA_TYPE(TYPE_STRING);

    MIR_type_t return_type = data_type_to_mir_type(return_data_type);
    MIR_var_t params[] = { { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);

    {
      MIR_reg_t string_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_reg_t size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_int_op(vm->ctx, sizeof(unsigned int) + 1),
                                   generate_array_length_op(vm, ptr)));

      generate_malloc_atomic_expression(vm, string_ptr, MIR_new_reg_op(vm->ctx, size));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, size),
                                   generate_array_length_op(vm, ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(return_data_type),
                                   generate_string_at_op(vm, string_ptr, size),
                                   MIR_new_int_op(vm->ctx, 0)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_string_length_op(vm, string_ptr),
                                   generate_array_length_op(vm, ptr)));

      MIR_reg_t dest_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, dest_ptr),
                                   MIR_new_reg_op(vm->ctx, string_ptr),
                                   MIR_new_int_op(vm->ctx, sizeof(unsigned int))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->memcpy.proto),
                        MIR_new_ref_op(vm->ctx, vm->memcpy.func), MIR_new_reg_op(vm->ctx, dest_ptr),
                        generate_array_data_op(vm, ptr), generate_array_length_op(vm, ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, string_ptr)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_clear_function(CyVM* vm, DataType data_type)
{
  const char* name = "array.clear";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    MIR_var_t params[] = { { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) } };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);

    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_array_length_op(vm, ptr),
                                   MIR_new_int_op(vm->ctx, 0)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_remove_function(CyVM* vm, DataType data_type)
{
  const char* name = memory_sprintf("array.remove.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);

    MIR_type_t return_type = data_type_to_mir_type(element_data_type);
    MIR_var_t params[] = {
      { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) },
      { .name = "index", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t index = MIR_reg(vm->ctx, "index", vm->function->u.func);

    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_CCLEAR, MIR_new_reg_op(vm->ctx, ptr),
                                   MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index),
                                   generate_array_length_op(vm, ptr)));

      MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                   generate_array_data_op(vm, ptr)));

      MIR_reg_t value = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_reg_op(vm->ctx, value),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type))));

      MIR_reg_t size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, index),
                                   MIR_new_reg_op(vm->ctx, index), MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_SUB, MIR_new_reg_op(vm->ctx, size),
                                   generate_array_length_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, index)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MUL, MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MUL, MIR_new_reg_op(vm->ctx, index),
                                   MIR_new_reg_op(vm->ctx, index),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      MIR_reg_t sub_array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, sub_array_ptr),
                                   MIR_new_reg_op(vm->ctx, array_ptr),
                                   MIR_new_reg_op(vm->ctx, index)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_SUB, MIR_new_reg_op(vm->ctx, array_ptr),
                                   MIR_new_reg_op(vm->ctx, sub_array_ptr),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->memmove.proto),
                                        MIR_new_ref_op(vm->ctx, vm->memmove.func),
                                        MIR_new_reg_op(vm->ctx, array_ptr),
                                        MIR_new_reg_op(vm->ctx, sub_array_ptr),
                                        MIR_new_reg_op(vm->ctx, size)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_SUB, generate_array_length_op(vm, ptr),
                                   generate_array_length_op(vm, ptr), MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, value)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_array_reserve_function(CyVM* vm, DataType data_type)
{
  const char* name = memory_sprintf("array.reserve.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_VOID));
    ArrayMIR_var_t params;
    array_init(&params);

    MIR_var_t this = { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) };
    array_add(&params, this);

    for (int i = 0; i < *data_type.array.count; i++)
    {
      MIR_var_t n = { .name = memory_sprintf("n.%d", i), .type = data_type_to_mir_type(data_type) };
      array_add(&params, n);
    }

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, params.size, params.elems);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      params.size, params.elems);

    vm->function = function->func;

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t n = MIR_reg(vm->ctx, "n.0", vm->function->u.func);
    MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    {
      MIR_label_t continue_label = MIR_new_label(vm->ctx);
      MIR_label_t panic_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BLTS, MIR_new_label_op(vm->ctx, panic_label),
                                   MIR_new_reg_op(vm->ctx, n), MIR_new_int_op(vm->ctx, 0)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));

      MIR_append_insn(vm->ctx, vm->function, panic_label);

      generate_panic(vm, "Invalid reservation amount", (Token){ 0 });

      MIR_append_insn(vm->ctx, vm->function, continue_label);
    }

    {
      MIR_reg_t size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MUL, MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_reg_op(vm->ctx, n),
                                   MIR_new_int_op(vm->ctx, size_data_type(element_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                   generate_array_data_op(vm, ptr)));

      generate_realloc_expression(vm, MIR_new_reg_op(vm->ctx, array_ptr),
                                  MIR_new_reg_op(vm->ctx, array_ptr),
                                  MIR_new_reg_op(vm->ctx, size));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_array_data_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, array_ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_array_length_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, n)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_array_capacity_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, n)));
    }

    {
      MIR_reg_t i = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 0)));

      MIR_label_t break_label = MIR_new_label(vm->ctx);
      MIR_label_t continue_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function, continue_label);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BGE, MIR_new_label_op(vm->ctx, break_label),
                                   MIR_new_reg_op(vm->ctx, i), MIR_new_reg_op(vm->ctx, n)));

      MIR_reg_t dest =
        _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(element_data_type), vm->function->u.func);
      generate_default_initialization(vm, dest, element_data_type);

      if (element_data_type.type == TYPE_ARRAY)
      {
        Function* function = generate_array_reserve_function(vm, element_data_type);

        ArrayMIR_op_t arguments;
        array_init(&arguments);
        array_add(&arguments, MIR_new_ref_op(vm->ctx, function->proto));
        array_add(&arguments, MIR_new_ref_op(vm->ctx, function->func));
        array_add(&arguments, MIR_new_reg_op(vm->ctx, dest));

        for (int i = 1; i < *data_type.array.count; i++)
        {
          MIR_reg_t n = MIR_reg(vm->ctx, memory_sprintf("n.%d", i), vm->function->u.func);
          array_add(&arguments, MIR_new_reg_op(vm->ctx, n));
        }

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn_arr(vm->ctx, MIR_INLINE, arguments.size, arguments.elems));
      }

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, i, size_data_type(element_data_type)),
                     MIR_new_reg_op(vm->ctx, dest)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, i),
                                   MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 1)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));
      MIR_append_insn(vm->ctx, vm->function, break_label);
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static int int_hash(int n)
{
  return n;
}

static Function* generate_int_hash_function(CyVM* vm)
{
  const char* name = "int.hash";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)int_hash);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int float_hash(float n)
{
  union {
    float f;
    int i;
  } value;
  value.f = n;

  return value.i;
}

static Function* generate_float_hash_function(CyVM* vm)
{
  const char* name = "float.hash";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)float_hash);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static Function* generate_float_sqrt_function(CyVM* vm)
{
  const char* name = "float.sqrt";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT));
    MIR_var_t params[] = {
      { .name = "n", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_FLOAT)) },
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    MIR_reg_t n = MIR_reg(vm->ctx, "n", vm->function->u.func);

    {
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_FSQRT, MIR_new_reg_op(vm->ctx, n), MIR_new_reg_op(vm->ctx, n)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, n)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static int string_hash(CyString* n)
{
  uint32_t hash = 0x811c9dc5;

  for (int i = 0; i < n->size; i++)
  {
    hash ^= n->data[i];
    hash *= 0x01000193;
  }

  return hash;
}

static Function* generate_string_hash_function(CyVM* vm)
{
  const char* name = "string.hash";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "n", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_hash);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int string_index_of(CyString* haystack, CyString* needle)
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

static Function* generate_string_index_of_function(CyVM* vm)
{
  const char* name = "string.index_of";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "haystack", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "needle", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_index_of);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int string_count(CyString* haystack, CyString* needle)
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

static Function* generate_string_count_function(CyVM* vm)
{
  const char* name = "string.count";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER));
    MIR_var_t params[] = {
      { .name = "haystack", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "needle", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_count);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyString* string_replace(CyString* input, CyString* old, CyString* new)
{
  if (old == new)
    return input;

  int count = string_count(input, old);
  if (count == 0)
    return input;

  int size = input->size + count * (new->size - old->size);

  CyString* result = GC_malloc_atomic(sizeof(CyString) + size + 1);
  result->size = size;
  result->data[size] = '\0';

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

static Function* generate_string_replace_function(CyVM* vm)
{
  const char* name = "string.replace";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "old", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "new", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_replace);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyString* string_trim(CyString* input)
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

  CyString* result = GC_malloc_atomic(sizeof(CyString) + size + 1);
  result->size = size;
  result->data[size] = '\0';

  for (int i = start, j = 0; i <= end; i++, j++)
    result->data[j] = input->data[i];

  return result;
}

static Function* generate_string_trim_function(CyVM* vm)
{
  const char* name = "string.trim";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_trim);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int string_starts_with(CyString* input, CyString* target)
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

static Function* generate_string_starts_with_function(CyVM* vm)
{
  const char* name = "string.starts_with";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_starts_with);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int string_ends_with(CyString* input, CyString* target)
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

static Function* generate_string_ends_with_function(CyVM* vm)
{
  const char* name = "string.ends_with";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_ends_with);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static int string_contains(CyString* input, CyString* target)
{
  return string_index_of(input, target) != -1;
}

static Function* generate_string_contains_function(CyVM* vm)
{
  const char* name = "string.contains";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_BOOL));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "target", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_contains);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyArray* string_split(CyString* input, CyString* delim)
{
  if (delim->size == 0)
  {
    CyArray* result = GC_malloc(sizeof(CyArray));
    result->size = input->size;
    result->capacity = input->size;
    result->data = GC_malloc(sizeof(CyString*) * input->size);

    CyString** data = result->data;

    for (int i = 0; i < input->size; i++)
    {
      CyString* item = GC_malloc_atomic(sizeof(CyString) + 1 + 1);
      item->size = 1;
      item->data[0] = input->data[i];
      item->data[1] = '\0';

      *data = item;
      data += 1;
    }

    return result;
  }
  else
  {
    int count = string_count(input, delim) + 1;

    CyArray* result = GC_malloc(sizeof(CyArray));
    result->size = count;
    result->capacity = count;
    result->data = GC_malloc(sizeof(CyString*) * count);

    CyString** data = result->data;

    int current = 0;
    int previous = 0;

    while (current <= input->size - delim->size)
    {
      bool match = true;

      for (int j = 0; j < delim->size; j++)
      {
        if (input->data[current + j] != delim->data[j])
        {
          match = false;
          break;
        }
      }

      if (match)
      {
        const int size = current - previous;

        CyString* item = GC_malloc_atomic(sizeof(CyString) + size + 1);
        item->size = size;
        item->data[size] = '\0';
        memcpy(item->data, input->data + previous, size);

        *data = item;
        data += 1;

        current += delim->size;
        previous = current;
      }
      else
      {
        current++;
      }
    }

    const int size = input->size - previous;

    CyString* item = GC_malloc_atomic(sizeof(CyString) + size + 1);
    item->size = size;
    item->data[size] = '\0';
    memcpy(item->data, input->data + previous, size);

    *data = item;
    data += 1;

    return result;
  }
}

static Function* generate_string_split_function(CyVM* vm, DataType return_data_type)
{
  const char* name = "string.split";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(return_data_type);
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "delim", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_split);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyString* string_join(CyArray* input, CyString* delim)
{
  if (input->size == 0)
  {
    CyString* result = GC_malloc_atomic(sizeof(CyString) + 1);
    result->size = 0;
    result->data[result->size] = '\0';

    return result;
  }

  CyString** data = (CyString**)input->data;

  int size = delim->size * (input->size - 1);
  for (int i = 0; i < input->size; i++)
    size += data[i]->size;

  CyString* result = GC_malloc_atomic(sizeof(CyString) + size + 1);
  result->size = size;
  result->data[size] = '\0';

  for (int i = 0, k = 0; i < input->size; i++)
  {
    CyString* item = data[i];
    memcpy((char*)result->data + k, item->data, item->size);

    k += item->size;

    if (i != input->size - 1)
    {
      memcpy((char*)result->data + k, delim->data, delim->size);

      k += delim->size;
    }
  }

  return result;
}

static Function* generate_string_join_function(CyVM* vm, DataType array_data_type)
{
  const char* name = "string.join";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(array_data_type) },
      { .name = "delim", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_join);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyArray* string_to_array(CyString* input)
{
  CyArray* result = GC_malloc(sizeof(CyArray));
  result->size = input->size;
  result->capacity = input->size;
  result->data = GC_malloc_atomic(sizeof(char) * input->size);

  memcpy(result->data, input->data, input->size);

  return result;
}

static Function* generate_string_to_array_function(CyVM* vm, DataType return_data_type)
{
  const char* name = "string.to_array";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(return_data_type);
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_to_array);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static CyString* string_pad(CyString* input, int pad)
{
  const int size = pad + input->size;

  CyString* result = GC_malloc_atomic(sizeof(CyString) + size + 1);
  result->size = size;
  result->data[size] = '\0';

  for (int i = 0; i < pad; i++)
    result->data[i] = ' ';

  for (int i = 0; i < input->size; i++)
    result->data[pad + i] = input->data[i];

  return result;
}

static Function* generate_string_pad_function(CyVM* vm)
{
  const char* name = "string.pad";

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "input", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "pad", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
    };

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_import(vm->ctx, name);

    MIR_load_external(vm->ctx, name, (uintptr_t)string_pad);
    map_put_function(&vm->functions, name, function);
  }

  return function;
}

static Function* generate_string_concat_function(CyVM* vm, int count)
{
  assert(count >= 2);

  const char* name = count == 2 ? "string.concat" : memory_sprintf("string.concat.%d", count);

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));

    ArrayMIR_var_t params;
    array_init(&params);

    for (int i = 0; i < count; i++)
    {
      MIR_var_t param;
      param.name = memory_sprintf("n.%d", i);
      param.type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));

      array_add(&params, param);
    }

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, params.size, params.elems);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      params.size, params.elems);
    vm->function = function->func;

    MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_reg_t size = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    {
      for (int i = 0; i < count; i++)
      {
        MIR_reg_t n_ptr = MIR_reg(vm->ctx, params.elems[i].name, vm->function->u.func);
        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, size),
                                     i == 0 ? MIR_new_int_op(vm->ctx, sizeof(unsigned int) + 1)
                                            : MIR_new_reg_op(vm->ctx, size),
                                     generate_string_length_op(vm, n_ptr)));
      }

      generate_malloc_atomic_expression(vm, ptr, MIR_new_reg_op(vm->ctx, size));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_SUB, MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_reg_op(vm->ctx, size),
                                   MIR_new_int_op(vm->ctx, sizeof(unsigned int) + 1)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, generate_string_length_op(vm, ptr),
                                   MIR_new_reg_op(vm->ctx, size)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(DATA_TYPE(TYPE_STRING)),
                                   generate_string_at_op(vm, ptr, size),
                                   MIR_new_int_op(vm->ctx, 0)));
    }

    {
      MIR_reg_t dest_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, dest_ptr),
                                   MIR_new_reg_op(vm->ctx, ptr),
                                   MIR_new_int_op(vm->ctx, sizeof(unsigned int))));

      for (int i = 0; i < count; i++)
      {
        MIR_reg_t n_ptr = MIR_reg(vm->ctx, params.elems[i].name, vm->function->u.func);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, size),
                                     generate_string_length_op(vm, n_ptr)));

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, n_ptr),
                                     MIR_new_reg_op(vm->ctx, n_ptr),
                                     MIR_new_int_op(vm->ctx, sizeof(unsigned int))));

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->memcpy.proto),
                                          MIR_new_ref_op(vm->ctx, vm->memcpy.func),
                                          MIR_new_reg_op(vm->ctx, dest_ptr),
                                          MIR_new_reg_op(vm->ctx, n_ptr),
                                          MIR_new_reg_op(vm->ctx, size)));

        if (i + 1 != count)
        {
          MIR_append_insn(vm->ctx, vm->function,
                          MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, dest_ptr),
                                       MIR_new_reg_op(vm->ctx, dest_ptr),
                                       MIR_new_reg_op(vm->ctx, size)));
        }
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, ptr)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static void generate_default_initialization(CyVM* vm, MIR_reg_t dest, DataType data_type)
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
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_float_op(vm->ctx, 0.0)));
    return;
  case TYPE_STRING:
    generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, dest), "", 0);
    return;
  case TYPE_ARRAY:
    generate_default_array_initialization(vm, dest);
    return;
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static Function* generate_function_internal(CyVM* vm, DataType data_type)
{
  assert(data_type.type == TYPE_FUNCTION_INTERNAL);

  const char* name = data_type.function_internal.name;

  if (strcmp(name, "array.push") == 0)
    return generate_array_push_function(vm,
                                        array_at(&data_type.function_internal.parameter_types, 0),
                                        array_at(&data_type.function_internal.parameter_types, 1));
  else if (strcmp(name, "array.push_string") == 0)
    return generate_array_push_string_function(
      vm, array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.pop") == 0)
    return generate_array_pop_function(vm,
                                       array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.to_string") == 0)
    return generate_array_to_string_function(
      vm, array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.clear") == 0)
    return generate_array_clear_function(vm,
                                         array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.remove") == 0)
    return generate_array_remove_function(
      vm, array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.reserve") == 0)
    return generate_array_reserve_function(
      vm, array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "int.hash") == 0)
    return generate_int_hash_function(vm);
  else if (strcmp(name, "float.hash") == 0)
    return generate_float_hash_function(vm);
  else if (strcmp(name, "float.sqrt") == 0)
    return generate_float_sqrt_function(vm);
  else if (strcmp(name, "string.hash") == 0)
    return generate_string_hash_function(vm);
  else if (strcmp(name, "string.index_of") == 0)
    return generate_string_index_of_function(vm);
  else if (strcmp(name, "string.count") == 0)
    return generate_string_count_function(vm);
  else if (strcmp(name, "string.replace") == 0)
    return generate_string_replace_function(vm);
  else if (strcmp(name, "string.trim") == 0)
    return generate_string_trim_function(vm);
  else if (strcmp(name, "string.starts_with") == 0)
    return generate_string_starts_with_function(vm);
  else if (strcmp(name, "string.ends_with") == 0)
    return generate_string_ends_with_function(vm);
  else if (strcmp(name, "string.contains") == 0)
    return generate_string_contains_function(vm);
  else if (strcmp(name, "string.split") == 0)
    return generate_string_split_function(vm, *data_type.function_internal.return_type);
  else if (strcmp(name, "string.join") == 0)
    return generate_string_join_function(vm,
                                         array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "string.to_array") == 0)
    return generate_string_to_array_function(vm, *data_type.function_internal.return_type);
  else
    UNREACHABLE("Unexpected internal function");
}

static void generate_function_pointer(CyVM* vm, MIR_reg_t dest, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FUNCTION:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_ref_op(vm->ctx, data_type.function->item)));
    return;
  case TYPE_FUNCTION_MEMBER:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type), MIR_new_reg_op(vm->ctx, dest),
                   MIR_new_ref_op(vm->ctx, data_type.function_member.function->item)));
    return;
  case TYPE_FUNCTION_INTERNAL:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type), MIR_new_reg_op(vm->ctx, dest),
                   MIR_new_ref_op(vm->ctx, generate_function_internal(vm, data_type)->func)));
    return;
  default:
    UNREACHABLE("Unknown function type");
  }
}

static void generate_group_expression(CyVM* vm, MIR_reg_t dest, GroupExpr* expression)
{
  generate_expression(vm, dest, expression->expr);
}

static void generate_literal_expression(CyVM* vm, MIR_reg_t dest, LiteralExpr* expression)
{
  switch (expression->data_type.type)
  {
  case TYPE_INTEGER:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_int_op(vm->ctx, expression->integer)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_float_op(vm->ctx, expression->floating)));
    return;
  case TYPE_BOOL:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_int_op(vm->ctx, expression->boolean)));
    return;
  case TYPE_NULL:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));
    return;
  case TYPE_CHAR:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_int_op(vm->ctx, expression->string.data[0])));
    return;
  case TYPE_STRING:
    generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, dest), expression->string.data,
                                       expression->string.length);
    return;
  default:
    UNREACHABLE("Unhandled literal value");
  }
}

static void generate_binary_expression_function_call(CyVM* vm, MIR_reg_t dest,
                                                     BinaryExpr* expression, MIR_reg_t left,
                                                     MIR_reg_t right)
{
  MIR_insn_t insn =
    expression->function->data_type.type == TYPE_VOID
      ? MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, expression->function->proto),
                          MIR_new_ref_op(vm->ctx, expression->function->item),
                          MIR_new_reg_op(vm->ctx, left), MIR_new_reg_op(vm->ctx, right))
      : MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, expression->function->proto),
                          MIR_new_ref_op(vm->ctx, expression->function->item),
                          MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, left),
                          MIR_new_reg_op(vm->ctx, right));

  MIR_append_insn(vm->ctx, vm->function, insn);
}

static void generate_binary_expression(CyVM* vm, MIR_reg_t dest, BinaryExpr* expression)
{
  MIR_reg_t left = 0;
  MIR_reg_t right = 0;

  DataType data_type = expression->left_data_type;

  if (expression->op.type != TOKEN_OR && expression->op.type != TOKEN_AND &&
      !(expression->op.type == TOKEN_PLUS && data_type.type == TYPE_STRING))
  {
    left = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->left_data_type),
                             vm->function->u.func);
    right = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->right_data_type),
                              vm->function->u.func);

    generate_expression(vm, left, expression->left);
    generate_expression(vm, right, expression->right);
  }

  int op = 0;

  switch (expression->op.type)
  {
  case TOKEN_PLUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_ADD;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FADD;
    else if (data_type.type == TYPE_STRING)
    {
      ArrayExpr strings;
      array_init(&strings);

      ArrayExpr stack;
      array_init(&stack);
      array_add(&stack, expression->right);
      array_add(&stack, expression->left);

      while (array_size(&stack))
      {
        Expr* expr = array_at(&stack, stack.size - 1);
        array_del_last(&stack);

        if (expr->type == EXPR_BINARY && expr->binary.op.type == TOKEN_PLUS)
        {
          array_add(&stack, expr->binary.right);
          array_add(&stack, expr->binary.left);
        }
        else if (expr->type == EXPR_GROUP)
        {
          array_add(&stack, expr->group.expr);
        }
        else
        {
          array_add(&strings, expr);
        }
      }

      Function* string_concat = generate_string_concat_function(vm, strings.size);

      ArrayMIR_op_t arguments;
      array_init(&arguments);
      array_add(&arguments, MIR_new_ref_op(vm->ctx, string_concat->proto));
      array_add(&arguments, MIR_new_ref_op(vm->ctx, string_concat->func));
      array_add(&arguments, MIR_new_reg_op(vm->ctx, dest));

      Expr* string;
      array_foreach(&strings, string)
      {
        MIR_reg_t n = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->left_data_type),
                                        vm->function->u.func);
        generate_expression(vm, n, string);

        array_add(&arguments, MIR_new_reg_op(vm->ctx, n));
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn_arr(vm->ctx, MIR_INLINE, arguments.size, arguments.elems));
      return;
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
        vm->ctx, vm->function,
        MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->string_equals.proto),
                          MIR_new_ref_op(vm->ctx, vm->string_equals.func),
                          MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, left),
                          MIR_new_reg_op(vm->ctx, right)));
      return;
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      if (expression->function)
        generate_binary_expression_function_call(vm, dest, expression, left, right);
      else
        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_EQ, MIR_new_reg_op(vm->ctx, dest),
                                     MIR_new_reg_op(vm->ctx, left),
                                     MIR_new_reg_op(vm->ctx, right)));
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
        vm->ctx, vm->function,
        MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, vm->string_equals.proto),
                          MIR_new_ref_op(vm->ctx, vm->string_equals.func),
                          MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, left),
                          MIR_new_reg_op(vm->ctx, right)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_EQ, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));
      return;
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      if (expression->function)
        generate_binary_expression_function_call(vm, dest, expression, left, right);
      else
        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_NE, MIR_new_reg_op(vm->ctx, dest),
                                     MIR_new_reg_op(vm->ctx, left),
                                     MIR_new_reg_op(vm->ctx, right)));
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
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
      generate_binary_expression_function_call(vm, dest, expression, left, right);
      return;
    }
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      left = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->left_data_type),
                               vm->function->u.func);
      generate_expression(vm, left, expression->left);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BNES, MIR_new_label_op(vm->ctx, if_false_label),
                                   MIR_new_reg_op(vm->ctx, left), MIR_new_int_op(vm->ctx, 0)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      right = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->right_data_type),
                                vm->function->u.func);
      generate_expression(vm, right, expression->right);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, right)));

      MIR_append_insn(vm->ctx, vm->function, cont_label);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      left = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->left_data_type),
                               vm->function->u.func);
      generate_expression(vm, left, expression->left);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BNES, MIR_new_label_op(vm->ctx, if_false_label),
                                   MIR_new_reg_op(vm->ctx, left), MIR_new_int_op(vm->ctx, 0)));

      right = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->right_data_type),
                                vm->function->u.func);
      generate_expression(vm, right, expression->right);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, right)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function, cont_label);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for OR");

    break;

  default:
    UNREACHABLE("Unhandled binary operation");
  }

  MIR_append_insn(
    vm->ctx, vm->function,
    generate_debug_info(expression->op, MIR_new_insn(vm->ctx, op, MIR_new_reg_op(vm->ctx, dest),
                                                     MIR_new_reg_op(vm->ctx, left),
                                                     MIR_new_reg_op(vm->ctx, right))));
}

static void generate_unary_expression(CyVM* vm, MIR_reg_t dest, UnaryExpr* expression)
{
  MIR_reg_t expr =
    _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->data_type), vm->function->u.func);
  generate_expression(vm, expr, expression->expr);

  switch (expression->op.type)
  {
  case TOKEN_TILDE:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_XORS, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_int_op(vm->ctx, 0xFFFFFFFF)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for ~");

  case TOKEN_MINUS:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_NEGS, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr)));
      break;
    }
    else if (expression->data_type.type == TYPE_FLOAT)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_FNEG, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BFS, MIR_new_label_op(vm->ctx, if_false_label),
                                   MIR_new_reg_op(vm->ctx, expr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function, cont_label);

      break;
    }
    else
      UNREACHABLE("Unsupported unary type for !");

  default:
    UNREACHABLE("Unhandled unary expression");
  }
}

static Function* generate_string_array_cast_function(CyVM* vm, DataType data_type)
{
  const char* name = memory_sprintf("string.array_cast.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    DataType element_data_type = array_data_type_element(data_type);
    bool multiline = element_data_type.type == TYPE_ARRAY || element_data_type.type == TYPE_OBJECT;

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) },
      { .name = "dest", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "depth", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
      { .name = "list", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_ARRAY)) },
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);
    vm->function = function->func;

    Function* string_concat = generate_string_concat_function(vm, 2);
    Function* string_pad = generate_string_pad_function(vm);

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t dest = MIR_reg(vm->ctx, "dest", vm->function->u.func);
    MIR_reg_t depth = MIR_reg(vm->ctx, "depth", vm->function->u.func);
    MIR_reg_t list = MIR_reg(vm->ctx, "list", vm->function->u.func);

    MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                 generate_array_data_op(vm, ptr)));

    MIR_reg_t tmp = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    {
      if (multiline)
      {
        MIR_label_t if_false_label = MIR_new_label(vm->ctx);
        MIR_label_t continue_label = MIR_new_label(vm->ctx);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_BEQ, MIR_new_label_op(vm->ctx, if_false_label),
                                     generate_array_length_op(vm, ptr),
                                     MIR_new_int_op(vm->ctx, 0)));

        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "[\n", -1);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));

        MIR_append_insn(vm->ctx, vm->function, if_false_label);

        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "[", -1);

        MIR_append_insn(vm->ctx, vm->function, continue_label);
      }
      else
      {
        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "[", -1);
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, depth),
                                   MIR_new_reg_op(vm->ctx, depth), MIR_new_int_op(vm->ctx, 1)));
    }

    {
      MIR_reg_t expr =
        _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(element_data_type), vm->function->u.func);

      MIR_reg_t i = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 0)));

      MIR_label_t break_label = MIR_new_label(vm->ctx);
      MIR_label_t continue_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function, continue_label);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BGE, MIR_new_label_op(vm->ctx, break_label),
                                   MIR_new_reg_op(vm->ctx, i), generate_array_length_op(vm, ptr)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_reg_op(vm->ctx, expr),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, i, size_data_type(element_data_type))));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "", -1);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_pad->proto),
                                        MIR_new_ref_op(vm->ctx, string_pad->func),
                                        MIR_new_reg_op(vm->ctx, tmp), MIR_new_reg_op(vm->ctx, tmp),
                                        MIR_new_reg_op(vm->ctx, depth)));

      generate_string_cast(vm, tmp, expr, depth, list, element_data_type);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, i),
                                   MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 1)));

      {
        MIR_label_t if_false_label = MIR_new_label(vm->ctx);
        MIR_label_t continue_label = MIR_new_label(vm->ctx);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_BGE, MIR_new_label_op(vm->ctx, if_false_label),
                                     MIR_new_reg_op(vm->ctx, i),
                                     generate_array_length_op(vm, ptr)));

        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp),
                                           multiline ? ",\n" : ", ", -1);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                                          MIR_new_ref_op(vm->ctx, string_concat->func),
                                          MIR_new_reg_op(vm->ctx, dest),
                                          MIR_new_reg_op(vm->ctx, dest),
                                          MIR_new_reg_op(vm->ctx, tmp)));

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));

        MIR_append_insn(vm->ctx, vm->function, if_false_label);

        if (multiline)
        {
          generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "\n", -1);

          MIR_append_insn(
            vm->ctx, vm->function,
            MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                              MIR_new_ref_op(vm->ctx, string_concat->func),
                              MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, dest),
                              MIR_new_reg_op(vm->ctx, tmp)));
        }

        MIR_append_insn(vm->ctx, vm->function, continue_label);
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));
      MIR_append_insn(vm->ctx, vm->function, break_label);
    }

    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, depth),
                                   MIR_new_reg_op(vm->ctx, depth), MIR_new_int_op(vm->ctx, -1)));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "]", -1);

      if (multiline)
      {
        MIR_append_insn(
          vm->ctx, vm->function,
          MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_pad->proto),
                            MIR_new_ref_op(vm->ctx, string_pad->func), MIR_new_reg_op(vm->ctx, tmp),
                            MIR_new_reg_op(vm->ctx, tmp), MIR_new_reg_op(vm->ctx, depth)));
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, dest)));
    }

    map_put_function(&vm->functions, name, function);

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static Function* generate_string_object_cast_function(CyVM* vm, DataType data_type)
{
  const char* name = memory_sprintf("string.object_cast.%s", data_type_to_string(data_type));

  Function* function = map_get_function(&vm->functions, name);
  if (!function)
  {
    unsigned char list_count = 1;
    DataType list_element_data_type = DATA_TYPE(TYPE_ANY);
    DataType list_data_type = DATA_TYPE(TYPE_ARRAY);
    list_data_type.array.count = &list_count;
    list_data_type.array.data_type = &list_element_data_type;

    MIR_type_t return_type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING));
    MIR_var_t params[] = {
      { .name = "ptr", .size = 0, .type = data_type_to_mir_type(data_type) },
      { .name = "dest", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_STRING)) },
      { .name = "depth", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_INTEGER)) },
      { .name = "list", .size = 0, .type = data_type_to_mir_type(DATA_TYPE(TYPE_ARRAY)) },
    };

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

    function = ALLOC(Function);
    function->proto =
      MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", name), return_type != MIR_T_UNDEF,
                        &return_type, sizeof(params) / sizeof_ptr(params), params);
    function->func = MIR_new_func_arr(vm->ctx, name, return_type != MIR_T_UNDEF, &return_type,
                                      sizeof(params) / sizeof_ptr(params), params);

    vm->function = function->func;

    map_put_function(&vm->functions, name, function);

    ClassStmt* class = data_type.class;

    Function* string_concat = generate_string_concat_function(vm, 2);
    Function* string_concat_4 = generate_string_concat_function(vm, 4);
    Function* string_pad = generate_string_pad_function(vm);
    Function* array_push = generate_array_push_function(vm, list_data_type, list_element_data_type);

    MIR_reg_t ptr = MIR_reg(vm->ctx, "ptr", vm->function->u.func);
    MIR_reg_t dest = MIR_reg(vm->ctx, "dest", vm->function->u.func);
    MIR_reg_t depth = MIR_reg(vm->ctx, "depth", vm->function->u.func);
    MIR_reg_t list = MIR_reg(vm->ctx, "list", vm->function->u.func);

    MIR_reg_t tmp = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_reg_t tmp2 = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_reg_t tmp3 = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BF, MIR_new_label_op(vm->ctx, if_false_label),
                                   MIR_new_reg_op(vm->ctx, ptr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "null", -1);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, dest)));

      MIR_append_insn(vm->ctx, vm->function, cont_label);
    }

    {
      MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                   generate_array_data_op(vm, list)));

      MIR_reg_t i = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 0)));

      MIR_label_t break_label = MIR_new_label(vm->ctx);
      MIR_label_t continue_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function, continue_label);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BGE, MIR_new_label_op(vm->ctx, break_label),
                                   MIR_new_reg_op(vm->ctx, i), generate_array_length_op(vm, list)));

      MIR_reg_t expr = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(list_element_data_type),
                                         vm->function->u.func);
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(list_element_data_type),
                     MIR_new_reg_op(vm->ctx, expr),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(list_element_data_type), 0,
                                    array_ptr, i, size_data_type(list_element_data_type))));

      {
        MIR_label_t cont_label = MIR_new_label(vm->ctx);
        MIR_label_t if_false_label = MIR_new_label(vm->ctx);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_BEQ, MIR_new_label_op(vm->ctx, if_false_label),
                                     MIR_new_reg_op(vm->ctx, expr), MIR_new_reg_op(vm->ctx, ptr)));

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

        MIR_append_insn(vm->ctx, vm->function, if_false_label);

        {
          MIR_label_t cont_label = MIR_new_label(vm->ctx);
          MIR_label_t if_false_label = MIR_new_label(vm->ctx);

          MIR_append_insn(vm->ctx, vm->function,
                          MIR_new_insn(vm->ctx, MIR_BT, MIR_new_label_op(vm->ctx, if_false_label),
                                       MIR_new_reg_op(vm->ctx, i)));

          generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), class->name.lexeme,
                                             -1);
          MIR_append_insn(
            vm->ctx, vm->function,
            MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                              MIR_new_ref_op(vm->ctx, string_concat->func),
                              MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, dest),
                              MIR_new_reg_op(vm->ctx, tmp)));
          MIR_append_insn(vm->ctx, vm->function,
                          MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

          MIR_append_insn(vm->ctx, vm->function, if_false_label);

          generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp),
                                             memory_sprintf("%s$", class->name.lexeme), -1);
          MIR_append_insn(
            vm->ctx, vm->function,
            MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                              MIR_new_ref_op(vm->ctx, string_concat->func),
                              MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, dest),
                              MIR_new_reg_op(vm->ctx, tmp)));

          MIR_append_insn(
            vm->ctx, vm->function,
            MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_int_cast.proto),
                              MIR_new_ref_op(vm->ctx, vm->string_int_cast.func),
                              MIR_new_reg_op(vm->ctx, tmp), MIR_new_reg_op(vm->ctx, i)));
          MIR_append_insn(
            vm->ctx, vm->function,
            MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                              MIR_new_ref_op(vm->ctx, string_concat->func),
                              MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, dest),
                              MIR_new_reg_op(vm->ctx, tmp)));

          MIR_append_insn(vm->ctx, vm->function, cont_label);
        }

        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), "(...)", -1);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                                          MIR_new_ref_op(vm->ctx, string_concat->func),
                                          MIR_new_reg_op(vm->ctx, dest),
                                          MIR_new_reg_op(vm->ctx, dest),
                                          MIR_new_reg_op(vm->ctx, tmp)));

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, dest)));

        MIR_append_insn(vm->ctx, vm->function, cont_label);
      }

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, i),
                                   MIR_new_reg_op(vm->ctx, i), MIR_new_int_op(vm->ctx, 1)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, continue_label)));

      MIR_append_insn(vm->ctx, vm->function, break_label);
    }

    {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_BT, MIR_new_label_op(vm->ctx, if_false_label),
                                   generate_array_length_op(vm, list)));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), class->name.lexeme, -1);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp),
                                         memory_sprintf("%s$", class->name.lexeme), -1);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_int_cast.proto),
                          MIR_new_ref_op(vm->ctx, vm->string_int_cast.func),
                          MIR_new_reg_op(vm->ctx, tmp), generate_array_length_op(vm, list)));
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function, cont_label);
    }

    {
      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp),
                                         class->variables.size ? "(\n" : "(", -1);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, depth),
                                   MIR_new_reg_op(vm->ctx, depth), MIR_new_int_op(vm->ctx, 1)));
    }

    if (data_type.type == TYPE_OBJECT)
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, array_push->proto),
                                        MIR_new_ref_op(vm->ctx, array_push->func),
                                        MIR_new_reg_op(vm->ctx, list),
                                        MIR_new_reg_op(vm->ctx, ptr)));

    VarStmt* variable;
    array_foreach(&class->variables, variable)
    {
      MIR_reg_t expr = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(variable->data_type),
                                         vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(variable->data_type),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   generate_object_field_op(vm, variable, ptr)));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp),
                                         memory_sprintf("%s = ", variable->name.lexeme), -1);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_pad->proto),
                                        MIR_new_ref_op(vm->ctx, string_pad->func),
                                        MIR_new_reg_op(vm->ctx, tmp), MIR_new_reg_op(vm->ctx, tmp),
                                        MIR_new_reg_op(vm->ctx, depth)));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp2), "", -1);
      generate_string_cast(vm, tmp2, expr, depth, list, variable->data_type);

      if (_i + 1 == class->variables.size)
        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp3),
                                           memory_sprintf("\n", variable->name.lexeme), -1);
      else
        generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp3),
                                           memory_sprintf(",\n", variable->name.lexeme), -1);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 7, MIR_new_ref_op(vm->ctx, string_concat_4->proto),
                                        MIR_new_ref_op(vm->ctx, string_concat_4->func),
                                        MIR_new_reg_op(vm->ctx, dest),
                                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp),
                                        MIR_new_reg_op(vm->ctx, tmp2),
                                        MIR_new_reg_op(vm->ctx, tmp3)));
    }

    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ADD, MIR_new_reg_op(vm->ctx, depth),
                                   MIR_new_reg_op(vm->ctx, depth), MIR_new_int_op(vm->ctx, -1)));

      generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, tmp), ")", -1);

      if (class->variables.size)
        MIR_append_insn(
          vm->ctx, vm->function,
          MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_pad->proto),
                            MIR_new_ref_op(vm->ctx, string_pad->func), MIR_new_reg_op(vm->ctx, tmp),
                            MIR_new_reg_op(vm->ctx, tmp), MIR_new_reg_op(vm->ctx, depth)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, dest)));
    }

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  }

  return function;
}

static void generate_string_cast(CyVM* vm, MIR_reg_t dest, MIR_reg_t expr, MIR_reg_t depth,
                                 MIR_reg_t list, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_BOOL:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_bool_cast.proto),
                        MIR_new_ref_op(vm->ctx, vm->string_bool_cast.func),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_float_cast.proto),
                        MIR_new_ref_op(vm->ctx, vm->string_float_cast.func),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  case TYPE_INTEGER:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_int_cast.proto),
                        MIR_new_ref_op(vm->ctx, vm->string_int_cast.func),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  case TYPE_CHAR:
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, vm->string_char_cast.proto),
                        MIR_new_ref_op(vm->ctx, vm->string_char_cast.func),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;

  case TYPE_STRING:
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  case TYPE_ARRAY: {
    Function* function = generate_string_array_cast_function(vm, data_type);
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_call_insn(vm->ctx, 7, MIR_new_ref_op(vm->ctx, function->proto),
                                      MIR_new_ref_op(vm->ctx, function->func),
                                      MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr),
                                      MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, depth),
                                      MIR_new_reg_op(vm->ctx, list)));
    return;
  }
  case TYPE_OBJECT: {
    FuncStmt* function = get_function_member(data_type, "__str__");
    if (function)
    {
      Function* string_concat = generate_string_concat_function(vm, 2);
      MIR_reg_t tmp = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, function->proto),
                                        MIR_new_ref_op(vm->ctx, function->item),
                                        MIR_new_reg_op(vm->ctx, tmp),
                                        MIR_new_reg_op(vm->ctx, expr)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(
                        vm->ctx, 5, MIR_new_ref_op(vm->ctx, string_concat->proto),
                        MIR_new_ref_op(vm->ctx, string_concat->func), MIR_new_reg_op(vm->ctx, dest),
                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, tmp)));
      return;
    }
    else
    {
      Function* function = generate_string_object_cast_function(vm, data_type);
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_call_insn(vm->ctx, 7, MIR_new_ref_op(vm->ctx, function->proto),
                          MIR_new_ref_op(vm->ctx, function->func), MIR_new_reg_op(vm->ctx, dest),
                          MIR_new_reg_op(vm->ctx, expr), MIR_new_reg_op(vm->ctx, dest),
                          MIR_new_reg_op(vm->ctx, depth), MIR_new_reg_op(vm->ctx, list)));
      return;
    }
  }

  case TYPE_ANY:
  case TYPE_FUNCTION_POINTER: {
    MIR_label_t cont_label = MIR_new_label(vm->ctx);
    MIR_label_t if_false_label = MIR_new_label(vm->ctx);

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_BF, MIR_new_label_op(vm->ctx, if_false_label),
                                 MIR_new_reg_op(vm->ctx, expr)));

    const char* name = data_type_to_string(data_type);
    generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, dest), name, -1);

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));
    MIR_append_insn(vm->ctx, vm->function, if_false_label);

    generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, dest), "null", -1);

    MIR_append_insn(vm->ctx, vm->function, cont_label);
    return;
  }

  default: {
    const char* name = data_type_to_string(data_type);
    generate_string_literal_expression(vm, MIR_new_reg_op(vm->ctx, dest), name, -1);
    return;
  }
  }
}

static void generate_cast_expression(CyVM* vm, MIR_reg_t dest, CastExpr* expression)
{
  MIR_type_t type = data_type_to_mir_type(expression->from_data_type);
  MIR_reg_t expr = 0;

  if (type != MIR_T_UNDEF && expression->from_data_type.type != TYPE_FUNCTION &&
      expression->from_data_type.type != TYPE_FUNCTION_MEMBER &&
      expression->from_data_type.type != TYPE_FUNCTION_INTERNAL)
  {
    expr = _MIR_new_temp_reg(vm->ctx, type, vm->function->u.func);
    generate_expression(vm, expr, expression->expr);
  }

  if (expression->to_data_type.type == TYPE_FLOAT &&
      expression->from_data_type.type == TYPE_INTEGER)
  {
    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_insn(vm->ctx, MIR_I2F, MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  }
  else if (expression->to_data_type.type == TYPE_STRING)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_BOOL:
    case TYPE_FLOAT:
    case TYPE_INTEGER:
    case TYPE_CHAR:
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
      MIR_reg_t depth = 0;
      MIR_reg_t list = 0;

      if (expression->from_data_type.type == TYPE_OBJECT ||
          expression->from_data_type.type == TYPE_ARRAY)
      {
        depth = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->from_data_type),
                                  vm->function->u.func);

        MIR_append_insn(vm->ctx, vm->function,
                        MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, depth),
                                     MIR_new_int_op(vm->ctx, 0)));

        list = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->from_data_type),
                                 vm->function->u.func);

        generate_default_array_initialization(vm, list);
        generate_default_initialization(vm, dest, DATA_TYPE(TYPE_STRING));
      }

      generate_string_cast(vm, dest, expr, depth, list, expression->from_data_type);
      return;
    }
    case TYPE_ANY: {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_reg_t id = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_URSH, MIR_new_reg_op(vm->ctx, id),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 48)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_BNE, MIR_new_label_op(vm->ctx, if_false_label),
                     MIR_new_reg_op(vm->ctx, id),
                     MIR_new_int_op(vm->ctx, data_type_to_typeid(vm, expression->to_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_AND, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_int_op(vm->ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));
      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      generate_panic(vm, "Invalid type cast", expression->type.token);

      MIR_append_insn(vm->ctx, vm->function, cont_label);
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
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_I2F, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr)));
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
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_ANDS, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 0xFF)));
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
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
      return;
    case TYPE_FLOAT:
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_F2I, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr)));
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
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_FNE, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_float_op(vm->ctx, 0.0f)));

      return;
    case TYPE_INTEGER:
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_NES, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 0)));
      return;
    case TYPE_STRING:
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_NES, MIR_new_reg_op(vm->ctx, dest),
                                   generate_string_length_op(vm, expr),
                                   MIR_new_int_op(vm->ctx, 0)));
      return;
    case TYPE_ARRAY:
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_NES, MIR_new_reg_op(vm->ctx, dest),
                                   generate_array_length_op(vm, expr), MIR_new_int_op(vm->ctx, 0)));
      return;
    case TYPE_ANY:
    case TYPE_NULL:
    case TYPE_OBJECT:
    case TYPE_FUNCTION_POINTER: {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_NES, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 0)));

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
      uint64_t id = data_type_to_typeid(vm, expression->from_data_type) << 48;

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_AND, MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_int_op(vm->ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_OR, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, id)));
      return;
    }
    case TYPE_NULL:
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->to_data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
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
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_reg_t id = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_URSH, MIR_new_reg_op(vm->ctx, id),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 48)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_BNE, MIR_new_label_op(vm->ctx, if_false_label),
                     MIR_new_reg_op(vm->ctx, id),
                     MIR_new_int_op(vm->ctx, data_type_to_typeid(vm, expression->to_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_AND, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_int_op(vm->ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));
      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      generate_panic(vm, "Invalid type cast", expression->type.token);

      MIR_append_insn(vm->ctx, vm->function, cont_label);
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
      generate_default_initialization(vm, dest, expression->to_data_type);
      return;
    case TYPE_ANY: {
      MIR_label_t cont_label = MIR_new_label(vm->ctx);
      MIR_label_t if_false_label = MIR_new_label(vm->ctx);

      MIR_reg_t id = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_URSH, MIR_new_reg_op(vm->ctx, id),
                                   MIR_new_reg_op(vm->ctx, expr), MIR_new_int_op(vm->ctx, 48)));

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, MIR_BNE, MIR_new_label_op(vm->ctx, if_false_label),
                     MIR_new_reg_op(vm->ctx, id),
                     MIR_new_int_op(vm->ctx, data_type_to_typeid(vm, expression->to_data_type))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_AND, MIR_new_reg_op(vm->ctx, dest),
                                   MIR_new_reg_op(vm->ctx, expr),
                                   MIR_new_int_op(vm->ctx, 0xFFFFFFFFFFFFUL)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));
      MIR_append_insn(vm->ctx, vm->function, if_false_label);

      generate_panic(vm, "Invalid type cast", expression->type.token);

      MIR_append_insn(vm->ctx, vm->function, cont_label);
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
      generate_function_pointer(vm, dest, expression->from_data_type);
      return;
    case TYPE_NULL:
      generate_default_initialization(vm, dest, expression->to_data_type);
      return;

    default:
      break;
    }
  }

  if (equal_data_type(expression->from_data_type, expression->to_data_type))
  {
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->to_data_type),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, expr)));
    return;
  }

  UNREACHABLE("Unsupported cast type");
}

static void generate_variable_expression(CyVM* vm, MIR_reg_t dest, VarExpr* expression)
{
  MIR_type_t type = data_type_to_mir_type(expression->data_type);
  if (type == MIR_T_UNDEF)
    return;

  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    generate_function_pointer(vm, dest, expression->data_type);
    return;
  }

  Scope scope = expression->variable->scope;
  switch (scope)
  {
  case SCOPE_LOCAL: {
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_reg_op(vm->ctx, expression->variable->reg)));

    return;
  }
  case SCOPE_GLOBAL: {
    MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, ptr),
                                 MIR_new_ref_op(vm->ctx, expression->variable->item)));
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_mem_op(vm->ctx, type, 0, ptr, 0, 1)));

    return;
  }
  case SCOPE_CLASS: {
    MIR_reg_t ptr = MIR_reg(vm->ctx, "this.0", vm->function->u.func);
    MIR_append_insn(
      vm->ctx, vm->function,
      generate_debug_info(expression->name,
                          MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                       MIR_new_reg_op(vm->ctx, dest),
                                       generate_object_field_op(vm, expression->variable, ptr))));

    return;
  }
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static void generate_assignment_expression(CyVM* vm, MIR_reg_t dest, AssignExpr* expression)
{
  MIR_reg_t value = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->value_data_type),
                                      vm->function->u.func);
  generate_expression(vm, value, expression->value);

  VarStmt* variable = expression->variable;
  if (variable)
  {
    switch (variable->scope)
    {
    case SCOPE_LOCAL: {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, variable->reg),
                                   MIR_new_reg_op(vm->ctx, value)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, value)));

      return;
    }

    case SCOPE_GLOBAL: {
      MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, ptr),
                                   MIR_new_ref_op(vm->ctx, expression->variable->item)));
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(
          vm->ctx, data_type_to_mov_type(expression->data_type),
          MIR_new_mem_op(vm->ctx, data_type_to_mir_type(expression->data_type), 0, ptr, 0, 1),
          MIR_new_reg_op(vm->ctx, value)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, value)));

      return;
    }

    case SCOPE_CLASS: {
      MIR_reg_t ptr;

      if (expression->target->type == EXPR_ACCESS)
      {
        ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
        generate_expression(vm, ptr, expression->target->access.expr);
      }
      else
      {
        ptr = MIR_reg(vm->ctx, "this.0", vm->function->u.func);
      }

      MIR_append_insn(
        vm->ctx, vm->function,
        generate_debug_info(expression->op,
                            MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                         generate_object_field_op(vm, expression->variable, ptr),
                                         MIR_new_reg_op(vm->ctx, value))));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, value)));

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

    MIR_reg_t ptr =
      _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->target->index.expr_data_type),
                        vm->function->u.func);
    generate_expression(vm, ptr, expression->target->index.expr);

    MIR_reg_t index =
      _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->target->index.index_data_type),
                        vm->function->u.func);
    generate_expression(vm, index, expression->target->index.index);

    if (expression->target->index.expr_data_type.type == TYPE_OBJECT)
    {
      if (expression->function->data_type.type == TYPE_VOID)
        MIR_append_insn(
          vm->ctx, vm->function,
          MIR_new_call_insn(vm->ctx, 5, MIR_new_ref_op(vm->ctx, expression->function->proto),
                            MIR_new_ref_op(vm->ctx, expression->function->item),
                            MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index),
                            MIR_new_reg_op(vm->ctx, value)));
      else
        MIR_append_insn(
          vm->ctx, vm->function,
          MIR_new_call_insn(vm->ctx, 6, MIR_new_ref_op(vm->ctx, expression->function->proto),
                            MIR_new_ref_op(vm->ctx, expression->function->item),
                            MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, ptr),
                            MIR_new_reg_op(vm->ctx, index), MIR_new_reg_op(vm->ctx, value)));
    }
    else
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_CCLEAR, MIR_new_reg_op(vm->ctx, ptr),
                                   MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index),
                                   generate_array_length_op(vm, ptr)));

      MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

      MIR_append_insn(
        vm->ctx, vm->function,
        generate_debug_info(expression->op,
                            MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                         generate_array_data_op(vm, ptr))));

      DataType element_data_type =
        array_data_type_element(expression->target->index.expr_data_type);

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type)),
                     MIR_new_reg_op(vm->ctx, value)));

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                                   MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, value)));
    }
  }
}

static void generate_call_expression(CyVM* vm, MIR_reg_t dest, CallExpr* expression)
{
  MIR_item_t proto = NULL;
  MIR_item_t func = NULL;

  if (expression->callee_data_type.type == TYPE_ALIAS)
  {
    generate_default_initialization(vm, dest, *expression->callee_data_type.alias.data_type);
    return;
  }
  else if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    Function* internal = generate_function_internal(vm, expression->callee_data_type);
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
    proto = data_type_to_proto(vm, expression->callee_data_type);
  }
  else
  {
    proto = expression->function->proto;
    func = expression->function->item;
  }

  assert(proto != NULL);

  ArrayMIR_op_t arguments;
  array_init(&arguments);
  array_add(&arguments, MIR_new_ref_op(vm->ctx, proto));

  if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  {
    MIR_reg_t callee_ptr = _MIR_new_temp_reg(
      vm->ctx, data_type_to_mir_type(expression->callee_data_type), vm->function->u.func);
    generate_expression(vm, callee_ptr, expression->callee);

    array_add(&arguments, MIR_new_reg_op(vm->ctx, callee_ptr));
  }
  else
  {
    assert(func != NULL);

    array_add(&arguments, MIR_new_ref_op(vm->ctx, func));
  }

  if (expression->return_data_type.type != TYPE_VOID)
    array_add(&arguments, MIR_new_reg_op(vm->ctx, dest));

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    MIR_reg_t temp =
      _MIR_new_temp_reg(vm->ctx, sized_mir_type_to_mir_type(proto->u.proto->args->varr[_i].type),
                        vm->function->u.func);
    generate_expression(vm, temp, argument);

    array_add(&arguments, MIR_new_reg_op(vm->ctx, temp));
  }

  MIR_append_insn(
    vm->ctx, vm->function,
    generate_debug_info(expression->callee_token,
                        MIR_new_insn_arr(vm->ctx, MIR_CALL, arguments.size, arguments.elems)));
}

static void generate_access_expression(CyVM* vm, MIR_reg_t dest, AccessExpr* expression)
{
  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL)
  {
    generate_function_pointer(vm, dest, expression->data_type);
    return;
  }

  MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->expr_data_type),
                                    vm->function->u.func);
  generate_expression(vm, ptr, expression->expr);

  if (expression->expr_data_type.type == TYPE_STRING)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest),
                                   generate_string_length_op(vm, ptr)));

      return;
    }

    UNREACHABLE("Unhandled string access name");
  }
  else if (expression->expr_data_type.type == TYPE_ARRAY)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest),
                                   generate_array_length_op(vm, ptr)));
      return;
    }
    else if (strcmp(expression->name.lexeme, "capacity") == 0)
    {
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(vm->ctx, dest),
                                   generate_array_capacity_op(vm, ptr)));
      return;
    }

    UNREACHABLE("Unhandled array access name");
  }
  else
  {
    MIR_append_insn(
      vm->ctx, vm->function,
      generate_debug_info(expression->name,
                          MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                       MIR_new_reg_op(vm->ctx, dest),
                                       generate_object_field_op(vm, expression->variable, ptr))));

    return;
  }
}

static void generate_index_expression(CyVM* vm, MIR_reg_t dest, IndexExpr* expression)
{
  MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->expr_data_type),
                                    vm->function->u.func);
  generate_expression(vm, ptr, expression->expr);

  MIR_reg_t index = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(expression->index_data_type),
                                      vm->function->u.func);
  generate_expression(vm, index, expression->index);

  switch (expression->expr_data_type.type)
  {
  case TYPE_STRING: {
    MIR_reg_t length = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, length),
                                 generate_string_length_op(vm, ptr)));

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_CCLEAR, MIR_new_reg_op(vm->ctx, ptr),
                                 MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index),
                                 MIR_new_reg_op(vm->ctx, length)));

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_CCLEAR, MIR_new_reg_op(vm->ctx, index),
                                 MIR_new_reg_op(vm->ctx, index), MIR_new_reg_op(vm->ctx, index),
                                 MIR_new_reg_op(vm->ctx, length)));

    MIR_append_insn(
      vm->ctx, vm->function,
      generate_debug_info(expression->index_token,
                          MIR_new_insn(vm->ctx, data_type_to_mov_type(expression->data_type),
                                       MIR_new_reg_op(vm->ctx, dest),
                                       generate_string_at_op(vm, ptr, index))));
    return;
  }
  case TYPE_ARRAY: {
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_CCLEAR, MIR_new_reg_op(vm->ctx, ptr),
                                 MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index),
                                 generate_array_length_op(vm, ptr)));

    MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);

    MIR_append_insn(
      vm->ctx, vm->function,
      generate_debug_info(expression->index_token,
                          MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, array_ptr),
                                       generate_array_data_op(vm, ptr))));

    DataType element_data_type = array_data_type_element(expression->expr_data_type);

    MIR_append_insn(
      vm->ctx, vm->function,
      generate_debug_info(
        expression->index_token,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_reg_op(vm->ctx, dest),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type), 0,
                                    array_ptr, index, size_data_type(element_data_type)))));

    return;
  }
  case TYPE_OBJECT: {
    if (expression->function->data_type.type == TYPE_VOID)
      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_call_insn(vm->ctx, 4, MIR_new_ref_op(vm->ctx, expression->function->proto),
                          MIR_new_ref_op(vm->ctx, expression->function->item),
                          MIR_new_reg_op(vm->ctx, ptr), MIR_new_reg_op(vm->ctx, index)));
    else
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_call_insn(vm->ctx, 5,
                                        MIR_new_ref_op(vm->ctx, expression->function->proto),
                                        MIR_new_ref_op(vm->ctx, expression->function->item),
                                        MIR_new_reg_op(vm->ctx, dest), MIR_new_reg_op(vm->ctx, ptr),
                                        MIR_new_reg_op(vm->ctx, index)));

    return;
  }
  default:
    UNREACHABLE("Unhandled index type");
  }
}

static void generate_array_expression(CyVM* vm, MIR_reg_t dest, LiteralArrayExpr* expression)
{
  if (expression->values.size)
  {
    MIR_reg_t array_ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    DataType element_data_type = array_data_type_element(expression->data_type);

    generate_malloc_expression(vm, dest, MIR_new_int_op(vm->ctx, sizeof(CyArray)));

    if (data_type_is_pointer(element_data_type))
      generate_malloc_expression(
        vm, array_ptr,
        MIR_new_int_op(vm->ctx, size_data_type(element_data_type) * expression->values.size));
    else
      generate_malloc_atomic_expression(
        vm, array_ptr,
        MIR_new_int_op(vm->ctx, size_data_type(element_data_type) * expression->values.size));

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, generate_array_length_op(vm, dest),
                                 MIR_new_int_op(vm->ctx, expression->values.size)));

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, generate_array_capacity_op(vm, dest),
                                 MIR_new_int_op(vm->ctx, expression->values.size)));

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, generate_array_data_op(vm, dest),
                                 MIR_new_reg_op(vm->ctx, array_ptr)));

    MIR_reg_t item =
      _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(element_data_type), vm->function->u.func);
    Expr* value;
    array_foreach(&expression->values, value)
    {
      generate_expression(vm, item, value);

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(vm->ctx, data_type_to_mov_type(element_data_type),
                     MIR_new_mem_op(vm->ctx, data_type_to_sized_mir_type(element_data_type),
                                    _i * size_data_type(element_data_type), array_ptr, 0, 1),
                     MIR_new_reg_op(vm->ctx, item)));
    }
  }
  else
  {
    generate_default_array_initialization(vm, dest);
  }
}

static void generate_is_expression(CyVM* vm, MIR_reg_t dest, IsExpr* expression)
{
  if (expression->is_data_type.type == TYPE_ANY)
  {
    generate_expression(vm, dest, expression->expr);

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_NE, MIR_new_reg_op(vm->ctx, dest),
                                 MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 0)));
    return;
  }

  generate_expression(vm, dest, expression->expr);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_URSH, MIR_new_reg_op(vm->ctx, dest),
                               MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, 48)));

  uint64_t id = data_type_to_typeid(vm, expression->is_data_type);
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_EQ, MIR_new_reg_op(vm->ctx, dest),
                               MIR_new_reg_op(vm->ctx, dest), MIR_new_int_op(vm->ctx, id)));
}

static void generate_if_expression(CyVM* vm, MIR_reg_t dest, IfExpr* expression)
{
  MIR_reg_t condition =
    _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)), vm->function->u.func);
  generate_expression(vm, condition, expression->condition);

  MIR_label_t cont_label = MIR_new_label(vm->ctx);
  MIR_label_t if_false_label = MIR_new_label(vm->ctx);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_BFS, MIR_new_label_op(vm->ctx, if_false_label),
                               MIR_new_reg_op(vm->ctx, condition)));

  generate_expression(vm, dest, expression->left);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

  MIR_append_insn(vm->ctx, vm->function, if_false_label);

  generate_expression(vm, dest, expression->right);

  MIR_append_insn(vm->ctx, vm->function, cont_label);
}

static void generate_expression(CyVM* vm, MIR_reg_t dest, Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    generate_literal_expression(vm, dest, &expression->literal);
    return;
  case EXPR_BINARY:
    generate_binary_expression(vm, dest, &expression->binary);
    return;
  case EXPR_GROUP:
    generate_group_expression(vm, dest, &expression->group);
    return;
  case EXPR_UNARY:
    generate_unary_expression(vm, dest, &expression->unary);
    return;
  case EXPR_VAR:
    generate_variable_expression(vm, dest, &expression->var);
    return;
  case EXPR_ASSIGN:
    generate_assignment_expression(vm, dest, &expression->assign);
    return;
  case EXPR_CALL:
    generate_call_expression(vm, dest, &expression->call);
    return;
  case EXPR_CAST:
    generate_cast_expression(vm, dest, &expression->cast);
    return;
  case EXPR_ACCESS:
    generate_access_expression(vm, dest, &expression->access);
    return;
  case EXPR_INDEX:
    generate_index_expression(vm, dest, &expression->index);
    return;
  case EXPR_ARRAY:
    generate_array_expression(vm, dest, &expression->array);
    return;
  case EXPR_IF:
    generate_if_expression(vm, dest, &expression->cond);
    return;
  case EXPR_IS:
    generate_is_expression(vm, dest, &expression->is);
    return;
  }

  UNREACHABLE("Unhandled expression");
}

static void generate_expression_statement(CyVM* vm, ExprStmt* statement)
{
  MIR_reg_t temp = 0;

  MIR_type_t type = data_type_to_mir_type(statement->data_type);
  if (type != MIR_T_UNDEF)
    temp = _MIR_new_temp_reg(vm->ctx, type, vm->function->u.func);

  generate_expression(vm, temp, statement->expr);
}

static void generate_if_statement(CyVM* vm, IfStmt* statement)
{
  MIR_reg_t condition =
    _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)), vm->function->u.func);
  generate_expression(vm, condition, statement->condition);

  MIR_label_t cont_label = MIR_new_label(vm->ctx);
  MIR_label_t if_false_label = MIR_new_label(vm->ctx);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_BFS, MIR_new_label_op(vm->ctx, if_false_label),
                               MIR_new_reg_op(vm->ctx, condition)));

  generate_statements(vm, &statement->then_branch);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, cont_label)));

  MIR_append_insn(vm->ctx, vm->function, if_false_label);

  if (statement->else_branch.elems)
    generate_statements(vm, &statement->else_branch);

  MIR_append_insn(vm->ctx, vm->function, cont_label);
}

static void generate_while_statement(CyVM* vm, WhileStmt* statement)
{
  MIR_label_t previous_continue_label = vm->continue_label;
  MIR_label_t previous_break_label = vm->break_label;

  vm->continue_label = MIR_new_label(vm->ctx);
  vm->break_label = MIR_new_label(vm->ctx);

  MIR_label_t loop_label = MIR_new_label(vm->ctx);

  generate_statements(vm, &statement->initializer);

  MIR_append_insn(vm->ctx, vm->function, loop_label);

  MIR_reg_t condition =
    _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)), vm->function->u.func);
  generate_expression(vm, condition, statement->condition);
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_BFS, MIR_new_label_op(vm->ctx, vm->break_label),
                               MIR_new_reg_op(vm->ctx, condition)));

  generate_statements(vm, &statement->body);

  MIR_append_insn(vm->ctx, vm->function, vm->continue_label);

  generate_statements(vm, &statement->incrementer);

  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, loop_label)));

  MIR_append_insn(vm->ctx, vm->function, vm->break_label);

  vm->continue_label = previous_continue_label;
  vm->break_label = previous_break_label;
}

static void generate_return_statement(CyVM* vm, ReturnStmt* statement)
{
  if (statement->expr)
  {
    MIR_reg_t temp = _MIR_new_temp_reg(
      vm->ctx, sized_mir_type_to_mir_type(*vm->function->u.func->res_types), vm->function->u.func);
    generate_expression(vm, temp, statement->expr);

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, temp)));
  }
  else
  {
    MIR_append_insn(vm->ctx, vm->function, MIR_new_ret_insn(vm->ctx, 0));
  }
}

static void generate_continue_statement(CyVM* vm)
{
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, vm->continue_label)));
}

static void generate_break_statement(CyVM* vm)
{
  MIR_append_insn(vm->ctx, vm->function,
                  MIR_new_insn(vm->ctx, MIR_JMP, MIR_new_label_op(vm->ctx, vm->break_label)));
}

static void generate_variable_declaration(CyVM* vm, VarStmt* statement)
{
  if (statement->scope == SCOPE_GLOBAL)
  {
    if (statement->initializer)
    {
      MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, ptr),
                                   MIR_new_ref_op(vm->ctx, statement->item)));
      MIR_reg_t initializer = _MIR_new_temp_reg(
        vm->ctx, data_type_to_mir_type(statement->data_type), vm->function->u.func);
      generate_expression(vm, initializer, statement->initializer);

      MIR_append_insn(
        vm->ctx, vm->function,
        MIR_new_insn(
          vm->ctx, data_type_to_mov_type(statement->data_type),
          MIR_new_mem_op(vm->ctx, data_type_to_mir_type(statement->data_type), 0, ptr, 0, 1),
          MIR_new_reg_op(vm->ctx, initializer)));
    }
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    if (statement->initializer)
    {
      generate_expression(vm, statement->reg, statement->initializer);
    }
    else
    {
      generate_default_initialization(vm, statement->reg, statement->data_type);
    }
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void generate_function_declaration(CyVM* vm, FuncStmt* statement)
{
  if (statement->import)
    return;

  if (statement->item == NULL || statement->proto == NULL)
    init_function_declaration(vm, statement);

  MIR_item_t previous_function = vm->function;
  vm->function = statement->item;

  MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
  MIR_set_curr_func(vm->ctx, vm->function->u.func);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    variable->reg =
      MIR_new_func_reg(vm->ctx, vm->function->u.func, data_type_to_mir_type(variable->data_type),
                       memory_sprintf("%s.%d", variable->name.lexeme, variable->index));
  }

  generate_statements(vm, &statement->body);

  if (statement->data_type.type == TYPE_VOID)
    MIR_append_insn(vm->ctx, vm->function, MIR_new_ret_insn(vm->ctx, 0));

  MIR_finish_func(vm->ctx);
  MIR_set_curr_func(vm->ctx, previous_func);
  vm->function = previous_function;
}

static void generate_function_template_declaration(CyVM* vm, FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    generate_function_declaration(vm, function_declaration);
  }
}

static void generate_class_declaration(CyVM* vm, ClassStmt* statement)
{
  ArrayFuncStmt initializer_functions = {
    .size = 0,
    .cap = 1,
    .elems = alloca(sizeof(FuncStmt)),
  };

  FuncStmt* function;
  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name_raw.lexeme, "__init__") == 0)
      array_add(&initializer_functions, function);

    generate_function_declaration(vm, function);
  }

  FuncTemplateStmt* function_template;
  array_foreach(&statement->function_templates, function_template)
  {
    generate_function_template_declaration(vm, function_template);
  }

  unsigned int index = 0;

  do
  {
    FuncStmt* initializer_function =
      initializer_functions.size ? initializer_functions.elems[index] : NULL;

    MIR_item_t previous_function = vm->function;
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);

    if (initializer_function)
    {
      vm->function = initializer_function->item_prototype;
      MIR_set_curr_func(vm->ctx, vm->function->u.func);
    }
    else
    {
      vm->function = statement->default_constructor->item_prototype;
      MIR_set_curr_func(vm->ctx, vm->function->u.func);
    }

    bool contains_pointers = false;
    VarStmt* variable;
    array_foreach(&statement->variables, variable)
    {
      if (data_type_is_pointer(variable->data_type))
      {
        contains_pointers = true;
        break;
      }
    }

    MIR_reg_t ptr = MIR_reg(vm->ctx, "this.0", vm->function->u.func);

    if (contains_pointers)
      generate_malloc_expression(vm, ptr, MIR_new_int_op(vm->ctx, statement->size));
    else
      generate_malloc_atomic_expression(vm, ptr, MIR_new_int_op(vm->ctx, statement->size));

    array_foreach(&statement->variables, variable)
    {
      MIR_reg_t initializer = _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(variable->data_type),
                                                vm->function->u.func);

      generate_default_initialization(vm, initializer, variable->data_type);

      MIR_append_insn(vm->ctx, vm->function,
                      MIR_new_insn(vm->ctx, data_type_to_mov_type(variable->data_type),
                                   generate_object_field_op(vm, variable, ptr),
                                   MIR_new_reg_op(vm->ctx, initializer)));
    }

    array_foreach(&statement->variables, variable)
    {
      if (variable->initializer)
      {
        MIR_reg_t initializer = _MIR_new_temp_reg(
          vm->ctx, data_type_to_mir_type(variable->data_type), vm->function->u.func);

        generate_expression(vm, initializer, variable->initializer);

        MIR_append_insn(
          vm->ctx, vm->function,
          generate_debug_info(variable->name,
                              MIR_new_insn(vm->ctx, data_type_to_mov_type(variable->data_type),
                                           generate_object_field_op(vm, variable, ptr),
                                           MIR_new_reg_op(vm->ctx, initializer))));
      }
    }

    if (initializer_function)
    {
      ArrayMIR_op_t arguments;
      array_init(&arguments);

      array_add(&arguments, MIR_new_ref_op(vm->ctx, initializer_function->proto));
      array_add(&arguments, MIR_new_ref_op(vm->ctx, initializer_function->item));

      for (unsigned int i = 0; i < initializer_function->parameters.size; i++)
      {
        VarStmt* parameter = array_at(&initializer_function->parameters, i);
        const char* name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);

        MIR_reg_t var_reg = MIR_reg(vm->ctx, name, vm->function->u.func);
        array_add(&arguments, MIR_new_reg_op(vm->ctx, var_reg));
      }

      MIR_append_insn(vm->ctx, vm->function,
                      generate_debug_info(
                        initializer_function->name,
                        MIR_new_insn_arr(vm->ctx, MIR_INLINE, arguments.size, arguments.elems)));
    }

    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_ret_insn(vm->ctx, 1, MIR_new_reg_op(vm->ctx, ptr)));

    index++;

    MIR_finish_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, previous_func);
    vm->function = previous_function;
  } while (index < initializer_functions.size);
}

static void generate_class_template_declaration(CyVM* vm, ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    generate_class_declaration(vm, class_declaration);
  }
}

static void generate_import_declaration(CyVM* vm, ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    generate_statement(vm, body_statement);
  }
}

static void generate_statement(CyVM* vm, Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    generate_expression_statement(vm, &statement->expr);
    return;
  case STMT_IF:
    generate_if_statement(vm, &statement->cond);
    return;
  case STMT_WHILE:
    generate_while_statement(vm, &statement->loop);
    return;
  case STMT_RETURN:
    generate_return_statement(vm, &statement->ret);
    return;
  case STMT_CONTINUE:
    generate_continue_statement(vm);
    return;
  case STMT_BREAK:
    generate_break_statement(vm);
    return;
  case STMT_VARIABLE_DECL:
    generate_variable_declaration(vm, &statement->var);
    return;
  case STMT_FUNCTION_DECL:
    generate_function_declaration(vm, &statement->func);
    return;
  case STMT_IMPORT_DECL:
    generate_import_declaration(vm, &statement->import);
    return;
  case STMT_CLASS_DECL:
    generate_class_declaration(vm, &statement->class);
    return;
  case STMT_CLASS_TEMPLATE_DECL:
    generate_class_template_declaration(vm, &statement->class_template);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    generate_function_template_declaration(vm, &statement->func_template);
    return;
  }

  UNREACHABLE("Unhandled statement");
}

static void generate_statements(CyVM* vm, ArrayStmt* statements)
{
  Stmt* statement;
  array_foreach(statements, statement)
  {
    generate_statement(vm, statement);
  }
}

static void init_function_declaration(CyVM* vm, FuncStmt* statement)
{
  MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
  MIR_set_curr_func(vm->ctx, NULL);

  ArrayMIR_var_t vars;
  array_init(&vars);

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    MIR_var_t var;
    var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
    var.type = statement->import ? data_type_to_sized_mir_type(parameter->data_type)
                                 : data_type_to_mir_type(parameter->data_type);

    array_add(&vars, var);
  }

  MIR_type_t res_types[] = { statement->import ? data_type_to_sized_mir_type(statement->data_type)
                                               : data_type_to_mir_type(statement->data_type) };

  statement->proto =
    MIR_new_proto_arr(vm->ctx, memory_sprintf("%s.proto", statement->name.lexeme),
                      statement->data_type.type != TYPE_VOID, res_types, vars.size, vars.elems);

  if (statement->import)
  {
    statement->item = MIR_new_import(vm->ctx, statement->name.lexeme);
    MIR_load_external(vm->ctx, statement->name.lexeme, (uintptr_t)statement->import);
  }
  else
  {
    statement->item =
      MIR_new_func_arr(vm->ctx, statement->name.lexeme, statement->data_type.type != TYPE_VOID,
                       res_types, vars.size, vars.elems);

    array_foreach(&statement->parameters, parameter)
    {
      parameter->reg = MIR_reg(vm->ctx, vars.elems[_i].name, statement->item->u.func);
    }
  }

  MIR_set_curr_func(vm->ctx, previous_func);
}

static void init_function_template_declaration(CyVM* vm, FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    init_function_declaration(vm, function_declaration);
  }
}

static void init_class_declaration(CyVM* vm, ClassStmt* statement)
{
  ArrayFuncStmt initializer_functions = {
    .size = 0,
    .cap = 1,
    .elems = alloca(sizeof(FuncStmt)),
  };

  FuncStmt* function;
  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name_raw.lexeme, "__init__") == 0)
      array_add(&initializer_functions, function);

    init_function_declaration(vm, function);
  }

  FuncTemplateStmt* function_template;
  array_foreach(&statement->function_templates, function_template)
  {
    init_function_template_declaration(vm, function_template);
  }

  unsigned int index = 0;

  do
  {
    MIR_func_t previous_func = MIR_get_curr_func(vm->ctx);
    MIR_set_curr_func(vm->ctx, NULL);

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
      vm->ctx, initalizer_name, 1, (MIR_type_t[]){ data_type_to_mir_type(DATA_TYPE(TYPE_OBJECT)) },
      vars.size, vars.elems);

    MIR_item_t proto = MIR_new_proto_arr(
      vm->ctx, memory_sprintf("%s.proto", initalizer_name), 1,
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

    MIR_set_curr_func(vm->ctx, previous_func);
    index++;

  } while (index < initializer_functions.size);
}

static void init_import_declaration(CyVM* vm, ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    init_statement(vm, body_statement);
  }
}

static void init_class_template_declaration(CyVM* vm, ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    init_class_declaration(vm, class_declaration);
  }
}

static void init_variable_declaration(CyVM* vm, VarStmt* statement)
{
  if (statement->scope == SCOPE_GLOBAL)
  {
    uint64_t init = 0;
    statement->item = MIR_new_data(
      vm->ctx,
      memory_sprintf("%s.%s", statement->name.lexeme, data_type_to_string(statement->data_type)),
      data_type_to_mir_type(statement->data_type), 1, &init);

    MIR_reg_t ptr = _MIR_new_temp_reg(vm->ctx, MIR_T_I64, vm->function->u.func);
    MIR_append_insn(vm->ctx, vm->function,
                    MIR_new_insn(vm->ctx, MIR_MOV, MIR_new_reg_op(vm->ctx, ptr),
                                 MIR_new_ref_op(vm->ctx, statement->item)));

    MIR_reg_t initializer =
      _MIR_new_temp_reg(vm->ctx, data_type_to_mir_type(statement->data_type), vm->function->u.func);
    generate_default_initialization(vm, initializer, statement->data_type);

    MIR_append_insn(
      vm->ctx, vm->function,
      MIR_new_insn(
        vm->ctx, data_type_to_mov_type(statement->data_type),
        MIR_new_mem_op(vm->ctx, data_type_to_mir_type(statement->data_type), 0, ptr, 0, 1),
        MIR_new_reg_op(vm->ctx, initializer)));
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void init_statement(CyVM* vm, Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_VARIABLE_DECL:
    init_variable_declaration(vm, &statement->var);
    return;
  case STMT_FUNCTION_DECL:
    init_function_declaration(vm, &statement->func);
    return;
  case STMT_IMPORT_DECL:
    init_import_declaration(vm, &statement->import);
    return;
  case STMT_CLASS_DECL:
    init_class_declaration(vm, &statement->class);
    return;
  case STMT_CLASS_TEMPLATE_DECL:
    init_class_template_declaration(vm, &statement->class_template);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    init_function_template_declaration(vm, &statement->func_template);
    return;
  default:
    return;
  }
}

static void init_statements(CyVM* vm, ArrayStmt* statements)
{
  Stmt* statement;
  array_foreach(statements, statement)
  {
    init_statement(vm, statement);
  }
}

CyVM* cyth_init(void)
{
  CyVM* vm = malloc(sizeof(CyVM));
  vm->ctx = MIR_init();
  vm->module = MIR_new_module(vm->ctx, "main");
  vm->function = MIR_new_func(vm->ctx, "<start>", 0, 0, 0);
  vm->continue_label = NULL;
  vm->break_label = NULL;
  vm->jmp = NULL;
  vm->logging = 0;
  vm->error_callback = NULL;
  vm->panic_callback = NULL;
  array_init(&vm->statements);

  MIR_load_external(vm->ctx, "panic", (uintptr_t)panic);
  vm->panic.proto = MIR_new_proto_arr(vm->ctx, "panic.proto", 0, NULL, 4,
                                      (MIR_var_t[]){
                                        { .name = "vm", .size = 0, .type = MIR_T_I64 },
                                        { .name = "what", .size = 0, .type = MIR_T_I64 },
                                        { .name = "pc", .size = 0, .type = MIR_T_I64 },
                                        { .name = "fp", .size = 0, .type = MIR_T_I64 },
                                      });
  vm->panic.func = MIR_new_import(vm->ctx, "panic");

  MIR_load_external(vm->ctx, "malloc", (uintptr_t)GC_malloc);
  vm->malloc.proto =
    MIR_new_proto_arr(vm->ctx, "malloc.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->malloc.func = MIR_new_import(vm->ctx, "malloc");

  MIR_load_external(vm->ctx, "malloc_atomic", (uintptr_t)GC_malloc_atomic);
  vm->malloc_atomic.proto =
    MIR_new_proto_arr(vm->ctx, "malloc_atomic.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->malloc_atomic.func = MIR_new_import(vm->ctx, "malloc_atomic");

  MIR_load_external(vm->ctx, "realloc", (uintptr_t)GC_realloc);
  vm->realloc.proto =
    MIR_new_proto_arr(vm->ctx, "realloc.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 2,
                      (MIR_var_t[]){ { .name = "ptr", .size = 0, .type = MIR_T_I64 },
                                     { .name = "size", .size = 0, .type = MIR_T_I64 } });
  vm->realloc.func = MIR_new_import(vm->ctx, "realloc");

  MIR_load_external(vm->ctx, "memcpy", (uintptr_t)memcpy);
  vm->memcpy.proto =
    MIR_new_proto_arr(vm->ctx, "memcpy.proto", 0, (MIR_type_t[]){ MIR_T_I64 }, 3,
                      (MIR_var_t[]){ { .name = "dest", .size = 0, .type = MIR_T_I64 },
                                     { .name = "soruce", .size = 0, .type = MIR_T_I64 },
                                     { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->memcpy.func = MIR_new_import(vm->ctx, "memcpy");

  MIR_load_external(vm->ctx, "memmove", (uintptr_t)memmove);
  vm->memmove.proto =
    MIR_new_proto_arr(vm->ctx, "memmove.proto", 0, (MIR_type_t[]){ MIR_T_I64 }, 3,
                      (MIR_var_t[]){ { .name = "dest", .size = 0, .type = MIR_T_I64 },
                                     { .name = "soruce", .size = 0, .type = MIR_T_I64 },
                                     { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->memmove.func = MIR_new_import(vm->ctx, "memmove");

  MIR_load_external(vm->ctx, "string.equals", (uintptr_t)string_equals);
  vm->string_equals.proto =
    MIR_new_proto_arr(vm->ctx, "string.equals.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 2,
                      (MIR_var_t[]){ { .name = "left", .size = 0, .type = MIR_T_I64 },
                                     { .name = "right", .size = 0, .type = MIR_T_I64 } });
  vm->string_equals.func = MIR_new_import(vm->ctx, "string.equals");

  MIR_load_external(vm->ctx, "string.bool_cast", (uintptr_t)string_bool_cast);
  vm->string_bool_cast.proto =
    MIR_new_proto_arr(vm->ctx, "string.bool_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->string_bool_cast.func = MIR_new_import(vm->ctx, "string.bool_cast");

  MIR_load_external(vm->ctx, "string.int_cast", (uintptr_t)string_int_cast);
  vm->string_int_cast.proto =
    MIR_new_proto_arr(vm->ctx, "string.int_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->string_int_cast.func = MIR_new_import(vm->ctx, "string.int_cast");

  MIR_load_external(vm->ctx, "string.float_cast", (uintptr_t)string_float_cast);
  vm->string_float_cast.proto =
    MIR_new_proto_arr(vm->ctx, "string.float_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_F } });
  vm->string_float_cast.func = MIR_new_import(vm->ctx, "string.float_cast");

  MIR_load_external(vm->ctx, "string.char_cast", (uintptr_t)string_char_cast);
  vm->string_char_cast.proto =
    MIR_new_proto_arr(vm->ctx, "string.char_cast.proto", 1, (MIR_type_t[]){ MIR_T_I64 }, 1,
                      (MIR_var_t[]){ { .name = "n", .size = 0, .type = MIR_T_I64 } });
  vm->string_char_cast.func = MIR_new_import(vm->ctx, "string.char_cast");

  map_init_function(&vm->functions, 0, 0);
  map_init_mir_item(&vm->string_constants, 0, 0);
  map_init_mir_item(&vm->items, 0, 0);
  map_init_s64(&vm->typeids, 0, 0);

  return vm;
}

void cyth_set_error_callback(CyVM* vm,
                             void (*error_callback)(int start_line, int start_column, int end_line,
                                                    int end_column, const char* message))
{
  vm->error_callback = error_callback;
}

void cyth_set_panic_callback(CyVM* vm,
                             void (*panic_callback)(const char* function, int line, int column))
{
  vm->panic_callback = panic_callback;
}

void cyth_set_logging(CyVM* vm, int logging)
{
  vm->logging = logging;
}

int cyth_load_function(CyVM* vm, const char* signature, uintptr_t func)
{
  lexer_init((char*)signature, vm->error_callback);
  ArrayToken tokens = lexer_scan();

  if (lexer_errors())
    return false;

  parser_init(tokens, vm->error_callback);
  Stmt* statement = parser_parse_import_function_declaration_statement((void*)func);

  if (parser_errors() || statement == NULL)
    return false;

  array_add(&vm->statements, statement);
  return true;
}

int cyth_load_string(CyVM* vm, char* string)
{
  lexer_init(string, vm->error_callback);
  ArrayToken tokens = lexer_scan();

  if (lexer_errors())
    return false;

  parser_init(tokens, vm->error_callback);
  ArrayStmt statements = parser_parse();

  if (parser_errors())
    return false;

  Stmt* statement;
  array_foreach(&statements, statement)
  {
    array_add(&vm->statements, statement);
  }

  return true;
}

int cyth_load_file(CyVM* vm, const char* filename)
{
  bool result = false;

  FILE* file = fopen(filename, "rb");
  if (!file)
    goto clean_up;

  if (fseek(file, 0, SEEK_END) != 0)
    goto clean_up;

  long size = ftell(file);
  if (size < 0)
    goto clean_up;

  rewind(file);

  char* string = memory_alloc(size + 1);
  if (!string)
    goto clean_up;

  size_t read_size = fread(string, 1, size, file);
  if (read_size != (size_t)size)
    goto clean_up;

  string[size] = '\0';
  result = cyth_load_string(vm, string);

clean_up:
  fclose(file);
  return result;
}

int cyth_compile(CyVM* vm)
{
  checker_init(vm->statements, vm->error_callback, NULL);
  checker_validate();

  bool result = !checker_errors();
  if (result)
  {
    VarStmt* global_local;
    ArrayVarStmt global_local_statements = checker_global_locals();
    array_foreach(&global_local_statements, global_local)
    {
      global_local->reg = MIR_new_func_reg(
        vm->ctx, vm->function->u.func, data_type_to_mir_type(global_local->data_type),
        memory_sprintf("%s.%d", global_local->name.lexeme, global_local->index));
    }

    init_statements(vm, &vm->statements);
    generate_statements(vm, &vm->statements);
  }

  MIR_append_insn(vm->ctx, vm->function, MIR_new_ret_insn(vm->ctx, 0));
  MIR_finish_func(vm->ctx);
  MIR_finish_module(vm->ctx);

  if (vm->logging)
    MIR_output(vm->ctx, stdout);

  MIR_load_module(vm->ctx, vm->module);
  MIR_gen_init(vm->ctx);
  MIR_gen_set_optimize_level(vm->ctx, 3);
  MIR_link(vm->ctx, MIR_set_gen_interface, NULL);

  vm->start = (Start)MIR_gen(vm->ctx, vm->function);

  GC_set_no_dls(true);

  for (MIR_item_t item = DLIST_HEAD(MIR_item_t, vm->module->items); item != NULL;
       item = DLIST_NEXT(MIR_item_t, item))
  {
    if (item->item_type != MIR_data_item)
      continue;

    if (item->u.data->el_type != MIR_T_I64)
      continue;

    GC_add_roots(item->addr, (char*)item->addr + sizeof(uintptr_t));
  }

  memory_reset();
  return result;
}

void cyth_run(CyVM* vm)
{
  cyth_try_catch(vm, { vm->start(); });
}

void cyth_destroy(CyVM* vm)
{
  for (MIR_item_t item = DLIST_HEAD(MIR_item_t, vm->module->items); item != NULL;
       item = DLIST_NEXT(MIR_item_t, item))
  {
    if (item->item_type != MIR_data_item)
      continue;

    if (item->u.data->el_type != MIR_T_I64)
      continue;

    GC_remove_roots(item->addr, (char*)item->addr + sizeof(uintptr_t));
  }

  MIR_gen_finish(vm->ctx);
  MIR_finish(vm->ctx);
  free(vm);
}

void* cyth_alloc(int atomic, uintptr_t size)
{
  return atomic ? GC_malloc_atomic(size) : GC_malloc(size);
}

uintptr_t cyth_get_function(CyVM* vm, const char* name)
{
  for (MIR_item_t item = DLIST_HEAD(MIR_item_t, vm->module->items); item != NULL;
       item = DLIST_NEXT(MIR_item_t, item))
  {
    if (item->item_type != MIR_func_item)
      continue;

    if (strcmp(name, item->u.func->name) == 0)
      return MIR_gen(vm->ctx, item);
  }

  return 0;
}

uintptr_t cyth_get_variable(CyVM* vm, const char* name)
{
  for (MIR_item_t item = DLIST_HEAD(MIR_item_t, vm->module->items); item != NULL;
       item = DLIST_NEXT(MIR_item_t, item))
  {
    if (item->item_type != MIR_data_item)
      continue;

    if (strcmp(name, item->u.func->name) == 0)
      return (uintptr_t)item->addr;
  }

  return 0;
}

uintptr_t sig_fp;
CyVM* sig_vm;

static void panic(CyVM* vm, const char* what, uintptr_t pc, uintptr_t fp)
{
  if (vm->panic_callback)
    vm->panic_callback(what, 0, 0);

  for (MIR_item_t item = DLIST_TAIL(MIR_item_t, vm->module->items); item != NULL;
       item = DLIST_PREV(MIR_item_t, item))
  {
    if (item->item_type != MIR_func_item)
      continue;

    uintptr_t offset = 0;

    for (MIR_insn_t insn = DLIST_HEAD(MIR_insn_t, item->u.func->insns); insn != NULL;
         insn = DLIST_NEXT(MIR_insn_t, insn))
    {
      uintptr_t ptr = (uintptr_t)item->u.func->machine_code + offset;
      if (pc >= ptr && pc < ptr + insn->size)
      {
        if (insn->line && insn->column)
          if (vm->panic_callback)
            vm->panic_callback(item->u.func->name, insn->line, insn->column);
      }

      offset += insn->size;
    }
  }

  if (!fp)
  {
#if defined(__clang__) || defined(__GNUC__)
    fp = (uintptr_t)__builtin_frame_address(0);
#elif defined(_MSC_VER)
    fp = (uintptr_t)_AddressOfReturnAddress() - 8;
#endif
  }

  uintptr_t sig_fp_min = (uintptr_t)alloca(sizeof(uintptr_t));

  while (fp >= sig_fp_min && fp <= sig_fp)
  {
    uintptr_t pc = *(uintptr_t*)(fp + sizeof(uintptr_t));

    for (MIR_item_t item = DLIST_TAIL(MIR_item_t, vm->module->items); item != NULL;
         item = DLIST_PREV(MIR_item_t, item))
    {
      if (item->item_type != MIR_func_item)
        continue;

      uintptr_t offset = 0;

      for (MIR_insn_t insn = DLIST_HEAD(MIR_insn_t, item->u.func->insns); insn != NULL;
           insn = DLIST_NEXT(MIR_insn_t, insn))
      {
        offset += insn->size;

        uintptr_t ptr = (uintptr_t)item->u.func->machine_code + offset;
        if (pc >= ptr && pc < ptr + insn->size)
        {
          if (insn->line && insn->column)
            if (vm->panic_callback)
              vm->panic_callback(item->u.func->name, insn->line, insn->column);
        }
      }
    }

    fp = *(uintptr_t*)fp;
  }

  if (vm->jmp == NULL)
  {
    fprintf(stderr, "Panic was not caught, terminating program!\n");
    exit(-1);
  }

  cyth_longjmp(*vm->jmp, 1);
}

#ifdef _WIN32
PVOID handler;
static LONG WINAPI vector_handler(EXCEPTION_POINTERS* ExceptionInfo)
{
  uintptr_t pc = 0;
  uintptr_t fp = 0;

#ifdef _M_ARM64
  pc = ExceptionInfo->ContextRecord->Pc ? ExceptionInfo->ContextRecord->Pc
                                        : ExceptionInfo->ContextRecord->Lr - 4;
  fp = ExceptionInfo->ContextRecord->Fp;
#else
  pc = ExceptionInfo->ContextRecord->Rip ? ExceptionInfo->ContextRecord->Rip
                                         : (*(uintptr_t*)ExceptionInfo->ContextRecord->Rsp) - 2;
  fp = ExceptionInfo->ContextRecord->Rbp;
#endif

  switch (ExceptionInfo->ExceptionRecord->ExceptionCode)
  {
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    panic(sig_vm, "Division by zero", pc, fp);
    return EXCEPTION_CONTINUE_EXECUTION;

  case EXCEPTION_STACK_OVERFLOW:
    panic(sig_vm, "Stack overflow", pc, fp);
    return EXCEPTION_CONTINUE_EXECUTION;

  case EXCEPTION_ACCESS_VIOLATION:
    panic(sig_vm, "Invalid memory or null pointer access", pc, fp);
    return EXCEPTION_CONTINUE_EXECUTION;

  default:
    return EXCEPTION_CONTINUE_SEARCH;
  }
}
#else
static void sig_handler(int sig, siginfo_t* si, void* ctx)
{
  ucontext_t* uc = (ucontext_t*)ctx;
  uintptr_t pc = 0;
  uintptr_t fp = 0;

#if defined(__linux__) && defined(__x86_64__)
  pc = uc->uc_mcontext.gregs[REG_RIP] ? (uintptr_t)uc->uc_mcontext.gregs[REG_RIP]
                                      : (*(uintptr_t*)uc->uc_mcontext.gregs[REG_RSP]) - 2;
  fp = uc->uc_mcontext.gregs[REG_RBP];
#elif defined(__linux__) && defined(__aarch64__)
  pc = uc->uc_mcontext.pc ? uc->uc_mcontext.pc : uc->uc_mcontext.regs[30] - 4;
  fp = uc->uc_mcontext.regs[29];
#elif defined(__APPLE__) && defined(__x86_64__)
  pc = uc->uc_mcontext->__ss.__rip ? uc->uc_mcontext->__ss.__rip
                                   : (*(uintptr_t*)uc->uc_mcontext->__ss.__rsp) - 2;
  fp = uc->uc_mcontext->__ss.__rbp;
#elif defined(__APPLE__) && defined(__aarch64__)
  pc = uc->uc_mcontext->__ss.__pc ? uc->uc_mcontext->__ss.__pc : uc->uc_mcontext->__ss.__lr - 4;
  fp = uc->uc_mcontext->__ss.__fp;
#endif

  if (sig == SIGSEGV)
  {
    void* stack_base;
    size_t stack_size;

#if defined(__APPLE__)
    stack_size = pthread_get_stacksize_np(pthread_self());
    void* stack_addr = pthread_get_stackaddr_np(pthread_self());

    int stack_variable;
    if (stack_addr > (void*)&stack_variable)
      stack_base = (uint8_t*)stack_addr - stack_size;
    else
      stack_base = stack_addr;
#else
    pthread_attr_t attributes;
    pthread_getattr_np(pthread_self(), &attributes);
    pthread_attr_getstack(&attributes, &stack_base, &stack_size);
    pthread_attr_destroy(&attributes);
#endif

    uint8_t* fault = si->si_addr;

    if (fault < (uint8_t*)stack_base && fault >= (uint8_t*)stack_base - stack_size)
      panic(sig_vm, "Stack overflow", pc, fp);
    else if (fault < (uint8_t*)0xffff)
      panic(sig_vm, "Invalid memory or null pointer access", pc, fp);
    else
      panic(sig_vm, "Internal runtime error", pc, fp);
  }
  else if (sig == SIGFPE)
  {
    panic(sig_vm, "Division by zero", pc, fp);
  }
}
#endif

void* cyth_push_jmp(CyVM* vm, void* new)
{
  jmp_buf* old = vm->jmp;
  vm->jmp = new;

  if (!old)
  {
#ifndef _WIN32
    static char stack[SIGSTKSZ * 2];
    stack_t ss = {
      .ss_size = SIGSTKSZ * 2,
      .ss_sp = stack,
    };
    sigaltstack(&ss, NULL);

    struct sigaction sa = { 0 };
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sa.sa_sigaction = sig_handler;
    sigaction(SIGSEGV, &sa, NULL);

    sa.sa_sigaction = sig_handler;
    sigaction(SIGFPE, &sa, NULL);
#else
    ULONG size = 1024 * 1024;
    SetThreadStackGuarantee(&size);

    handler = AddVectoredExceptionHandler(1, vector_handler);
#endif

#if defined(__clang__) || defined(__GNUC__)
    sig_fp = (uintptr_t)__builtin_frame_address(0);
#elif defined(_MSC_VER)
    sig_fp = (uintptr_t)_AddressOfReturnAddress() - 8;
#endif

    sig_vm = vm;
  }

  return old;
}

void cyth_pop_jmp(CyVM* vm, void* old)
{
  vm->jmp = old;

  if (!old)
  {
#ifdef _WIN32
    RemoveVectoredExceptionHandler(handler);
    _resetstkoflw();
#else
    sigaction(SIGSEGV, NULL, NULL);
    sigaction(SIGFPE, NULL, NULL);
#endif

    sig_fp = 0;
    sig_vm = NULL;
  }
}

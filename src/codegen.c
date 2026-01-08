#include "codegen.h"
#include "array.h"
#include "checker.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "parser.h"
#include "statement.h"

#include <binaryen-c.h>
#include <math.h>

typedef struct
{
  const char* function;
  Token token;
  BinaryenExpressionRef expression;
} DebugInfo;

typedef struct
{
  enum
  {
    SUBTYPE_ARRAY,
    SUBTYPE_CLASS,
    SUBTYPE_FUNCTION,
  } type;

  unsigned int index;

  union {
    unsigned int hash;
    const char* key;
    ClassStmt* class;
  };
} TypeBuilderSubtype;

array_def(BinaryenExpressionRef, BinaryenExpressionRef);
array_def(BinaryenPackedType, BinaryenPackedType);
array_def(BinaryenHeapType, BinaryenHeapType);
array_def(BinaryenType, BinaryenType);
array_def(TypeBuilderSubtype, TypeBuilderSubtype);
array_def(DebugInfo, DebugInfo);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statement(Stmt* statement);
static BinaryenExpressionRef generate_statements(ArrayStmt* statements);
static BinaryenExpressionRef generate_class_declaration(ClassStmt* statement,
                                                        TypeBuilderRef type_builder_ref,
                                                        ArrayTypeBuilderSubtype* subtypes);

static const char* generate_string_concat_function(int count);
static const char* generate_string_equals_function(void);
static const char* generate_array_push_function(DataType this_data_type, DataType value_data_type);

static BinaryenExpressionRef generate_default_initialization(DataType data_type);
static BinaryenExpressionRef generate_string_cast_function(DataType data_type,
                                                           DataType list_data_type,
                                                           BinaryenExpressionRef value,
                                                           BinaryenExpressionRef depth,
                                                           BinaryenExpressionRef list);
static BinaryenExpressionRef generate_string_literal_expression(const char* literal, int length);

static BinaryenPackedType data_type_to_binaryen_packed_type(DataType data_type);
static BinaryenType data_type_to_binaryen_type(DataType data_type);
static BinaryenType data_type_to_temporary_binaryen_type(TypeBuilderRef type_builder_ref,
                                                         ArrayTypeBuilderSubtype* subtypes,
                                                         DataType data_type);

static struct
{
  BinaryenModuleRef module;
  BinaryenType class;
  ArrayStmt statements;

  ArrayBinaryenType global_local_types;
  MapStringBinaryenHeapType heap_types;
  BinaryenHeapType string_heap_type;
  BinaryenType string_type;
  MapSInt string_constants;
  ArrayDebugInfo debug_info;

  const char* function;
  int strings;
  int loop;

  void (*result_callback)(size_t size, void* data, size_t source_map_size, void* source_map);
} codegen;

static const char* get_function_member(DataType data_type, const char* name)
{
  ClassStmt* class = data_type.class;
  VarStmt* variable = map_get_var_stmt(class->members, name);

  if (!variable || variable->data_type.type != TYPE_FUNCTION_MEMBER)
    return NULL;

  FuncStmt* function = variable->data_type.function_member.function;
  return function->name.lexeme;
}

static void generate_debug_info(Token token, BinaryenExpressionRef expression, const char* function)
{
  DebugInfo debug_info = { .expression = expression, .token = token, .function = function };
  array_add(&codegen.debug_info, debug_info);
}

static const char* generate_string_pad_function(void)
{
#define COUNTER() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define CHAR() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define RESULT() (BinaryenLocalGet(codegen.module, 2, codegen.string_type))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.pad";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef loop_body_list[] = {
      BinaryenBreak(codegen.module, "string.pad.block",
                    BinaryenBinary(codegen.module, BinaryenEqInt32(), COUNTER(), CONSTANT(0)),
                    NULL),

      BinaryenLocalSet(codegen.module, 0,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), COUNTER(), CONSTANT(1))),

      BinaryenArraySet(codegen.module, RESULT(), COUNTER(), CHAR()),

      BinaryenBreak(codegen.module, "string.pad.loop", NULL, NULL),
    };

    BinaryenExpressionRef loop_body =
      BinaryenBlock(codegen.module, "string.pad.block", loop_body_list,
                    sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());
    BinaryenExpressionRef loop = BinaryenLoop(codegen.module, "string.pad.loop", loop_body);

    BinaryenExpressionRef body_list[] = {
      BinaryenLocalSet(codegen.module, 2,
                       BinaryenArrayNew(codegen.module, codegen.string_heap_type, COUNTER(), NULL)),

      loop,

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, &codegen.string_type, 1, body);
  }

  return name;

#undef COUNTER
#undef CHAR
#undef RESULT
#undef CONSTANT
}

static const char* generate_string_bool_cast_function(void)
{
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.bool_cast";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef value = BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32());
    BinaryenExpressionRef body =
      BinaryenSelect(codegen.module, value, generate_string_literal_expression("true", -1),
                     generate_string_literal_expression("false", -1));

    BinaryenType params_list[] = { BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef CONSTANT
}

static const char* generate_string_float_cast_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeFloat32()))
#define INT_NUMBER() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt64()))
#define FLOAT_NUMBER() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define BUFFER() (BinaryenLocalGet(codegen.module, 3, codegen.string_type))
#define INDEX() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define RESULT() (BinaryenLocalGet(codegen.module, 5, codegen.string_type))
#define DECIMAL_COUNT() (BinaryenLocalGet(codegen.module, 6, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))
#define CONSTANTL(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt64(_v)))
#define CONSTANTF(_v) (BinaryenConst(codegen.module, BinaryenLiteralFloat32(_v)))

  const char* name = "string.float_cast";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    const int size = 64;
    const int decimals = 6;
    const int base = 10;

    BinaryenExpressionRef decimal_loop;
    {
      BinaryenExpressionRef divider =
        BinaryenBinary(codegen.module, BinaryenRemUInt32(), FLOAT_NUMBER(), CONSTANT(base));
      BinaryenExpressionRef adder =
        BinaryenBinary(codegen.module, BinaryenAddInt32(), divider, CONSTANT('0'));

      BinaryenExpressionRef adder_if_body_list[] = {
        BinaryenLocalSet(codegen.module, 4,
                         BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
        BinaryenArraySet(codegen.module, BUFFER(), INDEX(), adder)
      };
      BinaryenExpressionRef adder_if_body = BinaryenBlock(
        codegen.module, NULL, adder_if_body_list,
        sizeof(adder_if_body_list) / sizeof_ptr(adder_if_body_list), BinaryenTypeNone());

      BinaryenExpressionRef adder_if_condition =
        BinaryenSelect(codegen.module, BinaryenExpressionCopy(divider, codegen.module), CONSTANT(1),
                       BinaryenBinary(codegen.module, BinaryenNeInt32(), INDEX(), CONSTANT(size)));

      BinaryenExpressionRef adder_if =
        BinaryenIf(codegen.module, adder_if_condition, adder_if_body, NULL);

      BinaryenExpressionRef loop_body_list[] = {
        adder_if,
        BinaryenLocalSet(
          codegen.module, 2,
          BinaryenBinary(codegen.module, BinaryenDivUInt32(), FLOAT_NUMBER(), CONSTANT(base))),
        BinaryenLocalSet(
          codegen.module, 6,
          BinaryenBinary(codegen.module, BinaryenSubInt32(), DECIMAL_COUNT(), CONSTANT(1))),
        BinaryenBreak(
          codegen.module, "string.float_cast.loop",
          BinaryenBinary(codegen.module, BinaryenNeInt32(), DECIMAL_COUNT(), CONSTANT(0)), NULL),
      };
      BinaryenExpressionRef loop_body =
        BinaryenBlock(codegen.module, NULL, loop_body_list,
                      sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());

      decimal_loop = BinaryenLoop(codegen.module, "string.float_cast.loop", loop_body);
    }

    BinaryenExpressionRef dot_append;
    {
      BinaryenExpressionRef dot_append_body_list[] = {
        BinaryenLocalSet(codegen.module, 4,
                         BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
        BinaryenArraySet(codegen.module, BUFFER(), INDEX(), CONSTANT('.'))
      };
      BinaryenExpressionRef dot_append_body = BinaryenBlock(
        codegen.module, NULL, dot_append_body_list,
        sizeof(dot_append_body_list) / sizeof_ptr(dot_append_body_list), BinaryenTypeNone());
      dot_append = BinaryenIf(
        codegen.module, BinaryenBinary(codegen.module, BinaryenNeInt32(), INDEX(), CONSTANT(size)),
        dot_append_body, NULL);
    }

    BinaryenExpressionRef integer_loop;
    {
      BinaryenExpressionRef divider =
        BinaryenBinary(codegen.module, BinaryenRemUInt64(), INT_NUMBER(), CONSTANTL(base));
      BinaryenExpressionRef adder =
        BinaryenBinary(codegen.module, BinaryenAddInt32(),
                       BinaryenUnary(codegen.module, BinaryenWrapInt64(), divider), CONSTANT('0'));

      BinaryenExpressionRef adder_if_body_list[] = {
        BinaryenLocalSet(codegen.module, 4,
                         BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
        BinaryenArraySet(codegen.module, BUFFER(), INDEX(), adder)
      };
      BinaryenExpressionRef adder_if_body = BinaryenBlock(
        codegen.module, NULL, adder_if_body_list,
        sizeof(adder_if_body_list) / sizeof_ptr(adder_if_body_list), BinaryenTypeNone());

      BinaryenExpressionRef loop_body_list[] = {
        adder_if_body,
        BinaryenLocalSet(
          codegen.module, 1,
          BinaryenBinary(codegen.module, BinaryenDivUInt64(), INT_NUMBER(), CONSTANTL(base))),
        BinaryenBreak(codegen.module, "string.int_cast.loop",
                      BinaryenBinary(codegen.module, BinaryenNeInt64(), INT_NUMBER(), CONSTANTL(0)),
                      NULL),
      };
      BinaryenExpressionRef loop_body =
        BinaryenBlock(codegen.module, NULL, loop_body_list,
                      sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());

      integer_loop = BinaryenLoop(codegen.module, "string.int_cast.loop", loop_body);
    }

    BinaryenExpressionRef minus_append;
    {
      BinaryenExpressionRef minus_append_body_list[] = {
        BinaryenLocalSet(codegen.module, 4,
                         BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
        BinaryenArraySet(codegen.module, BUFFER(), INDEX(), CONSTANT('-'))
      };
      BinaryenExpressionRef minus_append_body = BinaryenBlock(
        codegen.module, NULL, minus_append_body_list,
        sizeof(minus_append_body_list) / sizeof_ptr(minus_append_body_list), BinaryenTypeNone());
      minus_append =
        BinaryenIf(codegen.module,
                   BinaryenBinary(codegen.module, BinaryenLtFloat32(), INPUT(), CONSTANTF(0.0f)),
                   minus_append_body, NULL);
    }

    BinaryenExpressionRef inf_exit;
    {
      inf_exit =
        BinaryenIf(codegen.module,
                   BinaryenBinary(codegen.module, BinaryenEqFloat32(),
                                  BinaryenUnary(codegen.module, BinaryenAbsFloat32(), INPUT()),
                                  CONSTANTF(INFINITY)),
                   BinaryenReturn(codegen.module,
                                  BinaryenSelect(codegen.module,
                                                 BinaryenBinary(codegen.module, BinaryenLtFloat32(),
                                                                INPUT(), CONSTANTF(0.0f)),
                                                 generate_string_literal_expression("-inf", -1),
                                                 generate_string_literal_expression("inf", -1))),
                   NULL);
    }

    BinaryenExpressionRef nan_exit;
    {
      nan_exit = BinaryenIf(
        codegen.module, BinaryenBinary(codegen.module, BinaryenNeFloat32(), INPUT(), INPUT()),
        BinaryenReturn(
          codegen.module,
          BinaryenSelect(
            codegen.module,
            BinaryenBinary(codegen.module, BinaryenShrUInt32(),
                           BinaryenUnary(codegen.module, BinaryenReinterpretFloat32(), INPUT()),
                           CONSTANT(31)),
            generate_string_literal_expression("-nan", -1),
            generate_string_literal_expression("nan", -1))),
        NULL);
    }

    BinaryenExpressionRef body_list[] = {
      inf_exit,
      nan_exit,

      BinaryenLocalSet(codegen.module, 1,
                       BinaryenUnary(codegen.module, BinaryenTruncSatSFloat32ToInt64(),
                                     BinaryenUnary(codegen.module, BinaryenAbsFloat32(), INPUT()))),
      BinaryenLocalSet(
        codegen.module, 2,
        BinaryenUnary(
          codegen.module, BinaryenTruncSatSFloat32ToInt32(),
          BinaryenBinary(
            codegen.module, BinaryenMulFloat32(), CONSTANTF(1000000.0f),
            BinaryenBinary(
              codegen.module, BinaryenSubFloat32(),
              BinaryenUnary(codegen.module, BinaryenAbsFloat32(), INPUT()),
              BinaryenUnary(codegen.module, BinaryenConvertSInt64ToFloat32(), INT_NUMBER()))))),
      BinaryenLocalSet(
        codegen.module, 3,
        BinaryenArrayNew(codegen.module, codegen.string_heap_type, CONSTANT(size), NULL)),
      BinaryenLocalSet(codegen.module, 4, CONSTANT(size)),
      BinaryenLocalSet(codegen.module, 6, CONSTANT(decimals)),

      decimal_loop,
      dot_append,
      integer_loop,
      minus_append,

      BinaryenLocalSet(codegen.module, 5,
                       BinaryenArrayNew(codegen.module, codegen.string_heap_type,
                                        BinaryenBinary(codegen.module, BinaryenSubInt32(),
                                                       CONSTANT(size), INDEX()),
                                        NULL)),
      BinaryenArrayCopy(codegen.module, RESULT(), CONSTANT(0), BUFFER(), INDEX(),
                        BinaryenArrayLen(codegen.module, RESULT())),
      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { BinaryenTypeFloat32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = {
      BinaryenTypeInt64(), BinaryenTypeInt32(), codegen.string_type,
      BinaryenTypeInt32(), codegen.string_type, BinaryenTypeInt32(),
    };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INT_NUMBER
#undef FLOAT_NUMBER
#undef BUFFER
#undef INDEX
#undef RESULT
#undef CONSTANT
#undef CONSTANTF
}

static const char* generate_string_index_of_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(_v) (BinaryenLocalSet(codegen.module, 2, _v))
#define J() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define J_SET(_v) (BinaryenLocalSet(codegen.module, 3, _v))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define TARGET_LEN() (BinaryenArrayLen(codegen.module, TARGET()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.index_of";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef inner_loop;
    {
      BinaryenExpressionRef block_list[] = {
        BinaryenIf(codegen.module,
                   BinaryenBinary(
                     codegen.module, BinaryenNeInt32(),
                     BinaryenArrayGet(codegen.module, INPUT(),
                                      BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), J()),
                                      codegen.string_type, false),
                     BinaryenArrayGet(codegen.module, TARGET(), J(), codegen.string_type, false)),
                   BinaryenBreak(codegen.module, "string.index_of.outer_loop.continue", NULL, NULL),
                   NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.index_of.inner_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), J(), TARGET_LEN()), NULL),

        block,

        J_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), J(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.index_of.inner_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.index_of.inner_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        J_SET(CONSTANT(0)),
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.index_of.inner_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      inner_loop = break_block;
    }

    BinaryenExpressionRef outer_loop;
    {
      BinaryenExpressionRef block_list[] = {
        inner_loop,
        BinaryenReturn(codegen.module, I()),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, "string.index_of.outer_loop.continue", block_list,
                      sizeof(block_list) / sizeof_ptr(block_list), BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.index_of.outer_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGtSInt32(), I(),
                                     BinaryenBinary(codegen.module, BinaryenSubInt32(), INPUT_LEN(),
                                                    TARGET_LEN())),
                      NULL),

        block,

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.index_of.outer_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.index_of.outer_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.index_of.outer_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      outer_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                 BinaryenReturn(codegen.module, CONSTANT(0)), NULL),

      outer_loop,

      CONSTANT(-1),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef TARGET
#undef I
#undef I_SET
#undef J
#undef J_SET
#undef INPUT_LEN
#undef TARGET_LEN
#undef CONSTANT
}

static const char* generate_string_count_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(_v) (BinaryenLocalSet(codegen.module, 2, _v))
#define J() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define J_SET(_v) (BinaryenLocalSet(codegen.module, 3, _v))
#define COUNTER() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define COUNTER_SET(_v) (BinaryenLocalSet(codegen.module, 4, _v))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define TARGET_LEN() (BinaryenArrayLen(codegen.module, TARGET()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.count";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef inner_loop;
    {
      BinaryenExpressionRef block_list[] = {
        BinaryenIf(codegen.module,
                   BinaryenBinary(
                     codegen.module, BinaryenNeInt32(),
                     BinaryenArrayGet(codegen.module, INPUT(),
                                      BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), J()),
                                      codegen.string_type, false),
                     BinaryenArrayGet(codegen.module, TARGET(), J(), codegen.string_type, false)),
                   BinaryenBreak(codegen.module, "string.count.outer_loop.continue", NULL, NULL),
                   NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.count.inner_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), J(), TARGET_LEN()), NULL),

        block,

        J_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), J(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.count.inner_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.count.inner_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        J_SET(CONSTANT(0)),
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.count.inner_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      inner_loop = break_block;
    }

    BinaryenExpressionRef outer_loop;
    {
      BinaryenExpressionRef block_list[] = {
        inner_loop,
        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),
        BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                   I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
                   I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), TARGET_LEN()))),

        BinaryenBreak(codegen.module, "string.count.outer_loop", NULL, NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, "string.count.outer_loop.continue", block_list,
                      sizeof(block_list) / sizeof_ptr(block_list), BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.count.outer_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGtSInt32(), I(),
                                     BinaryenBinary(codegen.module, BinaryenSubInt32(), INPUT_LEN(),
                                                    TARGET_LEN())),
                      NULL),

        block,

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.count.outer_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.count.outer_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.count.outer_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      outer_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {

      outer_loop,

      COUNTER(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef TARGET
#undef I
#undef I_SET
#undef J
#undef J_SET
#undef COUNTER
#undef COUNTER_SET
#undef INPUT_LEN
#undef TARGET_LEN
#undef CONSTANT
}

static const char* generate_string_replace_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define INPUT_GET(_i) (BinaryenArrayGet(codegen.module, INPUT(), (_i), BinaryenTypeInt32(), false))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define REPLACE() (BinaryenLocalGet(codegen.module, 2, codegen.string_type))
#define I() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define I_SET(_v) (BinaryenLocalSet(codegen.module, 3, _v))
#define J() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define J_SET(_v) (BinaryenLocalSet(codegen.module, 4, _v))
#define COUNTER() (BinaryenLocalGet(codegen.module, 5, BinaryenTypeInt32()))
#define COUNTER_SET(_v) (BinaryenLocalSet(codegen.module, 5, _v))
#define RESULT() (BinaryenLocalGet(codegen.module, 6, codegen.string_type))
#define RESULT_SET(_v) (BinaryenLocalSet(codegen.module, 6, _v))
#define RESULT_PUT(_i, _v) (BinaryenArraySet(codegen.module, RESULT(), (_i), (_v)))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define TARGET_LEN() (BinaryenArrayLen(codegen.module, TARGET()))
#define REPLACE_LEN() (BinaryenArrayLen(codegen.module, REPLACE()))
#define RESULT_LEN() (BinaryenArrayLen(codegen.module, RESULT()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.replace";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef inner_loop;
    {
      BinaryenExpressionRef block_list[] = {
        BinaryenIf(codegen.module,
                   BinaryenBinary(
                     codegen.module, BinaryenNeInt32(),
                     BinaryenArrayGet(codegen.module, INPUT(),
                                      BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), J()),
                                      codegen.string_type, false),
                     BinaryenArrayGet(codegen.module, TARGET(), J(), codegen.string_type, false)),
                   BinaryenBreak(codegen.module, "string.replace.outer_loop.continue", NULL, NULL),
                   NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.replace.inner_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), J(), TARGET_LEN()), NULL),
        BinaryenBreak(codegen.module, "string.replace.outer_loop.continue",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(),
                                     BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), J()),
                                     INPUT_LEN()),
                      NULL),

        block,

        J_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), J(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.replace.inner_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.replace.inner_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        J_SET(CONSTANT(0)),
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.replace.inner_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      inner_loop = break_block;
    }

    BinaryenExpressionRef outer_loop;
    {
      BinaryenExpressionRef block_list[] = {
        inner_loop,

        BinaryenArrayCopy(codegen.module, RESULT(), COUNTER(), REPLACE(), CONSTANT(0),
                          REPLACE_LEN()),
        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), REPLACE_LEN())),

        BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                   BinaryenIf(codegen.module,
                              BinaryenBinary(codegen.module, BinaryenLtSInt32(), I(), INPUT_LEN()),
                              RESULT_PUT(COUNTER(), INPUT_GET(I())), NULL),
                   NULL),

        BinaryenIf(
          codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
          COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),
          NULL),

        BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                   I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
                   I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), TARGET_LEN()))),

        BinaryenBreak(codegen.module, "string.replace.outer_loop", NULL, NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, "string.replace.outer_loop.continue", block_list,
                      sizeof(block_list) / sizeof_ptr(block_list), BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.replace.outer_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGtSInt32(), COUNTER(), RESULT_LEN()),
                      NULL),

        block,

        BinaryenIf(codegen.module,
                   BinaryenBinary(codegen.module, BinaryenLtSInt32(), I(), INPUT_LEN()),
                   RESULT_PUT(COUNTER(), INPUT_GET(I())), NULL),
        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),
        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),

        BinaryenBreak(codegen.module, "string.replace.outer_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.replace.outer_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        COUNTER_SET(CONSTANT(0)),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.replace.outer_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      outer_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module,
                 BinaryenCall(codegen.module, generate_string_equals_function(),
                              (BinaryenExpressionRef[]){ TARGET(), REPLACE() }, 2,
                              BinaryenTypeInt32()),
                 BinaryenReturn(codegen.module, INPUT()), NULL),

      COUNTER_SET(BinaryenCall(codegen.module, generate_string_count_function(),
                               (BinaryenExpressionRef[]){ INPUT(), TARGET() }, 2,
                               BinaryenTypeInt32())),
      BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), COUNTER()),
                 BinaryenReturn(codegen.module, INPUT()), NULL),

      RESULT_SET(BinaryenArrayNew(
        codegen.module, codegen.string_heap_type,
        BinaryenBinary(codegen.module, BinaryenAddInt32(), INPUT_LEN(),
                       BinaryenBinary(codegen.module, BinaryenMulInt32(),
                                      BinaryenBinary(codegen.module, BinaryenSubInt32(),
                                                     REPLACE_LEN(), TARGET_LEN()),
                                      COUNTER())),
        CONSTANT(0))),

      outer_loop,

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(),
                                 codegen.string_type };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_GET
#undef TARGET
#undef REPLACE
#undef I
#undef I_SET
#undef J
#undef J_SET
#undef COUNTER
#undef COUNTER_SET
#undef RESULT
#undef RESULT_SET
#undef RESULT_PUT
#undef INPUT_LEN
#undef TARGET_LEN
#undef REPLACE_LEN
#undef RESULT_LEN
#undef CONSTANT
}

static const char* generate_string_split_function(DataType string_array_data_type)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define DELIM() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define DELIM_LEN() (BinaryenArrayLen(codegen.module, DELIM()))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(value) (BinaryenLocalSet(codegen.module, 2, (value)))
#define J() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define J_SET(value) (BinaryenLocalSet(codegen.module, 3, (value)))
#define COUNTER() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define COUNTER_SET(value) (BinaryenLocalSet(codegen.module, 4, (value)))
#define TEMP() (BinaryenLocalGet(codegen.module, 5, codegen.string_type))
#define TEMP_SET(value) (BinaryenLocalSet(codegen.module, 5, (value)))
#define RESULT() (BinaryenLocalGet(codegen.module, 6, string_array_type))
#define RESULT_SET(value) (BinaryenLocalSet(codegen.module, 6, (value)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.split";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType string_array_type = data_type_to_binaryen_type(string_array_data_type);

    BinaryenExpressionRef append;
    {
      BinaryenExpressionRef append_list[] = {
        TEMP_SET(
          BinaryenArrayNew(codegen.module, codegen.string_heap_type, COUNTER(), CONSTANT(0))),

        BinaryenArrayCopy(codegen.module, TEMP(), CONSTANT(0), INPUT(),
                          BinaryenBinary(codegen.module, BinaryenSubInt32(), I(), COUNTER()),
                          COUNTER()),
        BinaryenCall(codegen.module,
                     generate_array_push_function(string_array_data_type, DATA_TYPE(TYPE_STRING)),
                     (BinaryenExpressionRef[]){ RESULT(), TEMP() }, 2, BinaryenTypeNone()),
      };
      append = BinaryenBlock(codegen.module, NULL, append_list,
                             sizeof(append_list) / sizeof_ptr(append_list), BinaryenTypeNone());
    }

    BinaryenExpressionRef inner_loop;
    {
      BinaryenExpressionRef block_list[] = {
        BinaryenIf(codegen.module,
                   BinaryenBinary(
                     codegen.module, BinaryenNeInt32(),
                     BinaryenArrayGet(codegen.module, INPUT(),
                                      BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), J()),
                                      codegen.string_type, false),
                     BinaryenArrayGet(codegen.module, DELIM(), J(), codegen.string_type, false)),
                   BinaryenBreak(codegen.module, "string.split.loop.continue", NULL, NULL), NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.split.inner_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), J(), DELIM_LEN()), NULL),

        block,

        J_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), J(), CONSTANT(1))),

        BinaryenBreak(codegen.module, "string.split.inner_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.split.inner_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        BinaryenBreak(
          codegen.module, "string.split.loop.continue",
          BinaryenBinary(codegen.module, BinaryenGtSInt32(),
                         BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), DELIM_LEN()),
                         INPUT_LEN()),
          NULL),
        J_SET(CONSTANT(0)),
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.split.inner_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      inner_loop = break_block;
    }

    BinaryenExpressionRef loop;
    {
      BinaryenExpressionRef block_list[] = {
        inner_loop,
        append,

        COUNTER_SET(CONSTANT(0)),
        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), DELIM_LEN())),

        BinaryenBreak(codegen.module, "string.split.loop", DELIM_LEN(), NULL),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, "string.split.loop.continue", block_list,
                      sizeof(block_list) / sizeof_ptr(block_list), BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.split.loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), I(), INPUT_LEN()), NULL),

        block,

        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),
        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),

        BinaryenBreak(codegen.module, "string.split.loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop_body =
        BinaryenLoop(codegen.module, "string.split.loop", loop_block);

      BinaryenExpressionRef break_block_list[] = { loop_body };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.split.loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      RESULT_SET(generate_default_initialization(string_array_data_type)),

      BinaryenIf(codegen.module, DELIM_LEN(), BinaryenNop(codegen.module), I_SET(CONSTANT(1))),
      BinaryenIf(codegen.module, DELIM_LEN(), BinaryenNop(codegen.module),
                 COUNTER_SET(CONSTANT(1))),

      loop,

      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenLeSInt32(), I(), INPUT_LEN()),
                 BinaryenExpressionCopy(append, codegen.module), NULL),

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    string_array_type);

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { string_array_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(),
                                 codegen.string_type, string_array_type };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_LEN
#undef DELIM
#undef DELIM_LEN
#undef I
#undef I_SET
#undef J
#undef J_SET
#undef COUNTER
#undef COUNTER_SET
#undef TEMP
#undef TEMP_SET
#undef RESULT
#undef RESULT_SET
#undef CONSTANT
}

static const char* generate_string_join_function(DataType string_array_data_type)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, string_array_type))
#define INPUT_LEN() (BinaryenStructGet(codegen.module, 1, INPUT(), BinaryenTypeInt32(), false))
#define INPUT_GET(index)                                                                           \
  (BinaryenArrayGet(codegen.module,                                                                \
                    BinaryenStructGet(codegen.module, 0, INPUT(), array_type, false), (index),     \
                    BinaryenTypeInt32(), false))
#define DELIM() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define DELIM_LEN() (BinaryenArrayLen(codegen.module, DELIM()))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(value) (BinaryenLocalSet(codegen.module, 2, (value)))
#define COUNTER() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define COUNTER_SET(value) (BinaryenLocalSet(codegen.module, 3, (value)))
#define TEMP() (BinaryenLocalGet(codegen.module, 4, codegen.string_type))
#define TEMP_SET(value) (BinaryenLocalSet(codegen.module, 4, (value)))
#define RESULT() (BinaryenLocalGet(codegen.module, 5, codegen.string_type))
#define RESULT_SET(value) (BinaryenLocalSet(codegen.module, 5, (value)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.join";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType string_array_type = data_type_to_binaryen_type(string_array_data_type);
    BinaryenType array_type = BinaryenExpressionGetType(
      BinaryenStructGet(codegen.module, 0, INPUT(), BinaryenTypeAuto(), false));

    BinaryenExpressionRef calculate_loop;
    {
      BinaryenExpressionRef block_list[] = {
        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(),
                                   BinaryenArrayLen(codegen.module, INPUT_GET(I())))),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.join.calculate_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), I(), INPUT_LEN()), NULL),

        block,

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.join.calculate_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.join.calculate_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),
        COUNTER_SET(BinaryenBinary(
          codegen.module, BinaryenMulInt32(),
          BinaryenBinary(codegen.module, BinaryenSubInt32(), INPUT_LEN(), CONSTANT(1)),
          DELIM_LEN())),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.join.calculate_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      calculate_loop = break_block;
    }

    BinaryenExpressionRef populate_loop;
    {
      BinaryenExpressionRef block_list[] = {
        BinaryenArrayCopy(codegen.module, RESULT(), COUNTER(), DELIM(), CONSTANT(0), DELIM_LEN()),
        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), DELIM_LEN())),
      };
      BinaryenExpressionRef block =
        BinaryenBlock(codegen.module, NULL, block_list, sizeof(block_list) / sizeof_ptr(block_list),
                      BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.join.populate_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), I(), INPUT_LEN()), NULL),

        TEMP_SET(INPUT_GET(I())),
        BinaryenArrayCopy(codegen.module, RESULT(), COUNTER(), TEMP(), CONSTANT(0),
                          BinaryenArrayLen(codegen.module, TEMP())),

        COUNTER_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(),
                                   BinaryenArrayLen(codegen.module, TEMP()))),

        BinaryenIf(
          codegen.module,
          BinaryenBinary(codegen.module, BinaryenLtSInt32(),
                         BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1)),
                         INPUT_LEN()),
          block, NULL),

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.join.populate_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.join.populate_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),
        COUNTER_SET(CONSTANT(0)),
        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.join.populate_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      populate_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(
        codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), INPUT_LEN()),
        BinaryenReturn(codegen.module, generate_default_initialization(DATA_TYPE(TYPE_STRING))),
        NULL),

      calculate_loop,

      RESULT_SET(
        BinaryenArrayNew(codegen.module, codegen.string_heap_type, COUNTER(), CONSTANT(0))),

      populate_loop,

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { string_array_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32(), codegen.string_type,
                                 codegen.string_type };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_LEN
#undef INPUT_GET
#undef DELIM
#undef DELIM_LEN
#undef I
#undef I_SET
#undef COUNTER
#undef COUNTER_SET
#undef TEMP
#undef TEMP_SET
#undef RESULT
#undef RESULT_SET
#undef CONSTANT
}

static const char* generate_string_trim_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define INPUT_GET(_i) (BinaryenArrayGet(codegen.module, INPUT(), (_i), BinaryenTypeInt32(), false))
#define I() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define I_SET(value) (BinaryenLocalSet(codegen.module, 1, (value)))
#define LEFT() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define LEFT_SET(value) (BinaryenLocalSet(codegen.module, 2, (value)))
#define RIGHT() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define RIGHT_SET(value) (BinaryenLocalSet(codegen.module, 3, (value)))
#define RESULT() (BinaryenLocalGet(codegen.module, 4, codegen.string_type))
#define RESULT_SET(value) (BinaryenLocalSet(codegen.module, 4, (value)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.trim";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef left_loop;
    {
      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.trim.left_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), LEFT(), INPUT_LEN()),
                      NULL),

        I_SET(INPUT_GET(LEFT())),
        BinaryenBreak(
          codegen.module, "string.trim.left_loop.break",
          BinaryenSelect(
            codegen.module, BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT(' ')),
            BinaryenSelect(
              codegen.module,
              BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT('\t')),
              BinaryenSelect(codegen.module,
                             BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT('\n')),
                             BinaryenSelect(codegen.module,
                                            BinaryenBinary(codegen.module, BinaryenNeInt32(), I(),
                                                           CONSTANT('\r')),
                                            CONSTANT(1), CONSTANT(0)),
                             CONSTANT(0)),
              CONSTANT(0)),
            CONSTANT(0)),
          NULL),

        LEFT_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), LEFT(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.trim.left_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.trim.left_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),
        LEFT_SET(CONSTANT(0)),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.trim.left_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      left_loop = break_block;
    }

    BinaryenExpressionRef right_loop;
    {
      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.trim.right_loop.break",
                      BinaryenBinary(codegen.module, BinaryenLtSInt32(), RIGHT(), LEFT()), NULL),

        I_SET(INPUT_GET(RIGHT())),
        BinaryenBreak(
          codegen.module, "string.trim.right_loop.break",
          BinaryenSelect(
            codegen.module, BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT(' ')),
            BinaryenSelect(
              codegen.module,
              BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT('\t')),
              BinaryenSelect(codegen.module,
                             BinaryenBinary(codegen.module, BinaryenNeInt32(), I(), CONSTANT('\n')),
                             BinaryenSelect(codegen.module,
                                            BinaryenBinary(codegen.module, BinaryenNeInt32(), I(),
                                                           CONSTANT('\r')),
                                            CONSTANT(1), CONSTANT(0)),
                             CONSTANT(0)),
              CONSTANT(0)),
            CONSTANT(0)),
          NULL),

        RIGHT_SET(BinaryenBinary(codegen.module, BinaryenSubInt32(), RIGHT(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.trim.right_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.trim.right_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),
        RIGHT_SET(BinaryenBinary(codegen.module, BinaryenSubInt32(), INPUT_LEN(), CONSTANT(1))),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.trim.right_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      right_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      left_loop,
      right_loop,

      I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(),
                           BinaryenBinary(codegen.module, BinaryenSubInt32(), RIGHT(), LEFT()),
                           CONSTANT(1))),
      RESULT_SET(BinaryenArrayNew(codegen.module, codegen.string_heap_type, I(), CONSTANT(0))),
      BinaryenArrayCopy(codegen.module, RESULT(), CONSTANT(0), INPUT(), LEFT(), I()),

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(),
                                 codegen.string_type };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_LEN
#undef INPUT_GET
#undef I
#undef I_SET
#undef LEFT
#undef LEFT_SET
#undef RIGHT
#undef RIGHT_SET
#undef RESULT
#undef RESULT_SET
#undef CONSTANT
}

static const char* generate_string_starts_with_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define INPUT_GET(_i) (BinaryenArrayGet(codegen.module, INPUT(), (_i), BinaryenTypeInt32(), false))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define TARGET_LEN() (BinaryenArrayLen(codegen.module, TARGET()))
#define TARGET_GET(_i)                                                                             \
  (BinaryenArrayGet(codegen.module, TARGET(), (_i), BinaryenTypeInt32(), false))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(value) (BinaryenLocalSet(codegen.module, 2, (value)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.starts_with";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef check_loop;
    {
      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.starts_with.check_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), I(), TARGET_LEN()), NULL),

        BinaryenIf(
          codegen.module,
          BinaryenBinary(codegen.module, BinaryenNeInt32(), INPUT_GET(I()), TARGET_GET(I())),
          BinaryenReturn(codegen.module, CONSTANT(false)), NULL),

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.starts_with.check_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.starts_with.check_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.starts_with.check_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      check_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                 BinaryenReturn(codegen.module, CONSTANT(true)), NULL),
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGtSInt32(), TARGET_LEN(), INPUT_LEN()),
                 BinaryenReturn(codegen.module, CONSTANT(false)), NULL),

      check_loop,

      CONSTANT(true),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_LEN
#undef INPUT_GET
#undef TARGET
#undef TARGET_LEN
#undef TARGET_GET
#undef I
#undef I_SET
#undef CONSTANT
}

static const char* generate_string_ends_with_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define INPUT_LEN() (BinaryenArrayLen(codegen.module, INPUT()))
#define INPUT_GET(_i) (BinaryenArrayGet(codegen.module, INPUT(), (_i), BinaryenTypeInt32(), false))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define TARGET_LEN() (BinaryenArrayLen(codegen.module, TARGET()))
#define TARGET_GET(_i)                                                                             \
  (BinaryenArrayGet(codegen.module, TARGET(), (_i), BinaryenTypeInt32(), false))
#define I() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define I_SET(value) (BinaryenLocalSet(codegen.module, 2, (value)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.ends_with";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef check_loop;
    {
      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, "string.ends_with.check_loop.break",
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), I(), TARGET_LEN()), NULL),

        BinaryenIf(
          codegen.module,
          BinaryenBinary(
            codegen.module, BinaryenNeInt32(),
            INPUT_GET(BinaryenBinary(
              codegen.module, BinaryenSubInt32(),
              BinaryenBinary(codegen.module, BinaryenSubInt32(), INPUT_LEN(), I()), CONSTANT(1))),

            TARGET_GET(BinaryenBinary(
              codegen.module, BinaryenSubInt32(),
              BinaryenBinary(codegen.module, BinaryenSubInt32(), TARGET_LEN(), I()), CONSTANT(1)))),
          BinaryenReturn(codegen.module, CONSTANT(false)), NULL),

        I_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), I(), CONSTANT(1))),
        BinaryenBreak(codegen.module, "string.ends_with.check_loop", NULL, NULL),
      };
      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, NULL, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());
      BinaryenExpressionRef loop =
        BinaryenLoop(codegen.module, "string.ends_with.check_loop", loop_block);

      BinaryenExpressionRef break_block_list[] = {
        I_SET(CONSTANT(0)),

        loop,
      };
      BinaryenExpressionRef break_block =
        BinaryenBlock(codegen.module, "string.ends_with.check_loop.break", break_block_list,
                      sizeof(break_block_list) / sizeof_ptr(break_block_list), BinaryenTypeNone());

      check_loop = break_block;
    }

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module, BinaryenUnary(codegen.module, BinaryenEqZInt32(), TARGET_LEN()),
                 BinaryenReturn(codegen.module, CONSTANT(true)), NULL),
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGtSInt32(), TARGET_LEN(), INPUT_LEN()),
                 BinaryenReturn(codegen.module, CONSTANT(false)), NULL),

      check_loop,

      CONSTANT(true),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef INPUT
#undef INPUT_LEN
#undef INPUT_GET
#undef TARGET
#undef TARGET_LEN
#undef TARGET_GET
#undef I
#undef I_SET
#undef CONSTANT
}

static const char* generate_string_contains_function(void)
{
#define INPUT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define TARGET() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.contains";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenBinary(codegen.module, BinaryenNeInt32(),
                     BinaryenCall(codegen.module, generate_string_index_of_function(),
                                  (BinaryenExpressionRef[]){ INPUT(), TARGET() }, 2,
                                  BinaryenTypeInt32()),
                     CONSTANT(-1)),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef INPUT
#undef TARGET
#undef CONSTANT
}

static const char* generate_string_int_cast_function(void)
{
#define NUMBER() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define BUFFER() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define INDEX() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define SIZE() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define TEMP() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.int_cast";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    const int base = 10;

    BinaryenExpressionRef divider =
      BinaryenBinary(codegen.module, BinaryenRemUInt32(), NUMBER(), CONSTANT(base));
    BinaryenExpressionRef adder =
      BinaryenBinary(codegen.module, BinaryenAddInt32(), divider, CONSTANT('0'));

    BinaryenExpressionRef loop_body_list[] = {
      BinaryenLocalSet(codegen.module, 2,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
      BinaryenArraySet(codegen.module, BUFFER(), INDEX(), adder),
      BinaryenLocalSet(
        codegen.module, 0,
        BinaryenBinary(codegen.module, BinaryenDivUInt32(), NUMBER(), CONSTANT(base))),
      BinaryenBreak(codegen.module, "string.int_cast.loop",
                    BinaryenBinary(codegen.module, BinaryenNeInt32(), NUMBER(), CONSTANT(0)), NULL),
    };
    BinaryenExpressionRef loop_body =
      BinaryenBlock(codegen.module, NULL, loop_body_list,
                    sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());
    BinaryenExpressionRef loop = BinaryenLoop(codegen.module, "string.int_cast.loop", loop_body);

    BinaryenExpressionRef negative_check_body_list[] = {
      BinaryenLocalSet(codegen.module, 0,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), CONSTANT(0), NUMBER())),
      BinaryenLocalSet(codegen.module, 4, CONSTANT(1))
    };
    BinaryenExpressionRef negative_check_body = BinaryenBlock(
      codegen.module, NULL, negative_check_body_list,
      sizeof(negative_check_body_list) / sizeof_ptr(negative_check_body_list), BinaryenTypeNone());
    BinaryenExpressionRef negative_check = BinaryenIf(
      codegen.module, BinaryenBinary(codegen.module, BinaryenLtSInt32(), NUMBER(), CONSTANT(0)),
      negative_check_body, NULL);

    BinaryenExpressionRef negative_append_body_list[] = {
      BinaryenLocalSet(codegen.module, 2,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
      BinaryenArraySet(codegen.module, BUFFER(), INDEX(), CONSTANT('-'))
    };
    BinaryenExpressionRef negative_append_body =
      BinaryenBlock(codegen.module, NULL, negative_append_body_list,
                    sizeof(negative_append_body_list) / sizeof_ptr(negative_append_body_list),
                    BinaryenTypeNone());
    BinaryenExpressionRef negative_append = BinaryenIf(
      codegen.module, BinaryenBinary(codegen.module, BinaryenEqInt32(), INDEX(), CONSTANT(1)),
      negative_append_body, NULL);

    BinaryenExpressionRef size_cond1_body_list[] = {
      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(8))),
      BinaryenLocalSet(
        codegen.module, 4,
        BinaryenBinary(codegen.module, BinaryenDivUInt32(), TEMP(), CONSTANT(100000000))),
    };
    BinaryenExpressionRef size_cond1 = BinaryenBlock(
      codegen.module, NULL, size_cond1_body_list,
      sizeof(size_cond1_body_list) / sizeof_ptr(size_cond1_body_list), BinaryenTypeNone());

    BinaryenExpressionRef size_cond2_body_list[] = {
      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(4))),
      BinaryenLocalSet(
        codegen.module, 4,
        BinaryenBinary(codegen.module, BinaryenDivUInt32(), TEMP(), CONSTANT(10000))),
    };
    BinaryenExpressionRef size_cond2 = BinaryenBlock(
      codegen.module, NULL, size_cond2_body_list,
      sizeof(size_cond2_body_list) / sizeof_ptr(size_cond2_body_list), BinaryenTypeNone());

    BinaryenExpressionRef size_cond3_body_list[] = {
      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(2))),
      BinaryenLocalSet(codegen.module, 4,
                       BinaryenBinary(codegen.module, BinaryenDivUInt32(), TEMP(), CONSTANT(100))),
    };
    BinaryenExpressionRef size_cond3 = BinaryenBlock(
      codegen.module, NULL, size_cond3_body_list,
      sizeof(size_cond3_body_list) / sizeof_ptr(size_cond3_body_list), BinaryenTypeNone());

    BinaryenExpressionRef size_cond4_body_list[] = {
      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(1))),
    };
    BinaryenExpressionRef size_cond4 = BinaryenBlock(
      codegen.module, NULL, size_cond4_body_list,
      sizeof(size_cond4_body_list) / sizeof_ptr(size_cond4_body_list), BinaryenTypeNone());

    BinaryenExpressionRef size_body_list[] = {
      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), TEMP(), CONSTANT(1))),
      BinaryenLocalSet(codegen.module, 4, NUMBER()),

      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeUInt32(), TEMP(), CONSTANT(100000000)),
                 size_cond1, NULL),
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeUInt32(), TEMP(), CONSTANT(10000)),
                 size_cond2, NULL),
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeUInt32(), TEMP(), CONSTANT(100)),
                 size_cond3, NULL),
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeUInt32(), TEMP(), CONSTANT(10)),
                 size_cond4, NULL)
    };
    BinaryenExpressionRef size =
      BinaryenBlock(codegen.module, NULL, size_body_list,
                    sizeof(size_body_list) / sizeof_ptr(size_body_list), BinaryenTypeNone());

    BinaryenExpressionRef body_list[] = {
      negative_check,
      size,

      BinaryenLocalSet(codegen.module, 1,
                       BinaryenArrayNew(codegen.module, codegen.string_heap_type, SIZE(), NULL)),
      BinaryenLocalSet(codegen.module, 2, SIZE()),

      loop,
      negative_append,

      BUFFER()
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { codegen.string_type, BinaryenTypeInt32(), BinaryenTypeInt32(),
                                 BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef NUMBER
#undef BUFFER
#undef INDEX
#undef SIZE
#undef TEMP
#undef CONSTANT
}

static const char* generate_string_array_cast_function(DataType data_type, DataType list_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define DEPTH() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define LIST() (BinaryenLocalGet(codegen.module, 2, list_type))
#define RESULT() (BinaryenLocalGet(codegen.module, 3, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  BinaryenType this_type = data_type_to_binaryen_type(data_type);
  DataType element_data_type = array_data_type_element(data_type);
  bool multiline = element_data_type.type == TYPE_ARRAY || element_data_type.type == TYPE_OBJECT;

  const char* name = memory_sprintf("string.array_cast.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType element_type = data_type_to_binaryen_type(element_data_type);
    BinaryenType list_type = data_type_to_binaryen_type(list_data_type);

    BinaryenExpressionRef loop_body_list[] = {
      BinaryenBreak(codegen.module, "string.array_cast.block",
                    BinaryenBinary(codegen.module, BinaryenGeUInt32(), COUNTER(), SIZE()), NULL),

      BinaryenLocalSet(
        codegen.module, 3,
        BinaryenCall(
          codegen.module, generate_string_concat_function(4),
          (BinaryenExpressionRef[]){
            RESULT(),

            multiline ? BinaryenCall(codegen.module, generate_string_pad_function(),
                                     (BinaryenExpressionRef[]){
                                       BinaryenBinary(codegen.module, BinaryenAddInt32(), DEPTH(),
                                                      CONSTANT(1)),
                                       CONSTANT(' '),
                                     },
                                     2, codegen.string_type)
                      : generate_string_literal_expression("", -1),

            generate_string_cast_function(
              element_data_type, list_data_type,
              BinaryenArrayGet(codegen.module, ARRAY(), COUNTER(), element_type, false),
              BinaryenBinary(codegen.module, BinaryenAddInt32(), DEPTH(), CONSTANT(1)), LIST()),

            BinaryenIf(codegen.module,
                       BinaryenBinary(
                         codegen.module, BinaryenLtUInt32(), COUNTER(),
                         BinaryenBinary(codegen.module, BinaryenSubInt32(), SIZE(), CONSTANT(1))),
                       generate_string_literal_expression(multiline ? ",\n" : ", ", -1),
                       generate_string_literal_expression("", -1)),
          },
          4, codegen.string_type)),

      BinaryenLocalSet(codegen.module, 4,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),

      BinaryenBreak(codegen.module, "string.array_cast.loop", NULL, NULL),
    };

    BinaryenExpressionRef loop_body =
      BinaryenBlock(codegen.module, "string.array_cast.block", loop_body_list,
                    sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());
    BinaryenExpressionRef loop = BinaryenLoop(codegen.module, "string.array_cast.loop", loop_body);

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(
        codegen.module, BinaryenBinary(codegen.module, BinaryenEqInt32(), SIZE(), CONSTANT(0)),
        BinaryenReturn(codegen.module, generate_string_literal_expression("[]", -1)), NULL),

      BinaryenLocalSet(codegen.module, 3,
                       generate_string_literal_expression(multiline ? "[\n" : "[", -1)),

      loop,

      BinaryenLocalSet(
        codegen.module, 3,
        BinaryenCall(codegen.module, generate_string_concat_function(4),
                     (BinaryenExpressionRef[]){
                       RESULT(),

                       generate_string_literal_expression(multiline ? "\n" : "]", -1),

                       multiline ? BinaryenCall(codegen.module, generate_string_pad_function(),
                                                (BinaryenExpressionRef[]){ DEPTH(), CONSTANT(' ') },
                                                2, codegen.string_type)
                                 : generate_string_literal_expression("", -1),

                       generate_string_literal_expression(multiline ? "]" : "", -1),

                     },
                     4, codegen.string_type)),

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { this_type, BinaryenTypeInt32(), list_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { codegen.string_type, BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef DEPTH
#undef LIST
#undef RESULT
#undef COUNTER
#undef ARRAY
#undef CAPACITY
#undef SIZE
#undef CONSTANT
}

static const char* generate_string_object_cast_function(DataType data_type, DataType list_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define DEPTH() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define LIST() (BinaryenLocalGet(codegen.module, 2, list_type))
#define RESULT() (BinaryenLocalGet(codegen.module, 3, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, LIST(), BinaryenTypeAuto(), false))
#define SIZE() (BinaryenStructGet(codegen.module, 1, LIST(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  BinaryenType this_type = data_type_to_binaryen_type(data_type);
  BinaryenType list_type = data_type_to_binaryen_type(list_data_type);
  const char* name = memory_sprintf("string.object_cast.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    ClassStmt* class = data_type.class;

    BinaryenType params_list[] = { this_type, BinaryenTypeInt32(), list_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { codegen.string_type, BinaryenTypeInt32() };

    BinaryenFunctionRef func = BinaryenAddFunction(
      codegen.module, name, params, results, vars_list, sizeof(vars_list) / sizeof_ptr(vars_list),
      BinaryenBlock(codegen.module, NULL, NULL, 0, BinaryenTypeNone()));

    BinaryenExpressionRef loop_body_list[] = {
      BinaryenBreak(codegen.module, "string.object_cast.block",
                    BinaryenBinary(codegen.module, BinaryenGeUInt32(), COUNTER(), SIZE()), NULL),

      BinaryenIf(
        codegen.module,
        BinaryenRefEq(codegen.module, THIS(),
                      BinaryenRefCast(codegen.module,
                                      BinaryenArrayGet(codegen.module, ARRAY(), COUNTER(),
                                                       BinaryenTypeAnyref(), false),
                                      BinaryenTypeEqref())),
        BinaryenReturn(
          codegen.module,
          BinaryenCall(
            codegen.module, generate_string_concat_function(4),
            (BinaryenExpressionRef[]){
              generate_string_literal_expression(class->name.lexeme, class->name.length),

              BinaryenIf(codegen.module,
                         BinaryenBinary(codegen.module, BinaryenNeInt32(), COUNTER(), CONSTANT(0)),
                         generate_string_literal_expression("$", -1),
                         generate_string_literal_expression("", -1)),

              BinaryenIf(codegen.module,
                         BinaryenBinary(codegen.module, BinaryenNeInt32(), COUNTER(), CONSTANT(0)),
                         BinaryenCall(codegen.module, generate_string_int_cast_function(),
                                      (BinaryenExpressionRef[]){ COUNTER() }, 1,
                                      codegen.string_type),
                         generate_string_literal_expression("", -1)),

              generate_string_literal_expression("(...)", -1),
            },
            4, codegen.string_type)

            ),
        NULL),

      BinaryenLocalSet(codegen.module, 4,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),

      BinaryenBreak(codegen.module, "string.object_cast.loop", NULL, NULL),
    };

    BinaryenExpressionRef loop_body =
      BinaryenBlock(codegen.module, "string.object_cast.block", loop_body_list,
                    sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());
    BinaryenExpressionRef loop = BinaryenLoop(codegen.module, "string.object_cast.loop", loop_body);

    ArrayBinaryenExpressionRef list;
    array_init(&list);
    array_add(&list, generate_string_literal_expression(class->name.lexeme, class->name.length));
    array_add(&list,
              BinaryenIf(codegen.module,
                         BinaryenBinary(codegen.module, BinaryenNeInt32(), COUNTER(), CONSTANT(0)),
                         generate_string_literal_expression("$", -1),
                         generate_string_literal_expression("", -1)));
    array_add(
      &list,
      BinaryenIf(
        codegen.module, BinaryenBinary(codegen.module, BinaryenNeInt32(), COUNTER(), CONSTANT(0)),
        BinaryenCall(codegen.module, generate_string_int_cast_function(),
                     (BinaryenExpressionRef[]){
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), SIZE(), CONSTANT(1)),
                     },
                     1, codegen.string_type),
        generate_string_literal_expression("", -1)));

    array_add(&list, generate_string_literal_expression(class->variables.size ? "(\n" : "(", -1));

    VarStmt* var;
    array_foreach(&class->variables, var)
    {
      array_add(&list, BinaryenCall(
                         codegen.module, generate_string_pad_function(),
                         (BinaryenExpressionRef[]){
                           BinaryenBinary(codegen.module, BinaryenAddInt32(), DEPTH(), CONSTANT(1)),
                           CONSTANT(' '),
                         },
                         2, codegen.string_type));
      array_add(&list, generate_string_literal_expression(var->name.lexeme, var->name.length));
      array_add(&list, generate_string_literal_expression(" = ", -1));

      array_add(&list, generate_string_cast_function(
                         var->data_type, list_data_type,
                         BinaryenStructGet(codegen.module, var->index, THIS(),
                                           data_type_to_binaryen_type(var->data_type), false),
                         BinaryenBinary(codegen.module, BinaryenAddInt32(), DEPTH(), CONSTANT(1)),
                         LIST()));
      if (_i + 1 != class->variables.size)
        array_add(&list, generate_string_literal_expression(",\n", -1));
    }

    if (class->variables.size)
    {
      array_add(&list, generate_string_literal_expression("\n", -1));
      array_add(&list, BinaryenCall(codegen.module, generate_string_pad_function(),
                                    (BinaryenExpressionRef[]){ DEPTH(), CONSTANT(' ') }, 2,
                                    codegen.string_type));
    }

    array_add(&list, generate_string_literal_expression(")", -1));

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, THIS()),
                 BinaryenReturn(codegen.module, generate_string_literal_expression("null", -1)),
                 NULL),

      loop,

      BinaryenCall(codegen.module,
                   generate_array_push_function(list_data_type, DATA_TYPE(TYPE_ANY)),
                   (BinaryenExpressionRef[]){ LIST(), THIS() }, 2, BinaryenTypeNone()),

      BinaryenLocalSet(codegen.module, 3,
                       BinaryenCall(codegen.module, generate_string_concat_function(list.size),
                                    list.elems, list.size, codegen.string_type)),

      RESULT(),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenFunctionSetBody(func, body);
  }

  return name;

#undef THIS
#undef DEPTH
#undef LIST
#undef RESULT
#undef COUNTER
#undef ARRAY
#undef SIZE
#undef CONSTANT
}

static BinaryenExpressionRef generate_string_cast_function(DataType data_type,
                                                           DataType list_data_type,
                                                           BinaryenExpressionRef value,
                                                           BinaryenExpressionRef depth,
                                                           BinaryenExpressionRef list)
{
  switch (data_type.type)
  {
  case TYPE_STRING:
    return value;
  case TYPE_BOOL:
    return BinaryenCall(codegen.module, generate_string_bool_cast_function(), &value, 1,
                        codegen.string_type);
  case TYPE_FLOAT:
    return BinaryenCall(codegen.module, generate_string_float_cast_function(), &value, 1,
                        codegen.string_type);
  case TYPE_INTEGER:
    return BinaryenCall(codegen.module, generate_string_int_cast_function(), &value, 1,
                        codegen.string_type);
  case TYPE_CHAR:
    return BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, &value, 1);
  case TYPE_ARRAY:
    return BinaryenCall(codegen.module,
                        generate_string_array_cast_function(data_type, list_data_type),
                        (BinaryenExpressionRef[]){ value, depth, list }, 3, codegen.string_type);
  case TYPE_OBJECT: {
    const char* name = get_function_member(data_type, "__str__");
    if (name)
      return BinaryenCall(codegen.module, name, &value, 1, codegen.string_type);
    else
      return BinaryenCall(codegen.module,
                          generate_string_object_cast_function(data_type, list_data_type),
                          (BinaryenExpressionRef[]){
                            value,
                            depth,
                            list,
                          },
                          3, codegen.string_type);
  }

  case TYPE_ANY:
  case TYPE_FUNCTION_POINTER: {
    const char* name = data_type_to_string(data_type);
    return BinaryenSelect(codegen.module, BinaryenRefIsNull(codegen.module, value),
                          generate_string_literal_expression("null", -1),
                          generate_string_literal_expression(name, -1));
  }

  default: {
    const char* name = data_type_to_string(data_type);
    return generate_string_literal_expression(name, -1);
  }
  }
}

static const char* generate_string_concat_function(int count)
{
  const char* name = count == 2 ? "string.concat" : memory_sprintf("string.concat.%d", count);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef total_length = NULL;
    for (int i = 0; i < count; i++)
    {
      BinaryenExpressionRef array = BinaryenLocalGet(codegen.module, i, codegen.string_type);
      BinaryenExpressionRef length = BinaryenArrayLen(codegen.module, array);

      if (total_length)
        total_length = BinaryenBinary(codegen.module, BinaryenAddInt32(), total_length, length);
      else
        total_length = length;
    }

    ArrayBinaryenExpressionRef body_list;
    array_init(&body_list);
    array_add(&body_list,
              BinaryenLocalSet(
                codegen.module, count,
                BinaryenArrayNew(codegen.module, codegen.string_heap_type, total_length, NULL)));

    for (int i = 0; i < count; i++)
    {
      BinaryenExpressionRef source = BinaryenLocalGet(codegen.module, i, codegen.string_type);
      BinaryenExpressionRef length = BinaryenArrayLen(codegen.module, source);
      BinaryenExpressionRef total_length = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));

      for (int j = 0; j < i; j++)
      {
        BinaryenExpressionRef array = BinaryenLocalGet(codegen.module, j, codegen.string_type);
        BinaryenExpressionRef length = BinaryenArrayLen(codegen.module, array);

        total_length = BinaryenBinary(codegen.module, BinaryenAddInt32(), total_length, length);
      }

      array_add(&body_list,
                BinaryenArrayCopy(codegen.module,
                                  BinaryenLocalGet(codegen.module, count, codegen.string_type),
                                  total_length, BinaryenExpressionCopy(source, codegen.module),
                                  BinaryenConst(codegen.module, BinaryenLiteralInt32(0)), length));
    }

    array_add(&body_list, BinaryenLocalGet(codegen.module, count, codegen.string_type));

    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list.elems, body_list.size, codegen.string_type);

    ArrayBinaryenType params_list;
    array_init(&params_list);
    for (int i = 0; i < count; i++)
    {
      array_add(&params_list, codegen.string_type);
    }

    BinaryenType params = BinaryenTypeCreate(params_list.elems, params_list.size);

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, &codegen.string_type, 1, body);
  }

  return name;
}

static const char* generate_string_equals_function(void)
{
#define LEFT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define RIGHT() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.equals";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    const char* loop_name = "string.equals.loop";

    BinaryenExpressionRef check_value_condition = BinaryenBinary(
      codegen.module, BinaryenNeInt32(),
      BinaryenArrayGet(codegen.module, LEFT(), COUNTER(), BinaryenTypeInt32(), false),
      BinaryenArrayGet(codegen.module, RIGHT(), COUNTER(), BinaryenTypeInt32(), false));
    BinaryenExpressionRef check_value_body = BinaryenReturn(codegen.module, CONSTANT(0));
    BinaryenExpressionRef check_value =
      BinaryenIf(codegen.module, check_value_condition, check_value_body, NULL);

    BinaryenExpressionRef increment_count =
      BinaryenLocalSet(codegen.module, 2,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1)));

    BinaryenExpressionRef branch = BinaryenBreak(codegen.module, loop_name, NULL, NULL);

    BinaryenExpressionRef counter_body_list[] = { check_value, increment_count, branch };
    BinaryenExpressionRef counter_body =
      BinaryenBlock(codegen.module, NULL, counter_body_list,
                    sizeof(counter_body_list) / sizeof_ptr(counter_body_list), BinaryenTypeNone());
    BinaryenExpressionRef counter_condition = BinaryenBinary(
      codegen.module, BinaryenLtSInt32(), COUNTER(), BinaryenArrayLen(codegen.module, LEFT()));
    BinaryenExpressionRef counter =
      BinaryenIf(codegen.module, counter_condition, counter_body, NULL);

    BinaryenExpressionRef loop = BinaryenLoop(codegen.module, loop_name, counter);
    BinaryenExpressionRef loop_block_list[] = { loop, BinaryenReturn(codegen.module, CONSTANT(1)) };
    BinaryenExpressionRef loop_block =
      BinaryenBlock(codegen.module, NULL, loop_block_list,
                    sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());

    BinaryenExpressionRef condition =
      BinaryenBinary(codegen.module, BinaryenEqInt32(), BinaryenArrayLen(codegen.module, LEFT()),
                     BinaryenArrayLen(codegen.module, RIGHT()));
    BinaryenExpressionRef iterate = BinaryenIf(codegen.module, condition, loop_block, NULL);
    BinaryenExpressionRef fast_exit =
      BinaryenIf(codegen.module, BinaryenRefEq(codegen.module, LEFT(), RIGHT()),
                 BinaryenReturn(codegen.module, CONSTANT(1)), NULL);

    BinaryenExpressionRef body_list[] = { fast_exit, iterate,
                                          BinaryenReturn(codegen.module, CONSTANT(0)) };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;
#undef LEFT
#undef RIGHT
#undef COUNTER
#undef CONSTANT
}

static const char* generate_string_length_function(void)
{
  const char* name = "string.length";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
    BinaryenAddFunction(codegen.module, name, codegen.string_type, BinaryenTypeInt32(), NULL, 0,
                        BinaryenArrayLen(codegen.module, ref));
  }

  return name;
}

static const char* generate_string_at_function(void)
{
  const char* name = "string.at";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
    BinaryenExpressionRef index = BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32());

    BinaryenExpressionRef body = BinaryenLocalTee(
      codegen.module, 2, BinaryenArrayGet(codegen.module, ref, index, BinaryenTypeInt32(), false),
      BinaryenTypeInt32());

    BinaryenType params_list[] = { codegen.string_type, BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;
}

static void generate_string_export_functions(void)
{
  if (!BinaryenGetExport(codegen.module, generate_string_length_function()))
    BinaryenAddFunctionExport(codegen.module, generate_string_length_function(),
                              generate_string_length_function());

  if (!BinaryenGetExport(codegen.module, generate_string_at_function()))
    BinaryenAddFunctionExport(codegen.module, generate_string_at_function(),
                              generate_string_at_function());
}

static BinaryenHeapType generate_array_heap_binaryen_type(TypeBuilderRef type_builder_ref,
                                                          ArrayTypeBuilderSubtype* subtypes,
                                                          DataType data_type)
{
  const char* key = data_type_to_string(data_type);
  BinaryenHeapType array_binaryen_type =
    map_get_string_binaryen_heap_type(&codegen.heap_types, key);

  if (!array_binaryen_type)
  {
    BinaryenType element_data_type;
    TypeBuilderRef type_builder;
    int offset;

    if (type_builder_ref)
    {
      type_builder = type_builder_ref;
      element_data_type = data_type_to_temporary_binaryen_type(type_builder_ref, subtypes,
                                                               array_data_type_element(data_type));
      offset = TypeBuilderGetSize(type_builder);

      TypeBuilderGrow(type_builder, 2);
    }
    else
    {
      type_builder = TypeBuilderCreate(2);
      element_data_type = data_type_to_binaryen_type(array_data_type_element(data_type));
      offset = 0;
    }

    BinaryenPackedType packed_type =
      data_type_to_binaryen_packed_type(array_data_type_element(data_type));
    bool mutable = true;

    TypeBuilderSetArrayType(type_builder, offset, element_data_type, packed_type, mutable);
    BinaryenHeapType temporary_heap_type = TypeBuilderGetTempHeapType(type_builder, offset);
    BinaryenType temporary_type =
      TypeBuilderGetTempRefType(type_builder, temporary_heap_type, true);

    BinaryenType field_types[] = { temporary_type, BinaryenTypeInt32() };
    BinaryenPackedType field_packed_types[] = { BinaryenPackedTypeNotPacked(),
                                                BinaryenPackedTypeNotPacked() };
    bool field_mutables[] = { true, true };

    TypeBuilderSetStructType(type_builder, offset + 1, field_types, field_packed_types,
                             field_mutables, sizeof(field_types) / sizeof_ptr(field_types));

    if (type_builder_ref)
    {
      TypeBuilderSubtype subtype = { .type = SUBTYPE_ARRAY, .index = offset + 1, .key = key };
      array_add(subtypes, subtype);

      array_binaryen_type = TypeBuilderGetTempHeapType(type_builder, offset + 1);
    }
    else
    {
      BinaryenHeapType heap_types[2];
      TypeBuilderBuildAndDispose(type_builder, heap_types, 0, 0);

      array_binaryen_type = heap_types[1];
    }

    map_put_string_binaryen_heap_type(&codegen.heap_types, key, array_binaryen_type);
  }

  return array_binaryen_type;
}

static BinaryenHeapType generate_function_heap_binaryen_type(TypeBuilderRef type_builder_ref,
                                                             ArrayTypeBuilderSubtype* subtypes,
                                                             DataType data_type)
{
  const char* key = data_type_to_string(data_type);
  BinaryenHeapType function_binaryen_heap_type =
    map_get_string_binaryen_heap_type(&codegen.heap_types, key);

  if (!function_binaryen_heap_type)
  {
    TypeBuilderRef type_builder;
    int offset;

    if (type_builder_ref)
    {
      type_builder = type_builder_ref;
      offset = TypeBuilderGetSize(type_builder);

      TypeBuilderGrow(type_builder, 1);
    }
    else
    {
      type_builder = TypeBuilderCreate(1);
      offset = 0;
    }

    DataType return_data_type;
    ArrayDataType parameter_types;
    expand_function_data_type(data_type, &return_data_type, &parameter_types);

    ArrayBinaryenType parameter_binaryen_types;
    array_init(&parameter_binaryen_types);

    DataType parameter_type;
    array_foreach(&parameter_types, parameter_type)
    {
      array_add(&parameter_binaryen_types,
                data_type_to_temporary_binaryen_type(type_builder_ref, subtypes, parameter_type));
    }

    BinaryenType param_types =
      BinaryenTypeCreate(parameter_binaryen_types.elems, parameter_binaryen_types.size);

    TypeBuilderSetSignatureType(
      type_builder, offset, param_types,
      data_type_to_temporary_binaryen_type(type_builder_ref, subtypes, return_data_type));

    if (type_builder_ref)
    {
      TypeBuilderSubtype subtype = { .type = SUBTYPE_FUNCTION, .index = offset, .key = key };
      array_add(subtypes, subtype);

      function_binaryen_heap_type = TypeBuilderGetTempHeapType(type_builder, offset);
    }
    else
    {
      TypeBuilderBuildAndDispose(type_builder, &function_binaryen_heap_type, NULL, NULL);
    }

    map_put_string_binaryen_heap_type(&codegen.heap_types, key, function_binaryen_heap_type);
  }

  return function_binaryen_heap_type;
}

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_ALIAS:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_FUNCTION_GROUP:
    return BinaryenTypeNone();
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER: {
    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    return BinaryenTypeFromHeapType(
      generate_function_heap_binaryen_type(NULL, &subtypes, data_type), true);
  }
  case TYPE_NULL:
    return *data_type.null_function ? BinaryenTypeNullFuncref() : BinaryenTypeAnyref();
  case TYPE_ANY:
    return BinaryenTypeAnyref();
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();
  case TYPE_STRING:
    return codegen.string_type;
  case TYPE_OBJECT:
    if (!data_type.class->ref)
    {
      ArrayTypeBuilderSubtype subtypes;
      array_init(&subtypes);

      generate_class_declaration(data_type.class, NULL, &subtypes);
    }

    return data_type.class->ref;
  case TYPE_ARRAY:
    return BinaryenTypeFromHeapType(generate_array_heap_binaryen_type(NULL, NULL, data_type), true);
  default:
    UNREACHABLE("Unhandled data type");
  }
}

static BinaryenType data_type_to_temporary_binaryen_type(TypeBuilderRef type_builder_ref,
                                                         ArrayTypeBuilderSubtype* subtypes,
                                                         DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER:
    return TypeBuilderGetTempRefType(
      type_builder_ref, generate_function_heap_binaryen_type(type_builder_ref, subtypes, data_type),
      true);

  case TYPE_ARRAY:
    return TypeBuilderGetTempRefType(
      type_builder_ref, generate_array_heap_binaryen_type(type_builder_ref, subtypes, data_type),
      true);

  case TYPE_OBJECT:
    if (!data_type.class->ref)
      generate_class_declaration(data_type.class, type_builder_ref, subtypes);

    return data_type.class->ref;

  default:
    return data_type_to_binaryen_type(data_type);
  }
}

static BinaryenPackedType data_type_to_binaryen_packed_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_CHAR:
  case TYPE_BOOL:
    return BinaryenPackedTypeInt8();

  default:
    return BinaryenPackedTypeNotPacked();
  }
}

static BinaryenExpressionRef generate_default_initialization(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_INTEGER:
  case TYPE_CHAR:
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(0));
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER: {
    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    return BinaryenRefNull(codegen.module, data_type_to_binaryen_type(data_type));
  }
  case TYPE_OBJECT: {
    if (!data_type.class->ref)
    {
      ArrayTypeBuilderSubtype subtypes;
      array_init(&subtypes);

      generate_class_declaration(data_type.class, NULL, &subtypes);
    }

    return BinaryenRefNull(codegen.module, data_type.class->ref);
  }
  case TYPE_ANY:
    return BinaryenRefNull(codegen.module, BinaryenTypeAnyref());
  case TYPE_STRING:
    return generate_string_literal_expression("", 0);
  case TYPE_ARRAY:
    return BinaryenStructNew(codegen.module, NULL, 0,
                             generate_array_heap_binaryen_type(NULL, NULL, data_type));
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static const char* generate_array_remove_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define INDEX() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define INDEX_SET(_v) (BinaryenLocalSet(codegen.module, 1, (_v)))
#define RESULT() (BinaryenLocalGet(codegen.module, 2, return_type))
#define RESULT_SET(_v) (BinaryenLocalSet(codegen.module, 2, (_v)))
#define LENGTH() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define LENGTH_SET(_v) (BinaryenLocalSet(codegen.module, 3, (_v)))
#define SRC_INDEX() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define SRC_INDEX_SET(_v) (BinaryenLocalSet(codegen.module, 4, (_v)))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define ARRAY_GET(_i) (BinaryenArrayGet(codegen.module, ARRAY(), (_i), return_type, false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define SIZE_SET(_v) (BinaryenStructSet(codegen.module, 1, THIS(), (_v)))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type = array_data_type_element(this_data_type);
  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);

  const char* name = memory_sprintf("array.remove.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType return_type = data_type_to_binaryen_type(element_data_type);

    BinaryenExpressionRef body_list[] = {
      INDEX_SET(BinaryenSelect(codegen.module,
                               BinaryenBinary(codegen.module, BinaryenLtSInt32(), INDEX(), SIZE()),
                               INDEX(), BinaryenConst(codegen.module, BinaryenLiteralInt32(-1)))),
      RESULT_SET(ARRAY_GET(INDEX())),
      SRC_INDEX_SET(BinaryenBinary(codegen.module, BinaryenAddInt32(), INDEX(), CONSTANT(1))),
      LENGTH_SET(BinaryenBinary(codegen.module, BinaryenSubInt32(), SIZE(), SRC_INDEX())),
      SIZE_SET(BinaryenBinary(codegen.module, BinaryenSubInt32(), SIZE(), CONSTANT(1))),
      BinaryenArrayCopy(codegen.module, ARRAY(), INDEX(), ARRAY(), SRC_INDEX(), LENGTH()),
      BinaryenReturn(codegen.module, RESULT())
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { this_type, BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));
    BinaryenType vars_list[] = {
      return_type,
      BinaryenTypeInt32(),
      BinaryenTypeInt32(),
    };

    BinaryenAddFunction(codegen.module, name, params, return_type, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef INDEX
#undef INDEX_SET
#undef RESULT
#undef RESULT_SET
#undef LENGTH
#undef LENGTH_SET
#undef SRC_INDEX
#undef SRC_INDEX_SET
#undef ARRAY
#undef ARRAY_GET
#undef CAPACITY
#undef SIZE
#undef SIZE_SET
#undef CONSTANT
}

static const char* generate_array_push_function(DataType this_data_type, DataType value_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, value_type))
#define TEMP() (BinaryenLocalGet(codegen.module, 2, array_type))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type = array_data_type_element(this_data_type);

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);
  BinaryenType value_type = data_type_to_binaryen_type(value_data_type);

  const char* name = memory_sprintf("array.push.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType array_type = BinaryenExpressionGetType(
      BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false));

    BinaryenExpressionRef resize_list[] = {
      BinaryenLocalSet(codegen.module, 2, ARRAY()),
      BinaryenStructSet(
        codegen.module, 0, THIS(),
        BinaryenArrayNew(codegen.module, array_type,
                         BinaryenBinary(codegen.module, BinaryenMulInt32(), CONSTANT(2), SIZE()),
                         generate_default_initialization(element_data_type))),
      BinaryenArrayCopy(codegen.module, ARRAY(), CONSTANT(0), TEMP(), CONSTANT(0), SIZE())
    };
    BinaryenExpressionRef resize =
      BinaryenBlock(codegen.module, NULL, resize_list,
                    sizeof(resize_list) / sizeof_ptr(resize_list), BinaryenTypeNone());

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(
        codegen.module, BinaryenRefIsNull(codegen.module, ARRAY()),
        BinaryenStructSet(codegen.module, 0, THIS(),
                          BinaryenArrayNew(codegen.module, array_type, CONSTANT(16),
                                           generate_default_initialization(element_data_type))),
        NULL),

      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeSInt32(), SIZE(), CAPACITY()), resize,
                 NULL),

      BinaryenArraySet(codegen.module, ARRAY(), SIZE(), VALUE()),
      BinaryenStructSet(codegen.module, 1, THIS(),
                        BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(1)))
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { this_type, value_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));
    BinaryenType vars_list[] = { array_type };

    BinaryenAddFunction(codegen.module, name, params, BinaryenTypeNone(), vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef VALUE
#undef TEMP
#undef ARRAY
#undef CAPACITY
#undef SIZE
#undef CONSTANT
}

static const char* generate_array_push_string_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, value_type))
#define TEMP() (BinaryenLocalGet(codegen.module, 2, array_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 3, BinaryenTypeInt32()))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define VALUE_SIZE() (BinaryenArrayLen(codegen.module, VALUE()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type = array_data_type_element(this_data_type);

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);
  BinaryenType value_type = codegen.string_type;

  const char* name = "array.push_string";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType array_type = BinaryenExpressionGetType(
      BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false));

    BinaryenExpressionRef resize_list[] = {
      BinaryenLocalSet(codegen.module, 2, ARRAY()),

      BinaryenStructSet(
        codegen.module, 0, THIS(),
        BinaryenArrayNew(
          codegen.module, array_type,
          BinaryenBinary(codegen.module, BinaryenMulInt32(), CONSTANT(2),
                         BinaryenBinary(codegen.module, BinaryenAddInt32(), VALUE_SIZE(), SIZE())),
          generate_default_initialization(element_data_type))),

      BinaryenArrayCopy(codegen.module, ARRAY(), CONSTANT(0), TEMP(), CONSTANT(0), SIZE())
    };
    BinaryenExpressionRef resize =
      BinaryenBlock(codegen.module, NULL, resize_list,
                    sizeof(resize_list) / sizeof_ptr(resize_list), BinaryenTypeNone());

    BinaryenExpressionRef loop_list[] = {
      BinaryenBreak(codegen.module, "array.push_string.block",
                    BinaryenBinary(codegen.module, BinaryenGeUInt32(), COUNTER(), VALUE_SIZE()),
                    NULL),

      BinaryenArraySet(
        codegen.module, ARRAY(),
        BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), SIZE()),
        BinaryenArrayGet(codegen.module, VALUE(), COUNTER(), BinaryenTypeInt32(), false)),

      BinaryenLocalSet(codegen.module, 3,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),

      BinaryenBreak(codegen.module, "array.push_string.loop", NULL, NULL),
    };
    BinaryenExpressionRef loop =
      BinaryenBlock(codegen.module, "array.push_string.block", loop_list,
                    sizeof(loop_list) / sizeof_ptr(loop_list), BinaryenTypeNone());

    BinaryenExpressionRef body_list[] = {
      BinaryenIf(
        codegen.module, BinaryenRefIsNull(codegen.module, ARRAY()),
        BinaryenStructSet(codegen.module, 0, THIS(),
                          BinaryenArrayNew(codegen.module, array_type, VALUE_SIZE(),
                                           generate_default_initialization(element_data_type))),
        NULL),

      BinaryenIf(
        codegen.module,
        BinaryenBinary(codegen.module, BinaryenGeSInt32(),
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), VALUE_SIZE(), SIZE()),
                       CAPACITY()),
        resize, NULL),

      BinaryenLoop(codegen.module, "array.push_string.loop", loop),

      BinaryenStructSet(codegen.module, 1, THIS(),
                        BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), VALUE_SIZE())),

    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { this_type, value_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));
    BinaryenType vars_list[] = { array_type, BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, BinaryenTypeNone(), vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef VALUE
#undef TEMP
#undef COUNTER
#undef ARRAY
#undef CAPACITY
#undef SIZE
#undef VALUE_SIZE
#undef CONSTANT
}

static const char* generate_array_pop_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, value_type))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type = array_data_type_element(this_data_type);

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);
  BinaryenType element_type = data_type_to_binaryen_type(element_data_type);

  const char* name = memory_sprintf("array.pop.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenStructSet(codegen.module, 1, THIS(),
                        BinaryenBinary(codegen.module, BinaryenSubInt32(), SIZE(), CONSTANT(1))),

      BinaryenReturn(codegen.module,
                     BinaryenArrayGet(codegen.module, ARRAY(), SIZE(), BinaryenTypeAuto(), false))

    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { this_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { element_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef THIS
#undef VALUE
#undef ARRAY
#undef CAPACITY
#undef SIZE
#undef CONSTANT
}

static const char* generate_array_clear_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);

  const char* name = memory_sprintf("array.clear.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenStructSet(codegen.module, 1, THIS(), CONSTANT(0)),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { this_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results = BinaryenTypeNone();

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef THIS
#undef CONSTANT
}

static const char* generate_array_reserve_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define AMOUNT() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define COUNTER()                                                                                  \
  (BinaryenLocalGet(codegen.module, *this_data_type.array.count + 1, BinaryenTypeInt32()))
#define SUBTHIS() (BinaryenLocalGet(codegen.module, *this_data_type.array.count + 2, element_type))

#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type = array_data_type_element(this_data_type);

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);
  BinaryenType element_type = data_type_to_binaryen_type(element_data_type);

  const char* name = memory_sprintf("array.reserve.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef loop;

    if (*this_data_type.array.count > 1)
    {
      const char* block_name = "array.reserve.block";
      const char* loop_name = "array.reserve.loop";

      ArrayBinaryenExpressionRef operands;
      array_init(&operands);
      array_add(&operands, SUBTHIS());

      for (int i = 2; i <= *this_data_type.array.count; i++)
        array_add(&operands, BinaryenLocalGet(codegen.module, i, BinaryenTypeInt32()));

      BinaryenExpressionRef reserve =
        BinaryenCall(codegen.module, generate_array_reserve_function(element_data_type),
                     operands.elems, operands.size, BinaryenTypeNone());

      BinaryenExpressionRef loop_block_list[] = {
        BinaryenBreak(codegen.module, block_name,
                      BinaryenBinary(codegen.module, BinaryenGeSInt32(), COUNTER(), AMOUNT()),
                      NULL),

        BinaryenLocalSet(codegen.module, *this_data_type.array.count + 2,
                         generate_default_initialization(element_data_type)),

        reserve,

        BinaryenArraySet(codegen.module, ARRAY(), COUNTER(), SUBTHIS()),

        BinaryenLocalSet(
          codegen.module, *this_data_type.array.count + 1,
          BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),

        BinaryenBreak(codegen.module, loop_name, NULL, NULL)
      };

      BinaryenExpressionRef loop_block =
        BinaryenBlock(codegen.module, block_name, loop_block_list,
                      sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());

      loop = BinaryenLoop(codegen.module, loop_name, loop_block);
    }
    else
    {
      loop = BinaryenNop(codegen.module);
    }

    BinaryenType array_type = BinaryenExpressionGetType(
      BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false));

    BinaryenExpressionRef body_list[] = {
      BinaryenStructSet(codegen.module, 0, THIS(),
                        BinaryenArrayNew(codegen.module, array_type, AMOUNT(),
                                         generate_default_initialization(element_data_type))),
      BinaryenStructSet(codegen.module, 1, THIS(), AMOUNT()),

      loop,
    };

    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    ArrayBinaryenType params_list;
    array_init(&params_list);
    array_add(&params_list, this_type);

    for (int i = 0; i < *this_data_type.array.count; i++)
      array_add(&params_list, BinaryenTypeInt32());

    BinaryenType params = BinaryenTypeCreate(params_list.elems, params_list.size);

    BinaryenType results = BinaryenTypeNone();
    BinaryenType vars_list[] = { BinaryenTypeInt32(), element_type };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef AMOUNT
#undef COUNTER
#undef SUBTHIS
#undef ARRAY
#undef CONSTANT
}

static const char* generate_array_to_string_function(DataType this_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define RESULT() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  BinaryenType this_type = data_type_to_binaryen_type(this_data_type);

  const char* name = memory_sprintf("array.to_string.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef struct_ref = THIS();
    BinaryenExpressionRef array_ref =
      BinaryenStructGet(codegen.module, 0, struct_ref, BinaryenTypeAuto(), false);
    BinaryenExpressionRef length =
      BinaryenStructGet(codegen.module, 1, struct_ref, BinaryenTypeInt32(), false);

    BinaryenExpressionRef body_list[] = {
      BinaryenLocalSet(codegen.module, 1,
                       BinaryenArrayNew(codegen.module, codegen.string_heap_type, length, NULL)),

      BinaryenIf(codegen.module,
                 BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                               BinaryenRefIsNull(codegen.module, BinaryenExpressionCopy(
                                                                   array_ref, codegen.module))),

                 BinaryenArrayCopy(codegen.module, RESULT(), CONSTANT(0), array_ref, CONSTANT(0),
                                   BinaryenExpressionCopy(length, codegen.module)),
                 NULL),

      RESULT()
    };

    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    codegen.string_type);

    BinaryenType params_list[] = { this_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { codegen.string_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, &codegen.string_type, 1, body);
  }

  return name;

#undef THIS
#undef RESULT
#undef CONSTANT
}

static const char* generate_int_hash_function(void)
{
  const char* name = "int.hash";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType params_list[] = { BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(
      codegen.module, name, params, results, NULL, 0,
      BinaryenReturn(codegen.module, BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32())));
  }

  return name;
}

static const char* generate_float_hash_function(void)
{
  const char* name = "float.hash";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType params_list[] = { BinaryenTypeFloat32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0,
                        BinaryenUnary(codegen.module, BinaryenReinterpretFloat32(),
                                      BinaryenLocalGet(codegen.module, 0, BinaryenTypeFloat32())));
  }

  return name;
}

static const char* generate_float_sqrt_function(void)
{
  const char* name = "float.sqrt";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType params_list[] = { BinaryenTypeFloat32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeFloat32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0,
                        BinaryenUnary(codegen.module, BinaryenSqrtFloat32(),
                                      BinaryenLocalGet(codegen.module, 0, BinaryenTypeFloat32())));
  }

  return name;
}

static const char* generate_string_hash_function(void)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define HASH() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define SIZE() (BinaryenArrayLen(codegen.module, THIS()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.hash";

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef loop_body_list[] = {
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGeSInt32(), COUNTER(), SIZE()),
                 BinaryenBreak(codegen.module, "string.hash.block", NULL, NULL), NULL),
      BinaryenLocalSet(codegen.module, 2,
                       BinaryenBinary(codegen.module, BinaryenXorInt32(), HASH(),
                                      BinaryenArrayGet(codegen.module, THIS(), COUNTER(),
                                                       codegen.string_type, false))),
      BinaryenLocalSet(
        codegen.module, 2,
        BinaryenBinary(codegen.module, BinaryenMulInt32(), HASH(), CONSTANT(0x01000193))),

      BinaryenLocalSet(codegen.module, 1,
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1))),

      BinaryenBreak(codegen.module, "string.hash.loop", NULL, NULL)
    };
    BinaryenExpressionRef loop_body =
      BinaryenBlock(codegen.module, "string.hash.block", loop_body_list,
                    sizeof(loop_body_list) / sizeof_ptr(loop_body_list), BinaryenTypeNone());

    BinaryenExpressionRef body_list[] = {
      BinaryenLocalSet(codegen.module, 2, CONSTANT(0x811C9DC5)),
      BinaryenLoop(codegen.module, "string.hash.loop", loop_body),
      BinaryenReturn(codegen.module, HASH()),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef THIS
#undef COUNTER
#undef HASH
#undef SIZE
#undef CONSTANT
}

static const char* generate_string_to_array_function(DataType return_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define RESULT() (BinaryenLocalGet(codegen.module, 1, return_type))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = memory_sprintf("string.to_array");

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenType return_type = data_type_to_binaryen_type(return_data_type);

    BinaryenExpressionRef operands[] = {
      BinaryenArrayNew(codegen.module, codegen.string_heap_type,
                       BinaryenArrayLen(codegen.module, THIS()), NULL),
      BinaryenArrayLen(codegen.module, THIS()),
    };

    BinaryenExpressionRef body_list[] = {
      BinaryenLocalSet(codegen.module, 1,
                       BinaryenStructNew(codegen.module, operands,
                                         sizeof(operands) / sizeof_ptr(operands), return_type)),
      BinaryenArrayCopy(codegen.module,
                        BinaryenStructGet(codegen.module, 0, RESULT(), codegen.string_type, false),
                        CONSTANT(0), THIS(), CONSTANT(0), BinaryenArrayLen(codegen.module, THIS())),
      RESULT(),
    };

    BinaryenExpressionRef body = BinaryenBlock(
      codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list), return_type);

    BinaryenType params_list[] = { codegen.string_type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { return_type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, &return_type, 1, body);
  }

  return name;

#undef THIS
#undef RESULT
#undef CONSTANT
}

static const char* generate_memory(void)
{
  const char* counter_name = "__memory__";

  if (!BinaryenHasMemory(codegen.module))
  {
    BinaryenAddGlobal(codegen.module, counter_name, BinaryenTypeInt32(), true,
                      BinaryenConst(codegen.module, BinaryenLiteralInt32(0)));
    BinaryenSetMemory(codegen.module, 1, -1, NULL, NULL, NULL, NULL, NULL, NULL, 0, false, false,
                      NULL);
    BinaryenAddMemoryExport(codegen.module, "0", "memory");
  }

  return counter_name;
}

static const char* generate_alloc_function(void)
{
#define SIZE() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define RESULT() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))
#define PAGES() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define COUNTER() (BinaryenGlobalGet(codegen.module, counter_name, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "alloc";
  const char* counter_name = generate_memory();

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenLocalSet(
        codegen.module, 0,
        BinaryenBinary(codegen.module, BinaryenAndInt32(),
                       BinaryenBinary(codegen.module, BinaryenAddInt32(), SIZE(), CONSTANT(3)),
                       CONSTANT(-4))),

      BinaryenLocalSet(codegen.module, 1, COUNTER()),
      BinaryenGlobalSet(codegen.module, counter_name,
                        BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), SIZE())),

      BinaryenLocalSet(codegen.module, 2,
                       BinaryenBinary(codegen.module, BinaryenDivUInt32(),
                                      BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(),
                                                     CONSTANT(65535)),
                                      CONSTANT(65536))),

      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenGtSInt32(), PAGES(),
                                BinaryenMemorySize(codegen.module, "0", false)),
                 BinaryenDrop(codegen.module,
                              BinaryenMemoryGrow(
                                codegen.module,
                                BinaryenBinary(codegen.module, BinaryenSubInt32(), PAGES(),
                                               BinaryenMemorySize(codegen.module, "0", false)),
                                "0", false)),

                 NULL),

      RESULT()
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params_list[] = { BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { BinaryenTypeInt32() };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenType vars_list[] = { BinaryenTypeInt32(), BinaryenTypeInt32() };

    BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                        sizeof(vars_list) / sizeof_ptr(vars_list), body);
  }

  return name;

#undef SIZE
#undef RESULT
#undef PAGES
#undef COUNTER
#undef CONSTANT
}

static const char* generate_alloc_reset_function(void)
{
#define SIZE() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define RESULT() (BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32()))

  const char* name = "allocReset";
  const char* counter_name = generate_memory();

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenGlobalSet(codegen.module, counter_name,
                        BinaryenConst(codegen.module, BinaryenLiteralInt32(0))),
    };

    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params = BinaryenTypeNone();
    BinaryenType results = BinaryenTypeNone();

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef SIZE
#undef RESULT
}

static const char* generate_memory_function(void)
{
  generate_memory();

  const char* name = "memory";
  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = { BinaryenMemorySize(codegen.module, "0", false) };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeInt32());

    BinaryenType params = BinaryenTypeNone();
    BinaryenType results = BinaryenTypeInt32();

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;
}

static const char* generate_write_function(const char* name, BinaryenType type, unsigned int bytes)
{
#define PTR() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, type))

  generate_memory();

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenStore(codegen.module, bytes, 0, 0, PTR(), VALUE(), type, "0"),
    };
    BinaryenExpressionRef body =
      BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                    BinaryenTypeNone());

    BinaryenType params_list[] = { BinaryenTypeInt32(), type };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results = BinaryenTypeNone();

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef PTR
#undef VALUE
}

static const char* generate_read_function(const char* name, BinaryenType type, unsigned int bytes)
{
#define PTR() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))

  generate_memory();

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenLoad(codegen.module, bytes, false, 0, 0, type, PTR(), "0"),
    };
    BinaryenExpressionRef body = BinaryenBlock(codegen.module, NULL, body_list,
                                               sizeof(body_list) / sizeof_ptr(body_list), type);

    BinaryenType params_list[] = { BinaryenTypeInt32() };
    BinaryenType params =
      BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

    BinaryenType results_list[] = { type };
    BinaryenType results =
      BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

    BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);
  }

  return name;

#undef PTR
}

static const char* generate_function_internal(DataType data_type)
{
  assert(data_type.type == TYPE_FUNCTION_INTERNAL);

  const char* name = data_type.function_internal.name;

  if (strcmp(name, "array.push") == 0)
    return generate_array_push_function(array_at(&data_type.function_internal.parameter_types, 0),
                                        array_at(&data_type.function_internal.parameter_types, 1));
  if (strcmp(name, "array.push_string") == 0)
    return generate_array_push_string_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.to_string") == 0)
    return generate_array_to_string_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.pop") == 0)
    return generate_array_pop_function(array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.clear") == 0)
    return generate_array_clear_function(array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.reserve") == 0)
    return generate_array_reserve_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "array.remove") == 0)
    return generate_array_remove_function(
      array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "int.hash") == 0)
    return generate_int_hash_function();
  else if (strcmp(name, "float.sqrt") == 0)
    return generate_float_sqrt_function();
  else if (strcmp(name, "float.hash") == 0)
    return generate_float_hash_function();
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
  else if (strcmp(name, "string.split") == 0)
    return generate_string_split_function(*data_type.function_internal.return_type);
  else if (strcmp(name, "string.join") == 0)
    return generate_string_join_function(array_at(&data_type.function_internal.parameter_types, 0));
  else if (strcmp(name, "string.to_array") == 0)
    return generate_string_to_array_function(*data_type.function_internal.return_type);

  else if (strcmp(name, "alloc") == 0)
    return generate_alloc_function();
  else if (strcmp(name, "allocReset") == 0)
    return generate_alloc_reset_function();
  else if (strcmp(name, "memory") == 0)
    return generate_memory_function();

  else if (strcmp(name, "writeInt") == 0)
    return generate_write_function(name, BinaryenTypeInt32(), 4);
  else if (strcmp(name, "writeFloat") == 0)
    return generate_write_function(name, BinaryenTypeFloat32(), 4);
  else if (strcmp(name, "writeChar") == 0)
    return generate_write_function(name, BinaryenTypeInt32(), 1);
  else if (strcmp(name, "writeBool") == 0)
    return generate_write_function(name, BinaryenTypeInt32(), 1);

  else if (strcmp(name, "readInt") == 0)
    return generate_read_function(name, BinaryenTypeInt32(), 4);
  else if (strcmp(name, "readFloat") == 0)
    return generate_read_function(name, BinaryenTypeFloat32(), 4);
  else if (strcmp(name, "readChar") == 0)
    return generate_read_function(name, BinaryenTypeInt32(), 1);
  else if (strcmp(name, "readBool") == 0)
    return generate_read_function(name, BinaryenTypeInt32(), 1);

  else
    UNREACHABLE("Unexpected internal function");
}

static BinaryenExpressionRef generate_function_pointer(DataType data_type)
{
  const char* name;

  switch (data_type.type)
  {
  case TYPE_FUNCTION:
    name = data_type.function->name.lexeme;
    break;

  case TYPE_FUNCTION_MEMBER:
    name = data_type.function_member.function->name.lexeme;
    break;

  case TYPE_FUNCTION_INTERNAL:
    name = generate_function_internal(data_type);
    break;

  case TYPE_FUNCTION_GROUP:
    return BinaryenRefNull(codegen.module, BinaryenTypeNullref());

  default:
    UNREACHABLE("Unexpected function data type");
  }

  return BinaryenRefFunc(codegen.module, name, data_type_to_binaryen_type(data_type));
}

static BinaryenExpressionRef generate_group_expression(GroupExpr* expression)
{
  return generate_expression(expression->expr);
}

static BinaryenExpressionRef generate_string_literal_expression(const char* literal, int length)
{
  int index = map_get_sint(&codegen.string_constants, literal);
  if (!index)
  {
    index = ++codegen.strings;
    map_put_sint(&codegen.string_constants, literal, index);

    ArrayBinaryenExpressionRef values;
    array_init(&values);

    for (int i = 0; i < length || (length == -1 && literal[i] != '\0'); i++)
      array_add(&values, BinaryenConst(codegen.module, BinaryenLiteralInt32(literal[i])));

    BinaryenExpressionRef initializer =
      BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, values.elems, values.size);

    const char* name = memory_sprintf("string.%d", index);
    BinaryenAddGlobal(codegen.module, name, codegen.string_type, false, initializer);
  }

  const char* name = memory_sprintf("string.%d", index);
  return BinaryenGlobalGet(codegen.module, name, codegen.string_type);
}

static BinaryenExpressionRef generate_literal_expression(LiteralExpr* expression)
{
  switch (expression->data_type.type)
  {
  case TYPE_INTEGER:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->integer));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(expression->floating));
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->boolean));
  case TYPE_NULL:
    return BinaryenRefNull(codegen.module, data_type_to_binaryen_type(expression->data_type));
  case TYPE_CHAR:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->string.data[0]));
  case TYPE_STRING:
    return generate_string_literal_expression(expression->string.data, expression->string.length);
  default:
    UNREACHABLE("Unhandled literal value");
  }
}

static BinaryenExpressionRef generate_binary_expression(BinaryExpr* expression)
{
  BinaryenExpressionRef left = generate_expression(expression->left);
  BinaryenExpressionRef right = generate_expression(expression->right);
  BinaryenOp op = 0;

  DataType data_type = expression->operand_data_type;

  switch (expression->op.type)
  {
  case TOKEN_PLUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = BinaryenAddInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenAddFloat32();
    else if (data_type.type == TYPE_STRING)
    {
      const char prefix[] = "string.concat";

      ArrayBinaryenExpressionRef operands;
      array_init(&operands);

      if (BinaryenExpressionGetId(left) == BinaryenCallId() &&
          strncmp(BinaryenCallGetTarget(left), prefix, sizeof(prefix) - 1) == 0)
      {
        for (unsigned int i = 0; i < BinaryenCallGetNumOperands(left); i++)
        {
          BinaryenExpressionRef expression = BinaryenCallGetOperandAt(left, i);
          array_add(&operands, expression);
        }
      }
      else
      {
        array_add(&operands, left);
      }

      if (BinaryenExpressionGetId(right) == BinaryenCallId() &&
          strncmp(BinaryenCallGetTarget(right), prefix, sizeof(prefix) - 1) == 0)
      {
        for (unsigned int i = 0; i < BinaryenCallGetNumOperands(right); i++)
        {
          BinaryenExpressionRef expression = BinaryenCallGetOperandAt(right, i);
          array_add(&operands, expression);
        }
      }
      else
      {
        array_add(&operands, right);
      }

      return BinaryenCall(codegen.module, generate_string_concat_function(operands.size),
                          operands.elems, operands.size, codegen.string_type);
    }
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = BinaryenSubInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenSubFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = BinaryenMulInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenMulFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = BinaryenDivSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenDivFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
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
      op = BinaryenRemSInt32();
      break;
    case TOKEN_AMPERSAND:
      op = BinaryenAndInt32();
      break;
    case TOKEN_PIPE:
      op = BinaryenOrInt32();
      break;
    case TOKEN_CARET:
      op = BinaryenXorInt32();
      break;
    case TOKEN_LESS_LESS:
      op = BinaryenShlInt32();
      break;
    case TOKEN_GREATER_GREATER:
      op = BinaryenShrUInt32();
      break;
    default:
      UNREACHABLE("Unknown operator");
    }

    if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else if (data_type.type != TYPE_INTEGER && data_type.type != TYPE_CHAR)
      UNREACHABLE("Unsupported binary type for %, &, |, ^, <<, >>");

    break;
  }

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenEqInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      FuncStmt* function = expression->function;
      if (function)
      {
        BinaryenExpressionRef operands[] = { left, right };
        BinaryenExpressionRef call = BinaryenCall(
          codegen.module, function->name.lexeme, operands, sizeof(operands) / sizeof_ptr(operands),
          data_type_to_binaryen_type(expression->return_data_type));

        generate_debug_info(expression->op, call, codegen.function);
        return call;
      }
      else
        return BinaryenRefEq(codegen.module, left, right);
    }
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenCall(codegen.module, generate_string_equals_function(), operands,
                          sizeof(operands) / sizeof_ptr(operands), BinaryenTypeInt32());
    }
    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenNeInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenNeFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      FuncStmt* function = expression->function;
      if (function)
      {
        BinaryenExpressionRef operands[] = { left, right };
        BinaryenExpressionRef call = BinaryenCall(
          codegen.module, function->name.lexeme, operands, sizeof(operands) / sizeof_ptr(operands),
          data_type_to_binaryen_type(expression->return_data_type));

        generate_debug_info(expression->op, call, codegen.function);
        return call;
      }
      else
        return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                             BinaryenRefEq(codegen.module, left, right));
    }
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenCall(codegen.module, generate_string_equals_function(), operands,
                                        sizeof(operands) / sizeof_ptr(operands),
                                        BinaryenTypeInt32()));
    }
    else
      UNREACHABLE("Unsupported binary type for !=");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenLeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLeFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenGeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGeFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenLtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLtFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenGtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGtFloat32();
    else if (data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { left, right };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->return_data_type));

      generate_debug_info(expression->op, call, codegen.function);
      return call;
    }
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      BinaryenExpressionRef if_true = right;
      BinaryenExpressionRef if_false = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));

      return BinaryenIf(codegen.module, left, if_true, if_false);
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      BinaryenExpressionRef if_true = BinaryenConst(codegen.module, BinaryenLiteralInt32(1));
      BinaryenExpressionRef if_false = right;

      return BinaryenIf(codegen.module, left, if_true, if_false);
    }
    else
      UNREACHABLE("Unsupported binary type for OR");

    break;

  default:
    UNREACHABLE("Unhandled binary operation");
  }

  BinaryenExpressionRef binary = BinaryenBinary(codegen.module, op, left, right);
  generate_debug_info(expression->op, binary, codegen.function);

  return binary;
}

static BinaryenExpressionRef generate_unary_expression(UnaryExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->expr);

  switch (expression->op.type)
  {
  case TOKEN_TILDE:
    if (expression->data_type.type == TYPE_INTEGER)
      return BinaryenBinary(codegen.module, BinaryenXorInt32(), value,
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0xFFFFFFFF)));
    else
      UNREACHABLE("Unsupported unary type for ~");

  case TOKEN_MINUS:
    if (expression->data_type.type == TYPE_INTEGER)
      return BinaryenBinary(codegen.module, BinaryenSubInt32(),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)), value);
    else if (expression->data_type.type == TYPE_FLOAT)
      return BinaryenUnary(codegen.module, BinaryenNegFloat32(), value);
    else
      UNREACHABLE("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type.type == TYPE_BOOL)
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(), value);
    else
      UNREACHABLE("Unsupported unary type for !");

  default:
    UNREACHABLE("Unhandled unary expression");
  }
}

static BinaryenExpressionRef generate_cast_expression(CastExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->expr);

  if (expression->to_data_type.type == TYPE_FLOAT &&
      expression->from_data_type.type == TYPE_INTEGER)
  {
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
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
      BinaryenExpressionRef list = NULL;
      DataType list_data_type = DATA_TYPE(TYPE_ARRAY);

      if (expression->from_data_type.type == TYPE_OBJECT ||
          expression->from_data_type.type == TYPE_ARRAY)
      {
        list_data_type.array.data_type = ALLOC(DataType);
        list_data_type.array.count = ALLOC(unsigned char);
        *list_data_type.array.count = 1;
        *list_data_type.array.data_type = DATA_TYPE(TYPE_ANY);

        list = generate_default_initialization(list_data_type);
      }

      return generate_string_cast_function(expression->from_data_type, list_data_type, value,
                                           BinaryenConst(codegen.module, BinaryenLiteralInt32(0)),
                                           list);
    }
    case TYPE_ANY: {
      BinaryenExpressionRef result =
        BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, value),
                   BinaryenUnreachable(codegen.module),
                   BinaryenRefCast(codegen.module, BinaryenExpressionCopy(value, codegen.module),
                                   codegen.string_type));

      generate_debug_info(expression->type.token, BinaryenIfGetIfTrue(result), codegen.function);
      generate_debug_info(expression->type.token, BinaryenIfGetIfFalse(result), codegen.function);
      return result;
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
      return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_CHAR)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_INTEGER:
      return BinaryenBinary(codegen.module, BinaryenAndInt32(), value,
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0xFF)));
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
      return value;
    case TYPE_FLOAT:
      return BinaryenUnary(codegen.module, BinaryenTruncSatSFloat32ToInt32(), value);
    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_BOOL)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_FLOAT:
      return BinaryenBinary(codegen.module, BinaryenNeFloat32(), value,
                            BinaryenConst(codegen.module, BinaryenLiteralFloat32(0.0f)));
    case TYPE_INTEGER:
      return BinaryenBinary(codegen.module, BinaryenNeInt32(), value,
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)));
    case TYPE_STRING:
      return BinaryenBinary(codegen.module, BinaryenNeInt32(),
                            BinaryenArrayLen(codegen.module, value),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)));
    case TYPE_ARRAY:
      return BinaryenBinary(codegen.module, BinaryenNeInt32(),
                            BinaryenStructGet(codegen.module, 1, value, BinaryenTypeInt32(), false),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)));
    case TYPE_ANY:
    case TYPE_NULL:
    case TYPE_OBJECT:
    case TYPE_FUNCTION_POINTER:
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenRefIsNull(codegen.module, value));
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
      return value;

    default:
      break;
    }
  }
  else if (expression->to_data_type.type == TYPE_ARRAY)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_ANY: {
      BinaryenExpressionRef result =
        BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, value),
                   BinaryenUnreachable(codegen.module),
                   BinaryenRefCast(codegen.module, BinaryenExpressionCopy(value, codegen.module),
                                   data_type_to_binaryen_type(expression->to_data_type)));

      generate_debug_info(expression->type.token, BinaryenIfGetIfTrue(result), codegen.function);
      generate_debug_info(expression->type.token, BinaryenIfGetIfFalse(result), codegen.function);
      return result;
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
      return generate_default_initialization(expression->to_data_type);
    case TYPE_ANY: {
      BinaryenExpressionRef result =
        BinaryenIf(codegen.module, BinaryenRefIsNull(codegen.module, value),
                   BinaryenUnreachable(codegen.module),
                   BinaryenRefCast(codegen.module, BinaryenExpressionCopy(value, codegen.module),
                                   expression->to_data_type.class->ref));

      generate_debug_info(expression->type.token, BinaryenIfGetIfTrue(result), codegen.function);
      generate_debug_info(expression->type.token, BinaryenIfGetIfFalse(result), codegen.function);
      return result;
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
      return generate_function_pointer(expression->from_data_type);
    case TYPE_NULL:
      return generate_default_initialization(expression->to_data_type);

    default:
      break;
    }
  }

  if (equal_data_type(expression->from_data_type, expression->to_data_type))
  {
    return value;
  }

  UNREACHABLE("Unsupported cast type");
}

static BinaryenExpressionRef generate_variable_expression(VarExpr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);
  if (type == BinaryenTypeNone())
  {
    return BinaryenRefNull(codegen.module, BinaryenTypeAnyref());
  }

  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL ||
      expression->data_type.type == TYPE_FUNCTION_GROUP)
  {
    return generate_function_pointer(expression->data_type);
  }

  Scope scope = expression->variable->scope;
  switch (scope)
  {
  case SCOPE_LOCAL:
    return BinaryenLocalGet(codegen.module, expression->variable->index, type);
  case SCOPE_GLOBAL:
    return BinaryenGlobalGet(codegen.module, expression->name.lexeme, type);
  case SCOPE_CLASS: {
    BinaryenExpressionRef get =
      BinaryenStructGet(codegen.module, expression->variable->index,
                        BinaryenLocalGet(codegen.module, 0, codegen.class), type, false);
    generate_debug_info(expression->name, get, codegen.function);

    return get;
  }
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static BinaryenExpressionRef generate_assignment_expression(AssignExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->value);
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  VarStmt* variable = expression->variable;
  if (variable)
  {
    switch (variable->scope)
    {
    case SCOPE_LOCAL:
      return BinaryenLocalTee(codegen.module, variable->index, value, type);

    case SCOPE_GLOBAL: {
      BinaryenExpressionRef list[] = {
        BinaryenGlobalSet(codegen.module, variable->name.lexeme, value),
        BinaryenGlobalGet(codegen.module, variable->name.lexeme, type),
      };

      return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
    }

    case SCOPE_CLASS: {
      BinaryenExpressionRef ref;
      if (expression->target->type == EXPR_ACCESS)
        ref = generate_expression(expression->target->access.expr);
      else
        ref = BinaryenLocalGet(codegen.module, 0, codegen.class);

      BinaryenExpressionRef list[] = {
        BinaryenStructSet(codegen.module, variable->index, ref, value),
        BinaryenStructGet(codegen.module, variable->index,
                          BinaryenExpressionCopy(ref, codegen.module), codegen.class, false),
      };

      generate_debug_info(expression->op, list[0], codegen.function);

      return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
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

    BinaryenExpressionRef ref = generate_expression(expression->target->index.expr);
    BinaryenExpressionRef index = generate_expression(expression->target->index.index);

    if (expression->target->index.expr_data_type.type == TYPE_OBJECT)
    {
      BinaryenExpressionRef operands[] = { ref, index, value };
      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                     sizeof(operands) / sizeof_ptr(operands),
                     data_type_to_binaryen_type(expression->target->index.data_type));

      generate_debug_info(expression->target->index.index_token, call, codegen.function);
      return call;
    }
    else
    {
      BinaryenExpressionRef array =
        BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeAuto(), false);
      BinaryenExpressionRef length = BinaryenStructGet(
        codegen.module, 1, BinaryenExpressionCopy(ref, codegen.module), BinaryenTypeInt32(), false);
      BinaryenExpressionRef bounded_index = BinaryenSelect(
        codegen.module, BinaryenBinary(codegen.module, BinaryenLtSInt32(), index, length),
        BinaryenExpressionCopy(index, codegen.module),
        BinaryenConst(codegen.module, BinaryenLiteralInt32(-1)));

      BinaryenExpressionRef list[] = {
        BinaryenArraySet(codegen.module, array, bounded_index, value),
        BinaryenArrayGet(codegen.module, BinaryenExpressionCopy(array, codegen.module),
                         BinaryenExpressionCopy(index, codegen.module), type, false),
      };

      generate_debug_info(expression->target->index.index_token, list[0], codegen.function);
      return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
    }
  }
}

static BinaryenExpressionRef generate_call_expression(CallExpr* expression)
{
  BinaryenType return_type = data_type_to_binaryen_type(expression->return_data_type);

  ArrayBinaryenExpressionRef arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    array_add(&arguments, generate_expression(argument));
  }

  BinaryenExpressionRef call;

  if (expression->callee_data_type.type == TYPE_ALIAS)
  {
    return generate_default_initialization(*expression->callee_data_type.alias.data_type);
  }
  else if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  {
    call = BinaryenCallRef(codegen.module, generate_expression(expression->callee), arguments.elems,
                           arguments.size, return_type, false);
  }
  else
  {
    const char* name;
    if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
      name = generate_function_internal(expression->callee_data_type);
    else if (expression->callee_data_type.type == TYPE_PROTOTYPE)
      name = expression->function_name;
    else
      name = expression->function->name.lexeme;

    call = BinaryenCall(codegen.module, name, arguments.elems, arguments.size, return_type);
  }

  generate_debug_info(expression->callee_token, call, codegen.function);
  return call;
}

static BinaryenExpressionRef generate_access_expression(AccessExpr* expression)
{
  BinaryenExpressionRef ref = generate_expression(expression->expr);

  if (expression->data_type.type == TYPE_FUNCTION ||
      expression->data_type.type == TYPE_FUNCTION_MEMBER ||
      expression->data_type.type == TYPE_FUNCTION_INTERNAL ||
      expression->data_type.type == TYPE_FUNCTION_GROUP)
  {
    return generate_function_pointer(expression->data_type);
  }

  if (expression->expr_data_type.type == TYPE_STRING)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      return BinaryenArrayLen(codegen.module, ref);
    }

    UNREACHABLE("Unhandled string access name");
  }
  else if (expression->expr_data_type.type == TYPE_ARRAY)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      return BinaryenStructGet(codegen.module, 1, ref, BinaryenTypeInt32(), false);
    }
    else if (strcmp(expression->name.lexeme, "capacity") == 0)
    {
      return BinaryenIf(
        codegen.module,
        BinaryenRefIsNull(codegen.module,
                          BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeInt32(), false)),
        BinaryenConst(codegen.module, BinaryenLiteralInt32(0)),
        BinaryenArrayLen(codegen.module,
                         BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeInt32(), false)));
    }

    UNREACHABLE("Unhandled array access name");
  }
  else
  {
    BinaryenType type = data_type_to_binaryen_type(expression->data_type);

    BinaryenExpressionRef access =
      BinaryenStructGet(codegen.module, expression->variable->index, ref, type, false);
    generate_debug_info(expression->name, access, codegen.function);

    return access;
  }
}

static BinaryenExpressionRef generate_index_expression(IndexExpr* expression)
{
  BinaryenExpressionRef ref = generate_expression(expression->expr);
  BinaryenExpressionRef index = generate_expression(expression->index);
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  switch (expression->expr_data_type.type)
  {
  case TYPE_STRING: {
    BinaryenExpressionRef get = BinaryenArrayGet(codegen.module, ref, index, type, false);

    generate_debug_info(expression->index_token, get, codegen.function);
    return get;
  }
  case TYPE_ARRAY: {
    BinaryenExpressionRef array =
      BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeAuto(), false);
    BinaryenExpressionRef length = BinaryenStructGet(
      codegen.module, 1, BinaryenExpressionCopy(ref, codegen.module), BinaryenTypeInt32(), false);
    BinaryenExpressionRef bounded_index = BinaryenSelect(
      codegen.module, BinaryenBinary(codegen.module, BinaryenLtSInt32(), index, length),
      BinaryenExpressionCopy(index, codegen.module),
      BinaryenConst(codegen.module, BinaryenLiteralInt32(-1)));

    BinaryenExpressionRef get = BinaryenArrayGet(codegen.module, array, bounded_index, type, false);
    generate_debug_info(expression->index_token, get, codegen.function);

    return get;
  }
  case TYPE_OBJECT: {
    BinaryenExpressionRef operands[] = {
      ref,
      index,
    };

    BinaryenExpressionRef call =
      BinaryenCall(codegen.module, expression->function->name.lexeme, operands,
                   sizeof(operands) / sizeof_ptr(operands), type);

    generate_debug_info(expression->index_token, call, codegen.function);
    return call;
  }
  default:
    UNREACHABLE("Unhandled index type");
  }
}

static BinaryenExpressionRef generate_array_expression(LiteralArrayExpr* expression)
{
  BinaryenHeapType type = generate_array_heap_binaryen_type(NULL, NULL, expression->data_type);
  BinaryenHeapType array_type = BinaryenTypeGetHeapType(BinaryenStructTypeGetFieldType(type, 0));

  ArrayBinaryenExpressionRef values;
  array_init(&values);

  Expr* value;
  array_foreach(&expression->values, value)
  {
    array_add(&values, generate_expression(value));
  }

  if (values.elems)
  {
    BinaryenExpressionRef operands[] = {
      BinaryenArrayNewFixed(codegen.module, array_type, values.elems, values.size),
      BinaryenConst(codegen.module, BinaryenLiteralInt32(values.size))
    };

    return BinaryenStructNew(codegen.module, operands, sizeof(operands) / sizeof_ptr(operands),
                             type);
  }
  else
  {
    return BinaryenStructNew(codegen.module, NULL, 0, type);
  }
}

static BinaryenExpressionRef generate_is_expression(IsExpr* expression)
{
  BinaryenExpressionRef ref = generate_expression(expression->expr);

  switch (expression->expr_data_type.type)
  {
  case TYPE_ANY:
    return BinaryenSelect(codegen.module, BinaryenRefIsNull(codegen.module, ref),
                          BinaryenConst(codegen.module, BinaryenLiteralInt32(0)),
                          BinaryenRefTest(codegen.module,
                                          BinaryenExpressionCopy(ref, codegen.module),
                                          data_type_to_binaryen_type(expression->is_data_type)));
  default:
    UNREACHABLE("Unexpected expression data type");
  }
}

static BinaryenExpressionRef generate_if_expression(IfExpr* expression)
{
  BinaryenExpressionRef condition = generate_expression(expression->condition);
  BinaryenExpressionRef ifTrue = generate_expression(expression->left);
  BinaryenExpressionRef ifFalse = generate_expression(expression->right);

  return BinaryenIf(codegen.module, condition, ifTrue, ifFalse);
}

static BinaryenExpressionRef generate_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    return generate_literal_expression(&expression->literal);
  case EXPR_BINARY:
    return generate_binary_expression(&expression->binary);
  case EXPR_GROUP:
    return generate_group_expression(&expression->group);
  case EXPR_UNARY:
    return generate_unary_expression(&expression->unary);
  case EXPR_CAST:
    return generate_cast_expression(&expression->cast);
  case EXPR_VAR:
    return generate_variable_expression(&expression->var);
  case EXPR_ASSIGN:
    return generate_assignment_expression(&expression->assign);
  case EXPR_CALL:
    return generate_call_expression(&expression->call);
  case EXPR_ACCESS:
    return generate_access_expression(&expression->access);
  case EXPR_INDEX:
    return generate_index_expression(&expression->index);
  case EXPR_ARRAY:
    return generate_array_expression(&expression->array);
  case EXPR_IS:
    return generate_is_expression(&expression->is);
  case EXPR_IF:
    return generate_if_expression(&expression->cond);

  default:
    UNREACHABLE("Unhandled expression");
  }
}

static BinaryenExpressionRef generate_expression_statement(ExprStmt* statement)
{
  BinaryenExpressionRef expression = generate_expression(statement->expr);
  if (statement->data_type.type != TYPE_VOID)
  {
    expression = BinaryenDrop(codegen.module, expression);
  }

  return expression;
}

static BinaryenExpressionRef generate_if_statement(IfStmt* statement)
{
  BinaryenExpressionRef condition = generate_expression(statement->condition);
  BinaryenExpressionRef ifTrue = generate_statements(&statement->then_branch);
  BinaryenExpressionRef ifFalse = NULL;

  if (statement->else_branch.elems)
  {
    ifFalse = generate_statements(&statement->else_branch);
  }

  return BinaryenIf(codegen.module, condition, ifTrue, ifFalse);
}

static BinaryenExpressionRef generate_while_statement(WhileStmt* statement)
{
  int previous_loop = codegen.loop;
  codegen.loop = statement->keyword.start_line << 16 | statement->keyword.start_column;

  const char* continue_name =
    memory_sprintf("continue:%d:%d", codegen.loop >> 16, codegen.loop & 0xFFFF);
  const char* break_name = memory_sprintf("break:%d:%d", codegen.loop >> 16, codegen.loop & 0xFFFF);
  const char* loop_name = memory_sprintf("loop:%d:%d", codegen.loop >> 16, codegen.loop & 0xFFFF);

  BinaryenExpressionRef continue_block = generate_statements(&statement->body);
  BinaryenBlockSetName(continue_block, continue_name);

  ArrayBinaryenExpressionRef loop_block_list;
  array_init(&loop_block_list);
  array_add(&loop_block_list, continue_block);
  array_add(&loop_block_list, generate_statements(&statement->incrementer));
  array_add(&loop_block_list, BinaryenBreak(codegen.module, loop_name, NULL, NULL));

  BinaryenExpressionRef loop_block = BinaryenBlock(codegen.module, NULL, loop_block_list.elems,
                                                   loop_block_list.size, BinaryenTypeNone());

  BinaryenExpressionRef loop_body =
    BinaryenIf(codegen.module, generate_expression(statement->condition), loop_block, NULL);
  BinaryenExpressionRef loop = BinaryenLoop(codegen.module, loop_name, loop_body);

  ArrayBinaryenExpressionRef block_list;
  array_init(&block_list);
  array_add(&block_list, generate_statements(&statement->initializer));
  array_add(&block_list, loop);

  codegen.loop = previous_loop;

  return BinaryenBlock(codegen.module, break_name, block_list.elems, block_list.size,
                       BinaryenTypeNone());
}

static BinaryenExpressionRef generate_return_statement(ReturnStmt* statement)
{
  BinaryenExpressionRef expression = NULL;
  if (statement->expr)
    expression = generate_expression(statement->expr);

  return BinaryenReturn(codegen.module, expression);
}

static BinaryenExpressionRef generate_continue_statement(void)
{
  const char* name = memory_sprintf("continue:%d:%d", codegen.loop >> 16, codegen.loop & 0xFFFF);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_break_statement(void)
{
  const char* name = memory_sprintf("break:%d:%d", codegen.loop >> 16, codegen.loop & 0xFFFF);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_variable_declaration(VarStmt* statement)
{
  BinaryenExpressionRef initializer = generate_default_initialization(statement->data_type);

  if (statement->scope == SCOPE_GLOBAL)
  {
    const char* name = statement->name.lexeme;
    BinaryenType type = data_type_to_binaryen_type(statement->data_type);
    BinaryenAddGlobal(codegen.module, name, type, true, initializer);

    if (statement->initializer)
    {
      return BinaryenGlobalSet(codegen.module, statement->name.lexeme,
                               generate_expression(statement->initializer));
    }
    else
    {
      return NULL;
    }
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    if (statement->initializer)
    {
      initializer = generate_expression(statement->initializer);
    }

    return BinaryenLocalSet(codegen.module, statement->index, initializer);
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static BinaryenExpressionRef generate_function_declaration(FuncStmt* statement)
{
  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);

  bool parameters_contain_string = false;
  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    BinaryenType parameter_data_type = data_type_to_binaryen_type(parameter->data_type);
    array_add(&parameter_types, parameter_data_type);

    if (parameter_data_type == codegen.string_type)
      parameters_contain_string = true;
  }

  ArrayBinaryenType variable_types;
  array_init(&variable_types);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType variable_data_type = data_type_to_binaryen_type(variable->data_type);
    array_add(&variable_types, variable_data_type);
  }

  const char* name = statement->name.lexeme;
  const char* previous_function = codegen.function;
  codegen.function = name;

  BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  BinaryenType results = data_type_to_binaryen_type(statement->data_type);

  if (statement->import)
  {
    BinaryenAddFunctionImport(codegen.module, name, statement->import, name, params, results);

    if (parameters_contain_string)
      generate_string_export_functions();

    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);
  }
  else
  {
    BinaryenExpressionRef body = generate_statements(&statement->body);
    BinaryenFunctionRef func = BinaryenAddFunction(codegen.module, name, params, results,
                                                   variable_types.elems, variable_types.size, body);
    BinaryenAddFunctionExport(codegen.module, name, name);

    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    BinaryenHeapType heap_type =
      generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);

    BinaryenFunctionSetType(func, heap_type);
  }

  codegen.function = previous_function;
  return NULL;
}

static BinaryenExpressionRef generate_function_template_declaration(FuncTemplateStmt* statement)
{
  FuncStmt* function_declaration;
  array_foreach(&statement->functions, function_declaration)
  {
    generate_function_declaration(function_declaration);
  }

  return NULL;
}

static void generate_class_body_declaration(ClassStmt* statement, BinaryenHeapType heap_type)
{
  ArrayFuncStmt initializer_functions = {
    .size = 0,
    .cap = 1,
    .elems = alloca(sizeof(FuncStmt)),
  };

  BinaryenType previous_class = codegen.class;
  codegen.class = statement->ref;

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

    const char* initalizer_name =
      initializer_functions.size > 1
        ? function_data_type_to_string(statement->name.lexeme,
                                       initializer_function->function_data_type)
        : statement->name.lexeme;

    const char* previous_function = codegen.function;
    codegen.function = initalizer_name;

    VarStmt* variable;
    ArrayBinaryenExpressionRef default_initializers;
    array_init(&default_initializers);
    array_foreach(&statement->variables, variable)
    {
      BinaryenExpressionRef default_initializer =
        generate_default_initialization(variable->data_type);
      array_add(&default_initializers, default_initializer);
    }

    ArrayBinaryenExpressionRef initializer_body;
    array_init(&initializer_body);
    array_add(&initializer_body,
              BinaryenLocalSet(codegen.module, 0,
                               BinaryenStructNew(codegen.module, default_initializers.elems,
                                                 default_initializers.size, heap_type)));

    array_foreach(&statement->variables, variable)
    {
      if (variable->initializer)
      {
        BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, statement->ref);
        BinaryenExpressionRef value = generate_expression(variable->initializer);
        array_add(&initializer_body,
                  BinaryenStructSet(codegen.module, variable->index, ref, value));
      }
    }

    ArrayBinaryenType parameter_types;
    array_init(&parameter_types);
    array_add(&parameter_types, statement->ref);

    if (initializer_function)
    {
      const char* initalizer_function_name = initializer_function->name.lexeme;

      ArrayBinaryenExpressionRef parameters;
      array_init(&parameters);
      array_add(&parameters, BinaryenLocalGet(codegen.module, 0, statement->ref));

      for (unsigned int i = 1; i < initializer_function->parameters.size; i++)
      {
        VarStmt* parameter = array_at(&initializer_function->parameters, i);
        BinaryenType parameter_type = data_type_to_binaryen_type(parameter->data_type);

        array_add(&parameters, BinaryenLocalGet(codegen.module, i, parameter_type));
        array_add(&parameter_types, parameter_type);
      }

      BinaryenExpressionRef call =
        BinaryenCall(codegen.module, initalizer_function_name, parameters.elems, parameters.size,
                     BinaryenTypeNone());
      generate_debug_info(initializer_function->name, call, codegen.function);

      array_add(&initializer_body, call);
    }

    array_add(&initializer_body, BinaryenLocalGet(codegen.module, 0, statement->ref));

    BinaryenType initializer_params =
      BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
    BinaryenExpressionRef initializer = BinaryenBlock(codegen.module, NULL, initializer_body.elems,
                                                      initializer_body.size, statement->ref);

    BinaryenAddFunction(codegen.module, initalizer_name, initializer_params, statement->ref, NULL,
                        0, initializer);
    BinaryenAddFunctionExport(codegen.module, initalizer_name, initalizer_name);

    codegen.function = previous_function;
    index++;

  } while (index < initializer_functions.size);

  codegen.class = previous_class;
}

static BinaryenExpressionRef generate_class_declaration(ClassStmt* statement,
                                                        TypeBuilderRef type_builder_ref,
                                                        ArrayTypeBuilderSubtype* subtypes)
{
  if (statement->ref)
    return NULL;

  TypeBuilderRef type_builder;
  BinaryenIndex index;

  if (type_builder_ref)
  {
    type_builder = type_builder_ref;
    index = TypeBuilderGetSize(type_builder);

    TypeBuilderGrow(type_builder, 1);
  }
  else
  {
    type_builder = TypeBuilderCreate(1);
    index = 0;
  }

  BinaryenHeapType temporary_heap_type = TypeBuilderGetTempHeapType(type_builder, index);
  BinaryenType temporary_type = TypeBuilderGetTempRefType(type_builder, temporary_heap_type, true);

  statement->ref = temporary_type;

  ArrayBinaryenType types;
  ArrayBinaryenPackedType packed_types;
  ArrayBool mutables;

  array_init(&types);
  array_init(&packed_types);
  array_init(&mutables);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType type =
      data_type_to_temporary_binaryen_type(type_builder, subtypes, variable->data_type);

    BinaryenPackedType packed_type = BinaryenPackedTypeNotPacked();
    bool mutable = true;

    array_add(&types, type);
    array_add(&packed_types, packed_type);
    array_add(&mutables, mutable);
  }

  TypeBuilderSetStructType(type_builder, index, types.elems, packed_types.elems, mutables.elems,
                           types.size);

  if (type_builder_ref)
  {
    TypeBuilderSubtype subtype = { .type = SUBTYPE_CLASS, .index = index, .class = statement };
    array_add(subtypes, subtype);
  }
  else
  {
    if (array_size(subtypes))
    {
      assert(TypeBuilderGetSize(type_builder) <= 128);
      TypeBuilderCreateRecGroup(type_builder, 0, TypeBuilderGetSize(type_builder));
    }

    BinaryenHeapType heap_types[128];
    TypeBuilderBuildAndDispose(type_builder, heap_types, 0, 0);

    BinaryenType type = BinaryenTypeFromHeapType(heap_types[0], true);
    statement->ref = type;

    TypeBuilderSubtype subtype;
    array_foreach(subtypes, subtype)
    {
      BinaryenHeapType heap_type = heap_types[subtype.index];

      if (subtype.type == SUBTYPE_ARRAY)
      {
        map_put_string_binaryen_heap_type(&codegen.heap_types, subtype.key, heap_type);
      }
      else if (subtype.type == SUBTYPE_FUNCTION)
      {
        map_put_string_binaryen_heap_type(&codegen.heap_types, subtype.key, heap_type);
      }
      else if (subtype.type == SUBTYPE_CLASS)
      {
        BinaryenType type = BinaryenTypeFromHeapType(heap_type, true);
        subtype.class->ref = type;
      }
      else
      {
        UNREACHABLE("Unexpected subtype.");
      }
    }

    array_foreach(subtypes, subtype)
    {
      BinaryenHeapType heap_type = heap_types[subtype.index];
      if (subtype.type == SUBTYPE_CLASS)
      {
        generate_class_body_declaration(subtype.class, heap_type);
      }
    }

    generate_class_body_declaration(statement, heap_types[0]);
  }

  return NULL;
}

static BinaryenExpressionRef generate_class_template_declaration(ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    generate_class_declaration(class_declaration, NULL, &subtypes);
  }

  return NULL;
}

static BinaryenExpressionRef generate_import_declaration(ImportStmt* statement)
{
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    generate_statement(body_statement);
  }

  return NULL;
}

static BinaryenExpressionRef generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    return generate_expression_statement(&statement->expr);
  case STMT_IF:
    return generate_if_statement(&statement->cond);
  case STMT_WHILE:
    return generate_while_statement(&statement->loop);
  case STMT_RETURN:
    return generate_return_statement(&statement->ret);
  case STMT_CONTINUE:
    return generate_continue_statement();
  case STMT_BREAK:
    return generate_break_statement();
  case STMT_VARIABLE_DECL:
    return generate_variable_declaration(&statement->var);
  case STMT_FUNCTION_DECL:
    return generate_function_declaration(&statement->func);
  case STMT_IMPORT_DECL:
    return generate_import_declaration(&statement->import);
  case STMT_CLASS_TEMPLATE_DECL:
    return generate_class_template_declaration(&statement->class_template);
  case STMT_FUNCTION_TEMPLATE_DECL:
    return generate_function_template_declaration(&statement->func_template);
  case STMT_CLASS_DECL: {
    ArrayTypeBuilderSubtype subtypes;
    array_init(&subtypes);

    return generate_class_declaration(&statement->class, NULL, &subtypes);
  }

  default:
    UNREACHABLE("Unhandled statement");
  }
}

static BinaryenExpressionRef generate_statements(ArrayStmt* statements)
{
  ArrayBinaryenExpressionRef list;
  array_init(&list);

  Stmt* statement;
  array_foreach(statements, statement)
  {
    BinaryenExpressionRef ref = generate_statement(statement);
    if (ref)
      array_add(&list, ref);
  }

  BinaryenExpressionRef block =
    BinaryenBlock(codegen.module, NULL, list.elems, list.size, BinaryenTypeAuto());

  return block;
}

int codegen_init(char* source,
                 void (*error_callback)(int start_line, int start_column, int end_line,
                                        int end_column, const char* message),
                 void (*result_callback)(size_t size, void* data, size_t source_map_size,
                                         void* source_map))
{
  lexer_init(source, error_callback);
  ArrayToken tokens = lexer_scan();

  if (lexer_errors())
  {
    memory_reset();
    return false;
  }

  parser_init(tokens, error_callback);
  ArrayStmt statements = parser_parse();

  if (parser_errors())
  {
    memory_reset();
    return false;
  }

  checker_init(statements, error_callback);
  checker_validate();

  if (checker_errors())
  {
    memory_reset();
    return false;
  }

  codegen.module = BinaryenModuleCreate();
  codegen.class = BinaryenTypeNone();
  codegen.statements = statements;
  codegen.loop = 0;
  codegen.strings = 0;
  codegen.function = "<start>";
  codegen.result_callback = result_callback;

  array_init(&codegen.debug_info);
  array_init(&codegen.global_local_types);

  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  TypeBuilderSetArrayType(type_builder, 0, BinaryenTypeInt32(), BinaryenPackedTypeInt8(), true);
  TypeBuilderBuildAndDispose(type_builder, &codegen.string_heap_type, 0, 0);

  codegen.string_type = BinaryenTypeFromHeapType(codegen.string_heap_type, false);
  map_init_sint(&codegen.string_constants, 0, 0);
  map_init_string_binaryen_heap_type(&codegen.heap_types, 0, 0);

  return true;
}

void codegen_generate(int logging)
{
  BinaryenExpressionRef body = generate_statements(&codegen.statements);

  VarStmt* statement;
  ArrayVarStmt statements = global_locals();
  array_foreach(&statements, statement)
  {
    BinaryenType type = data_type_to_binaryen_type(statement->data_type);
    array_add(&codegen.global_local_types, type);
  }

  BinaryenAddFunction(codegen.module, codegen.function, BinaryenTypeNone(), BinaryenTypeNone(),
                      codegen.global_local_types.elems, codegen.global_local_types.size, body);
  BinaryenAddFunctionExport(codegen.module, codegen.function, codegen.function);
  BinaryenModuleSetFeatures(codegen.module, BinaryenFeatureReferenceTypes() | BinaryenFeatureGC() |
                                              BinaryenFeatureNontrappingFPToInt());

  DebugInfo info;
  BinaryenIndex file = BinaryenModuleAddDebugInfoFileName(codegen.module, "");
  array_foreach(&codegen.debug_info, info)
  {
    BinaryenFunctionSetDebugLocation(BinaryenGetFunction(codegen.module, info.function),
                                     info.expression, file, info.token.start_line,
                                     info.token.start_column);
  }

  if (BinaryenModuleValidate(codegen.module))
  {
    BinaryenModuleOptimize(codegen.module);

    if (codegen.result_callback)
    {
      BinaryenModuleAllocateAndWriteResult result =
        BinaryenModuleAllocateAndWrite(codegen.module, "");

      codegen.result_callback(result.binaryBytes, result.binary, strlen(result.sourceMap),
                              result.sourceMap);
    }
  }

  if (logging)
    BinaryenModulePrint(codegen.module);

  BinaryenModuleDispose(codegen.module);
  memory_reset();
}

#include "codegen.h"
#include "array.h"
#include "checker.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "statement.h"

#include <binaryen-c.h>
#include <math.h>

typedef struct
{
  unsigned int hash;
  unsigned int index;
} HashIndex;

typedef struct
{
  const char* function;
  Token token;
  BinaryenExpressionRef expression;
} DebugInfo;

array_def(BinaryenExpressionRef, BinaryenExpressionRef);
array_def(BinaryenPackedType, BinaryenPackedType);
array_def(BinaryenHeapType, BinaryenHeapType);
array_def(BinaryenType, BinaryenType);
array_def(HashIndex, HashIndex);
array_def(DebugInfo, DebugInfo);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statement(Stmt* statement);
static BinaryenExpressionRef generate_statements(ArrayStmt* statements);

static BinaryenType data_type_to_binaryen_type(DataType data_type);
static BinaryenType data_type_to_temporary_binaryen_type(TypeBuilderRef type_builder_ref,
                                                         ArrayHashIndex* hash_indices,
                                                         DataType data_type);

static struct
{
  BinaryenModuleRef module;
  BinaryenType class;
  ArrayStmt statements;

  ArrayBinaryenType global_local_types;
  MapBinaryenHeapType heap_types;
  BinaryenHeapType string_heap_type;
  BinaryenType string_type;
  MapSInt string_constants;
  ArrayDebugInfo debug_info;

  const char* function;
  int strings;
  int loops;
} codegen;

static void generate_debug_info(Token token, BinaryenExpressionRef expression, const char* function)
{
  DebugInfo debug_info = { .expression = expression, .token = token, .function = function };
  array_add(&codegen.debug_info, debug_info);
}

static void generate_string_export_functions(void)
{
  if (!BinaryenGetExport(codegen.module, "string.length"))
    BinaryenAddFunctionExport(codegen.module, "string.length", "string.length");

  if (!BinaryenGetExport(codegen.module, "string.at"))
    BinaryenAddFunctionExport(codegen.module, "string.at", "string.at");
}

static void generate_string_bool_cast_function(void)
{
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.bool_cast";

  BinaryenExpressionRef false_string[] = {
    CONSTANT('f'), CONSTANT('a'), CONSTANT('l'), CONSTANT('s'), CONSTANT('e'),
  };

  BinaryenExpressionRef true_string[] = {
    CONSTANT('t'),
    CONSTANT('r'),
    CONSTANT('u'),
    CONSTANT('e'),
  };

  BinaryenExpressionRef value = BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32());
  BinaryenExpressionRef body =
    BinaryenSelect(codegen.module, value,
                   BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, true_string,
                                         sizeof(true_string) / sizeof_ptr(true_string)),
                   BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, false_string,
                                         sizeof(false_string) / sizeof_ptr(false_string)));

  BinaryenType params_list[] = { BinaryenTypeInt32() };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { codegen.string_type };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenAddFunction(codegen.module, name, params, results, NULL, 0, body);

#undef CONSTANT
}

static void generate_string_float_cast_function(void)
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
      BinaryenBreak(codegen.module, "string.float_cast.loop",
                    BinaryenBinary(codegen.module, BinaryenNeInt32(), DECIMAL_COUNT(), CONSTANT(0)),
                    NULL),
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
    minus_append = BinaryenIf(
      codegen.module, BinaryenBinary(codegen.module, BinaryenLtFloat32(), INPUT(), CONSTANTF(0.0f)),
      minus_append_body, NULL);
  }

  BinaryenExpressionRef inf_exit;
  {
    BinaryenExpressionRef inf[] = {
      CONSTANT('i'),
      CONSTANT('n'),
      CONSTANT('f'),
    };

    BinaryenExpressionRef negative_inf[] = {
      CONSTANT('-'),
      CONSTANT('i'),
      CONSTANT('n'),
      CONSTANT('f'),
    };

    inf_exit =
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenEqFloat32(),
                                BinaryenUnary(codegen.module, BinaryenAbsFloat32(), INPUT()),
                                CONSTANTF(INFINITY)),
                 BinaryenReturn(
                   codegen.module,
                   BinaryenSelect(
                     codegen.module,
                     BinaryenBinary(codegen.module, BinaryenLtFloat32(), INPUT(), CONSTANTF(0.0f)),
                     BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, negative_inf,
                                           sizeof(negative_inf) / sizeof_ptr(negative_inf)),
                     BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, inf,
                                           sizeof(inf) / sizeof_ptr(inf)))),
                 NULL);
  }

  BinaryenExpressionRef nan_exit;
  {
    BinaryenExpressionRef nan[] = {
      CONSTANT('n'),
      CONSTANT('a'),
      CONSTANT('n'),
    };

    BinaryenExpressionRef negative_nan[] = {
      CONSTANT('-'),
      CONSTANT('n'),
      CONSTANT('a'),
      CONSTANT('n'),
    };

    nan_exit = BinaryenIf(
      codegen.module, BinaryenBinary(codegen.module, BinaryenNeFloat32(), INPUT(), INPUT()),
      BinaryenReturn(
        codegen.module,
        BinaryenSelect(
          codegen.module,
          BinaryenBinary(codegen.module, BinaryenShrUInt32(),
                         BinaryenUnary(codegen.module, BinaryenReinterpretFloat32(), INPUT()),
                         CONSTANT(31)),
          BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, negative_nan,
                                sizeof(negative_nan) / sizeof_ptr(negative_nan)),
          BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, nan,
                                sizeof(nan) / sizeof_ptr(nan)))),
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
      BinaryenUnary(codegen.module, BinaryenTruncSatSFloat32ToInt32(),
                    BinaryenBinary(
                      codegen.module, BinaryenMulFloat32(), CONSTANTF(1000000.0f),
                      BinaryenBinary(codegen.module, BinaryenSubFloat32(),
                                     BinaryenUnary(codegen.module, BinaryenAbsFloat32(), INPUT()),
                                     BinaryenUnary(codegen.module, BinaryenConvertSInt64ToFloat32(),
                                                   INT_NUMBER()))))),
    BinaryenLocalSet(
      codegen.module, 3,
      BinaryenArrayNew(codegen.module, codegen.string_heap_type, CONSTANT(size), NULL)),
    BinaryenLocalSet(codegen.module, 4, CONSTANT(size)),
    BinaryenLocalSet(codegen.module, 6, CONSTANT(decimals)),

    decimal_loop,
    dot_append,
    integer_loop,
    minus_append,

    BinaryenLocalSet(
      codegen.module, 5,
      BinaryenArrayNew(codegen.module, codegen.string_heap_type,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), CONSTANT(size), INDEX()),
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
#undef INPUT
#undef INT_NUMBER
#undef FLOAT_NUMBER
#undef BUFFER
#undef INDEX
#undef RESULT
#undef CONSTANT
#undef CONSTANTF
}

static void generate_string_int_cast_function(void)
{
#define NUMBER() (BinaryenLocalGet(codegen.module, 0, BinaryenTypeInt32()))
#define BUFFER() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define INDEX() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define RESULT() (BinaryenLocalGet(codegen.module, 3, codegen.string_type))
#define IS_NEGATIVE() (BinaryenLocalGet(codegen.module, 4, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.int_cast";
  const int size = 32;
  const int base = 10;

  BinaryenExpressionRef divider =
    BinaryenBinary(codegen.module, BinaryenRemUInt32(), NUMBER(), CONSTANT(base));
  BinaryenExpressionRef adder =
    BinaryenBinary(codegen.module, BinaryenAddInt32(), divider, CONSTANT('0'));

  BinaryenExpressionRef loop_body_list[] = {
    BinaryenLocalSet(codegen.module, 2,
                     BinaryenBinary(codegen.module, BinaryenSubInt32(), INDEX(), CONSTANT(1))),
    BinaryenArraySet(codegen.module, BUFFER(), INDEX(), adder),
    BinaryenLocalSet(codegen.module, 0,
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
  BinaryenExpressionRef negative_append_body = BinaryenBlock(
    codegen.module, NULL, negative_append_body_list,
    sizeof(negative_append_body_list) / sizeof_ptr(negative_append_body_list), BinaryenTypeNone());
  BinaryenExpressionRef negative_append =
    BinaryenIf(codegen.module, IS_NEGATIVE(), negative_append_body, NULL);

  BinaryenExpressionRef body_list[] = {
    negative_check,
    BinaryenLocalSet(
      codegen.module, 1,
      BinaryenArrayNew(codegen.module, codegen.string_heap_type, CONSTANT(size), NULL)),
    BinaryenLocalSet(codegen.module, 2, CONSTANT(size)),
    loop,
    negative_append,
    BinaryenLocalSet(
      codegen.module, 3,
      BinaryenArrayNew(codegen.module, codegen.string_heap_type,
                       BinaryenBinary(codegen.module, BinaryenSubInt32(), CONSTANT(size), INDEX()),
                       NULL)),
    BinaryenArrayCopy(codegen.module, RESULT(), CONSTANT(0), BUFFER(), INDEX(),
                      BinaryenArrayLen(codegen.module, RESULT())),
    RESULT(),
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

  BinaryenType vars_list[] = { codegen.string_type, BinaryenTypeInt32(), codegen.string_type,
                               BinaryenTypeInt32() };

  BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                      sizeof(vars_list) / sizeof_ptr(vars_list), body);

#undef NUMBER
#undef BUFFER
#undef INDEX
#undef RESULT
#undef IS_NEGATIVE
#undef CONSTANT
}

static void generate_string_concat_function(void)
{
  const char* name = "string.concat";

  BinaryenExpressionRef left = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
  BinaryenExpressionRef right = BinaryenLocalGet(codegen.module, 1, codegen.string_type);

  BinaryenExpressionRef left_length = BinaryenArrayLen(codegen.module, left);
  BinaryenExpressionRef right_length = BinaryenArrayLen(codegen.module, right);

  BinaryenExpressionRef length =
    BinaryenBinary(codegen.module, BinaryenAddInt32(), left_length, right_length);

  BinaryenExpressionRef index = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
  BinaryenExpressionRef clone =
    BinaryenArrayNew(codegen.module, codegen.string_heap_type, length, NULL);

  BinaryenExpressionRef body_list[] = {
    BinaryenLocalSet(codegen.module, 2, clone),
    BinaryenArrayCopy(codegen.module, BinaryenLocalGet(codegen.module, 2, codegen.string_type),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(left, codegen.module),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(left_length, codegen.module)),

    BinaryenArrayCopy(codegen.module, BinaryenLocalGet(codegen.module, 2, codegen.string_type),
                      BinaryenExpressionCopy(left_length, codegen.module),
                      BinaryenExpressionCopy(right, codegen.module),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(right_length, codegen.module)),

    BinaryenLocalGet(codegen.module, 2, codegen.string_type)
  };

  BinaryenExpressionRef body =
    BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                  codegen.string_type);

  BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { codegen.string_type };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenAddFunction(codegen.module, name, params, results, &codegen.string_type, 1, body);
}

static void generate_string_equals_function(void)
{
#define LEFT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define RIGHT() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.equals";
  const char* loop_name = "string.equals.loop";

  BinaryenExpressionRef check_value_condition = BinaryenBinary(
    codegen.module, BinaryenNeInt32(),
    BinaryenArrayGet(codegen.module, LEFT(), COUNTER(), BinaryenTypeInt32(), false),
    BinaryenArrayGet(codegen.module, RIGHT(), COUNTER(), BinaryenTypeInt32(), false));
  BinaryenExpressionRef check_value_body = BinaryenReturn(codegen.module, CONSTANT(0));
  BinaryenExpressionRef check_value =
    BinaryenIf(codegen.module, check_value_condition, check_value_body, NULL);

  BinaryenExpressionRef increment_count = BinaryenLocalSet(
    codegen.module, 2, BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1)));

  BinaryenExpressionRef branch = BinaryenBreak(codegen.module, loop_name, NULL, NULL);

  BinaryenExpressionRef counter_body_list[] = { check_value, increment_count, branch };
  BinaryenExpressionRef counter_body =
    BinaryenBlock(codegen.module, NULL, counter_body_list,
                  sizeof(counter_body_list) / sizeof_ptr(counter_body_list), BinaryenTypeNone());
  BinaryenExpressionRef counter_condition = BinaryenBinary(
    codegen.module, BinaryenLtSInt32(), COUNTER(), BinaryenArrayLen(codegen.module, LEFT()));
  BinaryenExpressionRef counter = BinaryenIf(codegen.module, counter_condition, counter_body, NULL);

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
  BinaryenExpressionRef body = BinaryenBlock(
    codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list), BinaryenTypeNone());

  BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { BinaryenTypeInt32() };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenType vars_list[] = { BinaryenTypeInt32() };

  BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                      sizeof(vars_list) / sizeof_ptr(vars_list), body);

#undef LEFT
#undef RIGHT
#undef COUNTER
#undef CONSTANT
}

static void generate_string_length_function(void)
{
  const char* name = "string.length";

  BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
  BinaryenAddFunction(codegen.module, name, codegen.string_type, BinaryenTypeInt32(), NULL, 0,
                      BinaryenArrayLen(codegen.module, ref));
}

static void generate_string_at_function(void)
{
  const char* name = "string.at";

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

static BinaryenHeapType generate_array_heap_binaryen_type(TypeBuilderRef type_builder_ref,
                                                          ArrayHashIndex* hash_indices,
                                                          DataType data_type)
{
  unsigned int hash = array_data_type_hash(data_type);
  BinaryenHeapType array_binaryen_type = map_get_binaryen_heap_type(&codegen.heap_types, hash);

  if (!array_binaryen_type)
  {
    BinaryenType element_data_type;
    TypeBuilderRef type_builder;
    int offset;

    if (type_builder_ref)
    {
      type_builder = type_builder_ref;
      element_data_type = data_type_to_temporary_binaryen_type(type_builder_ref, hash_indices,
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

    BinaryenPackedType packed_type = BinaryenPackedTypeNotPacked();
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
      HashIndex hash_index = { .hash = hash, .index = offset + 1 };
      array_add(hash_indices, hash_index);

      array_binaryen_type = TypeBuilderGetTempHeapType(type_builder, offset + 1);
    }
    else
    {
      BinaryenHeapType heap_types[2];
      TypeBuilderBuildAndDispose(type_builder, heap_types, 0, 0);

      array_binaryen_type = heap_types[1];
    }

    map_put_binaryen_heap_type(&codegen.heap_types, hash, array_binaryen_type);
  }

  return array_binaryen_type;
}

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
    return BinaryenTypeNone();
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();
  case TYPE_STRING:
    return codegen.string_type;
  case TYPE_OBJECT:
    return data_type.class->ref;
  case TYPE_ARRAY:
    return BinaryenTypeFromHeapType(generate_array_heap_binaryen_type(NULL, NULL, data_type), true);
  default:
    UNREACHABLE("Unhandled data type");
  }
}

static BinaryenType data_type_to_temporary_binaryen_type(TypeBuilderRef type_builder_ref,
                                                         ArrayHashIndex* hash_indices,
                                                         DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_ARRAY:
    return TypeBuilderGetTempRefType(
      type_builder_ref,
      generate_array_heap_binaryen_type(type_builder_ref, hash_indices, data_type), true);
  default:
    return data_type_to_binaryen_type(data_type);
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
  case TYPE_OBJECT:
    return BinaryenRefNull(codegen.module, data_type.class->ref);
  case TYPE_STRING:
    return BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, NULL, 0);
  case TYPE_ARRAY:
    return BinaryenStructNew(codegen.module, NULL, 0,
                             generate_array_heap_binaryen_type(NULL, NULL, data_type));
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static const char* generate_array_push_function(DataType callee_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, value_type))
#define TEMP() (BinaryenLocalGet(codegen.module, 2, array_type))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type =
    array_data_type_element(array_at(&callee_data_type.function_internal.parameter_types, 0));

  BinaryenType this_type =
    data_type_to_binaryen_type(array_at(&callee_data_type.function_internal.parameter_types, 0));
  BinaryenType value_type =
    data_type_to_binaryen_type(array_at(&callee_data_type.function_internal.parameter_types, 1));

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

static const char* generate_array_pop_function(DataType callee_data_type)
{
#define THIS() (BinaryenLocalGet(codegen.module, 0, this_type))
#define VALUE() (BinaryenLocalGet(codegen.module, 1, value_type))
#define ARRAY() (BinaryenStructGet(codegen.module, 0, THIS(), BinaryenTypeAuto(), false))
#define CAPACITY() (BinaryenArrayLen(codegen.module, ARRAY()))
#define SIZE() (BinaryenStructGet(codegen.module, 1, THIS(), BinaryenTypeInt32(), false))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  DataType element_data_type =
    array_data_type_element(array_at(&callee_data_type.function_internal.parameter_types, 0));

  BinaryenType this_type =
    data_type_to_binaryen_type(array_at(&callee_data_type.function_internal.parameter_types, 0));
  BinaryenType element_type = data_type_to_binaryen_type(element_data_type);

  const char* name = memory_sprintf("array.pop.%d", this_type);

  if (!BinaryenGetFunction(codegen.module, name))
  {
    BinaryenExpressionRef body_list[] = {
      BinaryenIf(codegen.module,
                 BinaryenBinary(codegen.module, BinaryenLeSInt32(), SIZE(), CONSTANT(0)),
                 BinaryenUnreachable(codegen.module), NULL),

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
      BinaryenBlock(codegen.module, "NULL", body_list, sizeof(body_list) / sizeof_ptr(body_list),
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

static BinaryenExpressionRef generate_group_expression(GroupExpr* expression)
{
  return generate_expression(expression->expr);
}

static BinaryenExpressionRef generate_string_literal_expression(const char* literal)
{
  int index = map_get_sint(&codegen.string_constants, literal);
  if (!index)
  {
    index = ++codegen.strings;
    map_put_sint(&codegen.string_constants, literal, index);

    ArrayBinaryenExpressionRef values;
    array_init(&values);

    size_t length = strlen(literal);
    for (size_t i = 0; i < length; i++)
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
  case TYPE_OBJECT:
    return BinaryenRefNull(codegen.module, BinaryenTypeNullref());
  case TYPE_CHAR:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->string[0]));
  case TYPE_STRING:
    return generate_string_literal_expression(expression->string);
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
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenAddInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenAddFloat32();
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenCall(codegen.module, "string.concat", operands,
                          sizeof(operands) / sizeof_ptr(operands), codegen.string_type);
    }
    else
      UNREACHABLE("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenSubInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenSubFloat32();
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenMulInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenMulFloat32();
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenDivSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenDivFloat32();
    else
      UNREACHABLE("Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenRemSInt32();
    else
      UNREACHABLE("Unsupported binary type for %");

    break;

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = BinaryenEqInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else if (data_type.type == TYPE_OBJECT)
      return BinaryenRefEq(codegen.module, left, right);
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenCall(codegen.module, "string.equals", operands,
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
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenRefEq(codegen.module, left, right));
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenCall(codegen.module, "string.equals", operands,
                                        sizeof(operands) / sizeof_ptr(operands),
                                        BinaryenTypeInt32()));
    }
    else
      UNREACHABLE("Unsupported binary type for !=");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenLeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLeFloat32();
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenGeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGeFloat32();
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenLtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLtFloat32();
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenGtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGtFloat32();
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

  if (expression->from_data_type.type == expression->to_data_type.type)
  {
    return value;
  }
  else if (expression->to_data_type.type == TYPE_FLOAT &&
           expression->from_data_type.type == TYPE_INTEGER)
  {
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
  }
  else if (expression->to_data_type.type == TYPE_STRING)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_BOOL:
      return BinaryenCall(codegen.module, "string.bool_cast", &value, 1, codegen.string_type);
    case TYPE_FLOAT:
      return BinaryenCall(codegen.module, "string.float_cast", &value, 1, codegen.string_type);
    case TYPE_INTEGER:
      return BinaryenCall(codegen.module, "string.int_cast", &value, 1, codegen.string_type);
    case TYPE_CHAR:
      return BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, &value, 1);
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
    case TYPE_OBJECT:
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenRefIsNull(codegen.module, value));
    default:
      break;
    }
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

  Scope scope = expression->variable->scope;
  switch (scope)
  {
  case SCOPE_LOCAL:
  case SCOPE_GLOBAL_LOCAL:
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
    case SCOPE_GLOBAL_LOCAL:
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
    if (expression->target->type == EXPR_INDEX)
    {
      BinaryenExpressionRef ref = generate_expression(expression->target->index.expr);
      BinaryenExpressionRef index = generate_expression(expression->target->index.index);

      BinaryenExpressionRef array =
        BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeAuto(), false);
      BinaryenExpressionRef length = BinaryenStructGet(
        codegen.module, 1, BinaryenExpressionCopy(ref, codegen.module), BinaryenTypeInt32(), false);

      BinaryenExpressionRef unreachable = BinaryenUnreachable(codegen.module);
      BinaryenExpressionRef check = BinaryenIf(
        codegen.module, BinaryenBinary(codegen.module, BinaryenGeSInt32(), index, length),
        unreachable, NULL);

      BinaryenExpressionRef list[] = {
        check,
        BinaryenArraySet(codegen.module, array, BinaryenExpressionCopy(index, codegen.module),
                         value),
        BinaryenArrayGet(codegen.module, BinaryenExpressionCopy(array, codegen.module),
                         BinaryenExpressionCopy(index, codegen.module), BinaryenTypeAuto(), false),
      };

      generate_debug_info(expression->target->index.index_token, unreachable, codegen.function);
      generate_debug_info(expression->target->index.index_token, list[1], codegen.function);

      return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list),
                           BinaryenTypeAuto());
    }

    UNREACHABLE("Unhandled expression type");
  }
}

static BinaryenExpressionRef generate_call_expression(CallExpr* expression)
{
  const char* name;
  DataType callee_data_type = expression->callee_data_type;

  switch (callee_data_type.type)
  {
  case TYPE_PROTOTYPE:
    name = callee_data_type.class->name.lexeme;

    if (callee_data_type.class->initializer_function)
      generate_debug_info(expression->callee_token, callee_data_type.class->initializer_function,
                          name);

    break;
  case TYPE_FUNCTION:
    name = callee_data_type.function->name.lexeme;
    break;
  case TYPE_FUNCTION_MEMBER:
    name = callee_data_type.function_member.function->name.lexeme;
    break;
  case TYPE_FUNCTION_INTERNAL:
    name = callee_data_type.function_internal.name;

    if (strcmp(name, "array.push") == 0)
      name = generate_array_push_function(callee_data_type);
    else if (strcmp(name, "array.pop") == 0)
      name = generate_array_pop_function(callee_data_type);
    else if (strcmp(name, "int.hash") == 0)
      name = generate_int_hash_function();
    else if (strcmp(name, "float.hash") == 0)
      name = generate_float_hash_function();
    else if (strcmp(name, "string.hash") == 0)
      name = generate_string_hash_function();
    else
      UNREACHABLE("Unhandled function internal");

    break;
  case TYPE_ALIAS:
    return generate_default_initialization(*callee_data_type.alias.data_type);
  default:
    UNREACHABLE("Unhandled data type");
  }

  BinaryenType return_type = data_type_to_binaryen_type(expression->return_data_type);

  ArrayBinaryenExpressionRef arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    array_add(&arguments, generate_expression(argument));
  }

  BinaryenExpressionRef call =
    BinaryenCall(codegen.module, name, arguments.elems, arguments.size, return_type);
  generate_debug_info(expression->callee_token, call, codegen.function);

  return call;
}

static BinaryenExpressionRef generate_access_expression(AccessExpr* expression)
{
  BinaryenExpressionRef ref = generate_expression(expression->expr);

  if (expression->expr_data_type.type == TYPE_STRING)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      return BinaryenCall(codegen.module, "string.length", &ref, 1, BinaryenTypeInt32());
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
      return BinaryenArrayLen(
        codegen.module, BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeInt32(), false));
    }

    UNREACHABLE("Unhandled array access name");
  }
  else
  {
    BinaryenType type = data_type_to_binaryen_type(expression->data_type);
    BinaryenExpressionRef access =
      BinaryenStructGet(codegen.module, expression->variable->index, ref, type, false);
    generate_debug_info(expression->expr_token, access, codegen.function);

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
    BinaryenExpressionRef operands[] = { ref, index };
    return BinaryenCall(codegen.module, "string.at", operands,
                        sizeof(operands) / sizeof_ptr(operands), type);
  }
  case TYPE_ARRAY: {
    BinaryenExpressionRef array =
      BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeAuto(), false);
    BinaryenExpressionRef length = BinaryenStructGet(
      codegen.module, 1, BinaryenExpressionCopy(ref, codegen.module), BinaryenTypeInt32(), false);

    BinaryenExpressionRef unreachable = BinaryenUnreachable(codegen.module);
    BinaryenExpressionRef check =
      BinaryenIf(codegen.module, BinaryenBinary(codegen.module, BinaryenGeSInt32(), index, length),
                 unreachable, NULL);

    BinaryenExpressionRef list[] = {
      check,
      BinaryenArrayGet(codegen.module, array, BinaryenExpressionCopy(index, codegen.module),
                       BinaryenTypeAuto(), false),
    };

    generate_debug_info(expression->index_token, unreachable, codegen.function);
    generate_debug_info(expression->index_token, list[1], codegen.function);

    return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list),
                         BinaryenTypeAuto());
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
  codegen.loops++;

  const char* continue_name = memory_sprintf("continue|%d", codegen.loops);
  const char* break_name = memory_sprintf("break|%d", codegen.loops);
  const char* loop_name = memory_sprintf("loop|%d", codegen.loops);

  BinaryenExpressionRef continue_block = generate_statements(&statement->body);
  BinaryenBlockSetName(continue_block, continue_name);

  ArrayBinaryenExpressionRef loop_block_list;
  array_init(&loop_block_list);
  array_add(&loop_block_list, continue_block);

  if (statement->incrementer)
  {
    array_add(&loop_block_list, generate_statement(statement->incrementer));
  }

  array_add(&loop_block_list, BinaryenBreak(codegen.module, loop_name, NULL, NULL));

  BinaryenExpressionRef loop_block = BinaryenBlock(codegen.module, NULL, loop_block_list.elems,
                                                   loop_block_list.size, BinaryenTypeNone());

  BinaryenExpressionRef loop_body =
    BinaryenIf(codegen.module, generate_expression(statement->condition), loop_block, NULL);
  BinaryenExpressionRef loop = BinaryenLoop(codegen.module, loop_name, loop_body);

  ArrayBinaryenExpressionRef block_list;
  array_init(&block_list);

  if (statement->initializer)
  {
    array_add(&block_list, generate_statement(statement->initializer));
  }

  array_add(&block_list, loop);

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
  const char* name = memory_sprintf("continue|%d", codegen.loops);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_break_statement(void)
{
  const char* name = memory_sprintf("break|%d", codegen.loops);
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
  else if (statement->scope == SCOPE_LOCAL || statement->scope == SCOPE_GLOBAL_LOCAL)
  {
    if (statement->scope == SCOPE_GLOBAL_LOCAL)
    {
      BinaryenType type = data_type_to_binaryen_type(statement->data_type);
      array_add(&codegen.global_local_types, type);
    }

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

  if (statement->import.type == TOKEN_STRING)
  {
    BinaryenAddFunctionImport(codegen.module, name, statement->import.lexeme, name, params,
                              results);

    if (parameters_contain_string)
      generate_string_export_functions();
  }
  else
  {
    BinaryenExpressionRef body = generate_statements(&statement->body);
    BinaryenAddFunction(codegen.module, name, params, results, variable_types.elems,
                        variable_types.size, body);
    BinaryenAddFunctionExport(codegen.module, name, name);
  }

  codegen.function = previous_function;
  return NULL;
}

static BinaryenExpressionRef generate_class_declaration(ClassStmt* statement)
{
  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  BinaryenHeapType temporary_heap_type = TypeBuilderGetTempHeapType(type_builder, 0);
  BinaryenType temporary_type = TypeBuilderGetTempRefType(type_builder, temporary_heap_type, true);

  statement->ref = temporary_type;

  ArrayBinaryenType types;
  ArrayBinaryenPackedType packed_types;
  ArrayBool mutables;
  ArrayHashIndex hash_indices;

  array_init(&types);
  array_init(&packed_types);
  array_init(&mutables);
  array_init(&hash_indices);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType type =
      data_type_to_temporary_binaryen_type(type_builder, &hash_indices, variable->data_type);

    BinaryenPackedType packed_type = BinaryenPackedTypeNotPacked();
    bool mutable = true;

    array_add(&types, type);
    array_add(&packed_types, packed_type);
    array_add(&mutables, mutable);
  }

  TypeBuilderSetStructType(type_builder, 0, types.elems, packed_types.elems, mutables.elems,
                           types.size);

  if (array_size(&hash_indices))
  {
    assert(TypeBuilderGetSize(type_builder) <= 128);
    TypeBuilderCreateRecGroup(type_builder, 0, TypeBuilderGetSize(type_builder));
  }

  BinaryenHeapType heap_types[128];
  TypeBuilderBuildAndDispose(type_builder, heap_types, 0, 0);

  HashIndex hash_index;
  array_foreach(&hash_indices, hash_index)
  {
    map_put_binaryen_heap_type(&codegen.heap_types, hash_index.hash, heap_types[hash_index.index]);
  }

  BinaryenHeapType heap_type = heap_types[0];
  BinaryenType type = BinaryenTypeFromHeapType(heap_type, true);
  statement->ref = type;

  BinaryenType previous_class = codegen.class;
  codegen.class = type;

  const char* initalizer_name = statement->name.lexeme;
  const char* initalizer_function_name = memory_sprintf("%s.__init__", initalizer_name);

  FuncStmt* function;
  FuncStmt* initializer_function = NULL;

  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name.lexeme, initalizer_function_name) == 0)
      initializer_function = function;

    generate_function_declaration(function);
  }

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
      BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, type);
      BinaryenExpressionRef value = generate_expression(variable->initializer);
      array_add(&initializer_body, BinaryenStructSet(codegen.module, variable->index, ref, value));
    }
  }

  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);
  array_add(&parameter_types, type);

  if (initializer_function)
  {
    ArrayBinaryenExpressionRef parameters;
    array_init(&parameters);
    array_add(&parameters, BinaryenLocalGet(codegen.module, 0, type));

    for (unsigned int i = 1; i < initializer_function->parameters.size; i++)
    {
      VarStmt* parameter = array_at(&initializer_function->parameters, i);
      BinaryenType parameter_type = data_type_to_binaryen_type(parameter->data_type);

      array_add(&parameters, BinaryenLocalGet(codegen.module, i, parameter_type));
      array_add(&parameter_types, parameter_type);
    }

    statement->initializer_function =
      BinaryenCall(codegen.module, initalizer_function_name, parameters.elems, parameters.size,
                   BinaryenTypeNone());
    array_add(&initializer_body, statement->initializer_function);
  }

  array_add(&initializer_body, BinaryenLocalGet(codegen.module, 0, type));

  BinaryenType initializer_params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  BinaryenExpressionRef initializer =
    BinaryenBlock(codegen.module, NULL, initializer_body.elems, initializer_body.size, type);

  BinaryenAddFunction(codegen.module, initalizer_name, initializer_params, type, NULL, 0,
                      initializer);

  codegen.class = previous_class;
  return NULL;
}

static BinaryenExpressionRef generate_class_template_declaration(ClassTemplateStmt* statement)
{
  ClassStmt* class_declaration;
  array_foreach(&statement->classes, class_declaration)
  {
    generate_class_declaration(class_declaration);
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

  case STMT_CLASS_DECL:
  case STMT_CLASS_TEMPLATE_DECL:
    return NULL;

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
    if (statement->type == STMT_CLASS_DECL)
      generate_class_declaration(&statement->class);
  }

  array_foreach(statements, statement)
  {
    if (statement->type == STMT_CLASS_TEMPLATE_DECL)
      generate_class_template_declaration(&statement->class_template);
  }

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

void codegen_init(ArrayStmt statements)
{
  codegen.module = BinaryenModuleCreate();
  codegen.class = BinaryenTypeNone();
  codegen.statements = statements;
  codegen.loops = -1;
  codegen.strings = 0;
  codegen.function = "<start>";

  array_init(&codegen.debug_info);
  array_init(&codegen.global_local_types);

  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  TypeBuilderSetArrayType(type_builder, 0, BinaryenTypeInt32(), BinaryenPackedTypeInt8(), true);
  TypeBuilderBuildAndDispose(type_builder, &codegen.string_heap_type, 0, 0);

  codegen.string_type = BinaryenTypeFromHeapType(codegen.string_heap_type, false);
  map_init_sint(&codegen.string_constants, 0, 0);
  map_init_binaryen_heap_type(&codegen.heap_types, 0, 0);

  generate_string_concat_function();
  generate_string_equals_function();
  generate_string_length_function();
  generate_string_at_function();
  generate_string_bool_cast_function();
  generate_string_float_cast_function();
  generate_string_int_cast_function();
}

Codegen codegen_generate(void)
{
  Codegen result = { 0 };

  BinaryenExpressionRef body = generate_statements(&codegen.statements);
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

    BinaryenModuleAllocateAndWriteResult binaryen_result =
      BinaryenModuleAllocateAndWrite(codegen.module, "");

    result.data = binaryen_result.binary;
    result.size = binaryen_result.binaryBytes;
    result.source_map = binaryen_result.sourceMap;
    result.source_map_size = strlen(binaryen_result.sourceMap);
  }

  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);

  return result;
}

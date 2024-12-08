#include "codegen.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "map.h"
#include "statement.h"

#include <assert.h>
#include <binaryen-c.h>
#include <stdio.h>

array_def(BinaryenExpressionRef, BinaryenExpressionRef);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statement(Stmt* statement);

static struct
{
  BinaryenModuleRef module;
  ArrayStmt statements;
  MapUint variables;
} codegen;

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type)
  {
  case TYPE_VOID:
  case TYPE_NULL:
    return BinaryenTypeNone();
  case TYPE_BOOL:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();

  default:
    assert(!"Unhanled data type");
  }
}

static BinaryenExpressionRef generate_literal_expression(Expr* expression)
{
  switch (expression->literal.data_type)
  {
  case TYPE_INTEGER:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->literal.integer));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(expression->literal.floating));
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->literal.boolean));

  default:
    assert(!"Unhandled literal value");
  }
}

static BinaryenExpressionRef generate_binary_expression(Expr* expression)
{
  BinaryenExpressionRef left = generate_expression(expression->binary.left);
  BinaryenExpressionRef right = generate_expression(expression->binary.right);
  BinaryenOp op = 0;

  DataType data_type = expression->binary.data_type;

  switch (expression->binary.op.type)
  {
  case TOKEN_PLUS:
    if (data_type == TYPE_INTEGER)
      op = BinaryenAddInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenAddFloat32();
    else
      assert(!"Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type == TYPE_INTEGER)
      op = BinaryenSubInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenSubFloat32();
    else
      assert(!"Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type == TYPE_INTEGER)
      op = BinaryenMulInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenMulFloat32();
    else
      assert(!"Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type == TYPE_INTEGER)
      op = BinaryenDivSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenDivFloat32();
    else
      assert(!"Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
    if (data_type == TYPE_INTEGER)
      op = BinaryenRemSInt32();
    else
      assert(!"Unsupported binary type for %");

    break;

  case TOKEN_EQUAL_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenEqInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else
      assert(!"Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenNeInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenNeFloat32();
    else
      assert(!"Unsupported binary type for ==");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenLeSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenLeFloat32();
    else
      assert(!"Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenGeSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenGeFloat32();
    else
      assert(!"Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenLtSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenLtFloat32();
    else
      assert(!"Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenGtSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenGtFloat32();
    else
      assert(!"Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type == TYPE_BOOL)
    {
      BinaryenExpressionRef zero = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
      BinaryenExpressionRef ifTrue = right;
      BinaryenExpressionRef ifFalse = zero;

      return BinaryenIf(codegen.module, left, ifTrue, ifFalse);
    }
    else
      assert(!"Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type == TYPE_BOOL)
    {
      BinaryenExpressionRef one = BinaryenConst(codegen.module, BinaryenLiteralInt32(1));
      BinaryenExpressionRef ifTrue = one;
      BinaryenExpressionRef ifFalse = right;

      return BinaryenIf(codegen.module, left, ifTrue, ifFalse);
    }
    else
      assert(!"Unsupported binary type for OR");

    break;

  default:
    assert(!"Unhandled binary operation");
    break;
  }

  return BinaryenBinary(codegen.module, op, left, right);
}

static BinaryenExpressionRef generate_unary_expression(Expr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->unary.expr);

  switch (expression->unary.op.type)
  {
  case TOKEN_MINUS:
    if (expression->data_type == TYPE_INTEGER)
      return BinaryenBinary(codegen.module, BinaryenSubInt32(),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)), value);
    else if (expression->data_type == TYPE_FLOAT)
      return BinaryenUnary(codegen.module, BinaryenNegFloat32(), value);
    else
      assert(!"Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type == TYPE_BOOL)
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(), value);
    else
      assert(!"Unsupported unary type for !");

  default:
    assert(!"Unhandled unary expression");
  }
}

static BinaryenExpressionRef generate_cast_expression(Expr* expression)
{
  if (expression->data_type == TYPE_FLOAT && expression->cast.expr->data_type == TYPE_INTEGER)
  {
    BinaryenExpressionRef value = generate_expression(expression->cast.expr);
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
  }

  assert(!"Unsupported cast type");
}

static BinaryenExpressionRef generate_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    return generate_literal_expression(expression);
  case EXPR_BINARY:
    return generate_binary_expression(expression);
  case EXPR_GROUP:
    return generate_expression(expression->group.expr);
  case EXPR_UNARY:
    return generate_unary_expression(expression);
  case EXPR_CAST:
    return generate_cast_expression(expression);

  default:
    assert(!"Unhandled expression");
    break;
  }
}

static BinaryenExpressionRef generate_expression_statement(Stmt* statement)
{
  BinaryenExpressionRef expression = generate_expression(statement->expr.expr);
  return BinaryenDrop(codegen.module, expression);
}

static BinaryenExpressionRef generate_return_statement(Stmt* statement)
{
  BinaryenExpressionRef expression = generate_expression(statement->ret.expr);
  return BinaryenReturn(codegen.module, expression);
}

static BinaryenExpressionRef generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    return generate_expression_statement(statement);
  case STMT_RETURN:
    return generate_return_statement(statement);

  default:
    assert(!"Unhandled statement");
    break;
  }
}

static BinaryenExpressionRef generate_statements(ArrayStmt* statements)
{
  ArrayBinaryenExpressionRef list;
  array_init(&list);

  Stmt* statement;
  array_foreach(statements, statement)
  {
    array_add(&list, generate_statement(statement));
  }

  BinaryenExpressionRef block =
    BinaryenBlock(codegen.module, NULL, list.elems, list.size, BinaryenTypeAuto());

  return block;
}

static void generate_function_declaration(Stmt* declaration)
{
  const char* name = declaration->func.name.lexeme;

  BinaryenExpressionRef body = generate_statements(&declaration->func.body);

  BinaryenType return_type = data_type_to_binaryen_type(declaration->func.data_type);
  BinaryenAddFunction(codegen.module, name, BinaryenTypeNone(), return_type, NULL, 0, body);
  BinaryenAddFunctionExport(codegen.module, name, name);
}

static void generate_variable_declaration(Stmt* declaration)
{
  const char* name = declaration->var.name.lexeme;
  unsigned int index = map_size_uint(&codegen.variables);

  BinaryenExpressionRef initializer;
}

static void generate_declaration(Stmt* declaration)
{
  switch (declaration->type)
  {
  case STMT_FUNCTION_DECL:
    generate_function_declaration(declaration);
    return;
  case STMT_VARIABLE_DECL:
    generate_variable_declaration(declaration);
    return;

  default:
    assert(!"Unhandled declaration");
    break;
  }
}

static void generate_declarations(void)
{
  Stmt* declaration;
  array_foreach(&codegen.statements, declaration)
  {
    generate_declaration(declaration);
  }
}

void codegen_init(ArrayStmt statements)
{
  codegen.module = BinaryenModuleCreate();
  codegen.statements = statements;

  map_init_uint(&codegen.variables, 0, 0);
}

void codegen_generate(void)
{
  generate_declarations();

  // BinaryenFunctionRef main =

  // BinaryenSetStart(codegen.module, main);
  // BinaryenAddMemoryImport(codegen.module, NULL, "env", "memory", 0);
  // BinaryenAddTableImport(codegen.module, NULL, "env", "table");

  BinaryenModuleValidate(codegen.module);
  // BinaryenModuleOptimize(codegen.module);
  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);
}

#include "codegen.h"
#include "array.h"
#include "expression.h"
#include "scanner.h"

#include <assert.h>
#include <binaryen-c.h>
#include <stdio.h>

array_def(BinaryenExpressionRef, BinaryenExpressionRef);

static struct
{
  BinaryenModuleRef module;
  ArrayStmt statements;
} codegen;

static BinaryenExpressionRef generate_expression(Expr* expression)
{
  BinaryenExpressionRef ref = BinaryenNop(codegen.module);

  switch (expression->type)
  {
  case EXPR_LITERAL: {
    switch (expression->literal.type)
    {
    case TYPE_INTEGER:
      return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->literal.integer));
    case TYPE_FLOAT:
      return BinaryenConst(codegen.module, BinaryenLiteralFloat32(expression->literal.floating));
    case TYPE_VOID:
    case TYPE_NULL:
    case TYPE_BOOL:
    case TYPE_STRING:
      assert(!"Unexpected literal value.");
      break;
    }

    break;
  }
  case EXPR_BINARY: {
    BinaryenExpressionRef left = generate_expression(expression->binary.left);
    BinaryenExpressionRef right = generate_expression(expression->binary.right);
    BinaryenOp op = 0;

    switch (expression->binary.op.type)
    {
    case TOKEN_PLUS:
      if (expression->data_type == TYPE_INTEGER)
        op = BinaryenAddInt32();
      else
        op = BinaryenAddFloat32();
      break;
    case TOKEN_MINUS:
      if (expression->data_type == TYPE_INTEGER)
        op = BinaryenSubInt32();
      else
        op = BinaryenSubFloat32();
      break;
    case TOKEN_STAR:
      if (expression->data_type == TYPE_INTEGER)
        op = BinaryenMulInt32();
      else
        op = BinaryenMulFloat32();
      break;
    case TOKEN_SLASH:
      if (expression->data_type == TYPE_INTEGER)
        op = BinaryenDivSInt32();
      else
        op = BinaryenDivFloat32();
      break;
    default:
      break;
    }

    return BinaryenBinary(codegen.module, op, left, right);
  }
  case EXPR_UNARY:
  case EXPR_GROUP:
    return generate_expression(expression->group.expr);
  case EXPR_VAR:
  case EXPR_ASSIGN:
  case EXPR_CALL:
  case EXPR_CAST:
    break;
  }

  return ref;
}

static BinaryenExpressionRef generate_statement(Stmt* statement)
{
  BinaryenExpressionRef ref = BinaryenNop(codegen.module);

  switch (statement->type)
  {
  case STMT_EXPR:
    return BinaryenDrop(codegen.module, generate_expression(statement->expr.expr));
  }

  return ref;
}

static BinaryenExpressionRef generate_statements(void)
{
  ArrayBinaryenExpressionRef list;
  array_init(&list);

  Stmt* statement;
  array_foreach(&codegen.statements, statement)
  {
    array_add(&list, generate_statement(statement));
  }

  BinaryenExpressionRef block =
    BinaryenBlock(codegen.module, "block", list.elems, list.size, BinaryenTypeNone());

  return block;
}

void codegen_init(ArrayStmt statements)
{
  codegen.module = BinaryenModuleCreate();
  codegen.statements = statements;
}

void codegen_generate(void)
{
  BinaryenFunctionRef main = BinaryenAddFunction(
    codegen.module, "main", BinaryenTypeNone(), BinaryenTypeNone(), NULL, 0, generate_statements());

  BinaryenSetStart(codegen.module, main);
  // BinaryenAddMemoryImport(codegen.module, NULL, "env", "memory", 0);
  // BinaryenAddTableImport(codegen.module, NULL, "env", "table");

  BinaryenModuleValidate(codegen.module);
  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);
}

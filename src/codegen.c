#include "codegen.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "memory.h"
#include "statement.h"

#include <binaryen-c.h>
#include <stdio.h>

array_def(BinaryenExpressionRef, BinaryenExpressionRef);
array_def(BinaryenPackedType, BinaryenPackedType);
array_def(BinaryenType, BinaryenType);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statement(Stmt* statement);
static BinaryenExpressionRef generate_statements(ArrayStmt* statements);

static struct
{
  BinaryenModuleRef module;
  ArrayStmt statements;
  int loops;
} codegen;

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
    return BinaryenTypeNone();
  case TYPE_BOOL:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();
  case TYPE_OBJECT:
    return data_type.class->type;

  default:
    UNREACHABLE("Unhandled data type");
  }
}

static BinaryenExpressionRef generate_default_initialization(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_INTEGER:
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(0));
  case TYPE_OBJECT:
    return BinaryenRefNull(codegen.module, data_type.class->type);
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static BinaryenExpressionRef generate_group_expression(GroupExpr* expression)
{
  return generate_expression(expression->expr);
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
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenEqInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenNeInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenNeFloat32();
    else
      UNREACHABLE("Unsupported binary type for ==");

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

  return BinaryenBinary(codegen.module, op, left, right);
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
  if (expression->to_data_type.type == TYPE_FLOAT &&
      expression->from_data_type.type == TYPE_INTEGER)
  {
    BinaryenExpressionRef value = generate_expression(expression->expr);
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
  }

  UNREACHABLE("Unsupported cast type");
}

static BinaryenExpressionRef generate_variable_expression(VarExpr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  if (expression->index == -1)
    return BinaryenGlobalGet(codegen.module, expression->name.lexeme, type);
  else
    return BinaryenLocalGet(codegen.module, expression->index, type);
}

static BinaryenExpressionRef generate_assignment_expression(AssignExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->value);
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  if (expression->index == -1)
  {
    BinaryenExpressionRef list[] = {
      BinaryenGlobalSet(codegen.module, expression->name.lexeme, value),
      BinaryenGlobalGet(codegen.module, expression->name.lexeme, type),
    };

    return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
  }
  else
  {
    return BinaryenLocalTee(codegen.module, expression->index, value, type);
  }
}

static BinaryenExpressionRef generate_call_expression(CallExpr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  ArrayBinaryenExpressionRef arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    array_add(&arguments, generate_expression(argument));
  }

  return BinaryenCall(codegen.module, expression->name.lexeme, arguments.elems, arguments.size,
                      type);
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

  const char* break_name = memory_sprintf(&memory, "break|%d", codegen.loops);
  const char* loop_name = memory_sprintf(&memory, "loop|%d", codegen.loops);

  ArrayBinaryenExpressionRef loop_block_list;
  array_init(&loop_block_list);
  array_add(&loop_block_list, generate_statements(&statement->body));

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

  codegen.loops--;

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
  const char* name = memory_sprintf(&memory, "loop|%d", codegen.loops);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_break_statement(void)
{
  const char* name = memory_sprintf(&memory, "break|%d", codegen.loops);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_variable_declaration(VarStmt* statement)
{
  BinaryenExpressionRef initializer = generate_default_initialization(statement->data_type);

  if (statement->index == -1)
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
  else
  {
    if (statement->initializer)
    {
      initializer = generate_expression(statement->initializer);
    }

    return BinaryenLocalSet(codegen.module, statement->index, initializer);
  }
}

static BinaryenExpressionRef generate_function_declaration(FuncStmt* statement)
{
  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    BinaryenType parameter_data_type = data_type_to_binaryen_type(parameter->data_type);
    array_add(&parameter_types, parameter_data_type);
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
  BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  BinaryenType results = data_type_to_binaryen_type(statement->data_type);

  if (statement->body.elems)
  {
    BinaryenExpressionRef body = generate_statements(&statement->body);
    BinaryenAddFunction(codegen.module, name, params, results, variable_types.elems,
                        variable_types.size, body);
    BinaryenAddFunctionExport(codegen.module, name, name);
  }
  else
  {
    BinaryenAddFunctionImport(codegen.module, name, "env", name, params, results);
  }

  return NULL;
}

static BinaryenExpressionRef generate_class_declaration(ClassStmt* statement)
{
  ArrayBinaryenType types;
  ArrayBinaryenPackedType packed_types;
  ArrayBool mutables;
  ArrayBinaryenExpressionRef initializers;

  array_init(&types);
  array_init(&packed_types);
  array_init(&mutables);
  array_init(&initializers);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType type = data_type_to_binaryen_type(variable->data_type);
    BinaryenPackedType packed_type = BinaryenPackedTypeNotPacked();
    bool mutable = true;

    array_add(&types, type);
    array_add(&packed_types, packed_type);
    array_add(&mutables, mutable);

    if (variable->initializer)
    {
      array_add(&initializers, generate_expression(variable->initializer));
    }
    else
    {
      array_add(&initializers, generate_default_initialization(variable->data_type));
    }
  }

  FuncStmt* function;
  array_foreach(&statement->functions, function)
  {
    generate_function_declaration(function);
  }

  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  TypeBuilderSetStructType(type_builder, 0, types.elems, packed_types.elems, mutables.elems,
                           types.size);

  BinaryenHeapType heap_type;
  TypeBuilderBuildAndDispose(type_builder, &heap_type, 0, 0);

  statement->type = BinaryenTypeFromHeapType(heap_type, true);

  BinaryenExpressionRef constructor =
    BinaryenStructNew(codegen.module, initializers.elems, initializers.size, heap_type);

  BinaryenAddFunction(codegen.module, statement->name.lexeme, BinaryenTypeNone(), statement->type,
                      NULL, 0, constructor);

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
  case STMT_CLASS_DECL:
    return generate_class_declaration(&statement->class);

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
    {
      array_add(&list, ref);
    }
  }

  BinaryenExpressionRef block =
    BinaryenBlock(codegen.module, NULL, list.elems, list.size, BinaryenTypeAuto());

  return block;
}

void codegen_init(ArrayStmt statements)
{
  codegen.module = BinaryenModuleCreate();
  codegen.statements = statements;
  codegen.loops = -1;
}

void codegen_generate(void)
{
  BinaryenFunctionRef start =
    BinaryenAddFunction(codegen.module, "~start", BinaryenTypeNone(), BinaryenTypeNone(), NULL, 0,
                        generate_statements(&codegen.statements));

  BinaryenSetStart(codegen.module, start);
  BinaryenModuleSetFeatures(codegen.module, BinaryenFeatureReferenceTypes() | BinaryenFeatureGC());
  BinaryenModuleValidate(codegen.module);
  // BinaryenModuleOptimize(codegen.module);
  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);
}

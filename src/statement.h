#ifndef statement_h
#define statement_h

#include "expression.h"
#include "lexer.h"
#include "map.h"

#define STMT() (ALLOC(Stmt))

typedef struct _STMT Stmt;
array_def(struct _STMT*, Stmt);
array_def(struct _VAR_STMT*, VarStmt);
array_def(struct _FUNC_STMT*, FuncStmt);

typedef struct _EXPR_STMT
{
  DataType data_type;
  Expr* expr;
} ExprStmt;

typedef struct _RETURN_STMT
{
  Token keyword;
  Expr* expr;
} ReturnStmt;

typedef struct _CONTINUE_STMT
{
  Token keyword;
} ContinueStmt;

typedef struct _BREAK_STMT
{
  Token keyword;
} BreakStmt;

typedef struct _IF_STMT
{
  Token keyword;
  Expr* condition;
  ArrayStmt then_branch;
  ArrayStmt else_branch;
} IfStmt;

typedef struct _WHILE_STMT
{
  Token keyword;
  Expr* condition;
  Stmt* initializer;
  Stmt* incrementer;
  ArrayStmt body;
} WhileStmt;

typedef struct _FUNC_STMT
{
  DataType data_type;

  Token type;
  Token name;

  ArrayVarStmt variables;
  ArrayVarStmt parameters;
  ArrayStmt body;
} FuncStmt;

typedef struct _VAR_STMT
{
  int index;
  Scope scope;
  DataType data_type;

  Token type;
  Token name;
  Expr* initializer;
} VarStmt;

typedef struct _CLASS_STMT
{
  Token keyword;
  Token name;

  bool declared;
  MapVarStmt members;
  ArrayVarStmt variables;
  ArrayFuncStmt functions;

  uintptr_t ref;
} ClassStmt;

struct _STMT
{
  enum
  {
    STMT_EXPR,
    STMT_RETURN,
    STMT_CONTINUE,
    STMT_BREAK,
    STMT_IF,
    STMT_WHILE,
    STMT_FUNCTION_DECL,
    STMT_VARIABLE_DECL,
    STMT_CLASS_DECL,
  } type;

  union {
    ExprStmt expr;
    FuncStmt func;
    VarStmt var;
    ReturnStmt ret;
    IfStmt cond;
    WhileStmt loop;
    BreakStmt brk;
    ContinueStmt cont;
    ClassStmt class;
  };
};

#endif

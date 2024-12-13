#ifndef statement_h
#define statement_h

#include "expression.h"
#include "lexer.h"

#define STMT() (ALLOC(Stmt))

typedef struct _STMT Stmt;
array_def(Stmt*, Stmt);

typedef struct
{
  Expr* expr;
} ExprStmt;

typedef struct
{
  Token keyword;
  Expr* expr;
} ReturnStmt;

typedef struct
{
  Token keyword;
} ContinueStmt;

typedef struct
{
  Token keyword;
} BreakStmt;

typedef struct
{
  Token keyword;
  Expr* condition;
  ArrayStmt then_branch;
  ArrayStmt else_branch;
} IfStmt;

typedef struct
{
  Token keyword;
  Expr* condition;
  ArrayStmt body;
} WhileStmt;

typedef struct
{
  DataType data_type;

  Token type;
  Token name;

  ArrayStmt variables;
  ArrayStmt parameters;
  ArrayStmt body;
} FuncStmt;

typedef struct
{
  int index;
  DataType data_type;

  Token type;
  Token name;
  Expr* initializer;
} VarStmt;

typedef enum
{
  STMT_EXPR,
  STMT_RETURN,
  STMT_CONTINUE,
  STMT_BREAK,
  STMT_IF,
  STMT_WHILE,
  STMT_FUNCTION_DECL,
  STMT_VARIABLE_DECL,
} StmtType;

struct _STMT
{
  StmtType type;

  union {
    ExprStmt expr;
    FuncStmt func;
    VarStmt var;
    ReturnStmt ret;
    IfStmt cond;
    WhileStmt loop;
    BreakStmt brk;
    ContinueStmt cont;
  };
};

#endif

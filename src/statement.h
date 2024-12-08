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
  DataType data_type;

  Token type;
  Token name;

  ArrayStmt variables;
  ArrayStmt parameters;
  ArrayStmt body;
} FuncStmt;

typedef struct
{
  size_t index;
  DataType data_type;

  Token type;
  Token name;
  Expr* initializer;
} VarStmt;

typedef enum
{
  STMT_EXPR,
  STMT_RETURN,
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
  };
};

#endif

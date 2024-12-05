#ifndef statement_h
#define statement_h

#include "expression.h"
#include "lexer.h"

#define STMT() ((Stmt*)memory_alloc(&memory, sizeof(Stmt)))

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
  ArrayToken param_types;
  ArrayToken params;
  ArrayStmt body;
} FuncStmt;

typedef enum
{
  STMT_EXPR,
  STMT_RETURN,
  STMT_FUNCTION_DECL,
} StmtType;

struct _STMT
{
  StmtType type;

  union {
    ExprStmt expr;
    FuncStmt func;
    ReturnStmt ret;
  };
};

#endif

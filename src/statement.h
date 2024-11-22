#ifndef statement_h
#define statement_h

#include "expression.h"
#include "scanner.h"

#define STMT() ((Stmt*)memory_alloc(&memory, sizeof(Stmt)))

typedef struct _STMT Stmt;
array_def(Stmt*, Stmt);

typedef struct
{
  Expr* expr;
} ExprStmt;

typedef enum
{
  STMT_EXPR,
} StmtType;

struct _STMT
{
  StmtType type;

  union {
    ExprStmt expr;
  };
};

#endif

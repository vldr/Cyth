#ifndef expression_h
#define expression_h

#include "scanner.h"

#define EXPR() ((Expr*)memory_alloc(&memory, sizeof(Expr)))
#define BINARY_EXPR(destination, op, l, r)                                                         \
  do                                                                                               \
  {                                                                                                \
    Expr* _ = EXPR();                                                                              \
    _->type = EXPR_BINARY;                                                                         \
    _->binary.op = op;                                                                             \
    _->binary.left = l;                                                                            \
    _->binary.right = r;                                                                           \
    destination = _;                                                                               \
  } while (0)

#define UNARY_EXPR(destination, op, e)                                                             \
  do                                                                                               \
  {                                                                                                \
    Expr* _ = EXPR();                                                                              \
    _->type = EXPR_UNARY;                                                                          \
    _->unary.op = op;                                                                              \
    _->unary.expr = e;                                                                             \
    destination = _;                                                                               \
  } while (0)

typedef struct _EXPR Expr;
typedef struct _STMT Stmt;

array_def(Expr*, Expr);
array_def(Stmt*, Stmt);

typedef struct
{
  Expr* left;
  Token op;
  Expr* right;
} BinaryExpr;

typedef struct
{
  Token op;
  Expr* expr;
} UnaryExpr;

typedef struct
{
  Expr* expr;
} GroupExpr;

typedef struct
{
  enum
  {
    LITERAL_VOID,
    LITERAL_BOOL,
    LITERAL_INTEGER,
    LITERAL_FLOAT,
    LITERAL_STRING,
  } type;

  union {
    bool boolean;
    int integer;
    float floating;
    char* string;
  };
} LiteralExpr;

typedef struct
{
  Token name;
} VarExpr;

typedef struct
{
  Token name;
  Expr* value;
} AssignExpr;

typedef struct
{
  Token function;
  ArrayExpr arguments;
} CallExpr;

typedef enum
{
  EXPR_BINARY,
  EXPR_LOGICAL,
  EXPR_UNARY,
  EXPR_GROUP,
  EXPR_LITERAL,
  EXPR_VAR,
  EXPR_ASSIGN,
  EXPR_CALL,
} ExprType;

struct _EXPR
{
  ExprType type;

  union {
    BinaryExpr binary;
    UnaryExpr unary;
    GroupExpr group;
    LiteralExpr literal;
    VarExpr var;
    AssignExpr assign;
    CallExpr call;
  };
};

#endif

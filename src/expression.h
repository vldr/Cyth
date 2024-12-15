#ifndef expression_h
#define expression_h

#include "lexer.h"

#define DATA_TYPE(a) ((DataType){ .kind = a })
#define EXPR() (ALLOC(Expr))
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
array_def(Expr*, Expr);

typedef struct _DATA_TYPE
{
  enum
  {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_OBJECT,
    TYPE_ARRAY,
  } kind;

  union {
    const char* object;
    struct _DATA_TYPE* array;
  };
} DataType;

typedef struct
{
  DataType data_type;

  union {
    bool boolean;
    int integer;
    float floating;
    const char* string;
  };
} LiteralExpr;

typedef struct
{
  DataType data_type;

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
  int index;
  Token name;
} VarExpr;

typedef struct
{
  int index;
  Token name;
  Expr* value;
} AssignExpr;

typedef struct
{
  Token name;
  ArrayExpr arguments;
} CallExpr;

typedef struct
{
  Expr* expr;
} CastExpr;

typedef enum
{
  EXPR_LITERAL,
  EXPR_BINARY,
  EXPR_UNARY,
  EXPR_GROUP,
  EXPR_CAST,
  EXPR_VAR,
  EXPR_ASSIGN,
  EXPR_CALL,
} ExprType;

struct _EXPR
{
  ExprType type;
  DataType data_type;

  union {
    BinaryExpr binary;
    UnaryExpr unary;
    GroupExpr group;
    LiteralExpr literal;
    VarExpr var;
    AssignExpr assign;
    CallExpr call;
    CastExpr cast;
  };
};

#endif

#ifndef expression_h
#define expression_h

#include "lexer.h"

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

#define DATA_TYPE(a) ((DataType){ .type = a })

typedef struct _EXPR Expr;
array_def(Expr*, Expr);

typedef enum _SCOPE
{
  SCOPE_LOCAL,
  SCOPE_GLOBAL,
  SCOPE_CLASS
} Scope;

typedef struct _DATA_TYPE
{
  enum
  {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_FUNCTION,
    TYPE_PROTOTYPE,
    TYPE_OBJECT,
    TYPE_ARRAY,
  } type;

  union {
    struct _CLASS_STMT* class;
    struct _FUNC_STMT* function;
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
  DataType return_data_type;
  DataType operand_data_type;

  Expr* left;
  Token op;
  Expr* right;
} BinaryExpr;

typedef struct
{
  DataType data_type;

  Token op;
  Expr* expr;
} UnaryExpr;

typedef struct
{
  DataType data_type;

  Expr* expr;
} GroupExpr;

typedef struct
{
  DataType data_type;

  int index;
  Scope scope;
  Token name;
} VarExpr;

typedef struct
{
  DataType data_type;

  int index;
  Scope scope;
  Token name;
  Expr* value;
} AssignExpr;

typedef struct
{
  DataType callee_data_type;
  DataType return_data_type;

  Token callee_token;
  Expr* callee;
  ArrayExpr arguments;
} CallExpr;

typedef struct
{
  DataType from_data_type;
  DataType to_data_type;

  Expr* expr;
} CastExpr;

typedef struct
{
  DataType data_type;

  Expr* expr;
  Token name;
} AccessExpr;

struct _EXPR
{
  enum
  {
    EXPR_LITERAL,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_GROUP,
    EXPR_CAST,
    EXPR_VAR,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_ACCESS,
  } type;

  union {
    BinaryExpr binary;
    UnaryExpr unary;
    GroupExpr group;
    LiteralExpr literal;
    VarExpr var;
    AssignExpr assign;
    CallExpr call;
    CastExpr cast;
    AccessExpr access;
  };
};

#endif

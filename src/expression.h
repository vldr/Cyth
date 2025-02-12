#ifndef expression_h
#define expression_h

#include "lexer.h"
#include "map.h"

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
typedef struct _VAR_STMT VarStmt;
array_def(Expr*, Expr);
array_def(struct _DATA_TYPE, DataType);

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
    TYPE_CHAR,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_FUNCTION,
    TYPE_FUNCTION_MEMBER,
    TYPE_FUNCTION_INTERNAL,
    TYPE_PROTOTYPE,
    TYPE_OBJECT,
    TYPE_ARRAY,
  } type;

  union {
    struct _CLASS_STMT* class;
    struct _FUNC_STMT* function;

    struct
    {
      struct _FUNC_STMT* function;
      struct _EXPR* this;
    } function_member;

    struct
    {
      const char* name;
      struct _EXPR* this;

      struct _DATA_TYPE* return_type;
      ArrayDataType parameter_types;
    } function_internal;

    struct
    {
      struct _DATA_TYPE* data_type;
      int count;
    } array;
  };
} DataType;

typedef struct
{
  Token token;
  int count;
} DataTypeToken;

typedef struct
{
  DataType data_type;

  union {
    bool boolean;
    unsigned int integer;
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

  Token name;
  VarStmt* variable;
} VarExpr;

typedef struct
{
  DataType data_type;

  Token op;
  Expr* target;
  Expr* value;

  VarStmt* variable;
} AssignExpr;

typedef struct
{
  DataType callee_data_type;
  DataType return_data_type;

  Expr* callee;
  Token callee_token;
  ArrayExpr arguments;
} CallExpr;

typedef struct
{
  DataTypeToken type;

  DataType from_data_type;
  DataType to_data_type;

  Expr* expr;
} CastExpr;

typedef struct
{
  DataType data_type;
  DataType expr_data_type;

  Expr* expr;
  Token expr_token;
  Token name;

  VarStmt* variable;
} AccessExpr;

typedef struct
{
  DataType data_type;
  DataType expr_data_type;

  Expr* expr;
  Token expr_token;

  Expr* index;
  Token index_token;
} IndexExpr;

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
    EXPR_INDEX,
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
    IndexExpr index;
  };
};

#endif

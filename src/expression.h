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
typedef struct _LITERAL_ARRAY_EXPR LiteralArrayExpr;
typedef struct _VAR_STMT VarStmt;
array_def(Expr*, Expr);
array_def(LiteralArrayExpr*, LiteralArrayExpr);
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
      unsigned char count;

      ArrayLiteralArrayExpr* list;
    } array;
  };
} DataType;

typedef struct
{
  Token token;
  int count;
} DataTypeToken;

typedef struct _LITERAL_EXPR
{
  DataType data_type;

  union {
    bool boolean;
    unsigned int integer;
    float floating;
    const char* string;
  };
} LiteralExpr;

typedef struct _LITERAL_ARRAY_EXPR
{
  DataType data_type;

  ArrayExpr values;
  ArrayToken tokens;
} LiteralArrayExpr;

typedef struct _BINARY_EXPR
{
  DataType return_data_type;
  DataType operand_data_type;

  Expr* left;
  Token op;
  Expr* right;
} BinaryExpr;

typedef struct _UNARY_EXPR
{
  DataType data_type;

  Token op;
  Expr* expr;
} UnaryExpr;

typedef struct _GROUP_EXPR
{
  DataType data_type;

  Expr* expr;
} GroupExpr;

typedef struct _VAR_EXPR
{
  DataType data_type;

  Token name;
  VarStmt* variable;
} VarExpr;

typedef struct _ASSIGN_EXPR
{
  DataType data_type;

  Token op;
  Expr* target;
  Expr* value;

  VarStmt* variable;
} AssignExpr;

typedef struct _CALL_EXPR
{
  DataType callee_data_type;
  DataType return_data_type;

  Expr* callee;
  Token callee_token;
  ArrayExpr arguments;
} CallExpr;

typedef struct _CAST_EXPR
{
  DataTypeToken type;

  DataType from_data_type;
  DataType to_data_type;

  Expr* expr;
} CastExpr;

typedef struct _ACCESS_EXPR
{
  DataType data_type;
  DataType expr_data_type;

  Expr* expr;
  Token expr_token;
  Token name;

  VarStmt* variable;
} AccessExpr;

typedef struct _INDEX_EXPR
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
    EXPR_ARRAY,
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
    LiteralArrayExpr array;
    VarExpr var;
    AssignExpr assign;
    CallExpr call;
    CastExpr cast;
    AccessExpr access;
    IndexExpr index;
  };
};

#endif

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
array_def(struct _DATA_TYPE, DataType);

typedef enum _SCOPE
{
  SCOPE_NONE,
  SCOPE_LOCAL,
  SCOPE_GLOBAL,
  SCOPE_CLASS
} Scope;

typedef struct _DATA_TYPE
{
  enum
  {
    TYPE_VOID,
    TYPE_NULL,
    TYPE_ANY,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_ALIAS,
    TYPE_FUNCTION,
    TYPE_FUNCTION_MEMBER,
    TYPE_FUNCTION_INTERNAL,
    TYPE_FUNCTION_POINTER,
    TYPE_FUNCTION_TEMPLATE,
    TYPE_FUNCTION_GROUP,
    TYPE_PROTOTYPE,
    TYPE_PROTOTYPE_TEMPLATE,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_OBJECT,
  } type;

  union {
    struct _CLASS_STMT* class;
    struct _CLASS_TEMPLATE_STMT* class_template;
    struct _FUNC_STMT* function;
    bool* null_function;

    struct
    {
      DataTypeToken token;
      struct _DATA_TYPE* data_type;
    } alias;

    struct
    {
      struct _FUNC_TEMPLATE_STMT* function;
      struct _EXPR* this;
    } function_template;

    struct
    {
      struct _FUNC_STMT* function;
      struct _EXPR* this;
    } function_member;

    ArrayDataType function_group;

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
      unsigned char* count;

      ArrayExpr values;
      ArrayToken tokens;
      Token token;
    } array;
  };
} DataType;

typedef struct _LITERAL_EXPR
{
  DataType data_type;

  union {
    bool boolean;
    unsigned int integer;
    float floating;
    struct
    {
      const char* data;
      unsigned int length;
    } string;
  };
} LiteralExpr;

typedef struct _LITERAL_ARRAY_EXPR
{
  DataType data_type;

  Token token;
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

  struct _FUNC_STMT* function;
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
  ArrayDataTypeToken* template_types;
} VarExpr;

typedef struct _ASSIGN_EXPR
{
  DataType data_type;
  DataType value_data_type;

  Token op;
  Expr* target;
  Expr* value;

  VarStmt* variable;

  struct _FUNC_STMT* function;
} AssignExpr;

typedef struct _CALL_EXPR
{
  DataType callee_data_type;
  DataType return_data_type;

  Expr* callee;
  Token callee_token;
  ArrayExpr arguments;
  ArrayToken argument_tokens;

  struct _FUNC_STMT* function;
  const char* function_name;
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
  ArrayDataTypeToken* template_types;
} AccessExpr;

typedef struct _INDEX_EXPR
{
  DataType data_type;
  DataType expr_data_type;
  DataType index_data_type;

  Expr* expr;
  Token expr_token;

  Expr* index;
  Token index_token;

  struct _FUNC_STMT* function;
} IndexExpr;

typedef struct _IS_EXPR
{
  DataType expr_data_type;
  DataType is_data_type;

  Token expr_token;
  DataTypeToken is_data_type_token;

  Expr* expr;
} IsExpr;

typedef struct _IF_EXPR
{
  DataType data_type;

  Token body_token;
  Token condition_token;

  Expr* condition;
  Expr* left;
  Expr* right;
} IfExpr;

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
    EXPR_IS,
    EXPR_IF,
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
    IsExpr is;
    IfExpr cond;
  };
};

#endif

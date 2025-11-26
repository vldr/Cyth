#ifndef statement_h
#define statement_h

#include "expression.h"
#include "lexer.h"
#include "map.h"

#define STMT() (ALLOC(Stmt))

typedef struct _STMT Stmt;
array_def(struct _STMT*, Stmt);
array_def(struct _VAR_STMT*, VarStmt);
array_def(struct _CLASS_STMT*, ClassStmt);
array_def(struct _FUNC_STMT*, FuncStmt);
array_def(struct _FUNC_TEMPLATE_STMT*, FuncTemplateStmt);

typedef struct _EXPR_STMT
{
  DataType data_type;
  Expr* expr;
} ExprStmt;

typedef struct _RETURN_STMT
{
  Token keyword;
  Expr* expr;
} ReturnStmt;

typedef struct _CONTINUE_STMT
{
  Token keyword;
} ContinueStmt;

typedef struct _BREAK_STMT
{
  Token keyword;
} BreakStmt;

typedef struct _IF_STMT
{
  Token keyword;
  Expr* condition;
  ArrayStmt then_branch;
  ArrayStmt else_branch;
} IfStmt;

typedef struct _WHILE_STMT
{
  Token keyword;
  Expr* condition;
  ArrayStmt initializer;
  ArrayStmt incrementer;
  ArrayStmt body;
} WhileStmt;

typedef struct _FUNC_STMT
{
  DataType data_type;
  DataType function_data_type;
  DataTypeToken type;

  Token name;
  const char* name_raw;
  const char* import;

  ArrayVarStmt variables;
  ArrayVarStmt parameters;
  ArrayStmt body;

  struct MIR_item* item;
  struct MIR_item* proto;

  struct MIR_item* item_prototype;
  struct MIR_item* proto_prototype;
} FuncStmt;

typedef struct _FUNC_TEMPLATE_STMT
{
  DataTypeToken type;
  ArrayDataTypeToken parameters;

  Token name;

  int offset;
  int count;

  struct _CLASS_STMT* class;
  struct _FUNC_STMT* function;
  struct _WHILE_STMT* loop;
  struct _IF_STMT* cond;
  struct _ENVIRONMENT* environment;

  const char* import;

  ArrayToken types;
  ArrayFuncStmt functions;
} FuncTemplateStmt;

typedef struct _VAR_STMT
{
  DataType data_type;
  DataType initializer_data_type;
  DataTypeToken type;

  int index;
  int offset;
  Scope scope;

  union {
    struct MIR_item* item;
    uint32_t reg;
  };

  Token name;
  Token equals;
  Expr* initializer;
  FuncStmt* function;
} VarStmt;

typedef struct _CLASS_STMT
{
  Token keyword;
  Token name;

  struct _ENVIRONMENT* environment;
  MapVarStmt* members;
  ArrayVarStmt variables;
  ArrayFuncStmt functions;
  ArrayFuncTemplateStmt function_templates;
  struct _FUNC_STMT* default_constructor;

  uintptr_t ref;
  int alignment;
  int size;
} ClassStmt;

typedef struct _CLASS_TEMPLATE_STMT
{
  Token keyword;
  Token name;

  int offset;
  int count;
  ArrayToken types;
  ArrayClassStmt classes;
} ClassTemplateStmt;

typedef struct _IMPORT_STMT
{
  Token keyword;
  ArrayStmt body;
} ImportStmt;

struct _STMT
{
  enum
  {
    STMT_EXPR,
    STMT_RETURN,
    STMT_CONTINUE,
    STMT_BREAK,
    STMT_IF,
    STMT_WHILE,
    STMT_FUNCTION_DECL,
    STMT_FUNCTION_TEMPLATE_DECL,
    STMT_VARIABLE_DECL,
    STMT_CLASS_DECL,
    STMT_CLASS_TEMPLATE_DECL,
    STMT_IMPORT_DECL,
  } type;

  union {
    ExprStmt expr;
    FuncStmt func;
    FuncTemplateStmt func_template;
    VarStmt var;
    ReturnStmt ret;
    IfStmt cond;
    WhileStmt loop;
    BreakStmt brk;
    ContinueStmt cont;
    ClassStmt class;
    ClassTemplateStmt class_template;
    ImportStmt import;
  };
};

#endif

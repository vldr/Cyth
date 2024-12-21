#ifndef environment_h
#define environment_h

#include "map.h"
#include "statement.h"

typedef struct _ENVIRONMENT
{
  MapVarStmt variables;

  struct _ENVIRONMENT* parent;
} Environment;

Environment* environment_init(Environment* parent);
VarStmt* environment_get_variable(Environment* environment, const char* name);
void environment_set_variable(Environment* environment, const char* name, VarStmt* variable);
bool environment_check_variable(Environment* environment, const char* name);

#endif

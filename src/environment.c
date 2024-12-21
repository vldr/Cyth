#include "environment.h"
#include "memory.h"

Environment* environment_init(Environment* parent)
{
  Environment* environment = ALLOC(Environment);
  environment->parent = parent;
  map_init_var_stmt(&environment->variables, 0, 0);

  return environment;
}

bool environment_check_variable(Environment* environment, const char* name)
{
  return map_get_var_stmt(&environment->variables, name) != NULL;
}

VarStmt* environment_get_variable(Environment* environment, const char* name)
{
  while (environment)
  {
    VarStmt* variable = map_get_var_stmt(&environment->variables, name);
    if (variable)
    {
      return variable;
    }

    environment = environment->parent;
  }

  return NULL;
}

void environment_set_variable(Environment* environment, const char* name, VarStmt* variable)
{
  map_put_var_stmt(&environment->variables, name, variable);
}

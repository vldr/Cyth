#ifndef checker_h
#define checker_h

#include "statement.h"

ArrayVarStmt global_locals(void);
bool equal_data_type(DataType left, DataType right);
bool assignable_data_type(DataType left, DataType right);

unsigned int array_data_type_hash(DataType array_data_type);
DataType array_data_type_element(DataType array_data_type);

void checker_init(ArrayStmt statements);
void checker_validate(void);

#endif

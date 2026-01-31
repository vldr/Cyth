#ifndef checker_h
#define checker_h

#include "statement.h"

ArrayVarStmt global_locals(void);
void expand_function_data_type(DataType data_type, DataType* return_data_type,
                               ArrayDataType* parameter_types);
bool equal_data_type(DataType left, DataType right);
bool assignable_data_type(Expr** expression, DataType destination, DataType source);
int size_data_type(DataType data_type);
const char* data_type_to_string(DataType data_type);
const char* function_data_type_to_string(const char* name, DataType data_type);
DataType array_data_type_element(DataType array_data_type);

void checker_init(ArrayStmt statements,
                  void (*error_callback)(int start_line, int start_column, int end_line,
                                         int end_column, const char* message),
                  void (*link_callback)(int ref_line, int ref_column, int def_line, int def_column,
                                        int length));
int checker_errors(void);
void checker_validate(void);

#endif

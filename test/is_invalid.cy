


class A
    int field

class B
    float field

any Thing = A | B | string | int[]

int a
log(a is A)
log(a is B)
log(a is Thing)
log(a is string)
log(a is int[])

Thing b
log(b is int)
log(b is float)
log(b is char)
log(b is bool)
log(b is void())

#! 13:5-13:6 The 'is' statement cannot be used on this type.
#! 14:5-14:6 The 'is' statement cannot be used on this type.
#! 15:5-15:6 The 'is' statement cannot be used on this type.
#! 16:5-16:6 The 'is' statement cannot be used on this type.
#! 17:5-17:6 The 'is' statement cannot be used on this type.
#! 20:10-20:13 The type 'int' is not assignable to 'Thing'.
#! 21:10-21:15 The type 'float' is not assignable to 'Thing'.
#! 22:10-22:14 The type 'char' is not assignable to 'Thing'.
#! 23:10-23:14 The type 'bool' is not assignable to 'Thing'.
#! 24:10-24:14 The type 'void()' is not assignable to 'Thing'.
import "env"
    void log(bool n)

class A
    int field

class B
    float field

int a
log(a is A)
log(a is B)
log(a is any)
log(a is string)
log(a is int[])

any b
log(b is int)
log(b is float)
log(b is char)
log(b is bool)
log(b is void())

#! 11:5-11:6 Mismatched types, expected 'any' but got 'int'.
#! 12:5-12:6 Mismatched types, expected 'any' but got 'int'.
#! 13:5-13:6 Mismatched types, expected 'any' but got 'int'.
#! 14:5-14:6 Mismatched types, expected 'any' but got 'int'.
#! 15:5-15:6 Mismatched types, expected 'any' but got 'int'.
#! 18:10-18:13 The type 'int' is not assignable to 'any'.
#! 19:10-19:15 The type 'float' is not assignable to 'any'.
#! 20:10-20:14 The type 'char' is not assignable to 'any'.
#! 21:10-21:14 The type 'bool' is not assignable to 'any'.
#! 22:10-22:14 The type 'void()' is not assignable to 'any'.
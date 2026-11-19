


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
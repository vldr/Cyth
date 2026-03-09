void test(int a, float b, string c)

test(true, true, true)

class Test
    void __init__(int a, float b, string c)
    void test(int a, float b, string c)


Test(true, true, true).test(true, true, true)

void(int, float, string) fnptr
fnptr(true, true, true)

#! 3:6-3:10 Mismatched types, expected 'int' but got 'bool'.
#! 3:12-3:16 Mismatched types, expected 'float' but got 'bool'.
#! 3:18-3:22 Mismatched types, expected 'string' but got 'bool'.
#! 10:6-10:10 Mismatched types, expected 'int' but got 'bool'.
#! 10:12-10:16 Mismatched types, expected 'float' but got 'bool'.
#! 10:18-10:22 Mismatched types, expected 'string' but got 'bool'.
#! 10:29-10:33 Mismatched types, expected 'int' but got 'bool'.
#! 10:35-10:39 Mismatched types, expected 'float' but got 'bool'.
#! 10:41-10:45 Mismatched types, expected 'string' but got 'bool'.
#! 13:7-13:11 Mismatched types, expected 'int' but got 'bool'.
#! 13:13-13:17 Mismatched types, expected 'float' but got 'bool'.
#! 13:19-13:23 Mismatched types, expected 'string' but got 'bool'.
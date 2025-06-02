class A
    void test(int a, int b, int c)

void test(int a, int b, int c)

void(int, int, int) test2 = test

A().test(1, 2)
test(1, 2)
test2(1, 2)

int[][][] a
void(int[][][], int, int, int) test3 = a.reserve

test3(1,2)
a.reserve(1,2)

#! 8:1-8:9 Expected 3 parameter(s) but got 2.
#! 9:1-9:5 Expected 3 parameter(s) but got 2.
#! 10:1-10:6 Expected 3 parameter(s) but got 2.
#! 15:1-15:6 Expected 4 parameter(s) but got 2.
#! 16:1-16:10 Expected 3 parameter(s) but got 2.
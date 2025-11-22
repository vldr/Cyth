import "env"
    void log(string n)
    void log(int n)

class A<Q>
    void test<T, B>(T a, B b)
        log("0")

    void test<T, B>(T a, T b)
        log("1")

    void test(int a, int b)
        log("2")

    void test(int a, float b)
        log("3")

void test<T, B>(T a, B b)
    log("4")

void test<T, B>(T a, T b)
    log("5")

void test(int a, int b)
    log("6")

void test(int a, float b)
    log("7")

A<int>().test<int, float>(10, 10.0)
A<int>().test<int, float>(10, 10)
A<int>().test(10, 10)
A<int>().test(10, 10.0)
test<int, float>(10, 10.0)
test<int, float>(12, 12)
test(10, 10)
test(10, 10.0)

# 0
# 1
# 2
# 3
# 4
# 5
# 6
# 7
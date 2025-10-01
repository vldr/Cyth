import "env"
    void log(string n)

class A<Q>
    void test<T, B>(T a, B b)
    void test<T, B>(T a, T b)
    void test<T, B>(T a, Q b)

A<int>().test<int, float>(10, 10.0)
A<int>().test<int, float>(10, 10)

#! 9:10-9:14 Call of overloaded 'A<int>.test<int, float>(int, int)' is ambiguous; parameter types conflict with another function.
#! 10:10-10:14 Call of overloaded 'A<int>.test<int, float>(int, int)' is ambiguous; parameter types conflict with another function.
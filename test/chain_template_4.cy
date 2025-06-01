import "env"
    void log(int n)

class Test2<T>
    T a

class Test<K>
    Test2<K> b

Test<Test2<int>> obj = Test<Test2<int>>()
obj.b = Test2<Test2<int>>()
obj.b.a = Test2<int>()
obj.b.a.a = 12

int a = obj.b.a.a + 10
log(a)

# 22
import "env"
    void log(int n)

class Test_A2<T>
    T a

class Test_A<K>
    Test_A2<K> b

Test_A<Test_A2<int>> obj = Test_A<Test_A2<int>>()
obj.b = Test_A2<Test_A2<int>>()
obj.b.a = Test_A2<int>()
obj.b.a.a = 12
int a_a = obj.b.a.a + 10
log(a_a)

# 22

class Test_B3<A>
  A f

class Test_B2<B>
  B g

class Test_B<K>
  Test_B2<Test_B3<K>> r = Test_B2<Test_B3<K>>()

Test_B<int> a = Test_B<int>()
a.r = Test_B2<Test_B3<int>>()


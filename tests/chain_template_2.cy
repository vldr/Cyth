class Test3<K>
  K f

class Test2<K>
  Test2<K> a

class Test<K>
  Test2<K> r

Test<int> a = Test<int>()
a.r = Test2<int>()
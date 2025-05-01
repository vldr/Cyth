class Test3<G>
  G f

class Test2<F>
  F a

class Test<K>
  Test2<Test3<K>> r

Test<int> a
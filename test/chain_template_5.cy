import "env"
  void log(int n)

class Test<K>
  K r

class Test2<K>
  Test<K> a

Test2<int> a = Test2<int>()
a.a = Test<int>()
a.a.r = 12 
log(a.a.r + 10)

# 22
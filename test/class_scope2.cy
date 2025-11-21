import "env"
  void log(string a)
  void log(int n)

class A<K>
  B<K> b = B<K>()
  int a = b.field

class B<T>
  A<T> a
  int field = outer

A<int> a = A<int>()
int outer = 22

log("done " + a.a)
# done 0
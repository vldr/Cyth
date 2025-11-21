import "env"
  void log(string a)
  void log(int n)

class A
  B b = B()
  int a = b.field

class B
  A a
  int field = outer

A a = A()
int outer = 22

log("done " + a.a)
# done 0
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
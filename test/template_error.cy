class Q<T>
  T a

class A
  void test<T>()
    Q<T> a
    
  Q<T> test<T>(int qr)
    return Q<T>()


2 + A.test<void>(10)
2 + A.test<void>(10)

#! 2:3-2:4 The type cannot be void here. (occurred when creating A.test<void> at 12:7)
#! 12:3-12:4 Mismatched types, expected 'int' but got 'Q<void>'.
#! 13:3-13:4 Mismatched types, expected 'int' but got 'Q<void>'.
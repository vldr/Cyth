class Q<T>
  T a

class A
  void test<T>()
    Q<T> a
    
  void test<T>(int q)
    Q<T> q

A.test<void>

#! 2:3-2:4 The type cannot be void here. (occurred when creating A.test<void> at 11:3)
#! 9:10-9:11 The name 'q' already exists. (occurred when creating A.test<void> at 11:3)

MyObject obj
obj[0]
obj[0] = 2

class MyObject
  void __get__()
  void __set__(int a)

#! 3:1-3:4 The object cannot be indexed, missing '__get__' method that takes 'int'.
#! 4:1-4:4 The object cannot be indexed and assigned to, missing '__set__' method that takes 'int' and 'int'.
#! 7:8-7:15 The '__get__' method must have one argument.
#! 8:8-8:15 The '__set__' method must have two arguments.
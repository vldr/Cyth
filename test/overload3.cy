import "env"
  void log(string n)
  void log(int n)

class A
  void __set__(int index, int value)
    log("set " + index + " = " + value)

  void __set__(int index, string value)
    log("set " + index + " = " + value)

  int __get__(int index)
    log("get " + index)
    return 10

  string __get__(string index)
    log("get " + index)
    return "hello"

log(A()[10] = "hello")
# set 10 = hello
# hello

log(A()[10] = 20)
# set 10 = 20
# 20

log(A()[20])
# get 20
# 10

log(A()["world"])
# get world
# hello

class B
  void __set__(int index, int value)
    log("set " + index + " = " + value)

  int __get__(int index)
    log("get " + index)
    return 10

log(A()[10] = 20)
# set 10 = 20
# 20

log(A()[20])
# get 20
# 10
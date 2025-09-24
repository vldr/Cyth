import "env"
  void log(string n)
  void log(int n)

class A
  int __set__(int index, int value)
    log("set " + index + " = " + value)
    return 30

  string __set__(int index, string value)
    log("set " + index + " = " + value)
    return "world"

  int __get__(int index)
    log("get " + index)
    return 10

  string __get__(string index)
    log("get " + index)
    return "hello"

log(A()[10] = "hello")
# set 10 = hello
# world

log(A()[10] = 20)
# set 10 = 20
# 30

log(A()[20])
# get 20
# 10

log(A()["world"])
# get world
# hello

class B
  string __set__(int index, int value)
    log("set " + index + " = " + value)
    return "hello"

  int __get__(int index)
    log("get " + index)
    return 10

log(B()[10] = 20)
# set 10 = 20
# hello

log(B()[20])
# get 20
# 10
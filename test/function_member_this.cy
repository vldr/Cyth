import "env"
  void log(int n)

class A
  B b = B()

  void __init__()
    log(b.getValue())

class B
  int value = 12

  void __init__()
    log(getValue())

  int getValue()
    return value

A()

# 12
# 12
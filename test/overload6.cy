import "env"
    void log(string n)
    void log(int n)

Vector a
a = Vector(0, 0, 0)
# int

a = Vector(0.0, 0, 0)
# float

int someint
a = Vector(0, 0, someint)
# int

float somefloat
a = Vector(0, 0, somefloat)
# float

void(Vector, float, float, float) b = (void(Vector, float, float, float))a.__init__
b(a, 0, 0, 0)
# float

void(Vector, int, int, int) c = (void(Vector, int, int, int))a.__init__
c(a, 0, 0, 0)
# int

a = Vector("string")
# string

a = Vector((any)"string")
# any

a = Vector((int[]) [1,2,3])
# int[]

a = Vector((any) (int[]) [1,2,3])
# any

a = Vector(a)
# Vector

a = Vector((any)a)
# any

class Vector
  void __init__(float x, float y, float z)
    log("float")

  void __init__(int x, int y, int z)
    log("int")

  void __init__(string x)
    log("string")

  void __init__(int[] x)
    log("int[]")

  void __init__(Vector x)
    log("Vector")

  void __init__(any x)
    log("any")
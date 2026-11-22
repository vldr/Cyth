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

a = Vector((Thing)"string")
# Thing

a = Vector((int[]) [1,2,3])
# int[]

a = Vector((Thing) (int[]) [1,2,3])
# Thing

a = Vector(a)
# Vector

a = Vector((Thing)a)
# Thing

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

  void __init__(Thing x)
    log("Thing")

any Thing = Vector | string | int[]
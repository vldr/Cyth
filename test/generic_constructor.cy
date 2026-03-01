class Vector
  int x 
  int y

  void __init__(int x, int y)
    this.x = x
    this.y = y

    log("" + this)

void test<T>()
  log("" + T())

void test2<T>(int x, int y)
  T(x, y)

test<int[]>()
test2<Vector>(1, 2)

# []
# Vector(\n x = 1,\n y = 2\n)
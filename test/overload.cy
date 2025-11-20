import "env"
  void log(string a)
  void log(int n)
  
class Vector
  int x
  int y

  void __init__()
    this.x = 0
    this.y = 0

  void __init__(int x, int y)
    this.x = x
    this.y = y

  Vector __add__(Vector other)
    return Vector(x + other.x, y + other.y)

  Vector __add__(int other)
    return Vector(x + other, y + other)

  Vector __sub__(Vector other)
    return Vector(x - other.x, y - other.y)

  Vector __sub__(int other)
    return Vector(x - other, y - other)

  Vector __mul__(Vector other)
    return Vector(x * other.x, y * other.y)

  Vector __mul__(int other)
    return Vector(x * other, y * other)

  bool __eq__(Vector other)
    return x == other.x and y == other.y

  Vector clone()
    return Vector(x, y)

  Vector clone(int n)
    return Vector(n, n)

  string __str__()
    return x + ", " + y

log((string)(Vector() + Vector(2, 3)))
# 2, 3

log((string)(Vector() + 3))
# 3, 3

log((string)(Vector() - Vector(2, 3)))
# -2, -3

log((string)(Vector() - 3))
# -3, -3

log((string)(Vector() * Vector(2, 3)))
# 0, 0

log((string)(Vector() * 3))
# 0, 0

log((string)(Vector(1, 1) + 3))
# 4, 4

log((string)(Vector().clone(10)))
# 10, 10

class Type1
class Type2
class Type3
class Type4
class Type5
class Type6

any type1 = Type1()
any type2 = Type2()
any type3 = Type3()
any type4 = Type4()
any type5 = Type5()
any type6 = Type6()

int full(any value)
  match value
    case Type1
      return 1
    case Type2
      return 2
    case Type3
      return 3
    case Type4
      return 4
    case Type5
      return 5
    case Type6
      return 6
    default
      return -1

log(full(type1))
log(full(type2))
log(full(type3))
log(full(type4))
log(full(type5))
log(full(type6))

# 1
# 2
# 3
# 4
# 5
# 6

int middle(any value)
  match value
    case Type3
      return 3
    case Type4
      return 4
    case Type5
      return 5
    default
      return -1

log(middle(type1))
log(middle(type2))
log(middle(type3))
log(middle(type4))
log(middle(type5))
log(middle(type6))

# -1
# -1
# 3
# 4
# 5
# -1

int sparse(any value)
  match value
    case Type1
      return 1
    case Type3
      return 3
    case Type6
      return 6
    default
      return -1

log(sparse(type1))
log(sparse(type2))
log(sparse(type3))
log(sparse(type4))
log(sparse(type5))
log(sparse(type6))

# 1
# -1
# 3
# -1
# -1
# 6

int edges(any value)
  match value
    case Type1
      return 1
    case Type6
      return 6
    default
      return -1

log(edges(type1))
log(edges(type2))
log(edges(type3))
log(edges(type4))
log(edges(type5))
log(edges(type6))

# 1
# -1
# -1
# -1
# -1
# 6

int offset(any value)
  match value
    case Type2
      return 2
    case Type4
      return 4
    case Type5
      return 5
    default
      return -1

log(offset(type1))
log(offset(type2))
log(offset(type3))
log(offset(type4))
log(offset(type5))
log(offset(type6))

# -1
# 2
# -1
# 4
# 5
# -1

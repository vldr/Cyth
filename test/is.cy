class A
    int field

class B
    float field

class C
    int field

any Thing = A | B | C | string | int[]
Thing a = null

log(a is A)
log(a is B)
log(a is C)
log(a is Thing)
log(a is string)
log(a is int[])

# 0
# 0
# 0
# 0
# 0
# 0

a = A()

log(a is A)
log(a is B)
log(a is Thing)
log(a is string)
log(a is int[])

# 1
# 0
# 1
# 0
# 0

a = "hello"

log(a is A)
log(a is B)
log(a is C)
log(a is Thing)
log(a is string)
log(a is int[])

# 0
# 0
# 0
# 1
# 1
# 0

a = (int[]) [1, 2, 3]

log(a is A)
log(a is B)
log(a is C)
log(a is Thing)
log(a is string)
log(a is int[])

# 0
# 0
# 0
# 1
# 0
# 1
alias Int = int

log(Int())
log(Int(10))

# 0
# 10

alias Float = float

log(Float())
log(Float(10))
log(Float(12.5))

# 0
# 10
# 12.5

alias String = string

log(String())
log(String("Hello World"))

# 
# Hello World

alias Array = int[]

log((string)Array())
log((string)Array([1,2,3]))

# []
# [1, 2, 3]

class Object
    void __init__(int test)
        log(test)

alias AliasedObject = Object
AliasedObject(10)

# 10
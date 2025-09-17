import "env"
    void log(string n)

log("A: " + (int[]) [1,2,3])
log((string) (int[]) [1,2,3])

class Object
    int a = 12
    float b = 62.25
    bool c = true
    char d = 'a'
    string e = "hello world"
    any f
    void(string) g

Object object

log("B: " + object)
log((string) object)

object = Object()

log("C: " + object)
log((string) object)

object.f = "test"
object.g = log

log("D: " + object)
log((string) object)

# A: [1, 2, 3]
# [1, 2, 3]

# B: null
# null

# C: Object{a = 12, b = 62.25, c = true, d = a, e = hello world, f = null, g = null}
# Object{a = 12, b = 62.25, c = true, d = a, e = hello world, f = null, g = null}

# D: Object{a = 12, b = 62.25, c = true, d = a, e = hello world, f = any, g = void(string)}
# Object{a = 12, b = 62.25, c = true, d = a, e = hello world, f = any, g = void(string)}
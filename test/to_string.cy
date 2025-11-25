import "env"
    void log(string n)
    void log(int n)

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
object.g = (void(string))log

log("D: " + object)
log((string) object)

# A: [1, 2, 3]
# [1, 2, 3]
# B: null
# null
# C: Object(\n a = 12,\n b = 62.25,\n c = true,\n d = a,\n e = hello world,\n f = null,\n g = null\n)
# Object(\n a = 12,\n b = 62.25,\n c = true,\n d = a,\n e = hello world,\n f = null,\n g = null\n)
# D: Object(\n a = 12,\n b = 62.25,\n c = true,\n d = a,\n e = hello world,\n f = any,\n g = void(string)\n)
# Object(\n a = 12,\n b = 62.25,\n c = true,\n d = a,\n e = hello world,\n f = any,\n g = void(string)\n)
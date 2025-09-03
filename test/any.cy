import "env"
    any get(int key)
    any set(int key, any value)

    void log(string n)

class Object
    int data

    void __init__(int data)
        this.data = data

set(0, Object(12))
set(1, (int[]) [22, 44, 66])
set(2, "hello world")

set(3, (any) Object(12))
set(4, (any) (int[]) [22, 44, 66])
set(5, (any) "hello world")

Object a = (Object) get(0)
int[] b = (int[]) get(1)
string c = (string) get(2)

Object d = (Object) get(3)
int[] e = (int[]) get(4)
string f = (string) get(5)

log((string) a.data)
log((string) b[0])
log((string) b[1])
log((string) b[2])
log(c)

log((string) d.data)
log((string) e[0])
log((string) e[1])
log((string) e[2])
log(f)

# 12
# 22
# 44
# 66
# hello world
# 12
# 22
# 44
# 66
# hello world
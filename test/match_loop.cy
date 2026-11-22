
class Obj1
    int field = 10

class Obj2
    float field = 30.5

any Apple = string | bool | void() | Obj1 | Obj2
any Snapple = int | float

void test()
    log("hi")

Apple[] apples
apples.push(test)
apples.push(Obj1())
apples.push(Obj2())
apples.push(true)
apples.push(apples[0])

for Apple a in apples
    match a
        case string p
            log(p)
        case bool
            log(a)
        case void() 
            a()
        case Obj1
            log(a.field)
        case Obj2
            log(a.field)

# hi
# 10
# 30.5
# 1
# hi

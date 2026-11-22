any Thing = int | float | char | bool | void() | int(int) | void(Object)

Thing a = 10
match a
    case int
        log(a)
    default

# 10


a = 12.625
match a
    case float
        log(a)
    default

# 12.625

a = 'a'
match a
    case char
        log((string)a)
    default
    

# a

a = true
match a
    case bool
        log(a)
    default


# 1

void test()
    log("test")

a = test
match a
    case void()
        a()
    default


# test

class Object
    void test()
        log("test2")

a = Object.test
match a
    case void(Object)
        a(null)
    default


a = Object().test
match a
    case void(Object)
        a(null)
    default


# test2
# test2

a = 10.hash
match a
    case int(int)
        log(a(10))
    default


# 10

void test3()
    log("test3")

void() fnptr = test3
a = fnptr
((void())a)()

# test3

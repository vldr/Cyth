any a = 10
match a
    case int
        log(a)

# 10


a = 12.625
match a
    case float
        log(a)

# 12.625

a = 'a'
match a
    case char
        log((string)a)

# a

a = true
match a
    case bool
        log(a)

# 1

void test()
    log("test")

a = test
match a
    case void()
        a()

# test

class Object
    void test()
        log("test2")

a = Object.test
match a
    case void(Object)
        a(null)

a = Object().test
match a
    case void(Object)
        a(null)

# test2
# test2

a = 10.hash
match a
    case int(int)
        log(a(10))

# 10

void test3()
    log("test3")

void() fnptr = test3
a = fnptr
((void())a)()

# test3

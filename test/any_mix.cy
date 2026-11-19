any a = 10
log((int)a)

# 10

a = 12.625
log((float)a)

# 12.625

a = 'a'
log((string)(char)a)

# a

a = true
log((bool)a)

# 1

void test()
    log("test")

a = test
((void())a)()

# test

class Object
    void test()
        log("test2")

a = Object.test
((void(Object))a)(null)

a = Object().test
((void(Object))a)(null)

# test2
# test2

a = 10.hash
log(((int(int))a)(10))

# 10



class Test
    void __get__(any a)
        log((string)(Test)a)

    void __set__(int p, any a)
        log((string)(Test)a)

    void __set__(int p, char a)
        log("" + a)

Test()[Test()]
Test()[0] = Test()
Test()[0] = 68

# Test()
# Test()
# D
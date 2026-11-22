class Test
    void __get__(Thing a)
        log((string)(Test)a)

    void __set__(int p, Thing a)
        log((string)(Test)a)

    void __set__(int p, char a)
        log("" + a)

any Thing = Test

Test()[Test()]
Test()[0] = Test()
Test()[0] = 68

# Test()
# Test()
# D
import "env"
    void log(string n)
    void log(int n)

class Test
    void __get__(any a)
        log((string)(Test)a)

    void __set__(int p, any a)
        log((string)(Test)a)

    void __set__(int p, int a)
        log("3")

Test()[Test()]
Test()[0] = Test()
Test()[0] = 3

# Test()
# Test()
# 3
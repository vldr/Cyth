import "env"
    void log(bool n)

class A
    int a

    void __init__(int a)
        this.a = a

    bool __eq__(any test)
        if (A)test
            return ((A)test).a == a
            
        return not test

log(A(10) == A(12))
log(A(10) == A(10))
log(A(10) == null)

# 0
# 1
# 0
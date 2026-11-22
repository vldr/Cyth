class A
    int a

    void __init__(int a)
        this.a = a

    bool __eq__(Thing test)
        if (A)test
            return ((A)test).a == a
            
        return not test

any Thing = A

log(A(10) == A(12))
log(A(10) == A(10))
log(A(10) == null)

# 0
# 1
# 0